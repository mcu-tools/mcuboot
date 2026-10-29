- With `MCUBOOT_DIRECT_XIP_REVERT` or `MCUBOOT_RAM_LOAD_REVERT`, a
  failure to read the trailer of the active slot no longer erases that
  slot. The slot is skipped for this boot instead; previously a
  transient read error could destroy the only good image.
