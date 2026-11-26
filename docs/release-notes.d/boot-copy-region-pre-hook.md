- Added the boot_copy_region_pre_hook() image access hook. It is
  called from boot_copy_image() right before the primary slot is
  erased and the image is copied into it, in overwrite-only mode and
  for bootstrap installs when MCUBOOT_BOOTSTRAP is enabled. This hook
  placement is ideal for vendors with dedicated hardware encryption
  engines who want to implement encrypted XIP. Such engines typically
  cannot use the SWAP upgrade mode, making OVERWRITE_ONLY the only viable
  upgrade strategy. Builds with MCUBOOT_IMAGE_ACCESS_HOOKS that implement
  the image access hooks must now provide this function.