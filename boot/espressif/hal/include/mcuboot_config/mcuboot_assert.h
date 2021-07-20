/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

extern void mcuboot_assert_handler(const char *file, int line, const char *func);
#define assert(arg)                                                 \
    do {                                                            \
        if (!(arg)) {                                               \
            mcuboot_assert_handler(__FILE__, __LINE__, __func__);   \
        }                                                           \
    } while(0)
