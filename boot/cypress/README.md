### Port of MCUBoot library for evaluation with Cypress PSoC 6 chips

### Disclaimer

Given solution is included in `MCUboot` repository with purpose to demonstrate basic consepts and features of MCUboot library on Infineon Technologies devices.

### Supported platforms

| Family   | Platforms          |
---------- | -------------------|
| PSOC6    | PSOC6 1M, 2M, 512K |
| CYWxx829 | CYW20829, CYW89829 |
| XMC7x00  | XMC7200, XMC7100   |

### Solution description

There are two applications implemented:
* MCUBootApp - MCUboot-based bootloader implementation;
* BlinkyApp - simple blinking LED application which is a target of BOOT/UPGRADE;

Detailed description on each application is provided in dedicated files:

Bootloader - [MCUBootApp.md](./MCUBootApp/MCUBootApp.md)
Test Application - [BlinkyApp.md](./BlinkyApp/BlinkyApp.md)

Separate documentation is available for External Memory usage in mcuboot [ExternalMemory.md](./MCUBootApp/ExternalMemory.md)

### Downloading solution

Since libraries required by mcuboot Infineon implementation are implemented as submodules following commands needs to be executed.

To retrieve source code with subsequent submodules pull:

    git clone --recursive https://github.com/mcu-tools/mcuboot.git

Submodules can also be updated and initialized separately:

    cd mcuboot
    git submodule update --init --recursive



### Building solution

Root directory for build is **boot/cypress.**

This folder contains make files infrastructure for building both MCUbootApp and sample BlinkyApp application used for Bootloader demo functionality.

**GCC_ARM** is only supported toolchain.

It is recommended to use [ModusToolbox™ Software Environment](https://www.cypress.com/products/modustoolbox) which includes GCC Toolchain.

The default installation folder is expected by the makefile build system.

To use another installation folder, version of **ModusToolbox™ IDE** or another GCC Compiler, specify the path to a toolchain using the **TOOLCHAIN_PATH** parameter.

Below is an example on how to set toolchain path to the latest include with **ModusToolbox™ IDE**:

    make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json TOOLCHAIN_PATH=c:/Users/${USERNAME}/ModusToolbox/tools_3.2/gcc

**Python3** needs to be installed in system since build process required execution of prebuild and postbuild scripts in python.

### Build environment troubleshooting

Following CLI / IDE are supported for project build:

* Cygwin on Windows systems
* unix style shells on *nix systems
* Eclipse / ModusToolbox ("makefile project from existing source")

*Make* - make sure it is added to system's `PATH` variable and correct path is first in the list;

*Python/Python3* - make sure you have correct path referenced in `PATH`;

*Msys2* - to use systems PATH navigate to msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart MSYS2 shell.

This will inherit system's PATH so should find `python3.7` installed in regular way as well as imgtool and its dependencies.

