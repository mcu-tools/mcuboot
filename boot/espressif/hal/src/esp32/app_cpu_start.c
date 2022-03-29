/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_cpu_start.h"

#include "soc/dport_reg.h"
#include "soc/gpio_periph.h"
#include "soc/rtc_periph.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32/rom/cache.h"
#include "esp32/rom/uart.h"
#include "esp_cpu.h"
#include "esp_log.h"

static const char *TAG = "app_cpu_start";

void appcpu_start(uint32_t entry_addr)
{
    ESP_LOGI(TAG, "Starting APPCPU");

    Cache_Flush(1);
    Cache_Read_Enable(1);

    esp_cpu_unstall(1);

    DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_B_REG, DPORT_APPCPU_CLKGATE_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_C_REG, DPORT_APPCPU_RUNSTALL);
    DPORT_SET_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_APPCPU_CTRL_A_REG, DPORT_APPCPU_RESETTING);

    ets_set_appcpu_boot_addr(entry_addr);
    ets_delay_us(10000);
    uart_tx_wait_idle(0);
    ESP_LOGI(TAG, "APPCPU start sequence complete");
}
