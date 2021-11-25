/****************************************************************************
 * boot/nuttx/include/mcuboot_config/mcuboot_logging.h
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

#ifndef __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_LOGGING_H
#define __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_LOGGING_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <syslog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MCUBOOT_LOG_MODULE_DECLARE(...)
#define MCUBOOT_LOG_MODULE_REGISTER(...)

#define MCUBOOT_LOG_ERR(format, ...) \
    syslog(LOG_ERR, "%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_WRN(format, ...) \
    syslog(LOG_WARNING, "%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_INF(format, ...) \
    syslog(LOG_INFO, "%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#define MCUBOOT_LOG_DBG(format, ...) \
    syslog(LOG_DEBUG, "%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)

#endif /* __BOOT_NUTTX_INCLUDE_MCUBOOT_CONFIG_MCUBOOT_LOGGING_H */
