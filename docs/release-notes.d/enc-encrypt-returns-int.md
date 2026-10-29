- `boot_enc_encrypt()` and `boot_enc_decrypt()` now return `int`
  instead of `void`, and report an error when the slot has no valid
  key or the AES operation fails. Image copy, hashing, RAM loading and
  serial recovery stop on that error instead of using the
  untransformed data. Ports that provide their own implementation of
  these functions (`MCUBOOT_USE_CUSTOM_CRYPTO`) must update their
  signatures.
