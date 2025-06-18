 - Fixed issue with platforms that have
   MCUBOOT_SUPPORT_DEV_WITHOUT_ERASE set that did not scramble
   (delete) data sections from the trailer that should have been
   deleted.
 - Fixed issue with boot_scramble_region escaping flash area due
   to error in the range check.
