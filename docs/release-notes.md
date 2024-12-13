# MCUboot release notes

- Table of Contents
{:toc}

## Version 2.1.0

- Boot serial: Add response to echo command if support is not
  enabled, previously the command would have been accepted but no
  response indicating that the command is not supported would have
  been sent.
- Added support for using builtin keys for image validation
  (available with the PSA Crypto API based crypto backend for ECDSA signatures).
- Enforce that TLV entries that should be protected are.
  This can be disabled by defining `ALLOW_ROGUE_TLVS`
- bootutil: Fixed issue with comparing sector sizes for
  compatibility, this now also checks against the number of usable
  sectors (which is the slot size minus the swap status and moved
  up by one sector).
- bootutil: Added debug logging to show write location of swap status
  and details on sectors including if slot sizes are not optimal for
  a given board.
- Update ptest to support test selection. Ptest can now be invoked with `list`
  to show the available tests and `run` to run them. The `-t` argument will
  select specific tests to run.
- Allow sim tests to skip slow tests.  By setting `MCUBOOT_SKIP_SLOW_TESTS` in
  the environment, the sim will skip two tests that are very slow.  In one
  instance this reduces the test time from 2 hours to about 5 minutes.  These
  slow tests are useful, in that they test bad powerdown recovery, but are
  inconvenient when testing other areas.
- Zephyr: Fixes support for disabling instruction/data caches prior
  to chain-loading an application, this will be automatically
  enabled if one or both of these caches are present. This feature
  can be disabled by setting `CONFIG_BOOT_DISABLE_CACHES` to `n`.
- Zephyr: Fix issue with single application slot mode, serial
  recovery and encryption whereby an encrypted image is loaded
  and being wrongly treated as encrypted after decryption.
- Zephyr: Add estimated image footer size to cache in sysbuild.
- Added firmware loader configuration type support for Zephyr, this
  allows for a single application slot and firmware loader image in
  the secondary slot which is used to update the primary image
  (loading it in any way it sees fit e.g. via Bluetooth).
- Zephyr: Remove deprecated ZEPHYR_TRY_MASS_ERASE Kconfig option.
- Zephyr: Prevent MBEDTLS Kconfig selection when tinycrypt is used.
- Zephyr: Add USB CDC serial recovery check that now causes a build
  failure if console is enabled and device is the same as the USB
  CDC device.
- Zephyr: Add USB CDC serial recovery check that now causes a build
  failure if the main thread priority is below 0 (cooperative
  thread), this would prevent USB CDC from working as the driver
  would not have been able to fire callbacks.
- Use general flash operations to determine the flash reset vector. This
  improves support a bit for some configurations of external flash.
- fix a memory leak in the HKDF implementation.
- Zephyr: Added a MCUboot banner which displays the version of
  MCUboot being used and the version of zephyr. This can be
  disabled by setting ``CONFIG_MCUBOOT_BOOT_BANNER=n`` which
  will revert back to the default zephyr boot banner.

## Version 2.0.0

Note that this release, 2.0.0 is a new major number, and contains a small API
change in the interface between mcuboot and the platform.  All platforms
contained within the MCUboot tree have been updated, but any external platforms
will have to be adjusted.  The following commit makes the API change, in the
function `boot_save_shared_data`.

    commit 3016d00cd765e7c09a14af55fb4dcad945e4b982
    Author: Jamie McCrae <jamie.mccrae@nordicsemi.no>
    Date:   Tue Mar 14 12:35:51 2023 +0000
    
        bootutil: Add active slot number and max app size to shared data

### About this release

- Add error when flash device fails to open.
- Panic bootloader when flash device fails to open.
- Fixed issue with serial recovery not showing image details for
  decrypted images.
- Fixes issue with serial recovery in single slot mode wrongly
  iterating over 2 image slots.
- CDDL auto-generated function code has been replaced with zcbor function
  calls, this now allows the parameters to be supplied in any order.
- Added currently running slot ID and maximum application size to
  shared data function definition.
- Make the ECDSA256 TLV curve agnostic and rename it to ECDSA_SIG.
- imgtool: add P384 support along with SHA384.
- espressif: refactor after removing IDF submodule
- espressif: add ESP32-C6, ESP32-C2 and ESP32-H2 new chips support
- espressif: adjustments after IDF v5.1 compatibility, secure boot build and memory map organization
- Serial recovery image state and image set state optional commands added
- imgtool: add 'dumpinfo' command for signed image parsing.
- imgtool: add 'getpubhash' command to dump the sha256 hash of the public key
- imgtool's getpub can print the output to a file
- imgtool can dump the raw versions of the public keys
- Drop ECDSA P224 support
- Fixed an issue with boot_serial repeats not being processed when
  output was sent, this would lead to a divergence of commands
  whereby later commands being sent would have the previous command
  output sent instead.
- Fixed an issue with the boot_serial zcbor setup encoder function
  wrongly including the buffer address in the size which caused
  serial recovery to fail on some platforms.
- zcbor library files have been updated to version 0.7.0
- Reworked boot serial extensions so that they can be used by modules
  or from user repositories by switching to iterable sections.
- Removed Zephyr custom img list boot serial extension support.
- (Zephyr) Adds support for sharing boot information with
  application via retention subsystem
- Zephyr no longer builds in optimize for debug mode, this saves a
  significant amount of flash space.
- Reworked image encryption support for Zephyr, static dummy key files
  are no longer in the code, a pem file must be supplied to extract
  the private and public keys. The Kconfig menu has changed to only
  show a single option for enabling encryption and selecting the key
  file.
- Serial recovery can now read and handle encrypted seondary slot
  partitions.
- Serial recovery with MBEDTLS no longer has undefined operations which
  led to usage faults when the secondary slot image was encrypted.
- espressif: allow the use of a different toolchain for building

## Version 1.10.0

The 1.10.0 release of MCUboot contains...

### About this release

- Various fixes to boot serial.
- Various fixes to the mbed target.
- Various fixes to the Espressif native target.
- Various fixes to the Zephyr target.
- Workflow improvements with Zephyr CI.
- Add multi image support to the espressif esp32 target.
- Improvements and corrections to the simulator.
- Improve imgtool, including adding 3rd party signing support.
- Various fixes to the mynewt target.
- Various fixes to the nuttx target.
- Dates to dependencies for doc generation.
- Add downgrade prevention for modes using swap.
- Various general fixes to the boot code.
- Prefer swap move on zephyr if the scratch partition is not enabled.
- Upgrade fault-injection hardening, improving cases injections are detected.
- Add a new flash api `flash_area_get_sector`, along with support for each
  target, that replaces `flash_area_sector_from_off`. This is a step in cleaning
  up the flash API used by MCUboot.

### Security fixes

There are no security vulnerabilities reported on the MCUboot code for this
release. There have been several updates to the dependencies in the Ruby code
used to generate the documentation. This should only affect users that generate
their own documentation.

## Version 1.9.0

The 1.9.0 release of MCUboot contains various bug fixes, improves
support on some recent targets, and adds support for devices with a
write alignment larger than 8.

This change introduces a potentially incompatible change to the format
of the image trailer.  If `BOOT_MAX_ALIGN` is kept at 8, the trailer
format does not change.  However, to support larger write alignments,
this value can be increased, which will result in a different magic
number value.  These targets were previously unsupported in MCUboot,
so this change should not affect any existing targets.  The change has
been tested with a `BOOT_MAX_ALIGN` up to 32 bytes.

### About this release

- Add native flash encryption to Espressif targets
- Numerous documentation improvements
- Increase coverage of large images in the simulator
- Add stm32 watchdog support
- Add support for the `mimxrt685_evk` board
- Add support for "partial multi-image booting"
- Add support for clear image generation with encryption capability to
  imgtool
- Fix Zephyr when `CONFIG_BOOT_ENCRYPTION_KEY_FILE` is not defined
- Remove zephyr example test running in shell.  The Go version is
  primary and much more featureful.
- imgtool: make `--max-align` default reasonable in most cases.
- Implement the mcumgr echo command in serial boot mode

### Security fixes

## Version 1.8.0

The 1.8.0 release of MCUboot contains numerous fixes, and adds support
for the NuttX RTOS, and the Espressif ESP32 SDK.

### About this release

- Add support for the NuttX RTOS.
- Add support for the Espressif ESP32 SDK.
- `boot_serial` changed to use cddl-gen, which removes the dependency
  on tinycbor.
- Add various hooks to be able to change how image data is accessed.
- Cypress supports Mbed TLS for encryption.
- Support using Mbed TLS for ECDSA.  This can be useful if Mbed TLS is
  brought in for another reason.
- Add simulator support for testing direct-XIP and ramload.
- Support Mbed TLS 3.0.  Updates the submodule for Mbed TLS to 3.0.
- Enable direct-xip mode in Mbed-OS port.
- extract `bootutil_public` library, a common interface for MCUboot
  and the application.
- Allow to boot primary image if secondary one is unreachable.
- Add AES256 image encryption support.
- Add Multiimage boot for direct-xip and ram-load mode.
- Cargo files moved to top level, now `cargo test` can be run from the
  top level directory.
- Fault injection tests use updated TF-M.
- Thingy:53 now supports multi-image DFU.
- ram load and image encryption can be used together, allowing the
  entire contents of flash to always remain encrypted.

### Security fixes

- [GHSA-gcxh-546h-phg4](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-gcxh-546h-phg4)
  has been published.  There is not a fix at this time, but a caution
  to be sure to follow the instructions carefully, and make sure that
  the development keys in the repo are never used in a production
  system.

## Version 1.7.0

The 1.7.0 release of MCUboot adds support for the Mbed-OS platform,
Equal slots (direct-xip) upgrade mode, RAM loading upgrade mode,
hardening against hardware level fault injection and timing attacks
and single image mode.
There are bug fixes, and associated imgtool updates as well.

### About this release

- Initial support for the Mbed-OS platform.
- Added possibility to enter deep sleep mode after MCUboot app execution
  for cypress platform.
- Added hardening against hardware level fault injection and timing attacks.
- Introduced Abstract crypto primitives to simplify porting.
- Added RAM-load upgrade mode.
- Renamed single-image mode to single-slot mode.
- Allow larger primary slot in swap-move
- Fixed boostrapping in swap-move mode.
- Fixed issue causing that interrupted swap-move operation might brick device
  if the primary image was padded.
- Abstracting MCUboot crypto functions for cleaner porting
- Droped flash_area_read_is_empty() porting API.
- boot/zephyr: Added watchdog feed on nRF devices.
  See `CONFIG_BOOT_WATCHDOG_FEED` option.
- boot/zephyr: Added patch for turning off cache for Cortex M7 before
  chain-loading.
- boot/zephyr: added option to relocate interrupts to application
- boot/zephyr: clean ARM core configuration only when selected by user
- boot/boot_serial: allow nonaligned last image data chunk
- imgtool: added custom TLV support.
- imgtool: added possibility to set confirm flag for hex files as well.
- imgtool: Print image digest during verify.

### Zephyr-RTOS compatibility

This release of MCUboot works with the Zephyr "main" at the time of the
release. It was tested as of has 7a3b253ce. This version of MCUboot also
works with the Zephyr v2.4.0, however it is recommended to enable
`CONFIG_MCUBOOT_CLEANUP_ARM_CORE` while using that version.

## Version 1.6.0

The 1.6.0 release of MCUboot adds support for the PSOC6 platform,
X25519 encrypted images, rollback protection, hardware keys, and a
shared boot record to communicate boot attestation information to
later boot stages.  There are bug fixes, and associated imgtool
updates as well.

### About this release

- Initial support for the Cypress PSOC6 plaformt.  This platform
  builds using the Cypress SDK, which has been added as submodules.
- CBOR decoding in serial recovery replaced by code generated from a
  CDDL description.
- Add support for X25519 encrypted images.
- Add rollback protection.  There is support for a HW rollback counter
  (which must be provided as part of the platform), as well as a SW
  solution that protects against some types of rollback.
- Add an optional boot record in shared memory to communicate boot
  attributes to later-run code.
- Add support for hardware keys.
- Various fixes to work with the latest Zephyr version.

### Security issues addressed

- CVE-2020-7595 "xmlStringLenDecodeEntities in parser.c in libxml2
  2.9.10 has an infinite loop in a certain end-of-file situation." Fix
  by updating a dependency in documentation generation.

### Zephyr-RTOS compatibility

This release of MCUboot works the Zephyr "main" at the time of the
release.  It was tested as of has 1a89ca1238.  When Zephyr v2.3.0 is
released, there will be a possible 1.6.1 or similar release of Zephyr
if needed to address any issues.  There also may be branch releases of
MCUboot specifically for the current version of Zephyr, e.g.
v1.6.0-zephyr-2.2.1.

## Version 1.5.0

The 1.5.0 release of MCUboot adds support for encrypted images using
ECIES with secp256r1 as an Elliptic Curve alternative to RSA-OAEP. A
new swap method was added which allows for upgrades without using a
scratch partition. There are also lots of bug fixes, extra simulator
testing coverage and some imgtool updates.

### About this release

- TLVs were updated to use 16-bit lengths (from previous 8). This
  should work with no changes for little-endian targets, but will
  break compatibility with big-endian targets.
- A benchmark framework was added to Zephyr
- ed25519 signature validation can now build without using Mbed TLS
  by relying on a bundled tinycrypt based sha-512 implementation.
- imgtool was updated to correctly detect trailer overruns by image.
- Encrypted image TLVs can be saved in swap metadata during a swap
  upgrade instead of the plain AES key.
- imgtool can dump private keys in C format (getpriv command), which
  can be added as decryption keys. Optionally can remove superfluous
  fields from the ASN1 by passing it `--minimal`.
- Lots of other smaller bugs fixes.
- Added downgrade prevention feature (available when the overwrite-based
  image update strategy is used)

### Known issues

- TLV size change breaks compatibility with big-endian targets.

## Version 1.4.0

The 1.4.0 release of MCUboot primarily adds support for multi-image
booting.  With this release, MCUboot can manage two images that can be
updated independently.  With this, it also supports additions to the
TLV that allow these dependencies to be specified.

Multi-image support adds backward-incompatible changes to the format
of the images: specifically adding support for protected TLV entries.
If multiple images and dependencies are not used, the images will be
compatible with previous releases of MCUboot.

### About this release

- Fixed CVE-2019-5477, and CVE-2019-16892.  These fix issue with
  dependencies used in the generation of the documentation on github.
- Numerous code cleanups and refactorings
- Documentation updates for multi-image features
- Update imgtool.py to support the new features
- Updated the Mbed TLS submodule to current stable version 2.16.3
- Moved the Mbed TLS submodule from within sim/mcuboot-sys to ext.
  This will make it easier for other board supports to use this code.
- Added some additional overflow and bound checks to data in the image
  header, and TLV data.
- Add a `-x` (or `--hex_addr`) flag to imgtool to set the base address
  written to a hex-format image.  This allows the image to be flashed
  at an offset, without having to use additional tools to modify the
  image.

## Version 1.3.1

The 1.3.1 release of MCUboot consists mostly of small bug fixes and updates.
There are no breaking changes in functionality. This release should work with
Mynewt 1.6.0 and up, and any Zephyr `main` after sha
f51e3c296040f73bca0e8fe1051d5ee63ce18e0d.

### About this release

- Fixed a revert interruption bug
- Added ed25519 signing support
- Added RSA-3072 signing support
- Allow ec256 to run on CC310 interface
- Some preparation work was done to allow for multi image support, which
  should land in 1.4.0. This includes a simulator update for testing
  multi-images, and a new name for slot0/slot1 which are now called
  "primary slot" and "secondary slot".
- Other minor bugfixes and improvements

## Version 1.3.0

The 1.3.0 release of MCUboot brings in many fixes and updates.  There
are no breaking changes in functionality.  Many of the changes are
refactorings that will make the code easier to maintain going forward.
In addition, support has been added for encrypted images.  See [the
docs](encrypted_images.md) for more information.

### About this release

- Modernize the Zephyr build scripts.
- Add a `ptest` utility to help run the simulator in different
  configurations.
- Migrate the simulator to Rust 2018 edition.  The sim now requires at
  least Rust 1.32 to build.
- Simulator cleanups.  The simulator code is now built the same way
  for every configuration, and queries the MCUboot code for how it was
  compiled.
- Abstract logging in MCUboot.  This was needed to support the new
  logging system used in Zephyr.
- Add multiple flash support.  Allows slot1/scratch to be stored in an
  external flash device.
- Add support for [encrypted images](encrypted_images.md).
- Add support for flash devices that read as '0' when erased.
- Add support to Zephyr for the `nrf52840_pca10059`.  This board
  supports serial recovery over USB with CDC ACM.
- imgtool is now also available as a python package on pypi.org.
- Add an option to erase flash pages progressively during recovery to
  avoid possible timeouts (required especially by serial recovery
  using USB with CDC ACM).
- imgtool: big-endian support
- imgtool: saves in intel-hex format when output filename has `.hex`
  extension; otherwise saves in binary format.

## Version 1.2.0

The 1.2.0 release of MCUboot brings a lot of fixes/updates, where much of the
changes were on the boot serial functionality and imgtool utility. There are
no breaking changes in MCUboot functionality, but some of the CLI parameters
in imgtool were changed (either removed or added or updated).

### About this release

- imgtool accepts .hex formatted input
- Logging system is now configurable
- Most Zephyr configuration has been switched to Kconfig
- Build system accepts .pem files in build system to autogenerate required
  key arrays used internally
- Zephyr build switched to using built-in flash_map and TinyCBOR modules
- Serial boot has substantially decreased in space usage after refactorings
- Serial boot build doesn't require newlib-c anymore on Zephyr
- imgtool updates:
  + "create" subcommand can be used as an alias for "sign"
  + To allow imgtool to always perform the check that firmware does not
    overflow the status area, `--slot-size` was added and `--pad` was updated
    to act as a flag parameter.
  + `--overwrite-only` can be passed if not using swap upgrades
  + `--max-sectors` can be used to adjust the maximum amount of sectors that
    a swap can handle; this value must also be configured for the bootloader
  + `--pad-header` substitutes `--included-header` with reverted semantics,
    so it's not required for firmware built by Zephyr build system

### Known issues

None

## Version 1.1.0

The 1.1.0 release of MCUboot brings a lot of fixes/updates to its
inner workings, specially to its testing infrastructure which now
enables a more thorough quality assurance of many of the available
options. As expected of the 1.x.x release cycle, no breaking changes
were made. From the tooling perpective the main addition is
newt/imgtool support for password protected keys.

### About this release

- serial recovery functionality support under Zephyr
- simulator: lots of refactors were applied, which result in the
  simulator now leveraging the Rust testing infrastructure; testing
  of ecdsa (secp256r1) was added
- imgtool: removed PKCS1.5 support, added support for password
  protected keys
- tinycrypt 0.2.8 and the Mbed TLS ASN1 parser are now bundled with
  MCUboot (eg secp256r1 is now free of external dependencies!)
- Overwrite-only mode was updated to erase/copy only sectors that
  actually store firmware
- A lot of small code and documentation fixes and updates.

### Known issues

None

## Version 1.0.0

The 1.0.0 release of MCUboot introduces a format change.  It is
important to either use the `imgtool.py` also from this release, or
pass the `-2` to recent versions of the `newt` tool in order to
generate image headers with the new format.  There should be no
incompatible format changes throughout the 1.x.y release series.

### About this release

- Header format change.  This change was made to move all of the
  information about signatures out of the header and into the TLV
  block appended to the image.  This allows
  - The signature to be replaced without changing the image.
  - Multiple signatures to be applied.  This can be used, for example,
    to sign an image with two algorithms, to support different
    bootloader configurations based on these image.
  - The public key is referred to by its SHA1 hash (or a prefix of the
    hash), instead of an index that has to be maintained with the
    bootloader.
  - Allow new types of signatures in the future.
- Support for PKCS#1 v1.5 signatures has been dropped.  All RSA
  signatures should be made with PSS.  The tools have been changed to
  reflect this.
- The source for Tinycrypt has been placed in the MCUboot tree.  A
  recent version of Tinycrypt introduced breaking API changes.  To
  allow MCUboot to work across various platforms, we stop using the
  Tinycrypt bundled with the OS platform, and use our own version.  A
  future release of MCUboot will update the Tinycrypt version.
- Support for some new targets:
  - Nordic nRF51 and nRF52832 dev kits
  - Hexiwear K64
- Clearer sample applications have been added under `samples`.
- Test plans for [zephyr](testplan-zephyr.md), and
  [mynewt](testplan-mynewt.md).
- The simulator is now able to test RSA signatures.
- There is an unimplemented `load_addr` header for future support for
  RAM loading in the bootloader.
- Numerous documentation.

### Known issues

None

## Version 0.9.0

This is the first release of MCUboot, a secure bootloader for 32-bit MCUs.
It is designed to be operating system-agnostic and works over any transport -
wired or wireless. It is also hardware independent, and relies  on hardware
porting layers from the operating system it works with. For the first release,
we have support for three open source operating systems: Apache Mynewt, Zephyr
and RIOT.

### About this release

- This release supports building with and running Apache Mynewt and Zephyr
  targets.
- RIOT is supported as a running target.
- Image integrity is provided with SHA256.
- Image originator authenticity is provided supporting the following
  signature algorithms:
  - RSA 2048 and RSA PKCS#1 v1.5 or v2.1
  - Elliptic curve DSA with secp224r1 and secp256r1
- Two firmware upgrade algorithms are provided:
  - An overwrite only which upgrades slot 0 with the image in slot 1.
  - A swapping upgrade which enables image test, allowing for rollback to a
    previous known good image.
- Supports both Mbed TLS and tinycrypt as backend crypto libraries. One of them
  must be defined and the chosen signing algorithm will require a particular
  library according to this list:
  - RSA 2048 needs Mbed TLS
  - ECDSA secp224r1 needs Mbed TLS
  - ECDSA secp256r1 needs tinycrypt as well as the ASN.1 code from Mbed TLS
    (so still needs that present).

### Known issues

- The image header and TLV formats are planned to change with release 1.0:
  https://runtimeco.atlassian.net/browse/MCUB-66
