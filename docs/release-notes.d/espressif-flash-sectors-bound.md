- Fixed an out-of-bounds write in the Espressif port's
  `flash_area_get_sectors()` when an image slot or scratch area spans
  more than `MCUBOOT_MAX_IMG_SECTORS` sectors. The port now fails the
  call with `-ENOMEM` instead of overrunning the sector table, and the
  build rejects such slot and scratch sizes.
