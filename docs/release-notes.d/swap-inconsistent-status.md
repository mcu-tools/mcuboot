- Without `MCUBOOT_VALIDATE_PRIMARY_SLOT`, an inconsistent swap status
  (a gap left by a failed status write) now makes the boot fail
  instead of resuming the swap from the wrong step and booting an
  unvalidated primary slot. The abort was previously only an
  `assert(0)`. The swap-move and swap-offset algorithms now detect
  such a gap too; their check could never fire before.
