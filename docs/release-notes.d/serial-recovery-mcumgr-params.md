- Added an optional serial recovery ``MCUmgr parameters`` command (mcumgr OS
  management group, command ID 6), enabled with
  ``CONFIG_BOOT_MGMT_MCUMGR_PARAMS``. The read-only command reports the SMP
  transport ``buf_size`` and ``buf_count`` so that SMP clients can negotiate
  optimal serial fragmentation, matching the command provided by Zephyr's SMP
  server. It requires ``CONFIG_BOOT_MAX_LINE_INPUT_LEN`` to remain at its
  default of 128.
