### Port of MCUBoot library to be used with Cypress targets

**Solution Description**

Given solution demonstrates operation of MCUBoot on Cypress' PSoC6 device.

There are two applications implemented:
* MCUBootApp - PSoC6 MCUBoot-based bootloading application;
* BlinkyApp - simple PSoC6 blinking LED application which is a target of BOOT/UPGRADE;

The demonstration device is CY8CPROTO-062-4343W board which is PSoC6 device with 2M of Flash available.
The default flash map implemented is the following:

Single-image mode.

`[0x10000000, 0x10018000]` - MCUBootApp (bootloader) area;

`[0x10018000, 0x10028000]` - primary slot for BlinkyApp;

`[0x10028000, 0x10038000]` - secondary slot for BlinkyApp;

`[0x10038000, 0x10039000]` - scratch area (not used);

Size of slots `0x10000` - 64kb

MCUBootApp checks image integrity with SHA256, image authenticity with EC256 digital signature verification and uses completely SW implementation of cryptographic functions based on mbedTLS Library.

**Important**: make sure primary, secondary slot and bootloader app sizes are appropriate and correspond to flash area size defined in Applications' linker files.

**Important**: make sure RAM areas of CM0p-based MCUBootApp bootloader and CM4-based BlinkyApp do not overlap.
Memory (stack) corruption of CM0p application can cause failure if SystemCall-served operations invoked from CM4.

### Hardware cryptography acceleration

Cypress PSOC6 MCU family supports hardware acceleration of cryptography based on mbedTLS Library via shim layer. Implementation of this layer is supplied as separate submodule `cy-mbedtls-acceleration`. HW acceleration of cryptography shortens boot time more then 4 times, comparing to software implementation (observation results).

To enable hardware acceleration in `MCUBootApp` pass flag `USE_CRYPTO_HW=1` to `make` while build.

Hardware acceleration of cryptography is enabled for PSOC6 devices by default.

### How to modify memory map

__Option 1.__

Navigate to `sysflash.h` and modify the flash area(s) / slots sizes to meet your needs.

__Option 2.__

Navigate to `sysflash.h`, uncomment `CY_FLASH_MAP_EXT_DESC` definition.
Now define and initialize `struct flash_area *boot_area_descs[]` with flash memory addresses and sizes you need at the beginning of application, so flash APIs from `cy_flash_map.c` will use it.

__Note:__ for both options make sure you have updated `MCUBOOT_MAX_IMG_SECTORS` appropriatery with sector size assumed to be 512.

**How to override the flash map values during build process:**

Navigate to MCUBootApp.mk, find section `DEFINES_APP +=`
Update this line and or add similar for flash map parameters to override.

The possible list could be:

* MCUBOOT_MAX_IMG_SECTORS
* CY_FLASH_MAP_EXT_DESC
* CY_BOOT_SCRATCH_SIZE
* CY_BOOT_BOOTLOADER_SIZE
* CY_BOOT_PRIMARY_1_SIZE
* CY_BOOT_SECONDARY_1_SIZE
* CY_BOOT_PRIMARY_2_SIZE
* CY_BOOT_SECONDARY_2_SIZE

As an example in a makefile it should look like following:

`DEFINES_APP +=-DCY_FLASH_MAP_EXT_DESC`

`DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=512`

`DEFINES_APP +=-DCY_BOOT_PRIMARY_1_SIZE=0x15000`

**Multi-Image Operation**

Multi-image operation considers upgrading and verification of more then one image on the device.

To enable multi-image operation define `MCUBOOT_IMAGE_NUMBER` in `MCUBootApp/config/mcuboot_config.h` file should be set to 2 (only dual-image is supported at the moment). This could also be done on build time by passing `MCUBOOT_IMAGE_NUMBER=2` as parameter to `make`.

Default value of `MCUBOOT_IMAGE_NUMBER` is 1, which corresponds to single image configuratios.

In multi-image operation (two images are considered for simplicity) MCUBoot Bootloader application operates as following:

* Verifies Primary_1 and Primary_2 images;
* Verifies Secondary_1 and Secondary_2 images;
* Upgrades Secondary to Primary if valid images found;
* Boots image from Primary_1 slot only;
* Boots Primary_1 only if both - Primary_1 and Primary_2 are present and valid;

This ensures two dependent applications can be accepted by device only in case both images are valid.

**Default Flash map for Multi-Image operation:**

`0x10000000 - 0x10018000` - MCUBoot Bootloader

`0x10018000 - 0x10028000` - Primary_1 (BOOT) slot of Bootloader

`0x10028000 - 0x10038000` - Secondary_1 (UPGRADE) slot of Bootloader

`0x10038000 - 0x10048000` - Primary_2 (BOOT) slot of Bootloader

`0x10048000 - 0x10058000` - Secondary_2 (UPGRADE) slot of Bootloader

`0x10058000 - 0x10059000` - Scratch of Bootloader

Size of slots `0x10000` - 64kb

__Note:__ It is also possible to place secondary (upgrade) slots in external memory module so resulting image size can be doubled.
For more details about External Memory usage, please refer to separate guiding document `ExternalMemory.md`.

### Hardware limitations

Since this application is created to demonstrate MCUBoot library features and not as reference examples some considerations are taken.

1. `SCB5` used to configure serial port for debug prints. This is the most commonly used Serial Communication Block number among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definition of `CYBSP_UART_HW` in `main.c` of MCUBootApp to SCB* that correspond to your design.

2. `CY_SMIF_SLAVE_SELECT_0` is used as definition SMIF driver API. This configuration is used on evaluation kit for this example CY8CPROTO-062-4343W. If you try to use custom hardware with this application - change value of `smif_id` in `main.c` of MCUBootApp to value that corresponds to your design.


### Downloading Solution's Assets

There is a set assets required:

* MCUBooot Library (root repository)
* PSoC6 HAL Library
* PSoC6 Peripheral Drivers Library (PDL)
* mbedTLS Cryptographic Library

To get submodules - run the following command:

    git submodule update --init --recursive

### Building Solution

This folder contains make files infrastructure for building MCUBoot Bootloader. Same approach used in sample BlinkyLedApp application. Example command are provided below for couple different build configurations.

* Build MCUBootApp in `Debug` for signle image use case.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug MCUBOOT_IMAGE_NUMBER=1

* Build MCUBootApp in `Release` for multi image use case.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Release MCUBOOT_IMAGE_NUMBER=2

Root directory for build is **boot/cypress.**

**Programming solution**

There are couple ways of programming hex of MCUBootApp and BlinkyApp. Following instructions assume one of Cypress development kits, for example `CY8CPROTO_062_4343W`.

1. Direct usage of OpenOCD.
OpenOCD package is supplied with ModuToolbox IDE and can be found in installation folder under `./tools_2.1/openocd`.
Open terminal application -  and execute following command after substitution `PATH_TO_APPLICATION.hex` and `OPENOCD` paths.

Connect a board to your computer. Switch Kitprog3 to DAP-BULK mode by pressing `SW3 MODE` button until `LED2 STATUS` constantly shines.

        export OPENOCD=/Applications/ModusToolbox/tools_2.1/openocd 

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit" 

2. Using GUI tool `Cypress Programmer` - follow [link](https://www.cypress.com/products/psoc-programming-solutions) to download.
   Connect board to your computer. Switch Kitprog3 to DAP-BULK mode by pressing `SW3 MODE` button until `LED2 STATUS` constantly shines. Open `Cypress Programmer` and click `Connect`, then choose hex file: `MCUBootApp.hex` or `BlinkyApp.hex` and click `Program`.  Check log to ensure programming success. Reset board.

3. Using `DAPLINK`.
   Connect board to your computer. Switch embeded  Kitprog3 to `DAPLINK` mode by pressing `SW3 MODE` button until `LED2 STATUS` blinks fast and mass storage device appeared in OS. Drag and drop `hex` files you wish to program to `DAPLINK` drive in your OS.



**Currently supported platforms:**

* PSOC_062_2M

**Build environment troubleshooting:**

Regular shell/terminal combination on Linux and MacOS.

On Windows:

* Cygwin
* Msys2

Also IDE may be used:
* Eclipse / ModusToolbox ("makefile project from existing source")

*Make* - make sure it is added to system's `PATH` variable and correct path is first in the list;

*Python/Python3* - make sure you have correct path referenced in `PATH`;

*Msys2* - to use systems PATH navigate to msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart MSYS2 shell.

*Cygwin* - add following to build command `CURDIR=pwd | cygpath --mixed -f -` so that build command looks like that:

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M CURDIR=`pwd | cygpath --mixed -f -`

This will iherit system's PATH so should find `python3.7` installed in regular way as well as imgtool and its dependencies.

