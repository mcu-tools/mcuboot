
/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_ARM_CLEANUP_
#define H_ARM_CLEANUP_

/**
 * Cleanup interrupt priority and interupt enable registers.
 */
void cleanup_arm_nvic(void);
#endif
