###########################################################################
# Sample multi-part application Makefile
#
# Copyright (c) 2017 Linaro Limited
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

BOARD ?=

.PHONY: check boot hello1 clean_boot clean_hello1 \
	hello2 clean_hello2 flash_boot flash_hello1 flash_hello2

# For signing, use the default RSA demo key, to match the default in
# the mcuboot Makefile.
SIGNING_KEY = ../../root-rsa-2048.pem

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
PYOCD_FLASHTOOL = pyocd-flashtool

help:
	@echo "make <target> BOARD=<board>"
	@echo "<target>: all, boot, hello1"
	@echo "<board>: frdm_k64f, 96b_carbon, etc."

all: boot hello1 hello2

clean: clean_boot clean_hello1 clean_hello2
	@rm -f signed-hello1.bin
	@rm -f signed-hello2.bin
	@rm -f mcuboot.bin

boot: check
	@rm -f mcuboot.bin
	$(MAKE) -C ../.. BOARD=$(BOARD) -j$(nproc)
	cp ../../outdir/$(BOARD)/zephyr.bin mcuboot.bin

clean_boot: check
	rm -rf ../../outdir/$(BOARD)

# Build and sign "hello1".
hello1: check
	$(MAKE) -C hello1 BOARD=$(BOARD) -j$(nproc)
	$(IMGTOOL) sign \
		--key $(SIGNING_KEY) \
		--header-size $(BOOT_HEADER_LEN) \
		--align $(FLASH_ALIGNMENT) \
		--version 1.2 \
		--included-header \
		hello1/outdir/$(BOARD)/zephyr.bin \
		signed-hello1.bin

clean_hello1: check
	rm -rf hello1/outdir/$(BOARD)

# Build and sign "hello2".
# This is the same signing command as above, except that it adds the
# "--pad" argument.  This will also add the trailer that indicates
# this image is intended to be an upgrade.  It should be flashed into
# slot1 instead of slot0.
hello2: check
	$(MAKE) -C hello2 BOARD=$(BOARD) -j$(nproc)
	$(IMGTOOL) sign \
		--key $(SIGNING_KEY) \
		--header-size $(BOOT_HEADER_LEN) \
		--align $(FLASH_ALIGNMENT) \
		--version 1.2 \
		--included-header \
		--pad 0x60000 \
		hello2/outdir/$(BOARD)/zephyr.bin \
		signed-hello2.bin

clean_hello2: check
	rm -rf hello2/outdir/$(BOARD)

flash_boot:
	$(PYOCD_FLASHTOOL) -ce -a 0 mcuboot.bin

flash_hello1:
	$(PYOCD_FLASHTOOL) -a 0x20000 signed-hello1.bin

flash_hello2:
	$(PYOCD_FLASHTOOL) -a 0x80000 signed-hello2.bin

check:
	@if [ -z "$$ZEPHYR_BASE" ]; then echo "Zephyr environment not set up"; false; fi
	@if [ -z "$(BOARD)" ]; then echo "You must specity BOARD=<board>"; false; fi
