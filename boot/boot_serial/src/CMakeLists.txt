zephyr_sources(boot_serial.c)

zephyr_include_directories(${BOOT_DIR}/bootutil/include)

zephyr_include_directories($ENV{ZEPHYR_BASE}/ext/lib/mgmt/mcumgr/cborattr/include)
zephyr_sources($ENV{ZEPHYR_BASE}/ext/lib/mgmt/mcumgr/cborattr/src/cborattr.c)

zephyr_link_libraries_ifdef(CONFIG_TINYCBOR TINYCBOR)
