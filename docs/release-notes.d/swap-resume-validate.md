- Without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, the image left in the primary
  slot by a resumed swap is now validated, and the boot fails if it is
  not valid. A stale or corrupt swap trailer could previously resume a
  swap that was never started, installing the secondary slot's image
  in the primary slot and booting it without validating it.
