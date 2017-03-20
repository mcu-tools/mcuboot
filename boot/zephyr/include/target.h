/*
 *  Copyright (C) 2017, Linaro Ltd
 *  SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_TARGETS_TARGET_
#define H_TARGETS_TARGET_

#if defined(MCUBOOT_TARGET_CONFIG)
#include MCUBOOT_TARGET_CONFIG
#else
#error "Board is currently not supported by bootloader"
#endif

#endif
