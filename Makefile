# Makefile for building mcuboot as a Zephyr project.

# These are the main configuration choices, mainly having to do with
# what signature algorithm is desired.  Choose one of the blocks
# below, and uncomment the settings after it.

#####
# RSA
#####
CONF_FILE = boot/zephyr/prj.conf
CFLAGS += -DBOOTUTIL_SIGN_RSA

#############
# ECDSA P-256
#############
#CONF_FILE = boot/zephyr/prj-p256.conf
#CFLAGS += -DBOOTUTIL_SIGN_EC256

##############################
# End of configuration blocks.
##############################

# The board should be set to one of the targets supported by
# mcuboot/Zephyr.  These can be found in ``boot/zephyr/targets``
BOARD ?= qemu_x86

# The source to the Zephyr-specific code lives here.
SOURCE_DIR = boot/zephyr

# Needed for mbedtls config-boot.h file.
CFLAGS += -I$(CURDIR)/boot/zephyr/include

include ${ZEPHYR_BASE}/Makefile.inc
