- Serial recovery: added the `BOOT_SERIAL_RAW_PROTOCOL` option,
  which exchanges SMP packets over the serial port in raw binary
  form instead of the SMP over console encoding (no base64, length
  prefix, CRC or console framing). This reduces flash and RAM usage
  and increases transfer speed at the cost of requiring a
  binary-capable serial link. It mirrors Zephyr's
  `CONFIG_UART_MCUMGR_RAW_PROTOCOL` transport.
- Serial recovery: added `BOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT`
  (enabled by default with the raw protocol), which discards a
  partially received raw SMP packet after a period without new data
  so a stalled transfer cannot wedge recovery. Mirrors Zephyr's
  `CONFIG_MCUMGR_TRANSPORT_RAW_UART_INPUT_TIMEOUT`.
