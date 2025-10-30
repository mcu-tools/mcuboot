/*
 * Copyright (c) 2025 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boot_rng.h"

static const struct device *entropy_dev = NULL;
static bool initialized = false;

int
generator_hw_rng(uint32_t *val)
{
    *val = sys_rand32_get();

    return 0;
}
