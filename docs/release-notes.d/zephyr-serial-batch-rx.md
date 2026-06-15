- Zephyr: serial recovery UART ISR now reads the FIFO in configurable
  batches instead of one byte at a time. The batch size is controlled by
  the new `BOOT_SERIAL_UART_RX_BATCH_SIZE` Kconfig option (default 512).
  Within each batch, `memchr`/`memcpy` are used to locate line delimiters
  and copy data, reducing per-byte loop overhead compared to the previous
  single-byte loop.
