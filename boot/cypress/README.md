### Port Of Mcuboot Library For Evaluation With Cypress PSoC 6 Chips

### Disclaimer

Given solution is included in `mcuboot` repository with purpose to demonstrate basic consepts and features of MCUBoot library on Cypress PSoC 6 device. Applications are created per mcuboot library maintainers requirements. Implemetation differs from conventional and recomended by Cypress Semiconductors development flow for PSoC 6 devices. These applications are not recomended as a starting point for development and should not be considered as supported examples for PSoC 6 devices.

Examples provided to use with **ModusToolbox® Software Environment** are a recommended reference point to start development of MCUBoot based bootloaders for PSoC 6 devices.

Refer to **Cypress Semiconductors** [github](https://github.com/cypresssemiconductorco) page to find examples.

1. MCUboot-Based Basic Bootloader [mtb-example-psoc6-mcuboot-basic](https://github.com/cypresssemiconductorco/mtb-example-psoc6-mcuboot-basic)
2. MCUboot-Based Bootloader with Rollback to Factory App in External Flash [mtb-example-anycloud-mcuboot-rollback](https://github.com/cypresssemiconductorco/mtb-example-anycloud-mcuboot-rollback)

### Solution Description

There are two applications implemented:
* MCUBootApp - PSoC 6 MCUBoot-based bootloading application;
* BlinkyApp - simple PSoC 6 blinking LED application which is a target of BOOT/UPGRADE;

#### MCUBootApp

* There are two types of upgrade operation supported:
  * **Overwrite only** - secondady image is only copied to primary slot after validation
  * **Swap** - seconrady and primary slots images are swapped in process of upgrade. Upgrade operation can be reverted in case of bad secondary image. 

* There are two types of operation modes supported:
  * single image
  * multi image

* Secondary (upgrade) slot(s) can be placed in external memory. For more details about External Memory usage refer to separate guiding document `MCUBootApp/ExternalMemory.md`.

* MCUBootApp checks image integrity with SHA256, image authenticity with EC256 digital signature verification
* Cryptographic functions can be based on completely software implementation or be hardware accelerated. mbedTLS library is used in both cases.

Detailed description of **MCUBootApp** is provided in `MCUBootApp/MCUBootApp.md`

MCUBootApp checks image integrity with SHA256, image authenticity with EC256 digital signature verification and uses either completely software implementation of cryptographic functions or accelerated by hardware - both based on mbedTLS Library.

#### BlinkyApp
* Can be built to use as primary or secondary image for both internal and external flash memory
* Primary and secondary images differ in text printed to serial terminal and led blinking frequency.
* Watchdog timer functionality is supported to confirm successful start/upgrade of application
* User application side of mcuboot swap operation is demonstrated for secondary image build.

Detailed description of **BlinkyApp** is provided in `BlinkyApp/BlinkyApp.md`

### Downloading Solution's Assets

There is a set assets required:

* MCUBooot Library (root repository)
* PSoC 6 Peripheral Drivers Library (PDL)
* mbedTLS Cryptographic Library

Those are represented as submodules.

To retrieve source code with subsequent submodules pull:

    git clone --recursive https://github.com/mcu-tools/mcuboot.git

Submodules can also be updated and initialized separately:

    cd mcuboot
    git submodule update --init --recursive

### Building Solution

Root directory for build is `boot/cypress`.

Root folder contains make files infrastructure for building both MCUBootApp bootloading application and BlinkyApp user application.

Instructions on how to build and upload MCUBootApp bootloading application and sample user application are located in `MCUBootApp.md` and `BlinkyApp.md` files in corresponding folders.

**Toolchain**

**GCC_ARM** is the only supported (built and verified on GCC 7.2.1).

It is inluded with [ModusToolbox® Software Environment](https://www.cypress.com/products/modustoolbox-software-environment) and can be found in folder `./ModusToolbox/tools_2.1/gcc-7.2.1`.

Default installation folder is expected by makefile build system.

In case of using another installation folder, version of **ModusToolbox IDE** or another GCC Compiler - path to a toolchain should be specified to a build system using **TOOLCHAIN_PATH** flag.

**Example:**

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT TOOLCHAIN_PATH=/home/fw-security/ModusToolbox/tools_2.0/gcc-7.2.1

Supported platforms for `MCUBoot`, `BlinkyApp`:

* PSOC_062_2M
* PSOC_062_1M
* PSOC_062_512K

### Build Environment Troubleshooting

Following CLI / IDE are supported for project build:

* Cygwin on Windows systems
* unix style shells on *nix systems
* Eclipse / ModusToolbox ("makefile project from existing source")

*Make* - make sure it is added to system's `PATH` variable and correct path is first in the list;

*Python/Python3* - make sure you have correct path referenced in `PATH`;

*Msys2* - to use systems PATH navigate to msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart MSYS2 shell.

This will inherit system's PATH so should find `python3.7` installed in regular way as well as imgtool and its dependencies.

