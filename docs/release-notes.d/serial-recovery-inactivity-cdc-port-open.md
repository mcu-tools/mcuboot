- Zephyr: with `CONFIG_BOOT_SERIAL_INACTIVITY_TIMEOUT` and USB CDC ACM, the
  wait for the host to open the port now counts against the inactivity
  timeout. Previously serial recovery entered by boot mode waited for the
  port indefinitely, so a device whose host never opened it stayed in
  recovery until power-cycled.
