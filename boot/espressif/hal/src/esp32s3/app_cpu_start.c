/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_cpu_start.h"

#include "esp_rom_sys.h"
#include "soc/dport_reg.h"
#include "soc/gpio_periph.h"
#include "soc/rtc_periph.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32s3/rom/cache.h"
#include "esp32s3/rom/uart.h"
#include "esp_cpu.h"
#include "esp_log.h"

static const char *TAG = "app_cpu_start";

void appcpu_start(uint32_t entry_addr)
{
    ESP_LOGI(TAG, "Starting APPCPU");

    esp_cpu_unstall(1);

    // Enable clock and reset APP CPU. Note that OpenOCD may have already
    // enabled clock and taken APP CPU out of reset. In this case don't reset
    // APP CPU again, as that will clear the breakpoints which may have already
    // been set.
    if (!REG_GET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_CLKGATE_EN)) {
        REG_SET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_CLKGATE_EN);
        REG_CLR_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RUNSTALL);
        REG_SET_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RESETING);
        REG_CLR_BIT(SYSTEM_CORE_1_CONTROL_0_REG, SYSTEM_CONTROL_CORE_1_RESETING);
    }

    ets_set_appcpu_boot_addr(entry_addr);
    esp_rom_delay_us(10000);
    uart_tx_wait_idle(0);
    ESP_LOGI(TAG, "APPCPU start sequence complete");
}
