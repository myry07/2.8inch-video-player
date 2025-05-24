#ifndef SD_MMC_H_
#define SD_MMC_H_

#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#define MOUNT_POINT "/sdcard"

extern sdmmc_card_t *card;

void sd_mmc_init(void);
void sd_mmc_deinit(void);
void example_test(void);
void i2s_pcm(void);

#endif
