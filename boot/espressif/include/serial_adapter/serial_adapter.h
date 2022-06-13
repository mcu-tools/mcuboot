/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Serial write implementation used by MCUboot boot serial structure
 * in boot_serial.h
 */
void console_write(const char *str, int cnt);

/**
 * Serial read implementation used by MCUboot boot serial structure
 * in boot_serial.h
 */
int console_read(char *str, int str_cnt, int *newline);

/**
 * Initialize GPIOs used by console serial read/write
 */
void boot_console_init(void);

/**
 * Check if serial recovery detection pin is active
 */
bool boot_serial_detect_pin(void);
