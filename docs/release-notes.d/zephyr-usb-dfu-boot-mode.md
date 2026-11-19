- Zephyr: added the `BOOT_USB_DFU_BOOT_MODE` USB DFU entrance method.
  When the application requests it through Zephyr's boot mode retention
  system, the bootloader stays in USB DFU mode until an image is
  downloaded. It mirrors `BOOT_SERIAL_BOOT_MODE` for serial recovery and
  can be combined with the wait and GPIO entrance methods.
