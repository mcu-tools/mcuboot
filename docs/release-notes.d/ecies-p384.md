- Added ECIES-P384 image encryption (`MCUBOOT_ENCRYPT_EC384`), with
  HKDF and HMAC on SHA-384 and the new `IMAGE_TLV_ENC_EC384` (`0x35`).
  It is implemented for the PSA Crypto backend. On Zephyr,
  `CONFIG_BOOT_ENCRYPT_IMAGE` selects it when
  `CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P384` is used, and imgtool produces
  it when the `--encrypt` key is a P-384 key.
