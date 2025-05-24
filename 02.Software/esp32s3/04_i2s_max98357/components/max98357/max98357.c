#include <stdio.h>
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "max98357.h"

#define I2S_BLCK GPIO_NUM_12      // I2S bit clock io number
#define I2S_WS GPIO_NUM_13        // I2S word select io number
#define I2S_DOUT GPIO_NUM_14      // I2S data out io number
#define I2S_DIN I2S_PIN_NO_CHANGE // I2S data in io number

#define SAMPLE_RATE (44100)
#define BUFF_SIZE 1024

void i2s_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,                   // 必须与PCM一致
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 对应pcm_s16le
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // 单声道的正确设置
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .intr_alloc_flags = 0,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BLCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_DIN};

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    
}
