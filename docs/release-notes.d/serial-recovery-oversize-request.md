- Serial recovery: a console-framed request larger than
  `MCUBOOT_SERIAL_MAX_RECEIVE_SIZE` is now answered with
  `MGMT_ERR_EMSGSIZE` instead of being dropped silently, and the rest of
  that request is discarded so the next one is served normally.
