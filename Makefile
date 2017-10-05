# Makefile for building mcuboot as a Zephyr project.

########################
# Configuration choices.
########################

# Set this variable to determine the signature type used.  Currently,
# it should be set to either RSA, or ECDSA_P256.  This value can be
# overridden by the make invocation, e.g.:
#
#     make CONF_SIGNATURE_TYPE=ECDSA_P256
#
CONF_SIGNATURE_TYPE ?= RSA

# Should the bootloader attempt to validate the signature of slot0
# when just booting.  This adds the signature check time to every
# boot, but can mitigate against some changes that are able to modify
# the flash image itself.  Set to "YES" for the validation, or any
# other value to not.
#
CONF_VALIDATE_SLOT0 ?= YES

# If this is set to YES, overwrite slot0 with the upgrade image
# instead of swapping them.  This prevents the fallback recovery, but
# uses a much simpler code path.
#
CONF_UPGRADE_ONLY ?= NO

##############################
# End of configuration blocks.
##############################

ifeq ($(CONF_SIGNATURE_TYPE),RSA)
# RSA
CONF_FILE = boot/zephyr/prj.conf
CFLAGS += -DMCUBOOT_SIGN_RSA -DMCUBOOT_USE_MBED_TLS

else ifeq ($(CONF_SIGNATURE_TYPE),ECDSA_P256)
# ECDSA P-256
CONF_FILE = boot/zephyr/prj-p256.conf
CFLAGS += -DMCUBOOT_SIGN_EC256 -DMCUBOOT_USE_TINYCRYPT
NEED_TINYCRYPT = y
export NEED_TINYCRYPT

else
$(error Invalid CONF_SIGNATURE_TYPE specified)
endif

# Enable this option to have the bootloader verify the signature of
# the primary image upon every boot.  Without it, signature
# verification only happens on upgrade.
ifeq ($(CONF_VALIDATE_SLOT0),YES)
CFLAGS += -DMCUBOOT_VALIDATE_SLOT0
endif

# Enabling this option uses newer flash map APIs. This saves RAM and
# avoids deprecated API usage.
#
# (This can be deleted when flash_area_to_sectors() is removed instead
# of simply deprecated.)
CFLAGS += -DMCUBOOT_USE_FLASH_AREA_GET_SECTORS

# Enable this option to not use the swapping code and just overwrite
# the image on upgrade.
ifeq ($(CONF_UPGRADE_ONLY),YES)
CFLAGS += -DMCUBOOT_OVERWRITE_ONLY
endif

# The board should be set to one of the targets supported by
# mcuboot/Zephyr.  These can be found in ``boot/zephyr/targets``
BOARD ?= qemu_x86

# Additional board-specific Zephyr configuration
CONF_FILE += $(wildcard boot/zephyr/$(BOARD).conf)

# The source to the Zephyr-specific code lives here.
SOURCE_DIR = boot/zephyr

# Needed for mbedtls config-boot.h file.
CFLAGS += -I$(CURDIR)/boot/zephyr/include

DTC_OVERLAY_FILE := $(CURDIR)/boot/zephyr/dts.overlay
export DTC_OVERLAY_FILE

include ${ZEPHYR_BASE}/Makefile.inc
