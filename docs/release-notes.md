# MCUboot Release Notes

- Table of Contents
{:toc}

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
- tinycrypt 0.2.8 and the mbed-tls ASN1 parser are now bundled with
  mcuboot (eg secp256r1 is now free of external dependencies!)
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
- Test plans for [zephyr](%{ link testplan-zephyr.md %}), and
  [mynewt]({% link testplan-mynewt.md %}).
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
- Supports both mbed-TLS and tinycrypt as backend crypto libraries. One of them
  must be defined and the chosen signing algorithm will require a particular
  library according to this list:
  - RSA 2048 needs mbed TLS
  - ECDSA secp224r1 needs mbed TLS
  - ECDSA secp256r1 needs tinycrypt as well as the ASN.1 code from mbed TLS
    (so still needs that present).

### Known issues

- The image header and TLV formats are planned to change with release 1.0:
  https://runtimeco.atlassian.net/browse/MCUB-66
