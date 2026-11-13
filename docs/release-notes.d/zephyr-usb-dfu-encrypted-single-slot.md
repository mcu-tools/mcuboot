- Zephyr: encrypted images can now be used with USB DFU when MCUboot is
  built with a single application slot. The image is decrypted in place
  on the first boot after the download. This needs `BOOT_VALIDATE_SLOT0`
  or `BOOT_VALIDATE_SLOT0_ONCE`, since the decryption is triggered by a
  failed validation.
