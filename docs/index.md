# MCUboot

## Overview

MCUboot is a secure bootloader for 32-bit MCUs.   The goal of MCUboot is to
define a common infrastructure for the bootloader, system flash layout on
microcontroller systems, and to provide a secure bootloader that enables
easy software upgrade.

MCUboot is operating system and hardware independent and relies on
hardware porting layers from the operating system it works with.  Currently
MCUboot works with both the Apache Mynewt, and Zephyr operating systems, but
more ports are planned in the future. RIOT is currently supported as a boot
target with a complete port planned.

## Contents

- General - this document
- [Release notes](release-notes.md)
- [Bootloader design](design.md)
- [Encrypted images](encrypted_images.md)
- [imgtool](imgtool.md) - image signing and key management
- Usage instructions:
  - [Zephyr](readme-zephyr.md)
  - [Mynewt](readme-mynewt.md)
  - [RIOT](readme-riot.md)
- [Patch submission](SubmittingPatches.md) - information
  on how to contribute to mcuboot
- Testing
  - [Zephyr](testplan-zephyr.md) test plan
  - [mynewt](testplan-mynewt.md) test plan
- [Release process](release.md)

There is also a document about [signed images](signed_images.md) that is out
of date.  You should use `imgtool.py` instead of these documents.

## Roadmap

The issues being planned and worked on are tracked using GitHub issues. To participate
please visit:

[MCUboot Issues](https://github.com/JuulLabs-OSS/mcuboot/issues)

~~Issues were previously tracked on [MCUboot JIRA](https://runtimeco.atlassian.net/projects/MCUB/summary)
, but it is now deprecated.~~

## Browsing

Information and documentation on the bootloader is stored within the source.

~~It was previously also documented on confluence: [Confluence page](https://runtimeco.atlassian.net/wiki/discover/all-updates)
, but it is now deprecated and not currently maintained~~

For more information in the source, here are some pointers:

- [boot/bootutil](https://github.com/JuulLabs-OSS/mcuboot/tree/master/boot/bootutil): The core of the bootloader itself.
- [boot/boot\_serial](https://github.com/JuulLabs-OSS/mcuboot/tree/master/boot/boot_serial): Support for serial upgrade within the bootloader itself.
- [boot/zephyr](https://github.com/JuulLabs-OSS/mcuboot/tree/master/boot/zephyr): Port of the bootloader to Zephyr
- [boot/mynewt](https://github.com/JuulLabs-OSS/mcuboot/tree/master/boot/mynewt): Mynewt bootloader app
- [imgtool](https://github.com/JuulLabs-OSS/mcuboot/tree/master/scripts/imgtool.py): A tool to securely sign firmware images for booting by MCUboot.
- [sim](https://github.com/JuulLabs-OSS/mcuboot/tree/master/sim): A bootloader simulator for testing and regression

## Joining

Developers welcome!

* [Our developer mailing list](http://lists.runtime.co/mailman/listinfo/dev-mcuboot_lists.runtime.co)
* [Our Slack channel](https://mcuboot.slack.com/)<br />
  Get your invite [here!](https://join.slack.com/t/mcuboot/shared_invite/MjE2NDcwMTQ2MTYyLTE1MDA4MTIzNTAtYzgyZTU0NjFkMg)
* [Our IRC channel](http://irc.freenode.net), channel #mcuboot ([IRC
  link](irc://chat.freenode.net/#mcuboot)
