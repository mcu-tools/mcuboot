- RISCV targets in swap mode will no longer erroneously attempt
  to load the image to RAM and will boot the image directly, as
  this is fully supported by RISCV and looks to have been an
  error in a previous code submission.
