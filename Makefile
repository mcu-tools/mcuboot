BOARD ?= qemu_x86
CONF_FILE = boot/zephyr/prj.conf
SOURCE_DIR = boot/zephyr

# Needed for mbedtls config-boot.h file.
CFLAGS += -I$(CURDIR)/boot/zephyr/include

include ${ZEPHYR_BASE}/Makefile.inc
