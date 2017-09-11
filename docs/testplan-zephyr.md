# Zephyr Test Plan

The following roughly describes how mcuboot is tested on Zephyr.  The
testing is done with the code in `samples/zephyr`.  These examples
were written using the FRDM-K64F, but other boards should be similar.
At this time, however, the partitions are hardcoded in the Makefile
targets to flash.

Note that at the time of release of 0.9.0-rc2, the change [MPU flash
write][flashwrite] had not been merged.  This change fixes a problem interaction
between the MPU and the flash drivers.  Without this change, if the
MPU is enabled (the default), the bootloader will abort immediately on
boot, generally before printing any messages.

At this time, most of the test variants are done by modifying either
the code or Makefiles.  A future goal is to automate this testing.

## Sanity Check

Begin by running make in `samples/zephyr`:

    $ make clean
    $ make all

This will result in three binaries: `mcuboot.bin`,
`signed-hello1.bin`, and `signed-hello2.bin`.

The second file is marked as an "upgrade" by the image tool, so
has an appended image trailer.

Begin by doing a full erase, and programming the first image:

    $ pyocd-flashtool -ce
    $ make flash_boot

After it resets, look for "main: Starting bootloader", a few debug
messages, and lastly: "main: Unable to find bootable image".

Then, load hello1:

    $ make flash_hello1

This should print "main: Jumping to the first image slot", and you
should get an image "Hello World number 1!".

For kicks, program slot 2's image into slot one.  This has to be done
manually, and it is good to verify these addresses in the Makefile:

    $ pyocd-flashtool -a 0x20000 signed-hello1.bin

This should boot, printing "Upgraded hello!".

Now put back image 1, and put image 2 in as the upgrade:

    $ make flash_hello1
    $ make flash_hello2

This should print a message: `boot_swap_type: Swap type: test`, and
you should see "Upgraded hello!".

Now reset the target::

    $ pyocd-tool reset

And you should see a revert and "Hello world number 1" running.

Repeat this, to make sure we can mark the image as OK, and that a
revert doesn't happen:

    $ make flash_hello1
    $ make flash_hello2

We should have just booted the Upgraded hello.  Mark this as OK:

    $ pyocd-flashtool -a 0x7ffe8 image_ok.bin
    $ pyocd-tool reset

And make sure this stays in the "Upgraded hello" image.

## Other Signature Combinations

**note**: Make sure you don't have changes in your tree, as the
following step will undo them.

As part of the above sanity check, we have tested the RSA signature
algorithm, along with the new RSA-PSS signature algorithm.  To test
other configurations, we need to make some modifications to the code.
This is easiest to do by applying some patches (in
`testplan/zephyr`).  For each of these patches, perform something
along the lines of:

    $ cd ../..
    $ git apply testplan/zephyr/0001-try-rsa-pkcs1-v15.patch
    $ cd samples/zephyr
    $ make clean
    $ make all
    $ pyocd-flashtool -ce
    $ make flash_boot
    $ make flash_hello1

Make sure image one boots if it is supposed to (and doesn't if it is
not supposed to).  Then try the upgrade:

    $ make flash_hello2

After this, make sure that the the image does or doesn't perform the
upgrade (see test table below).

After the upgrade runs, reset to make sure the revert works (or
doesn't for the noted cases below):

    $ pyocd-tool reset

Then undo the change:

    $ cd ../..
    $ git checkout -- .

and repeat the above steps for each patch.

The following patches are available:

| Patch | hello1 boot? | Upgrade ? |
|-------|--------------|-----------|
| 0001-bad-old-rsa-in-boot-not-in-image.patch | no | no |
| 0001-bad-old-RSA-no-slot0-check.patch | yes | no |
| 0001-good-rsa-pkcs-v1.5-good.patch | yes | yes |
| 0001-bad-ECDSA-P256-bootloader-not-in-images.patch | no | no |
| 0001-partial-ECDSA-P256-bootloader-slot0-ok-slot1-bad.patch | yes | no |
| 0001-good-ECDSA-P256-bootloader-images-signed.patch | yes | yes |
| 0001-partial-ECDSA-P256-bootloader-slot-0-bad-sig.patch | no | yes<sup>1</sup> |
| 0001-partial-ECDSA-P256-bootloader-slot-1-bad-sig.patch | yes | no |
| 0001-partial-ECDSA-P256-slot-0-bad-no-verification.patch | no | yes<sup>1</sup> |

<sup>1</sup>These tests with hello1 bad should perform an upgrade when
hello2 is flashed, but they should not revert the image afterwards.

[flashwrite]: https://github.com/zephyrproject-rtos/zephyr/pull/654
