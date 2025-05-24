#include "sd_mmc.h"

void app_main(void)
{
    sd_mmc_init();
    i2s_pcm();
    sd_mmc_deinit();
}