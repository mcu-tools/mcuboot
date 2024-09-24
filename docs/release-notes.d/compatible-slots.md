- Added optional write block size checking to ensure expected
  sizes match what is available on the hardware.
- Added optional erase size checking to ensure expected sizes
  match what is available on the hardware.
- Added debug logs for zephyr to output discrepencies in erase
  and write block sizes in dts vs actual hardware configuration
  at run-time.
