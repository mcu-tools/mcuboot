/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

void start_cpu0_image(int image_index, int slot, unsigned int hdr_offset);
#ifdef CONFIG_ESP_MULTI_PROCESSOR_BOOT
void start_cpu1_image(int image_index, int slot, unsigned int hdr_offset);
#endif

void esp_app_image_load(int image_index, int slot, unsigned int hdr_offset, unsigned int *entry_addr);
