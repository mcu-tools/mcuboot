- Added `MCUBOOT_BOOTSTRAP_TRUST_CONFIRMED_PRIMARY`
  (`CONFIG_BOOT_BOOTSTRAP_TRUST_CONFIRMED_PRIMARY`), off by default.
  With bootstrap and without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, the
  bootstrap check skips the hash and signature verify of a primary
  whose header is valid and whose image_ok flag is set. An erased,
  invalid-header or interrupted-copy primary is still validated and
  bootstrapped. A confirmed primary corrupted or tampered with later
  boots unverified, so this option compromises security.
