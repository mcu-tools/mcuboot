- Zephyr: Added `BOOT_DISABLE_BRANCH_PREDICTION`, which disables branch
  prediction and invalidates the branch predictor before the MPU
  configuration is cleared and the application is booted. It is
  enabled by default on Cortex-M85.
