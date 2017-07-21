mcuboot 0.9 - Release Notes
===========================

This is the first release of MCUBoot, a secure bootloader for 32-bit MCUs.
It is designed to be operating system-agnostic and works over any transport -
wired or wireless. It is also hardware independent, and relies  on hardware
porting layers from the operating system it works with. For the first release,
we have support for three open source operating systems: Apache Mynewt, Zephyr
and RIOT.

About this release:
===================

* This release supports building with and running Apache Mynewt and Zephyr
  targets.
* RIOT is supported as a running target.
* Image integrity is provided with SHA256.
* Image originator authenticity is provided supporting the following
  signature algorithms:
  - RSA 2048 and RSA PKCS#1 v1.5 or v2.1
  - Elliptic curve DSA with secp224r1 and secp256r1
* Two firmware upgrade algorithms are provided:
  - An overwrite only which upgrades slot 0 with the image in slot 1.
  - A swapping upgrade which enables image test, allowing for rollback to a
  previous known good image.
* Supports both mbed-TLS and tinycrypt as backend crypto libraries. One of them
  must be defined and the chosen signing algorithm will require a particular
  library according to this list:
  - RSA 2048 needs mbed TLS
  - ECDSA secp224r1 needs mbed TLS
  - ECDSA secp256r1 needs tinycrypt as well as the ASN.1 code from mbed TLS
  (so still needs that present).

Known issues:
=============

* The image header and TLV formats are planned to change with release 1.0:
  https://runtimeco.atlassian.net/browse/MCUB-66
