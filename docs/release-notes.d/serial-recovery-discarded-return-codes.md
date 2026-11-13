- Zephyr serial recovery now fails console initialisation when the UART
  IRQ callback cannot be registered, instead of enabling RX on a device
  that will never deliver input.
