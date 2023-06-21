### Blinking LED test application for MCUboot bootloader

### Description

Implements simple Blinky LED CM4 application to demonstrate MCUboot Application operation in terms of BOOT and UPGRADE process.

It is started by MCUboot Application which is running on CM0p.

Functionality:

* Blinks RED led with 2 different rates, depending on type of image - BOOT or UPGRADE.
* Prints debug info and version of itself to terminal at 115200 baud.
* Can be built for BOOT slot or UPGRADE slot of bootloader.

Currently supported platforms

* PSOC_062_2M
* PSOC_062_1M
* PSOC_062_512K

### Hardware limitations

Since this application is created to demonstrate MCUboot library features and not as reference examples some considerations are taken.

1. Port/pin `P5_0` and `P5_1` used to configure serial port for debug prints. These pins are the most commonly used for serial port connection among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definitions of `CY_DEBUG_UART_TX` and `CY_DEBUG_UART_RX` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.
2. Port `GPIO_PRT13` pin `7U` used to define user connection LED. This pin is the most commonly used for USER_LED connection among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definitions of `LED_PORT` and `LED_PIN` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.

### Pre-build action

Pre-build action is implemented for defining start address and size of flash, as well as RAM start address and size for BlinkyApp.
These values are set by specifing following macros: `-DUSER_APP_SIZE`, `-DUSER_APP_START`, `-DRAM_SIZE`, `-DRAM_START` in makefile.

Pre-build action calls GCC preprocessor which intantiates defines for particular values in `BlinkyApp_template.ld`.

Default values set for currently supported targets:
* `BlinkyApp.mk` to `-DUSER_APP_START=0x10018000`

**Important**: make sure RAM areas of CM4-based BlinkyApp and CM0p-based MCUBootApp bootloader do not overlap.
Memory (stack) corruption of CM0p application can cause failure if SystemCall-served operations invoked from CM4.

### Building an application

Root directory for build is **boot/cypress.**

The following command will build regular HEX file of a BlinkyApp for BOOT slot. Substitute `PLATFORM=` to a paltform name you use in all following commands.

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

**How to build upgrade image for external memory:**

To prepare MCUBootApp for work with external memory please refer to `MCUBootApp/ExternalMemory.md`.

For build BlinkyApp upgrade image for external memory use command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x7FE8000 ERASED_VALUE=0xff

`HEADER_OFFSET` defines the offset from original boot image address. This one in line above suggests secondary slot will start from `0x18000000`.

`ERASED_VALUE` defines the memory cell contents in erased state. It is `0x00` for PSoC6's internal Flash and `0xff` for S25FL512S.

In case of using muti-image configuration, upgrade image for second application can be built using next command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x8028000 ERASED_VALUE=0xff

    Note: for S25FL512S block address shuld be mutiple by 0x40000

**How to build encrypted upgrade image :**

To prepare MCUBootApp for work with encrypted upgrade image please refer to `MCUBootApp/Readme.md`.

To obtain encrypted upgrade image of BlinkyApp extra flag `ENC_IMG=1` should be passed in command line, for example:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x20000 ENC_IMG=1

This also suggests user already placed corresponing `*.pem` key in `\keys` folder. The key variables are defined in root `Makefile` as `SIGN_KEY_FILE` and `ENC_KEY_FILE`

### Post-build

Post build action is executed at compile time for `BlinkyApp`. In case of build for `PSOC_062_2M` platform it calls `imgtool` from `MCUboot` scripts and adds signature to compiled image.

Flags passed to `imgtool` for signature are defined in `SIGN_ARGS` variable in BlinkyApp.mk.

### How to program an application

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
- `HEADER_OFFSET` - 0 (default) - no offset of output hex file, 0x%VALUE% - offset for output hex file. Value 0x10000 is slot size MCUboot Bootloader in this example.
- `IMG_TYPE` - `BOOT` (default) - build image for BOOT slot of MCUboot Bootloader, `UPGRADE` - build image for UPGRADE slot of MCUboot Bootloader.
- `ENC_IMG` - 0 (default) - build regular upgrade image, `1` - build encrypted upgrade image (MCUBootApp should also be built with this flash set 1)

**NOTE**: In case of `UPGRADE` image `HEADER_OFFSET` should be set to MCUboot Bootloader slot size.

### Example terminal output

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
