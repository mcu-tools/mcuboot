/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZCBOR_DEBUG_H__
#define ZCBOR_DEBUG_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "zcbor_common.h"

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((used))
static void zcbor_print_compare_lines(const uint8_t *str1, const uint8_t *str2, uint32_t size)
{
	for (uint32_t j = 0; j < size; j++) {
		printk ("%x ", str1[j]);
	}
	printk("\r\n");
	for (uint32_t j = 0; j < size; j++) {
		printk ("%x ", str2[j]);
	}
	printk("\r\n");
	for (uint32_t j = 0; j < size; j++) {
		printk ("%x ", str1[j] != str2[j]);
	}
	printk("\r\n");
	printk("\r\n");
}

__attribute__((used))
static void zcbor_print_compare_strings(const uint8_t *str1, const uint8_t *str2, uint32_t size)
{
	for (uint32_t i = 0; i <= size / 16; i++) {
		printk("line %d (char %d)\r\n", i, i*16);
		zcbor_print_compare_lines(&str1[i*16], &str2[i*16],
			MIN(16, (size - i*16)));
	}
	printk("\r\n");
}

__attribute__((used))
static void zcbor_print_compare_strings_diff(const uint8_t *str1, const uint8_t *str2, uint32_t size)
{
	bool printed = false;
	for (uint32_t i = 0; i <= size / 16; i++) {
		if (memcmp(&str1[i*16], &str2[i*16], MIN(16, (size - i*16)) != 0)) {
			printk("line %d (char %d)\r\n", i, i*16);
			zcbor_print_compare_lines(&str1[i*16], &str2[i*16],
				MIN(16, (size - i*16)));
			printed = true;
		}
	}
	if (printed) {
		printk("\r\n");
	}
}

#ifdef __cplusplus
}
#endif

#endif /* ZCBOR_DEBUG_H__ */
