/****************************************************************************
 * boot/nuttx/include/watchdog/watchdog.h
 *
 * Copyright (c) 2021 Espressif Systems (Shanghai) Co., Ltd.
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
 *
 ****************************************************************************/

#ifndef __BOOT_NUTTX_INCLUDE_WATCHDOG_WATCHDOG_H
#define __BOOT_NUTTX_INCLUDE_WATCHDOG_WATCHDOG_H

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: mcuboot_watchdog_feed
 *
 * Description:
 *   Reset the watchdog timer to the current timeout value, preventing any
 *   imminent watchdog timeouts.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void mcuboot_watchdog_feed(void);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: mcuboot_watchdog_init
 *
 * Description:
 *   Initialize the watchdog timer by setting the trigger timeout and 
 *   starting it.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   OK on success, ERROR if not.
 *
 ****************************************************************************/

int mcuboot_watchdog_init(void);

#endif /* __BOOT_NUTTX_INCLUDE_WATCHDOG_WATCHDOG_H */
