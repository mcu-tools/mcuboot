### Blinking LED test application for MCUBoot Bootloader.

**Description:**

Implements simple Blinky LED CM4 application to demonstrate MCUBoot Application operation in terms of BOOT and UPGRADE process.

It is started by MCUBoot Application which is running on CM0p.

Functionality:

* Blinks RED led with 2 different rates, depending on type of image - BOOT or UPGRADE.
* Prints debug info and version of itself to terminal at 115200 baud.
* Can be built for BOOT slot or UPGRADE slot of bootloader.

**Currently supported platforms:**

* PSOC_062_2M

**Pre-build action:**

Pre-build action is implemented for defining start address and size of flash, as well as RAM start address and size for BlinkyApp.
These values are set by specifing following macros: `-DUSER_APP_SIZE`, `-DUSER_APP_START`, `-DRAM_SIZE`, `-DRAM_START` in makefile.

Pre-build action calls GCC preprocessor which intantiates defines for particular values in `BlinkyApp_template.ld`.

Default values set for currently supported targets:
* PSOC_062_2M in `BlinkyApp.mk` to `-DUSER_APP_START=0x10018000`

**Building an application:**

Root directory for build is **boot/cypress.**

The following command will build regular HEX file of a BlinkyApp for BOOT slot:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT

This have following defaults suggested:

    BUILDCFG=Debug
    IMG_TYPE=BOOT

To build UPGRADE image use following command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x10000

    Note: HEADER_OFFSET=%SLOT_SIZE%

Example command-line for single-image:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT

**Building Multi-Image**

`BlinkyApp` can be built to use in multi-image bootloader configuration.

To get appropriate artifacts to use with multi image MCUBootApp, makefile flag `HEADER_OFFSET=` can be used.

Example usage:

Considering default config:

* first image BOOT (PRIMARY) slot start `0x10018000`
* slot size `0x10000`
* second image BOOT (PRIMARY) slot start `0x10038000`

To get appropriate artifact for second image PRIMARY slot run this command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT HEADER_OFFSET=0x20000

*Note:* only 2 images are supported at the moment.

**Post-Build:**

Post build action is executed at compile time for `BlinkyApp`. In case of build for `PSOC_062_2M` platform it calls `imgtool` from `MCUBoot` scripts and adds signature to compiled image.

Flags passed to `imgtool` for signature are defined in `SIGN_ARGS` variable in BlinkyApp.mk.

**How to program an application:**

Use any preferred tool for programming hex files.

Hex file names to use for programming:

`BlinkyApp` always produce build artifacts in 2 separate folders - `boot` and `upgrade`.

`BlinkyApp` built to run with `MCUBootApp` produces files with name BlinkyApp.hex in `boot` directory and `BlinkyApp_upgrade.hex` in `upgrade` folder. These files are ready to be flashed to the board. 

`BlinkyApp_unsigned.hex` hex file is also preserved in both cases for possible troubleshooting.

Files to use for programming are:

`BOOT` - boot/BlinkyApp.hex
`UPGRADE` - upgrade/BlinkyApp_upgrade.hex

**Flags:**
- `BUILDCFG` - configuration **Release** or **Debug**
- `MAKEINFO` - 0 (default) - less build info, 1 - verbose output of compilation.
- `HEADER_OFFSET` - 0 (default) - no offset of output hex file, 0x%VALUE% - offset for output hex file. Value 0x10000 is slot size MCUBoot Bootloader in this example.
- `IMG_TYPE` - `BOOT` (default) - build image for BOOT slot of MCUBoot Bootloader, `UPGRADE` - build image for UPGRADE slot of MCUBoot Bootloader.

**NOTE**: In case of `UPGRADE` image `HEADER_OFFSET` should be set to MCUBoot Bootloader slot size.

**Example terminal output:**

When user application programmed in BOOT slot:

    ===========================
    [BlinkyApp] BlinkyApp v1.0 [CM4]
    ===========================
    [BlinkyApp] GPIO initialized
    [BlinkyApp] UART initialized
    [BlinkyApp] Retarget I/O set to 115200 baudrate
    [BlinkyApp] Red led blinks with 1 sec period

When user application programmed in UPRADE slot and upgrade procedure was successful:

    ===========================
    [BlinkyApp] BlinkyApp v2.0 [+]
    ===========================

    [BlinkyApp] GPIO initialized
    [BlinkyApp] UART initialized
    [BlinkyApp] Retarget I/O set to 115200 baudrate
    [BlinkyApp] Red led blinks with 0.25 sec period
