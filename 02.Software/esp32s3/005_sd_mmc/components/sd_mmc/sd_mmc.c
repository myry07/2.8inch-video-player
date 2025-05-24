#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "esp_log.h"

#define MAX_CHAR_SIZE 64

#define SD_MMC_CLK GPIO_NUM_39
#define SD_MMC_CMD GPIO_NUM_38
#define SD_MMC_D0 GPIO_NUM_40
#define SD_MMC_D1 -1
#define SD_MMC_D2 -1
#define SD_MMC_D3 -1
#define SD_MMC_WIDTH 1

#define MAX_SAMPLES 1024

static const char *TAG = "SD_MMC";

#define MOUNT_POINT "/sdcard"

sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;

static esp_err_t s_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

esp_err_t read_pcm_file(const char *path, int16_t *buffer, size_t max_samples, size_t *samples_read)
{
    ESP_LOGI(TAG, "Opening PCM file %s for reading", path);
    FILE *f = fopen(path, "rb"); // 二进制读取
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file");
        return ESP_FAIL;
    }

    size_t read = fread(buffer, sizeof(int16_t), max_samples, f);
    fclose(f);

    *samples_read = read;

    ESP_LOGI(TAG, "Read %zu samples from PCM file", read);
    return ESP_OK;
}

void sd_mmc_init(void)
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    ESP_LOGI(TAG, "Initializing SD card");

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // host.slot = SDMMC_HOST_SLOT_0; // 关键：ESP32-S3 只能用 SLOT_0

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    slot_config.width = SD_MMC_WIDTH;
    slot_config.clk = SD_MMC_CLK;
    slot_config.cmd = SD_MMC_CMD;
    slot_config.d0 = SD_MMC_D0;
    slot_config.d1 = SD_MMC_D1;
    slot_config.d2 = SD_MMC_D2;
    slot_config.d3 = SD_MMC_D3;

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);
}

void sd_mmc_deinit(void)
{
    // All done, unmount partition and disable SDMMC peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
}

/* <---------------------------------------------------------------> */

void example_test(void)
{
    sd_mmc_init();

    esp_err_t ret;

    const char *file_hello = MOUNT_POINT "/hello.txt";
    char data[MAX_CHAR_SIZE];
    snprintf(data, MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
    ret = s_write_file(file_hello, data);
    if (ret != ESP_OK)
    {
        return;
    }

    const char *file_foo = MOUNT_POINT "/foo.txt";
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0)
    {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0)
    {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    ret = s_read_file(file_foo);
    if (ret != ESP_OK)
    {
        return;
    }

    const char *file_nihao = MOUNT_POINT "/nihao.txt";
    memset(data, 0, MAX_CHAR_SIZE);
    snprintf(data, MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name);
    ret = s_write_file(file_nihao, data);
    if (ret != ESP_OK)
    {
        return;
    }

    // Open file for reading
    ret = s_read_file(file_nihao);
    if (ret != ESP_OK)
    {
        return;
    }

    sd_mmc_deinit();
}

void i2s_pcm(void)
{
    size_t samples_read = 0;

    int16_t *pcm_buffer = malloc(MAX_SAMPLES * sizeof(int16_t));
    if (pcm_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        return;
    }

    if (read_pcm_file("/sdcard/test.pcm", pcm_buffer, MAX_SAMPLES, &samples_read) == ESP_OK)
    {
        ESP_LOGI(TAG, "i2s start !");
        // 后续播放逻辑可以在这里加
    }

    free(pcm_buffer);   
}