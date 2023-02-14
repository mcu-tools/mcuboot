/*
 * Copyright (c) 2023 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "board.h"
#include "hpm_debug_console.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/mcuboot_status.h"
#include "flash_map_backend/flash_map_backend.h"
#include "hpm_bootutil_ex.h"

extern int swap_set_image_ok(uint8_t image_index);
#define LED_FLASH_PERIOD_IN_MS 300

int main(void)
{
#ifdef MCUBOOT_APP_UPGRADE_MODE
    uint8_t image_ok = -1;
#endif
    board_init();
    board_init_led_pins();

    board_timer_create(LED_FLASH_PERIOD_IN_MS, board_led_toggle);
#ifdef MCUBOOT_APP_UPGRADE_MODE
    printf("hpmicro hello world app for mcuboot(UPGRADE MODE)\n");
    if (boot_read_image_state_by_id(0, &image_ok)) {
        printf("failed read image state\n");
        while (1) {

        }
    }
    if (image_ok == 1) {
        printf("image upgrade is permanent\n");
    } else {
        printf("image ok is %d\n", image_ok);
        printf("writting image ok flag to flash, if failed revert proccess will run at next reboot\n");
        swap_set_image_ok(0);
        printf("written image ok flag success, next reboot upgrade will be permanent\n");
    }
#else
    printf("hpmicro hello world app for mcuboot(BOOT MODE)\n");
#endif
    return 0;
}
