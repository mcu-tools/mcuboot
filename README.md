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

This is MCUboot version 1.8.0-rc1

MCUboot is a secure bootloader for 32-bit MCUs.
The goal of MCUboot is to define a common infrastructure for the bootloader, system flash layout on microcontroller systems, and to provide a secure bootloader that enables simple software upgrades.

MCUboot is operating-system and hardware independent and relies on hardware porting layers from the operating system.
Currently, MCUboot works with the following operating systems:
- [Zephyr](https://www.zephyrproject.org/)
- [Apache Mynewt](https://mynewt.apache.org/)
- [Apache NuttX](https://nuttx.apache.org/)
- [RIOT](https://www.riot-os.org/)
- [Mbed OS](https://os.mbed.com/)

RIOT is currently supported only as a boot target.
There is currently no defined plan to add any additional port.
However, we will accept any new port contributed by the community once it is good enough.

## Using MCUboot

Instructions for different operating systems can be found below:
- [Zephyr](docs/readme-zephyr.md)
- [Apache Mynewt](docs/readme-mynewt.md)
- [Apache NuttX](docs/readme-nuttx.md)
- [RIOT](docs/readme-riot.md)
- [Mbed OS](docs/readme-mbed.md)

There are also instructions for the [Espressif IDF](docs/readme-espressif.md) SDK and for the [Simulator](sim/README.rst).

## Roadmap

The issues being planned and worked on are tracked using GitHub issues.
To give your input, visit [MCUBoot GitHub Issues](https://github.com/mcu-tools/mcuboot/issues).

## Browsing

Information and documentation on the bootloader are stored within the source.

For more information, use the following pointers to the source:

- [boot/bootutil](boot/bootutil): The core of the bootloader itself.
- [boot/boot\_serial](boot/boot_serial): Support for serial upgrade within the bootloader itself.
- [boot/zephyr](boot/zephyr): Port of the bootloader to Zephyr.
- [boot/mynewt](boot/mynewt): Bootloader application for Mynewt.
- [boot/nuttx](boot/nuttx): Bootloader application and port of MCUboot interfaces for NuttX.
- [boot/mbed](boot/mbed): Port of the bootloader to Mbed OS.
- [boot/espressif](boot/espressif): Bootloader application and MCUboot port for Espressif SoCs.
- [imgtool](scripts/imgtool.py): A tool to securely sign firmware images for booting by MCUboot.
- [sim](sim): A bootloader simulator for testing and regression.

## Joining

Developers are welcome.

Use the following links to join or see more about the project:

* [Our developer mailing list](https://groups.io/g/MCUBoot)
* [Our Slack channel](https://mcuboot.slack.com/) - get [your invite](https://join.slack.com/t/mcuboot/shared_invite/MjE2NDcwMTQ2MTYyLTE1MDA4MTIzNTAtYzgyZTU0NjFkMg)
