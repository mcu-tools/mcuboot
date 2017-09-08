zephyr_sources(boot_serial.c)

#mbedtls-base64 library header
zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/crypto/mbedtls/include)

zephyr_include_directories(${BOOT_DIR}/bootutil/include)
