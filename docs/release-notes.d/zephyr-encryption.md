- Reworked image encryption support for Zephyr, static dummy key files
  are no longer in the code, a pem file must be supplied to extract
  the private and public keys. The Kconfig menu has changed to only
  show a single option for enabling encryption and selecting the key
  file.
- Serial recovery can now read and handle encrypted seondary slot
  partitions.
- Serial recovery with MBEDTLS no longer has undefined operations which
  led to usage faults when the secondary slot image was encrypted.
