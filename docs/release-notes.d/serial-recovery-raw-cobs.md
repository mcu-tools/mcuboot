- Serial recovery: added the `BOOT_SERIAL_RAW_PROTOCOL_COBS` option,
  which wraps the raw SMP encoding in COBS framing with a trailing
  CRC16 (CRC-16/XMODEM, the same CRC used by the SMP over console
  encoding). Each packet is sent as
  `COBS(header || payload || crc16) || 0x00`. The `0x00` delimiter
  gives an unambiguous resync point and the CRC16 rejects damaged
  packets, so the encoding is self-synchronising and needs no input
  timeout. It requires a matching client transport; no stock mcumgr
  client speaks it yet, so it is provided for evaluation.
- Serial recovery: `BOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT` is now a
  member of a choice together with the new
  `BOOT_SERIAL_RAW_PROTOCOL_COBS` and `BOOT_SERIAL_RAW_PROTOCOL_NONE`
  options, as the three are mutually exclusive ways to delimit and
  recover the raw stream. The input timeout remains the default.
  Migration: a configuration that previously set
  `CONFIG_BOOT_SERIAL_RAW_PROTOCOL_INPUT_TIMEOUT=n` to obtain the
  unframed raw behaviour must now set
  `CONFIG_BOOT_SERIAL_RAW_PROTOCOL_NONE=y` instead. Configurations
  that leave the timeout at its default, or set it `=y`, are
  unaffected.
