## Port of MCUboot library for evaluation with Cypress PSoC™ 6 and CYW20829 chips

### Disclaimer

This solution is included in the `MCUboot` repository in order to demonstrate the basic concepts and features of the MCUboot library on PSoC™ 6 and CYW20829 devices. Applications are created per MCUboot library maintainers requirements. The implementation differs from conventional and recommended by Cypress Semiconductors development flow for PSoC™ 6 and CYW20829 devices. These applications are not recommended as a starting point for development because they are not supported examples.

Examples provided to use with **ModusToolbox™ Software Environment** are a recommended reference point to start development of MCUboot based bootloaders for PSoC™ 6 and CYW20829 devices.

For examples, refer to the **Infineon Technologies AG** [github](https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software) page.

1. MCUboot-based basic bootloader [mtb-example-psoc6-mcuboot-basic](https://github.com/Infineon/mtb-example-psoc6-mcuboot-basic)
2. MCUboot-based bootloader with rollback to factory app in external flash [mtb-example-anycloud-mcuboot-rollback](https://github.com/Infineon/mtb-example-anycloud-mcuboot-rollback)

### Solution description

The two applications implemented:
* MCUBootApp - PSoC™ 6 and CYW20829 MCUboot-based bootloading application
* BlinkyApp - a simple PSoC™ 6 and CYW20829 blinking LED application, which is a target of BOOT/UPGRADE

#### MCUBootApp

* The two types of upgrade operation supported:
  * **Overwrite only** - The secondary image is only copied to the primary slot after validation.
  * **Swap** - The secondary and primary slots images are swapped during the upgrade process. Upgrade operation can be reverted if the secondary image is bad. "Bad image" does not set the imageOK flag in the image trailer. If imageOK is not set, MCUBootApp does not turn off WatchDog Timer and WDT resets the device to start the REVERT procedure.

* The two types of operation modes supported:
  * Single image
  * Multi image

* Some or all partitions (slots) can be placed in external memory. For more details about external memory usage, refer to [ExternalMemory.md](MCUBootApp/ExternalMemory.md).

* MCUBootApp checks the image integrity with SHA256, image authenticity with EC256 digital signature verification.
* Cryptographic functions can be based on completely software implementation or be hardware accelerated on some platforms. The mbedTLS library is used in both cases.

For more details on **MCUBootApp**, refer to [MCUBootApp.md](MCUBootApp/MCUBootApp.md).

#### BlinkyApp
* Can be built to use either primary or secondary image for both internal and external flash memory.
* Primary and secondary images differ in text printed to the serial terminal and LED-blinking frequency.
* The watchdog timer functionality is supported to confirm successful start/upgrade of the application.
* The user-application side of MCUboot swap operation is demonstrated by two kinds of user images, compiled for the primary and secondary slot.

For more details on **BlinkyApp**, refer to [BlinkyApp.md](BlinkyApp/BlinkyApp.md).

### Downloading solution's assets

The set of required libraries represented as submodules:

* MCUBooot library (root repository)
* Peripheral Drivers library (PDL)
* mbedTLS Cryptographic library

To retrieve source code with subsequent submodules, pull:

    git clone --recursive https://github.com/mcu-tools/mcuboot.git

Submodules can also be updated and initialized separately:

    cd mcuboot
    git submodule update --init --recursive

### Building solution

The root directory for build is `boot/cypress`.

The root folder contains a make-files infrastructure for building both MCUBootApp bootloading-application and BlinkyApp user-application.

For instructions on how to build and upload MCUBootApp bootloading-application and sample user-application, refer to the [MCUBootApp.md](MCUBootApp/MCUBootApp.md) and [BlinkyApp.md](BlinkyApp/BlinkyApp.md) files in corresponding folders.

**Toolchain**

**GCC_ARM** is only supported (built and verified on GCC 9.3.1).

It is included with [ModusToolbox™ Software Environment](https://www.cypress.com/products/modustoolbox).

The default installation folder is expected by the makefile build system.

To use another installation folder, version of **ModusToolbox™ IDE** or another GCC Compiler, specify the path to a toolchain using the **TOOLCHAIN_PATH** parameter.

Below is an example on how to set toolchin path to the latest include with **ModusToolbox™ IDE 3.0**:

    make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_swap_single.json TOOLCHAIN_PATH=c:/Users/$(USERNAME)/ModusToolbox/tools_3.0/gcc

### Build environment troubleshooting

The following CLI/IDE are supported for project build:

* Cygwin on Windows systems
* unix style shells on *nix systems
* Eclipse / ModusToolbox™ ("makefile project from existing source")

*Make* - Ensure that it is added to the system's `PATH` variable and the correct path is the first on the list.

*Python/Python3* - Ensure that you have the correct path referenced in `PATH`.

*Msys2* - To use the system's path, navigate to the msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart the MSYS2 shell. This will inherit the system's path and find `python` installed in a regular way as well as `imgtool` and its dependencies.
