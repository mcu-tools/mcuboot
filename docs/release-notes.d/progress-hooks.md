- Added `MCUBOOT_PROGRESS_HOOKS` (`CONFIG_MCUBOOT_PROGRESS_HOOKS` on
  Zephyr). When set, MCUboot calls `mcuboot_progress()` as a swap
  (scratch, move or offset) or a serial recovery upload advances, so a
  board can show progress during these long operations.
