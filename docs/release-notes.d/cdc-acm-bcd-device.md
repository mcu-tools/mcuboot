- With `CONFIG_BOOT_SERIAL_CDC_ACM`, the serial recovery USB device now
  reports the bootloader major and minor version in `bcdDevice`
  (MCUboot 2.5 reads as 0x0205) instead of the Zephyr kernel version.
