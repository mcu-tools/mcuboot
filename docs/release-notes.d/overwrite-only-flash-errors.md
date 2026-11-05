- Overwrite-only upgrades now stop when erasing the primary slot or
  reading the size of the new image fails, before anything is copied
  and before the secondary slot is cleared, and `boot_go()` returns a
  failure instead of booting a partly copied image. The upgrade is
  retried on the next boot. Previously these errors, and a failed copy
  without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, were caught only by
  `assert()`.
