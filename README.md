# mcuboot

## Overview

MCUBoot is a secure bootloader for 32-bit MCUs.   The goal of MCUBoot is to 
define a common infrastructure for the bootloader, system flash layout on 
microcontroller systems, and to provide a secure bootloader that enables 
easy software upgrade.

MCUboot is operating system and hardware independent, and relies on 
hardware porting layers from the operating system it works with.  Currently
mcuboot works with both the Apache Mynewt, and Zephyr operating systems, but
more ports are planned in the future.

## Roadmap

The MCUBoot project was originally taken from the Apache Mynewt operating system,
which had secure boot and software upgrade functionality instrinsic to it.  Currently
development is heads down on a first release of MCUboot that works across both the 
Zephyr operating system and Apache Mynewt operating system.

For more information on what's being planned, and worked on, please visit: 

https://runtimeco.atlassian.net/projects/MCUB/summary

## Browsing 

Information and documentation on the bootloader is stored within the source, and on confluence:

https://runtimeco.atlassian.net/wiki/discover/all-updates

For more information in the source, here are some pointers: 

- [boot/bootutil][https://github.com/runtimeco/mcuboot/tree/master/boot/bootutil]: The core of the bootloader itself.
- [boot/boot\_serial][https://github.com/runtimeco/mcuboot/tree/master/boot/boot_serial]: Support for serial upgrade within the bootloader itself.
- [boot/zephyr][https://github.com/runtimeco/mcuboot/tree/master/boot/zephyr]: Port of the bootloader to Zephyr
- [imgtool][https://github.com/runtimeco/mcuboot/tree/master/imgtool]: A tool to securely sign firmware images for booting by mcuboot.
- [sim][https://github.com/runtimeco/mcuboot/tree/master/sim]: A bootloader simulator for testing and regression

## Joining 

Developers welcome!  To join in the discussion, please join the developer mailing list: 

https://runtimeco.mobilize.io/registrations/groups/9881
