- Add support for Execute-in-Place (XIP) encryption mode for swap
  upgrades (`MCUBOOT_ENC_IMAGES_XIP`). In this mode, images remain encrypted in
  all flash slots and are decrypted on-the-fly by hardware during execution
  rather than via software during swap.
- Add mandatory signature verification over raw ciphertext before key
  extraction (using extended ECIES-P256 TLV with HKDF salt diversifier).
- Add `boot_image_check_hook()` default implementation for XIP encrypted images
  and `boot_xip_populate_rsp()` to pass derived AES key and nonce material to
  application startup code via `boot_rsp`.
