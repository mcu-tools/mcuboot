### Port of MCUBoot library for evaluation with Cypress PSoC 6 chips

### Disclaimer

Given solution is included in `MCUboot` repository with purpose to demonstrate basic consepts and features of MCUboot library on Cypress PSoC 6 device. Applications are created per MCUboot library maintainers requirements. Implemetation differs from conventional and recomended by Cypress Semiconductors development flow for PSoC 6 devices. These applications are not recomended as a starting point for development and should not be considered as supported examples for PSoC 6 devices.

Examples provided to use with **ModusToolbox® Software Environment** are a recommended reference point to start development of MCUboot based bootloaders for PSoC 6 devices.

Refer to **Cypress Semiconductors** [github](https://github.com/cypresssemiconductorco) page to find examples.

1. MCUboot-Based Basic Bootloader [mtb-example-psoc6-mcuboot-basic](https://github.com/cypresssemiconductorco/mtb-example-psoc6-mcuboot-basic)
2. MCUboot-Based Bootloader with Rollback to Factory App in External Flash [mtb-example-anycloud-mcuboot-rollback](https://github.com/cypresssemiconductorco/mtb-example-anycloud-mcuboot-rollback)

### Solution description

There are two applications implemented:
* MCUBootApp - PSoC6 MCUboot-based bootloading application;
* BlinkyApp - simple PSoC6 blinking LED application which is a target of BOOT/UPGRADE;

The default flash map for MCUBootApp implemented is next:

* [0x10000000, 0x10018000] - MCUBootApp (bootloader) area;
* [0x10018000, 0x10028000] - primary slot for BlinkyApp;
* [0x10028000, 0x10038000] - secondary slot for BlinkyApp;
* [0x10038000, 0x10039000] - scratch area;

The flash map is defined through sysflash.h and memory.c.

It is also possible to place secondary (upgrade) slots in external memory module. In this case primary slot can be doubled in size.
For more details about External Memory usage, please refer to separate guiding document `MCUBootApp/ExternalMemory.md`.

MCUBootApp checks image integrity with SHA256, image authenticity with EC256 digital signature verification and uses either completely software implementation of cryptographic functions or accelerated by hardware - both based on Mbed TLS Library.

### Downloading solution's assets

There is a set assets required:

* MCUBooot Library (root repository)
* PSoC6 Peripheral Drivers Library (PDL)
* Mbed TLS Cryptographic Library

Those are represented as submodules.

To retrieve source code with subsequent submodules pull:

    git clone --recursive https://github.com/mcu-tools/mcuboot.git

Submodules can also be updated and initialized separately:

    cd mcuboot
    git submodule update --init --recursive



### Building solution

Root directory for build is **boot/cypress.**

This folder contains make files infrastructure for building both MCUboot Bootloader and sample BlinkyApp application used for Bootloader demo functionality.

Instructions on how to build and upload MCUBootApp bootloader application and sample user applocation are located in `Readme.md` files in corresponding folders.

Supported platforms for `MCUboot`, `BlinkyApp`:

**GCC_ARM** is only supported (built and verified on GCC 9.3.1).

It is included with [ModusToolbox™ Software Environment](https://www.cypress.com/products/modustoolbox).

The default installation folder is expected by the makefile build system.

To use another installation folder, version of **ModusToolbox™ IDE** or another GCC Compiler, specify the path to a toolchain using the **TOOLCHAIN_PATH** parameter.

Below is an example on how to set toolchin path to the latest include with **ModusToolbox™ IDE 3.0**:

    make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json TOOLCHAIN_PATH=c:/Users/${USERNAME}/ModusToolbox/tools_3.0/gcc

### Build environment troubleshooting

Following CLI / IDE are supported for project build:

* Cygwin on Windows systems
* unix style shells on *nix systems
* Eclipse / ModusToolbox ("makefile project from existing source")

*Make* - make sure it is added to system's `PATH` variable and correct path is first in the list;

*Python/Python3* - make sure you have correct path referenced in `PATH`;

*Msys2* - to use systems PATH navigate to msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart MSYS2 shell.

This will inherit system's PATH so should find `python3.7` installed in regular way as well as imgtool and its dependencies.

