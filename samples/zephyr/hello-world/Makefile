# Top-level Makefile for the skeleton application.
#
# This provides a basic application structure suitable for loading by
# mcuboot, which is easy to customize on a per-board basis. It can be
# used as a starting point for new applications.

# The default board is FRDM-K64F. This can be overridden at
# the command line for other boards supported by Zephyr.
BOARD ?= frdm_k64f
# The top-level application configuration is prj.conf. This can also
# be overridden at the command line if you add another one.
CONF_FILE ?= prj.conf

# This will merge any board-specific extras from boards/$(BOARD).conf
# into the configuration.
CONF_FILE += $(wildcard boards/$(BOARD).conf)
# These additional "local" files, if present, will be merged in as
# well, but they are ignored by Git.
CONF_FILE += $(wildcard local.conf) $(wildcard boards/$(BOARD)-local.conf)

# Zephyr uses Device Tree (DT) to describe some board hardware
# configuration.
#
# For this simple example, all we need to tell DT is where on the chip
# flash to link this application image so mcuboot can find it. We do
# this with a device tree overlay file.
#
# See the Zephyr documentation for more information on DT:
# https://www.zephyrproject.org/doc/dts/device_tree.html
DTC_OVERLAY_FILE := $(CURDIR)/dts.overlay
export DTC_OVERLAY_FILE
# If you want to do your DT overlay on a per-board basis (say, if you
# need to support multiple different boards, each of which needs
# slightly different DT overlays), then comment the above lines and
# uncomment the following ones. You can then add board-specific
# overlay files named boards/$(BOARD).overlay.
#
# DTC_OVERLAY_DIR := $(CURDIR)/boards
# export DTC_OVERLAY_DIR

# This string ends up getting printed in the device console
FROM_WHO ?= "Zephyr"
CFLAGS += -DMCUBOOT_HELLO_WORLD_FROM=\"$(FROM_WHO)\"

# The Zephyr Makefiles do the rest.
include $(ZEPHYR_BASE)/Makefile.inc
