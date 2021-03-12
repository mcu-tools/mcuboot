/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "soc/soc.h"
#include "soc/rtc.h"
#include "soc/rtc_periph.h"
#include "soc/dport_reg.h"
#include "soc/efuse_periph.h"
#include "soc/gpio_periph.h"

rtc_vddsdio_config_t rtc_vddsdio_get_config(void)
{
    rtc_vddsdio_config_t result;
    uint32_t sdio_conf_reg = REG_READ(RTC_CNTL_SDIO_CONF_REG);
    result.drefh = (sdio_conf_reg & RTC_CNTL_DREFH_SDIO_M) >> RTC_CNTL_DREFH_SDIO_S;
    result.drefm = (sdio_conf_reg & RTC_CNTL_DREFM_SDIO_M) >> RTC_CNTL_DREFM_SDIO_S;
    result.drefl = (sdio_conf_reg & RTC_CNTL_DREFL_SDIO_M) >> RTC_CNTL_DREFL_SDIO_S;
    if (sdio_conf_reg & RTC_CNTL_SDIO_FORCE) {
        /* Get configuration from RTC */
        result.force = 1;
        result.enable = (sdio_conf_reg & RTC_CNTL_XPD_SDIO_REG_M) >> RTC_CNTL_XPD_SDIO_REG_S;
        result.tieh = (sdio_conf_reg & RTC_CNTL_SDIO_TIEH_M) >> RTC_CNTL_SDIO_TIEH_S;
        return result;
    }
    uint32_t efuse_reg = REG_READ(EFUSE_BLK0_RDATA4_REG);
    if (efuse_reg & EFUSE_RD_SDIO_FORCE) {
        /* Get configuration from EFUSE */
        result.force = 0;
        result.enable = (efuse_reg & EFUSE_RD_XPD_SDIO_REG_M) >> EFUSE_RD_XPD_SDIO_REG_S;
        result.tieh = (efuse_reg & EFUSE_RD_SDIO_TIEH_M) >> EFUSE_RD_SDIO_TIEH_S;
        /* DREFH/M/L eFuse are used for EFUSE_ADC_VREF instead. Therefore tuning
         * will only be available on older chips that don't have EFUSE_ADC_VREF
         */
        if(REG_GET_FIELD(EFUSE_BLK0_RDATA3_REG ,EFUSE_RD_BLK3_PART_RESERVE) == 0){
            /* BLK3_PART_RESERVE indicates the presence of EFUSE_ADC_VREF
             * in this case, DREFH/M/L are also set from EFUSE
             */
            result.drefh = (efuse_reg & EFUSE_RD_SDIO_DREFH_M) >> EFUSE_RD_SDIO_DREFH_S;
            result.drefm = (efuse_reg & EFUSE_RD_SDIO_DREFM_M) >> EFUSE_RD_SDIO_DREFM_S;
            result.drefl = (efuse_reg & EFUSE_RD_SDIO_DREFL_M) >> EFUSE_RD_SDIO_DREFL_S;
        }
        return result;
    }

    /* Otherwise, VDD_SDIO is controlled by bootstrapping pin */
    uint32_t strap_reg = REG_READ(GPIO_STRAP_REG);
    result.force = 0;
    result.tieh = (strap_reg & BIT(5)) ? RTC_VDDSDIO_TIEH_1_8V : RTC_VDDSDIO_TIEH_3_3V;
    result.enable = 1;
    return result;
}

void rtc_vddsdio_set_config(rtc_vddsdio_config_t config)
{
    uint32_t val = 0;
    val |= (config.force << RTC_CNTL_SDIO_FORCE_S);
    val |= (config.enable << RTC_CNTL_XPD_SDIO_REG_S);
    val |= (config.drefh << RTC_CNTL_DREFH_SDIO_S);
    val |= (config.drefm << RTC_CNTL_DREFM_SDIO_S);
    val |= (config.drefl << RTC_CNTL_DREFL_SDIO_S);
    val |= (config.tieh << RTC_CNTL_SDIO_TIEH_S);
    val |= RTC_CNTL_SDIO_PD_EN;
    REG_WRITE(RTC_CNTL_SDIO_CONF_REG, val);
}
