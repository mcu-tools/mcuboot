# MCUboot

MCUboot is a secure bootloader for 32-bits microcontrollers.

## Overview

MCUboot defines a common infrastructure for the bootloader and the system flash
layout on microcontroller systems, and provides a secure bootloader that
enables easy software upgrade.

MCUboot is not dependent on any specific operating system and hardware and
relies on hardware porting layers from the operating system it works with.
Currently MCUboot works with the following operating systems and SoCs:
- [Zephyr](https://www.zephyrproject.org/)
- [Apache Mynewt](https://mynewt.apache.org/)
- [Apache NuttX](https://nuttx.apache.org/)
- [RIOT](https://www.riot-os.org/)
- [Mbed OS](https://os.mbed.com/)
- [Espressif](https://www.espressif.com/)
- [Cypress/Infineon](https://www.cypress.com/)

RIOT is supported only as a boot target. We will accept any new port
contributed by the community once it is good enough.

MCUboot is an open governance project. See the [membership
list](https://github.com/mcu-tools/mcuboot/wiki/Members) for current
members, and the
[project charter](https://github.com/mcu-tools/mcuboot/wiki/MCUboot-Project-Charter)
for more details.

## Documentation

The MCUboot documentation is composed of the following pages:

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
  - [Espressif](readme-espressif.md)
  - [Cypress/Infineon](https://github.com/mcu-tools/mcuboot/tree/main/boot/cypress/README.md)
  - [Simulator](https://github.com/mcu-tools/mcuboot/tree/main/sim/README.rst)
- Testing
  - [Zephyr](testplan-zephyr.md) - Zephyr test plan
  - [Apache Mynewt](testplan-mynewt.md) - Apache Mynewt test plan
- [Release process](release.md)
- [Project security policy](SECURITY.md)
- [Patch submission](SubmittingPatches.md) - information
  on how to contribute to MCUboot

The documentation page about [signed images](signed_images.md) is currently
outdated. Follow the instructions in [imgtool](imgtool.md) instead.

## Roadmap

The issues being planned and worked on are tracked using GitHub issues. To
give your input, visit [MCUboot GitHub
Issues](https://github.com/mcu-tools/mcuboot/issues).

## Source files

You can find additional documentation on the bootloader in the source files.
For more information, use the following links:
- [boot/bootutil](https://github.com/mcu-tools/mcuboot/tree/main/boot/bootutil) - The core of the bootloader itself.
- [boot/boot\_serial](https://github.com/mcu-tools/mcuboot/tree/main/boot/boot_serial) - Support for serial upgrade within the bootloader itself.
- [boot/zephyr](https://github.com/mcu-tools/mcuboot/tree/main/boot/zephyr) - Port of the bootloader to Zephyr.
- [boot/mynewt](https://github.com/mcu-tools/mcuboot/tree/main/boot/mynewt) - Bootloader application for Apache Mynewt.
- [boot/nuttx](https://github.com/mcu-tools/mcuboot/tree/main/boot/nuttx) - Bootloader application and port of MCUboot interfaces for Apache NuttX.
- [boot/mbed](https://github.com/mcu-tools/mcuboot/tree/main/boot/mbed) - Port of the bootloader to Mbed OS.
- [boot/espressif](https://github.com/mcu-tools/mcuboot/tree/main/boot/espressif) - Bootloader application and MCUboot port for Espressif SoCs.
- [boot/cypress](https://github.com/mcu-tools/mcuboot/tree/main/boot/cypress) - Bootloader application and MCUboot port for Cypress/Infineon SoCs.
- [imgtool](https://github.com/mcu-tools/mcuboot/tree/main/scripts/imgtool.py) - A tool to securely sign firmware images for booting by MCUboot.
- [sim](https://github.com/mcu-tools/mcuboot/tree/main/sim) - A bootloader simulator for testing and regression.

## Joining the project

Developers are welcome!

Use the following links to join or see more about the project:

* [Our developer mailing list](https://groups.io/g/MCUBoot)
* [Our Slack channel](https://mcuboot.slack.com/) <br />
  Get [your invite](https://join.slack.com/t/mcuboot/shared_invite/MjE2NDcwMTQ2MTYyLTE1MDA4MTIzNTAtYzgyZTU0NjFkMg)
* [Current members](https://github.com/mcu-tools/mcuboot/wiki/Members)
* [Project charter](https://github.com/mcu-tools/mcuboot/wiki/MCUboot-Project-Charter)
