/*
 * Copyright (c) 2026 Intercreate, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_OS_
#define H_OS_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Return the low 32 bits of the system uptime in milliseconds. */
uint32_t os_uptime_get_ms_32(void);

#ifdef __cplusplus
}
#endif

#endif
