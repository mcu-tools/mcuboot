- Zephyr: USB DFU is now configured like serial recovery. `BOOT_USB_DFU`
  enables USB DFU support and pulls in the USB stack, while
  `BOOT_USB_DFU_WAIT` and `BOOT_USB_DFU_GPIO` are now independent and can
  be enabled together. Existing configurations must be updated:
  `BOOT_USB_DFU` has to be set alongside an entrance method, and
  `BOOT_USB_DFU_NO` no longer exists.
