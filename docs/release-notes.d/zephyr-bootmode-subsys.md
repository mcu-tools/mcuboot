- Zephyr: the boot mode entrance (`CONFIG_BOOT_SERIAL_BOOT_MODE` and
  `CONFIG_BOOT_FIRMWARE_LOADER_BOOT_MODE`) now also works with Zephyr's
  `CONFIG_BOOT_MODE` subsystem and `<zephyr/bootmode.h>`, which replace the
  deprecated `CONFIG_RETENTION_BOOT_MODE` and `<zephyr/retention/bootmode.h>`.
  Older Zephyr versions are still supported.
