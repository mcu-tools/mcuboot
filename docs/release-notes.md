# Release notes

- Table of Contents:
  * [Version 1.8.0](#version-180)
    + [About this release](#about-this-release)
    + [Security fixes](#security-fixes)
  * [Version 1.7.0](#version-170)
    + [About this release](#about-this-release)
    + [Zephyr-RTOS Compatibility](#zephyr-rtos-compatibility)
  * [Version 1.6.0](#version-160)
    + [About this release](#about-this-release-2)
    + [Security issues addressed](#security-issues-addressed)
    + [Zephyr-RTOS Compatibility](#zephyr-rtos-compatibility-1)
  * [Version 1.5.0](#version-150)
    + [About this release](#about-this-release-3)
    + [Known issues](#known-issues)
  * [Version 1.4.0](#version-140)
    + [About this release](#about-this-release-4)
  * [Version 1.3.1](#version-131)
    + [About this release](#about-this-release-5)
  * [Version 1.3.0](#version-130)
    + [About this release](#about-this-release-6)
  * [Version 1.2.0](#version-120)
    + [About this release](#about-this-release-7)
    + [Known issues](#known-issues-1)
  * [Version 1.1.0](#version-110)
    + [About this release](#about-this-release-8)
    + [Known issues](#known-issues-2)
  * [Version 1.0.0](#version-100)
    + [About this release](#about-this-release-9)
    + [Known issues](#known-issues-3)
  * [Version 0.9.0](#version-090)
    + [About this release](#about-this-release-10)
    + [Known issues](#known-issues-4)

## Version 1.8.0

The 1.8.0 release of MCUboot contains numerous fixes, adds support for the
NuttX RTOS, and for the Espressif ESP32 SDK.

### About this release

- Added support for the NuttX RTOS.
- Added support for the Espressif ESP32 SDK.
- `boot_serial` changed to use cddl-gen, which removes the dependency on
  tinycbor.
- Added various hooks to be able to change how image data is accessed.
- Cypress now supports Mbed TLS for encryption.
- Support using Mbed TLS for ECDSA.
  This can be useful if Mbed TLS is brought in for other reasons.
- Added simulator support for testing direct-XIP and ramload.
- Added support for Mbed TLS 3.0.
  Updated the submodule for Mbed TLS to 3.0.
- Enabled direct-xip mode in the Mbed OS port.
- Extracted `bootutil_public` library, a common interface for MCUboot and
  the application.
- Introduced the possibility to boot the primary image if the secondary
  one is unreachable.
- Added AES256 image encryption support.
- Added multi-image boot for both direct-xip and ram-load modes.

### Security fixes

- [GHSA-gcxh-546h-phg4](https://github.com/mcu-tools/mcuboot/security/advisories/GHSA-gcxh-546h-phg4)
  has been published. Currently, there is no fix available. Be sure to
  follow the instructions carefully, and make sure that the development
  keys in the repo are never used in a production system.

## Version 1.7.0

The 1.7.0 release of MCUBoot adds support for the Mbed OS platform, equal
slots (direct-xip) upgrade mode, RAM loading upgrade mode, hardening
against hardware-level fault injection and timing attacks and single image
mode. There are bug fixes, and associated imgtool updates as well.

### About this release

- Initial support for the Mbed OS platform.
- Added possibility to enter deep sleep mode after MCUboot app execution
  for the Cypress platform.
- Added hardening against hardware-level fault injection and timing
  attacks.
- Introduced Abstract crypto primitives to simplify porting.
- Added RAM-load upgrade mode.
- Renamed single-image mode to single-slot mode.
- Allow larger primary slot in swap-move
- Fixed bootstrapping in swap-move mode.
- Fixed issue causing that interrupted swap-move operation might brick the
  device if the primary image was padded.
- Abstracting MCUboot crypto functions for cleaner porting.
- Dropped `flash_area_read_is_empty()` porting API.
- boot/zephyr: Added watchdog feed on nRF devices.
  See `CONFIG_BOOT_WATCHDOG_FEED` option.
- boot/zephyr: Added patch for turning off cache for Cortex M7 before
  chain-loading.
- boot/zephyr: added option to relocate interrupts to application.
- boot/zephyr: clean ARM core configuration only when selected by the
  user.
- boot/boot_serial: allow nonaligned last image data chunk.
- imgtool: added custom TLV support.
- imgtool: added possibility to set confirm flag for hex files as well.
- imgtool: Print image digest during verify.

### Zephyr-RTOS compatibility

This release of MCUboot works with the Zephyr "main" at the time of the
release. It was tested as of hash `7a3b253ce`. This version of MCUboot
also works with the Zephyr v2.4.0, however, it is recommended to enable
`CONFIG_MCUBOOT_CLEANUP_ARM_CORE` while using that version.

## Version 1.6.0

The 1.6.0 release of MCUboot adds support for the PSOC6 platform, X25519
encrypted images, rollback protection, hardware keys, and a shared boot
record to communicate boot attestation information to later boot stages.
There are bug fixes and associated imgtool updates as well.

### About this release

- Initial support for the Cypress PSOC6 platform. This platform builds
  using the Cypress SDK, which has been added as submodules.
- Replaced CBOR decoding in serial recovery with code generated from a
  CDDL description.
- Added support for X25519 encrypted images.
- Added rollback protection. There is support for a hardware rollback
  counter (which must be provided as part of the platform), as well as a
  software solution that protects against some types of rollback.
- Added an optional boot record in shared memory to communicate boot
  attributes to later-run code.
- Added support for hardware keys.
- Various fixes to work with the latest Zephyr version.

### Security issues addressed

- CVE-2020-7595 "xmlStringLenDecodeEntities in parser.c in libxml2 2.9.10
  has an infinite loop in a certain end-of-file situation." Fix by
  updating a dependency in documentation generation.

### Zephyr-RTOS compatibility

This release of MCUboot works the Zephyr "main" at the time of the
release. It was tested as of hash `1a89ca1238`. When Zephyr v2.3.0 is
released, there will be a possible 1.6.1 or similar release of Zephyr if
needed to address any issues. There also may be branch releases of MCUboot
specifically for the current version of Zephyr, for example,
v1.6.0-zephyr-2.2.1.

## Version 1.5.0

The 1.5.0 release of MCUboot adds support for encrypted images using ECIES
with secp256r1 as an Elliptic Curve alternative to RSA-OAEP. A new swap
method was added which allows for upgrades without using a scratch
partition. There are also lots of bug fixes, extra simulator testing
coverage, and some imgtool updates.

### About this release

- TLVs were updated to use 16-bit lengths (from the previous 8). This
  should work with no changes for little-endian targets, but will break
  compatibility with big-endian targets.
- A benchmark framework was added to Zephyr
- ed25519 signature validation can now build without using Mbed TLS by
  relying on a bundled tinycrypt based sha-512 implementation.
- imgtool was updated to correctly detect trailer overruns by image.
- Encrypted image TLVs can be saved in swap metadata during a swap upgrade
  instead of the plain AES key.
- imgtool can dump private keys in C format (getpriv command), which can
  be added as decryption keys. Optionally can remove superfluous fields from
  the ASN1 by passing it `--minimal`.
- Lots of other smaller bugs fixes.
- Added downgrade prevention feature (available when the overwrite-based
  image update strategy is used)

### Known issues

- TLV size change breaks compatibility with big-endian targets.

## Version 1.4.0

The 1.4.0 release of MCUboot primarily adds support for multi-image
booting. With this release, MCUboot can manage two images that can be
updated independently. With this, it also supports additions to the TLV
that allows these dependencies to be specified.

Multi-image support adds backward-incompatible changes to the format of
the images: specifically adding support for protected TLV entries. If
multiple images and dependencies are not used, the images will be
compatible with previous releases of MCUboot.

### About this release

- Fixed CVE-2019-5477, and CVE-2019-16892. These fix an issue with
  dependencies used in the generation of the documentation on github.
- Numerous code cleanups and refactorings.
- Documentation updates for multi-image features.
- Updated imgtool.py to support the new features.
- Updated the Mbed TLS submodule to the current stable version 2.16.3.
- Moved the Mbed TLS submodule from within `sim/mcuboot-sys` to ext. This
  will make it easier for other boards to use this code.
- Added some additional overflow and bound checks to data in the image
  header, and TLV data.
- Added a `-x` (or `--hex_addr`) flag to imgtool to set the base address
  written to a hex-format image. This allows the image to be flashed at an
  offset, without having to use additional tools to modify the image.

## Version 1.3.1

The 1.3.1 release of MCUboot consists mostly of small bug fixes and
updates. There are no breaking changes in functionality. This release
should work with Mynewt 1.6.0 and up, and any Zephyr `main` after SHA
`f51e3c296040f73bca0e8fe1051d5ee63ce18e0d`.

### About this release

- Fixed a revert interruption bug.
- Added ed25519 signing support.
- Added RSA-3072 signing support.
- Allowed ec256 to run on CC310 interface.
- Some preparation work was done to allow for multi-image support, which
 should land in 1.4.0. This includes a simulator update for testing
 multi-images and a new name for slot0/slot1 which are now called "primary
 slot" and "secondary slot".
- Other minor bug fixes and improvements.

## Version 1.3.0

The 1.3.0 release of MCUboot brings in many fixes and updates. There are
no breaking changes in functionality. Many of the changes are refactorings
that will make the code easier to maintain going forward. In addition,
support has been added for encrypted images. See [the
docs](encrypted_images.md) for more information.

### About this release

- Modernized the Zephyr build scripts.

- Added a `ptest` utility to help run the simulator in different
  configurations.
- Migrated the simulator to Rust 2018 edition. The sim now requires at
  least Rust 1.32 to build.
- Simulator cleanups. The simulator code is now built the same way for
  every configuration, and queries the MCUboot code for how it was compiled.
- Abstract logging in MCUboot. This was needed to support the new logging
  system used in Zephyr.
- Added multiple flash support. Allows slot1/scratch to be stored in an
  external flash device.
- Added support for [encrypted images](encrypted_images.md).
- Added support for flash devices that read as '0' when erased.
- Added support to Zephyr for the `nrf52840_pca10059`. This board supports
  serial recovery over USB with CDC ACM.
- imgtool is now also available as a python package on pypi.org.
- Added an option to erase flash pages progressively during recovery to
  avoid possible timeouts (required especially by serial recovery using USB
  with CDC ACM).
- imgtool: big-endian support.
- imgtool: saves in intel-hex format when output filename has `.hex`
  extension; otherwise saves in binary format.

## Version 1.2.0

The 1.2.0 release of MCUboot brings a lot of fixes/updates, where much of
the changes were on the boot serial functionality and imgtool utility.
There are no breaking changes in MCUBoot functionality, but some of the
CLI parameters in imgtool were changed (either removed or added or
updated).

### About this release

- imgtool accepts .hex formatted input.
- Logging system is now configurable.
- Most Zephyr configuration has been switched to Kconfig.
- Build system accepts .pem files in build system to autogenerate required
  key arrays used internally.
- Zephyr build switched to using built-in flash_map and TinyCBOR modules.
- Serial boot has substantially decreased in space usage after
  refactorings.
- Serial boot build doesn't require newlib-c anymore on Zephyr.
- imgtool updates:
  + "create" subcommand can be used as an alias for "sign".
  + To allow imgtool to always perform the check that firmware does not
    overflow the status area, `--slot-size` was added and `--pad` was
    updated to act as a flag parameter.
  + `--overwrite-only` can be passed if not using swap upgrades.
  + `--max-sectors` can be used to adjust the maximum amount of sectors
    that a swap can handle; this value must also be configured for the
    bootloader.
  + `--pad-header` substitutes `--included-header` with reverted
    semantics, so it's not required for firmware built by the Zephyr build
    system.

### Known issues

None

## Version 1.1.0

The 1.1.0 release of MCUboot brings a lot of fixes/updates to its inner
workings, especially to its testing infrastructure which now enables a
more thorough quality assurance of many of the available options. As
expected of the 1.x.x release cycle, no breaking changes were made. From
the tooling perpective, the main addition is newt/imgtool support for
password-protected keys.

### About this release

- Serial recovery functionality support under Zephyr.
- Simulator: lots of refactors were applied, which result in the simulator
  now leveraging the Rust testing infrastructure; testing of ecdsa
  (secp256r1) was added.
- imgtool: removed PKCS1.5 support, added support for password-protected
  keys.
- tinycrypt 0.2.8 and the mbed-tls ASN1 parser are now bundled with
  mcuboot (for example, secp256r1 is now free of external dependencies).
- Overwrite-only mode was updated to erase/copy only sectors that actually
  store firmware.
- A lot of small code and documentation fixes and updates.

### Known issues

None

## Version 1.0.0

The 1.0.0 release of MCUboot introduces a format change. It is important
to either use the `imgtool.py` also from this release, or pass the `-2` to
recent versions of the `newt` tool in order to generate image headers with
the new format. There should be no incompatible format changes throughout
the 1.x.y release series.

### About this release

- Header format change. This change was made to move all of the
  information about signatures out of the header and into the TLV block
  appended to the image. This allows the following:
  - The signature to be replaced without changing the image.
  - Multiple signatures to be applied. This can be used, for example, to
    sign an image with two algorithms, to support different bootloader
    configurations based on these images.
  - The public key is referred to by its SHA1 hash (or a prefix of the
    hash), instead of an index that has to be maintained with the
    bootloader.
  - Allow new types of signatures in the future.
- Support for PKCS#1 v1.5 signatures has been dropped. All RSA signatures
  should be made with PSS. The tools have been changed to reflect this.
- The source for Tinycrypt has been placed in the MCUboot tree. A recent
  version of Tinycrypt introduced breaking API changes. To allow MCUboot to
  work across various platforms, we stop using the Tinycrypt bundled with
  the OS platform, and use our own version. A future release of MCUboot will
  update the Tinycrypt version.
- Support for some new targets:
  - Nordic nRF51 and nRF52832 development kits
  - Hexiwear K64
- Clearer sample applications have been added under `samples`.
- Test plans for [Zephyr](testplan-zephyr.md), and
  [Mynewt](testplan-mynewt.md).
- The simulator is now able to test RSA signatures.
- There is an unimplemented `load_addr` header for future support for RAM
  loading in the bootloader.
- Numerous documentation changes.

### Known issues

None

## Version 0.9.0

This is the first release of MCUboot, a secure bootloader for 32-bit MCUs.
It is designed to be operating system-agnostic and works over any
transport - wired or wireless. It is also hardware-independent, and relies
on hardware porting layers from the operating system it works with. For
the first release, we have support for three open-source operating
systems: Apache Mynewt, Zephyr and RIOT.

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
  - An overwrite only, which upgrades slot 0 with the image in slot 1.
  - A swapping upgrade, which enables image test, allowing for rollback to
    a previous known good image.
- Supports both Mbed TLS and tinycrypt as backend crypto libraries. One of
  them must be defined and the chosen signing algorithm will require a
  particular library according to this list:
  - RSA 2048 needs Mbed TLS.
  - ECDSA secp224r1 needs Mbed TLS.
  - ECDSA secp256r1 needs tinycrypt as well as the ASN.1 code from Mbed
  TLS (so still needs that present).

### Known issues

- The image header and TLV formats are planned to change with release 1.0:
  https://runtimeco.atlassian.net/browse/MCUB-66
