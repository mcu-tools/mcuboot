- Zephyr CDC ACM serial recovery now enables USB only after a controller that
  can detect VBUS reports it, as the USB device stack expects, waiting at most
  1 s before enabling regardless. Controllers without VBUS detection are
  enabled at once, as before.
