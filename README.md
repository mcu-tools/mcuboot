# [mcuboot](http://mcuboot.com/)

[![Coverity Scan Build Status](https://scan.coverity.com/projects/12307/badge.svg)][coverity]
[![Build/Test](https://img.shields.io/travis/JuulLabs-OSS/mcuboot/master.svg?label=travis-ci)][travis]

[coverity]: https://scan.coverity.com/projects/mcuboot
[travis]: https://travis-ci.org/JuulLabs-OSS/mcuboot

This is mcuboot version 1.5.0-rc1

MCUboot is a secure bootloader for 32-bit MCUs. The goal of MCUboot is to
define a common infrastructure for the bootloader, system flash layout on
microcontroller systems, and to provide a secure bootloader that enables
simple software upgrades.

MCUboot is operating system and hardware independent and relies on
hardware porting layers from the operating. Currently, mcuboot works
with both the Apache Mynewt and Zephyr operating systems, but more
ports are planned in the future. RIOT is currently supported as a boot
target with a complete port planned.

## Using MCUboot

Instructions for different operating systems can be found here:
- [Zephyr](docs/readme-zephyr.md)
- [Mynewt](docs/readme-mynewt.md)
- [RIOT](docs/readme-riot.md)

## Roadmap

The issues being planned and worked on are tracked using GitHub issues. To
participate please visit:

[MCUBoot GitHub Issues](https://github.com/JuulLabs-OSS/mcuboot/issues)

~~Issues were previously tracked on [MCUboot JIRA](https://runtimeco.atlassian.net/projects/MCUB/summary)
, but it is now deprecated.~~

## Browsing

Information and documentation on the bootloader are stored within the source.

~~It was previously also documented on confluence:
[MCUBoot Confluence](https://runtimeco.atlassian.net/wiki/discover/all-updates)
however, it is now deprecated and not currently maintained~~

For more information in the source, here are some pointers:

- [boot/bootutil](boot/bootutil): The core of the bootloader itself.
- [boot/boot\_serial](boot/boot_serial): Support for serial upgrade within the bootloader itself.
- [boot/zephyr](boot/zephyr): Port of the bootloader to Zephyr
- [boot/mynewt](boot/mynewt): Mynewt bootloader app
- [imgtool](scripts/imgtool.py): A tool to securely sign firmware images for booting by mcuboot.
- [sim](sim): A bootloader simulator for testing and regression

## Joining

Developers welcome!

* Our developer mailing list:
  http://lists.runtime.co/mailman/listinfo/dev-mcuboot_lists.runtime.co
* Our Slack channel: https://mcuboot.slack.com/ <br />
  Get your invite [here!](https://join.slack.com/t/mcuboot/shared_invite/MjE2NDcwMTQ2MTYyLTE1MDA4MTIzNTAtYzgyZTU0NjFkMg)
* Our IRC channel: http://irc.freenode.net, #mcuboot
