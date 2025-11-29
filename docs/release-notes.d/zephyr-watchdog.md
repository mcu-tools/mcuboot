- Watchdog support in Zephyr has been reworked and fixed to allow
  installing a timeout (with a configurable value) before starting
  it. The default timeout is set to 1 minute and this feature has
  been enabled by default. 3 Kconfig options have been added which
  control how the watchdog is used in MCUboot:

    * ``CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT`` controls setting up
      the watchdog in MCUboot (if not set up, it can still be set,
      if the driver supports this non-compliant behaviour).
    * ``CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT`` controls if
      a timeout is installed at bootup or not.
    * ``CONFIG_BOOT_WATCHDOG_TIMEOUT_MS`` sets the value of the
      timeout in ms.

- In addition, Zephyr modules can now over-ride the default
  watchdog functionality by replacing the weakly defined functions
  ``mcuboot_watchdog_setup`` and/or ``mcuboot_watchdog_feed``,
  these functions take no arguments.

