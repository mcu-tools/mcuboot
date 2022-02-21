/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

void esp_app_image_load(int image_index, int slot, unsigned int hdr_offset, unsigned int *entry_addr);
