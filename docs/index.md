# MCUboot

MCUboot is a secure bootloader for 32-bit MCUs.

## Overview

The goal of MCUboot is to define a common infrastructure for the bootloader, system flash layout on microcontroller systems, and to provide a secure bootloader that enables easy software upgrades.

MCUboot is operating-system and hardware independent and relies on hardware porting layers from the operating system it works with.
Currently, MCUboot works with the following operating systems:
- [Zephyr](https://www.zephyrproject.org/)
- [Apache Mynewt](https://mynewt.apache.org/)
- [Apache NuttX](https://nuttx.apache.org/)
- [RIOT](https://www.riot-os.org/)
- [Mbed OS](https://os.mbed.com/)

RIOT is currently supported only as a boot target.
There is currently no defined plan to add any additional port.
However, we will accept any new port contributed by the community once it is good enough.

MCUboot is an open governance project.
See the [membership list](https://github.com/mcu-tools/mcuboot/wiki/Members) for current members, and the [project charter](https://github.com/mcu-tools/mcuboot/wiki/MCUboot-Project-Charter) for more details.

## Contents

- General - this document
- [Release notes](release-notes.md)
- [Bootloader design](design.md)
- [Encrypted images](encrypted_images.md)
- [imgtool](imgtool.md) - image signing and key management
- [ECDSA](ecdsa.md) - information about ECDSA signature formats
- Usage instructions:
  - [Zephyr](readme-zephyr.md)
  - [Apache Mynewt](readme-mynewt.md)
  - [Apache NuttX](readme-nuttx.md)
  - [RIOT](readme-riot.md)
  - [Mbed OS](readme-mbed.md)
- Testing:
  - [Zephyr](testplan-zephyr.md) test plan
  - [Mynewt](testplan-mynewt.md) test plan
- [Patch submission](SubmittingPatches.md) - information on how to contribute to MCUboot
- [Release process](release.md)
- [Project security policy](SECURITY.md)

## Roadmap

The issues being planned and worked on are tracked using GitHub issues.
To give your input, visit [MCUBoot GitHub Issues](https://github.com/mcu-tools/mcuboot/issues).

## Browsing

Information and documentation on the bootloader are stored within the source.

For more information, use the following pointers to the source:

- [boot/bootutil](https://github.com/mcu-tools/mcuboot/tree/main/boot/bootutil): The core of the bootloader itself.
- [boot/boot\_serial](https://github.com/mcu-tools/mcuboot/tree/main/boot/boot_serial): Support for serial upgrade within the bootloader itself.
- [boot/zephyr](https://github.com/mcu-tools/mcuboot/tree/main/boot/zephyr): Port of the bootloader to Zephyr.
- [boot/mynewt](https://github.com/mcu-tools/mcuboot/tree/main/boot/mynewt): Bootloader application for Mynewt.
- [boot/nuttx](https://github.com/mcu-tools/mcuboot/tree/main/boot/nuttx): Bootloader application and port of MCUboot interfaces for NuttX.
- [boot/mbed](https://github.com/mcu-tools/mcuboot/tree/main/boot/mbed): Port of the bootloader to Mbed OS.
- [imgtool](https://github.com/mcu-tools/mcuboot/tree/main/scripts/imgtool.py): A tool to securely sign firmware images for booting by MCUboot.
- [sim](https://github.com/mcu-tools/mcuboot/tree/main/sim): A bootloader simulator for testing and regression.

## Joining

Developers are welcome.

Use the following links to join or see more about the project:

* [Our developer mailing list](https://groups.io/g/MCUBoot)
* [Our Slack channel](https://mcuboot.slack.com/) - get [your invite](https://join.slack.com/t/mcuboot/shared_invite/MjE2NDcwMTQ2MTYyLTE1MDA4MTIzNTAtYzgyZTU0NjFkMg)
* [Current members](https://github.com/mcu-tools/mcuboot/wiki/Members)
* [Project charter](https://github.com/mcu-tools/mcuboot/wiki/MCUboot-Project-Charter)
