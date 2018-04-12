###########################################################################
# Sample multi-part application Makefile
#
# Copyright (c) 2017 Linaro Limited
# Copyright (c) 2017 Open Source Foundries Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

# This is an example Makefile to demonstrate how to use mcuboot to
# deploy and upgrade images.  The image building should work on any
# supported target, however flashing will likely require changes to
# the flash addresses, depending on the partition layout of the device
# in question.
#
# running
#
#     make BOARD=frdm_k64f all
#
# should generate three "*.bin" files in this directory:
#
#   mcuboot.bin: The bootloader itself
#   signed-hello1.bin: A signed sample.
#   signed-hello2.bin: An upgrade image, signed and marked for
#   upgrade.
#
# "make flash_boot" should flash the bootloader into the flash,
# erasing the rest of the device.  If you examine the device at this
# time, you should see a message about the bootloader not being able
# to find a bootable image.
#
# "make flash_hello1" will then flash the first application into
# "slot0".  This should boot into this app, print a small message, and
# give the zephyr console.
#
# "make flash_hello2" will flash hello2 into the second slot.  The
# reset should upgrade and run the new image.  Resetting again should
# then revert back to the first app, since we did not mark this image
# as good.

# Extra .conf fragments to merge into the MCUboot .config, as a
# semicolon-separated list (i.e., a CMake list).
BOOTLOADER_OVERLAY_CONFIG ?=

BOARD ?= frdm_k64f

.PHONY: check boot hello1 clean_boot clean_hello1 \
	hello2 clean_hello2 flash_boot flash_hello1 flash_hello2

# For signing, use the default RSA demo key, to match the default in
# the mcuboot Makefile.
SIGNING_KEY ?= ../../root-rsa-2048.pem

# The header size should match that in hello1/prj.conf
# CONFIG_TEXT_SECTION_OFFSET.  This value needs to be a power of two
# that is at least as large as the size of the vector table.  The
# value given here of 0x200 should be sufficient for any supported
# devices, but it can be made smaller, as long as this value matches
# that used to build the app.
BOOT_HEADER_LEN = 0x200

# For upgrades, the signing tool needs to know the device alignment.
# This requirement will be going away soon.
FLASH_ALIGNMENT = 8

IMGTOOL = ../../scripts/imgtool.py
ASSEMBLE = ../../scripts/assemble.py
PYOCD_FLASHTOOL = pyocd-flashtool

SOURCE_DIRECTORY := $(CURDIR)
BUILD_DIRECTORY := $(CURDIR)/build/$(BOARD)
BUILD_DIR_BOOT := $(BUILD_DIRECTORY)/mcuboot
BUILD_DIR_HELLO1 := $(BUILD_DIRECTORY)/hello1
BUILD_DIR_HELLO2 := $(BUILD_DIRECTORY)/hello2

help:
	@echo "make <target> BOARD=<board>"
	@echo "<target>: all, boot, hello1, full.bin"
	@echo "<board>: frdm_k64f only for now"

all: boot hello1 hello2

full.bin: boot hello1 hello2
	$(ASSEMBLE) -b $(BUILD_DIR_BOOT) \
	    -p signed-hello1.bin \
	    -s signed-hello2.bin \
	    -o full.bin

clean: clean_boot clean_hello1 clean_hello2
	@rm -f signed-hello1.bin
	@rm -f signed-hello2.bin
	@rm -f mcuboot.bin

boot: check
	@rm -f mcuboot.bin
	(mkdir -p $(BUILD_DIR_BOOT) && \
		cd $(BUILD_DIR_BOOT) && \
		cmake -DOVERLAY_CONFIG=$(BOOTLOADER_OVERLAY_CONFIG) \
			-G"Unix Makefiles" \
			-DBOARD=$(BOARD) \
			$(SOURCE_DIRECTORY)/../../boot/zephyr && \
		make -j$(nproc))
	cp $(BUILD_DIR_BOOT)/zephyr/zephyr.bin mcuboot.bin

clean_boot: check
	rm -rf $(BUILD_DIR_BOOT)

# Build and sign "hello1".
hello1: check
	(mkdir -p $(BUILD_DIR_HELLO1) && \
		cd $(BUILD_DIR_HELLO1) && \
		cmake -DFROM_WHO=hello1 \
			-G"Unix Makefiles" \
			-DBOARD=$(BOARD) \
			$(SOURCE_DIRECTORY)/hello-world && \
		make -j$(nproc))
	$(IMGTOOL) sign \
		--key $(SIGNING_KEY) \
		--header-size $(BOOT_HEADER_LEN) \
		--align $(FLASH_ALIGNMENT) \
		--version 1.2 \
		--included-header \
		$(BUILD_DIR_HELLO1)/zephyr/zephyr.bin \
		signed-hello1.bin

clean_hello1: check
	rm -rf $(BUILD_DIR_HELLO1)

# Build and sign "hello2".
# This is the same signing command as above, except that it adds the
# "--pad" argument.  This will also add the trailer that indicates
# this image is intended to be an upgrade.  It should be flashed into
# slot1 instead of slot0.
hello2: check
	(mkdir -p $(BUILD_DIR_HELLO2) && \
		cd $(BUILD_DIR_HELLO2) && \
		cmake -DFROM_WHO=hello2 \
			-G"Unix Makefiles" \
			-DBOARD=$(BOARD) \
			$(SOURCE_DIRECTORY)/hello-world && \
		make -j$(nproc))
	$(IMGTOOL) sign \
		--key $(SIGNING_KEY) \
		--header-size $(BOOT_HEADER_LEN) \
		--align $(FLASH_ALIGNMENT) \
		--version 1.2 \
		--included-header \
		--pad 0x60000 \
		$(BUILD_DIR_HELLO2)/zephyr/zephyr.bin \
		signed-hello2.bin

clean_hello2: check
	rm -rf $(BUILD_DIR_HELLO2)

# These flash_* targets use pyocd to flash the images.  The addresses
# are hardcoded at this time.

flash_boot:
	$(PYOCD_FLASHTOOL) -ce -a 0 mcuboot.bin

flash_hello1:
	$(PYOCD_FLASHTOOL) -a 0x20000 signed-hello1.bin

flash_hello2:
	$(PYOCD_FLASHTOOL) -a 0x80000 signed-hello2.bin

flash_full:
	$(PYOCD_FLASHTOOL) -ce -a 0 full.bin

# These test- targets reinvoke make with the configuration set to test
# various configurations.  This will generally be followed by using
# the above flash targets.

# Test a good image, with a good upgrade, using RSA signatures.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello2 runs
# reset: hello1 runs
test-good-rsa: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-rsa.conf \
		all

# Test a good image, with a good upgrade, using ECDSA signatures.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello2 runs
# reset: hello1 runs
test-good-ecdsa: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-ecdsa-p256.conf \
		SIGNING_KEY=../../root-ec-p256.pem \
		all

# Test (with RSA) that overwrite-only works.  This should boot,
# upgrade correctly, but not revert once the upgrade has been done.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello2 runs
# reset: hello2 runs
test-overwrite: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-upgrade-only.conf \
		all

# Test that when configured for RSA, a wrong signature in the upgrade
# image will fail to upgrade.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello1 runs
# reset: hello1 runs
test-bad-rsa-upgrade: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-rsa.conf \
		boot hello1
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-rsa.conf \
		SIGNING_KEY=../../root-ec-p256.pem \
		hello2

# Test that when configured for ECDSA, a wrong signature in the upgrade
# image will fail to upgrade.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello1 runs
# reset: hello1 runs
test-bad-ecdsa-upgrade: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-ecdsa-p256.conf \
		SIGNING_KEY=../../root-ec-p256.pem \
		boot hello1
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-ecdsa-p256.conf \
		SIGNING_KEY=../../root-rsa-2048.pem \
		hello2

# Test that when configured to not validate slot0, we still boot, but
# don't upgrade.
# flash_boot: tries to boot and resets
# flash_hello1: hello1 runs
# flash_hello2: hello1 runs
# reset: hello1 runs
test-no-bootcheck: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-skip-slot0-validate.conf \
		SIGNING_KEY=../../root-ec-p256.pem \
		all

# Test a good image, with a wrong-signature upgrade, using RSA signatures.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello1 runs
# reset: hello1 runs
test-wrong-rsa: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-rsa.conf \
		boot hello1
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-rsa.conf \
		SIGNING_KEY=bad-keys/bad-rsa-2048.pem \
		hello2

# Test a good image, with a wrong-signature upgrade, using ECDSA signatures.
# flash_boot: Unable to find bootable image
# flash_hello1: hello1 runs
# flash_hello2: hello1 runs
# reset: hello1 runs
test-wrong-ecdsa: clean
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-ecdsa-p256.conf \
		SIGNING_KEY=../../root-ec-p256.pem \
		boot hello1
	$(MAKE) \
		BOOTLOADER_OVERLAY_CONFIG=$(PWD)/overlay-ecdsa-p256.conf \
		SIGNING_KEY=bad-keys/bad-ec-p256.pem \
		hello2

check:
	@if [ -z "$$ZEPHYR_BASE" ]; then echo "Zephyr environment not set up"; false; fi
	@if [ -z "$(BOARD)" ]; then echo "You must specify BOARD=<board>"; false; fi
