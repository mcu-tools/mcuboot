- Zephyr RAM load mode now uses a `mcuboot,image-ram` chosen
  devicetree node to specify the RAM region to load the image
  into.
- Manually setting the `BOOT_IMAGE_EXECUTABLE_RAM_START` and
  `BOOT_IMAGE_EXECUTABLE_RAM_SIZE` Kconfigs is now deprecated.
