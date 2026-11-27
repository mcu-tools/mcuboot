- Without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, the image left in the primary
  slot by a resumed swap is now validated before the swap is finalized.
  If it is not valid, its header is erased, so the boot fails on that
  and every later boot. A stale or corrupt swap trailer could
  previously resume a swap that was never started, installing the
  secondary slot's image in the primary slot and booting it without
  validating it.
