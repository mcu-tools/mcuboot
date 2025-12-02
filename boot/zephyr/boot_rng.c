/*
 * Copyright (c) 2025 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boot_rng.h"

int
generator_hw_rng(uint32_t *val)
{
    *val = sys_rand32_get();

    return 0;
}
