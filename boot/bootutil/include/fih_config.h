/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 */

#ifndef __FIH_CONFIG_H__
#define __FIH_CONFIG_H__

/*
 * MCUboot-specific configuration adapter for the fault_injection_hardening
 * library.  This file maps the MCUBOOT_FIH_PROFILE_* build options to the
 * generic FIH_PROFILE_* macros consumed by fault_injection_hardening.h.
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_FIH_PROFILE_HIGH)
#  define FIH_PROFILE_HIGH
#elif defined(MCUBOOT_FIH_PROFILE_MEDIUM)
#  define FIH_PROFILE_MEDIUM
#elif defined(MCUBOOT_FIH_PROFILE_LOW)
#  define FIH_PROFILE_LOW
#elif defined(MCUBOOT_FIH_PROFILE_OFF)
#  define FIH_PROFILE_OFF
#else
/* Default: fault injection hardening disabled */
#  define FIH_PROFILE_OFF
#endif

#endif /* __FIH_CONFIG_H__ */
