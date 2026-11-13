- Zephyr serial recovery now fails console initialisation when the UART
  IRQ callback cannot be registered, instead of enabling RX on a device
  that will never deliver input.
- Serial recovery now logs an error when decrypting an uploaded
  encrypted image fails. The upload response has already been sent by
  then, so the failure was previously silent until the image was
  rejected at boot.
