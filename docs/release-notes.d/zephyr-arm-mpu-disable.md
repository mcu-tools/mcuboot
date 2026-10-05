- Zephyr: On Cortex-M, MCUboot's weak `z_arm_clear_arm_mpu_config()`
  now disables the Arm MPU before clearing its regions, so clearing
  them no longer faults when the MPU background map is disabled.
