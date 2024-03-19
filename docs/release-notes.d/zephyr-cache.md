- Zephyr: Fixes support for disabling instruction/data caches prior
  to chain-loading an application, this will be automatically
  enabled if one or both of these caches are present. This feature
  can be disabled by setting `CONFIG_BOOT_DISABLE_CACHES` to `n`.
