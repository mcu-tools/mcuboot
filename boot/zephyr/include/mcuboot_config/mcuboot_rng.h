/*
 * Copyright (c) 2025 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __MCUBOOT_RNG_H__
#define __MCUBOOT_RNG_H__

#include "boot_rng.h"

#define MCUBOOT_RNG(...) generator_hw_rng(__VA_ARGS__)

#endif /* __MCUBOOT_RNG_H__ */