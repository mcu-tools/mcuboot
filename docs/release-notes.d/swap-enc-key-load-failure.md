- When the encryption key of either slot can't be loaded at the start
  of a swap, the upgrade is now skipped and the image in the primary
  slot is booted unchanged; the upgrade stays pending. This was
  previously checked only with `assert()`.
