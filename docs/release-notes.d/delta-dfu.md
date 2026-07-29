- bootutil: added overwrite-only delta DFU support with robust signed reversible
  protocol-v1 patch images.
- bootutil: delta DFU records include old bytes so interrupted delta applies
  and unconfirmed updates can be restored and retried across resets.
- imgtool: added `--delta-base` and `--delta-block-size` for generating signed
  delta images.
- simulator: added reset injection for forward apply and restore, target
  validation repair, and security-counter confirmation coverage.
