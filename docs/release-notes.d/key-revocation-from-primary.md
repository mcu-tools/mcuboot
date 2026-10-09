- Added `MCUBOOT_KEY_REVOCATION_FROM_PRIMARY` (Zephyr:
  ``CONFIG_BOOT_KEY_REVOCATION_FROM_PRIMARY``), an optional, decentralized
  key-revocation mechanism for builds with more than one embedded
  verification key. When enabled, MCUboot treats the currently active
  primary-slot image as a local trust anchor: it determines which embedded
  key signed it and rejects a secondary-slot upgrade candidate signed with
  an older (lower-indexed) key, erasing the rejected candidate the same way
  a genuinely invalid signature is erased. This requires a swap-based
  upgrade strategy (`MCUBOOT_SWAP_USING_MOVE`, `MCUBOOT_SWAP_USING_SCRATCH`,
  or `MCUBOOT_SWAP_USING_OFFSET`) and is not available with
  `MCUBOOT_BUILTIN_KEY`, `MCUBOOT_BYPASS_KEY_MATCH`, or `MCUBOOT_HW_KEY`
  (the latter always resolves to key index 0, which would make the check a
  no-op). It requires no new image format, TLV entry, or persistent
  revocation state; the embedded key array order (oldest to newest) is a
  signing-process/key-management convention that must be maintained by the
  project adopting the feature. This mechanism only revokes a key after the
  primary slot has moved to an image signed with a newer key, and is
  complementary to, not a replacement for, anti-rollback/security-counter
  based downgrade prevention. See the "Key revocation" section of
  `docs/design.md` for details.
