Building and using mcuboot with Zephyr
######################################

*mcuboot* began its life as the bootloader for Mynewt.  It has since
aquired the ability to be used as a bootloader for Zephyr as well.
There are some pretty significant differences in how apps are built
for Zephyr, and these are documented here.

Please see ``boot/bootutil/design.txt`` for documentation on the
design and operation of the bootloader itself.  This functionality
should be the same between Mynewt and Zephyr

Building the bootloader itself
==============================

The bootloader is an ordinary Zephyr application, at least from
Zephyr's point of view.  There is a bit of configuration that needs to
be made before building it.  Most of this is done in the top-level
``Makefile`` in the source tree.  There are comments there for
guidance.  It is important to select a signature algorithm, and decide
if slot0 should be validated on every boot.

There is a ``build_boot.sh`` script at the top level that can make
building a bit easier.  It assumes that the mcuboot tree is next to,
at the same level, as the zephyr source tree.  It takes a single
argument, which is the target to build.  This must match one of the
targets in ``boot/zephyr/targets`` to be a supported board.

Once this is finished building, the bootloader should reside in
``outdir/targname/zephyr.bin``.  Use the flashing tools you have to
install this image at the beginning of the flash.

Building Applications for the bootloader
========================================

In order build an application to be used within the bootloader, there
are a few configuration changes that need to be made to it (typically
in the app's prj.conf).

- ``CONFIG_TEXT_SECTION_OFFSET`` must be set to allow room for the
  boot image header.  It must also be aligned to a boundary that the
  particular MCU requires the vector table to be aligned on.  This is
  dependent upon the particular board you have chosen.  Starting with
  0x200 is a good way to start, since all of the boards will work with
  this alignment.

- ``CONFIG_FLASH_BASE_ADDRESS`` must be set to the base address in
  flash where the SLOT0 lives.  This should match the value found in
  ``boot/zephyr/target/*.h`` for your target, for
  ``FLASH_AREA_IMAGE_0_OFFSET``.  Note that some targets build for a
  higher-than-zero flash address, and this should be compensated for
  when setting this value.  It should generally be set to a small
  amount larger than its initial value.

With this, build the application as your normally would.

Signing the application
-----------------------

In order to upgrade to an image (or even boot it, if
``MCUBOOT_VALIDATE_SLOT0`` is enabled), the images must be signed.
To make development easier, mcuboot is distributed with some example
keys.  It is important to stress that these should never be used for
production, since the private key is publically available in this
repository.  See below on how to make your own signatures.

There is a ``sign.sh`` script that gives some examples of how to make
these signatures.

Flashing the application
------------------------

The application itself can flashed with regular flash tools, but will
need to be loaded at the offset of SLOT-0 for this particular target.
These images can also be marked for upgrade, and loaded into SLOT-1,
at which point the bootloader should perform an upgrade.  It is up to
the image to mark slot-0 as "image ok" before the next reboot,
otherwise the bootloader will revert the application.

Managing signing keys
=====================

The signing keys used by mcuboot are represented in standard formats,
and can be generated and processed using conventional tools.  However,
the Mynewt project has developed some tools to make this easier, and
the ``imgtool`` directory contains a small program to use these tools,
as well as some additional tools for generating and extracting public
keys.  If you will be using your own keys, it is recommended to build
this tool following the directions within the directory.

Generating a new keypair
------------------------

Generating a keypair with imgtool is a matter of running the keygen
subcommand::

    $ imgtool keygen -k mykey.pem -t rsa-2048

The argument to ``-t`` should be the desired key type.  See the
imgtool README.rst for more details on the possible keytypes.

Extracting the public key
-------------------------

The generated keypair above contains both the public and the private
key.  It is necessary to extract the public key and insert it into the
bootloader.  The keys live in ``boot/zephyr/keys.c``, and can be
extracted using imgtool::

    $ imgtool getpub -k mykey.pem

This will output the public key as a C array that can be dropped
directly into the ``keys.c`` file.

Once this is done, this new keypair file (``mykey.pem`` in this
example) can be used to sign images.
