- Zephyr: `MCUBOOT_CLEANUP_ARM_CORE` now supports AArch32 Cortex-A. Before
  chain-loading, MCUboot switches the GIC distributor and CPU interface off,
  stops the memory-mapped Arm timer from interrupting, invalidates the
  instruction cache, branch predictor and TLB, turns the MMU off and clears
  the translation table and domain registers, and hands over in supervisor
  mode with IRQ masked and asynchronous aborts unmasked. Cortex-M and
  Cortex-R behaviour is unchanged.
