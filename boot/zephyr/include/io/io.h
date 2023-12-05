/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Arm Limited
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_IO_
#define H_IO_

#include <stddef.h>

#ifdef CONFIG_SOC_FAMILY_NRF
#include <helpers/nrfx_reset_reason.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialises the configured LED.
 */
void io_led_init(void);

/*
 * Sets value of the configured LED.
 */
void io_led_set(int value);

/*
 * Checks if GPIO is set in the required way to remain in serial recovery mode
 *
 * @retval	false for normal boot, true for serial recovery boot
 */
bool io_detect_pin(void);

/*
 * Checks if board was reset using reset pin and if device should stay in
 * serial recovery mode
 *
 * @retval	false for normal boot, true for serial recovery boot
 */
bool io_detect_pin_reset(void);

/*
 * Checks board boot mode via retention subsystem and if device should stay in
 * serial recovery mode
 *
 * @retval	false for normal boot, true for serial recovery boot
 */
bool io_detect_boot_mode(void);

#ifdef CONFIG_SOC_FAMILY_NRF
static inline bool io_boot_skip_serial_recovery()
{
    uint32_t rr = nrfx_reset_reason_get();

    return !(rr == 0 || (rr & NRFX_RESET_REASON_RESETPIN_MASK));
}
#else
static inline bool io_boot_skip_serial_recovery()
{
    return false;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
