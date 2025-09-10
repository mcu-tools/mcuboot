- Fixed serial recovery with progressive erase for MCUboot modes of single
  updatable slot (`MCUBOOT_SINGLE_APPLICATION_SLOT`, `MCUBOOT_FIRMWARE_LOADER`,
  `MCUBOOT_SINGLE_APPLICATION_SLOT_RAM_LOAD`) which was previously failing due
  to attempting to access non-existent image status fields.
