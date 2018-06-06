zephyr_sources(boot_serial.c)

zephyr_include_directories(${BOOT_DIR}/bootutil/include)

zephyr_link_libraries_ifdef(CONFIG_TINYCBOR TINYCBOR)
