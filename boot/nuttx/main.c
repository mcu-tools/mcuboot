/****************************************************************************
 * boot/nuttx/main.c
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

#include <nuttx/config.h>

#include <syslog.h>

#include <sys/boardctl.h>

#include <bootutil/bootutil.h>
#include <bootutil/image.h>

#include "flash_map_backend/flash_map_backend.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Should we perform board-specific driver initialization?  There are two
 * ways that board initialization can occur:  1) automatically via
 * board_late_initialize() during bootupif CONFIG_BOARD_LATE_INITIALIZE
 * or 2).
 * via a call to boardctl() if the interface is enabled
 * (CONFIG_BOARDCTL=y).
 * If this task is running as an NSH built-in application, then that
 * initialization has probably already been performed otherwise we do it
 * here.
 */

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * do_boot
 ****************************************************************************/

static void do_boot(struct boot_rsp *rsp)
{
  const struct flash_area *flash_area;
  struct boardioc_boot_info_s info;
  int area_id;
  int ret;

  area_id = flash_area_id_from_image_offset(rsp->br_image_off);

  ret = flash_area_open(area_id, &flash_area);
  assert(ret == OK);

  syslog(LOG_INFO, "Booting from %s...\n", flash_area->fa_mtd_path);

  info.path        = flash_area->fa_mtd_path;
  info.header_size = rsp->br_hdr->ih_hdr_size;

  flash_area_close(flash_area);

  if (boardctl(BOARDIOC_BOOT_IMAGE, (uintptr_t)&info) != OK)
    {
      syslog(LOG_ERR, "Failed to load application image!\n");
      FIH_PANIC;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct boot_rsp rsp;
  fih_int fih_rc = FIH_FAILURE;

#ifndef CONFIG_NSH_ARCHINIT
  /* Perform architecture-specific initialization (if configured) */
  boardctl(BOARDIOC_INIT, 0);

#  ifdef CONFIG_BOARDCTL_FINALINIT
  /* Perform architecture-specific final-initialization (if configured) */
  boardctl(BOARDIOC_FINALINIT, 0);

#  endif
#endif

  syslog(LOG_INFO, "*** Booting MCUboot build %s ***\n", CONFIG_MCUBOOT_VERSION);

  FIH_CALL(boot_go, fih_rc, &rsp);

  if (fih_not_eq(fih_rc, FIH_SUCCESS))
    {
      syslog(LOG_ERR, "Unable to find bootable image\n");
      FIH_PANIC;
    }

  do_boot(&rsp);

  while (1);
}
