- Added support for automatically calculating the maximum number
  of sectors that are needed for a build by checking the erase
  sizes of the partitions using CMake for Zephyr. This behaviour
  can be reverted to the old manual behaviour by disabling
  ``CONFIG_BOOT_MAX_IMG_SECTORS_AUTO``
