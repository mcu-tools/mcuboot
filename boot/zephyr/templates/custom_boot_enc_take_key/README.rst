This is template for custom AES key provider.

The template can be used as a starter for your own implementation.
To use the template, copy it somewhere where you want to continue
development of the provider and then use following Kconfig options,
for MCUboot, to include it into your build:
CONFIG_BOOT_ENCRYPT_IMAGE_USE_CUSTOM_KEY_PROVIDER=y
CONFIG_BOOT_ENCRYPT_IMAGE_EMBEDDED_CUSTOM_KEY_PROVIDER_DIR=<dir>

where <dir> is absolute path or path relative to the MCUboot source
code dir.
