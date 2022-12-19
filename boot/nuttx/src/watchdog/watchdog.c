/****************************************************************************
 * boot/nuttx/src/watchdog/watchdog.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "watchdog/watchdog.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <nuttx/timers/watchdog.h>

#include <bootutil/bootutil_log.h>

/****************************************************************************
 * Public Functions
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

void mcuboot_watchdog_feed(void)
{
  int fd;
  int ret;

  fd = open(CONFIG_MCUBOOT_WATCHDOG_DEVPATH, O_RDONLY);
  if (fd < 0)
    {
      BOOT_LOG_ERR("Failed to open %s", CONFIG_MCUBOOT_WATCHDOG_DEVPATH);

      return;
    }

  ret = ioctl(fd, WDIOC_KEEPALIVE, 0);
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Failed to ping watchdog device: %d", errcode);
    }

  close(fd);
}

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

int mcuboot_watchdog_init(void)
{
  int fd;
  int ret;

  fd = open(CONFIG_MCUBOOT_WATCHDOG_DEVPATH, O_RDONLY);
  if (fd < 0)
    {
      BOOT_LOG_ERR("Failed to open %s", CONFIG_MCUBOOT_WATCHDOG_DEVPATH);
      goto errout;
    }

  ret = ioctl(fd, WDIOC_SETTIMEOUT, (unsigned long)CONFIG_MCUBOOT_WATCHDOG_TIMEOUT);
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Failed to set timeout in watchdog device: %d", errcode);
      goto errout_with_dev;
    }

  ret = ioctl(fd, WDIOC_START, 0);
  if (ret < 0)
    {
      int errcode = errno;

      BOOT_LOG_ERR("Failed to start watchdog device: %d", errcode);
      goto errout_with_dev;
    }

  close(fd);
  return OK;
  
errout_with_dev:
  close(fd);
errout:
  return ERROR;
}
