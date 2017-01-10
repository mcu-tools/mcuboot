BOARD ?= qemu_x86
CONF_FILE = boot/zephyr/prj.conf
SOURCE_DIR = boot/zephyr

include ${ZEPHYR_BASE}/Makefile.inc
