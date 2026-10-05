- Fixed the Zephyr build with a watchdog driver when
  `CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT` is disabled, including
  `CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT=n` for a watchdog started by the
  hardware or another image: `mcuboot_watchdog_feed()` read a channel
  variable declared only when a timeout was installed. It now feeds
  channel 0.
