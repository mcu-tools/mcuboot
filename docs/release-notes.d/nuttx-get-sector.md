- NuttX: fix `flash_area_get_sector()`, which left the returned sector
  uninitialized (and reported success) for any offset other than zero.
  This broke `MCUBOOT_SWAP_USING_MOVE`.
