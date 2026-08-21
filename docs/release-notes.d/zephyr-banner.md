- Zephyr MCUboot boot banner has been fixed after changes
  made to the boot banner after Zephyr 4.4. Note that the order
  has changed, previously the MCUboot version would be output
  first, now the Zephyr version (if `CONFIG_BOOT_BANNER` if
  enabled) will be output first.
