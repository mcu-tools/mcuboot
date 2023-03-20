# [MCUboot](http://mcuboot.com/)

[![Package on PyPI](https://img.shields.io/pypi/v/imgtool.svg)][pypi]
[![Coverity Scan Build Status](https://scan.coverity.com/projects/12307/badge.svg)][coverity]
[![Build Status (Sim)](https://github.com/mcu-tools/mcuboot/workflows/Sim/badge.svg)][sim]
[![Build Status (Mynewt)](https://github.com/mcu-tools/mcuboot/workflows/Mynewt/badge.svg)][mynewt]
[![Publishing Status (imgtool)](https://github.com/mcu-tools/mcuboot/workflows/imgtool/badge.svg)][imgtool]
[![Build Status (Travis CI)](https://img.shields.io/travis/mcu-tools/mcuboot/main.svg?label=travis-ci)][travis]
[![Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)][license]

[pypi]: https://pypi.org/project/imgtool/
[coverity]: https://scan.coverity.com/projects/mcuboot
[sim]: https://github.com/mcu-tools/mcuboot/actions?query=workflow:Sim
[mynewt]: https://github.com/mcu-tools/mcuboot/actions?query=workflow:Mynewt
[imgtool]: https://github.com/mcu-tools/mcuboot/actions?query=workflow:imgtool
[travis]: https://travis-ci.org/mcu-tools/mcuboot
[license]: https://github.com/mcu-tools/mcuboot/blob/main/LICENSE

This is MCUboot version 1.10.0-rc1

MCUboot is a secure bootloader for 32-bits microcontrollers. It defines a
common infrastructure for the bootloader and the system flash layout on
microcontroller systems, and provides a secure bootloader that enables easy
software upgrade.

MCUboot is not dependent on any specific operating system and hardware and
relies on hardware porting layers from the operating system it works with.
Currently, MCUboot works with the following operating systems and SoCs:
- [Zephyr](https://www.zephyrproject.org/)
- [Apache Mynewt](https://mynewt.apache.org/)
- [Apache NuttX](https://nuttx.apache.org/)
- [RIOT](https://www.riot-os.org/)
- [Mbed OS](https://os.mbed.com/)
- [Espressif](https://www.espressif.com/)
- [Cypress/Infineon](https://www.cypress.com/)

RIOT is supported only as a boot target. We will accept any new
port contributed by the community once it is good enough.

## MCUboot How-tos

See the following pages for instructions on using MCUboot with different
operating systems and SoCs:
- [Zephyr](docs/readme-zephyr.md)
- [Apache Mynewt](docs/readme-mynewt.md)
- [Apache NuttX](docs/readme-nuttx.md)
- [RIOT](docs/readme-riot.md)
- [Mbed OS](docs/readme-mbed.md)
- [Espressif](docs/readme-espressif.md)
- [Cypress/Infineon](boot/cypress/README.md)

There are also instructions for the [Simulator](sim/README.rst).

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
