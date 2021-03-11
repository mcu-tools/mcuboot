### Blinking LED Test Application For Mcubootapp Bootloading Application

### Description

Implements simple Blinky LED CM4 application to demonstrate MCUBootApp bootloading application operation in terms of boot and upgrade processes.

It is validated and started by MCUBootApp which is running on CM0p core of PSoC 6 device.

Functionality:

* Blinks RED led with 2 different rates, depending on type of image - BOOT or UPGRADE.
* Prints debug info and version of itself to terminal at 115200 baud rate.
* Manages watchdog timer started in MCUBootApp as one of confirmation mechanisms.
* Sets special bit in flash to confirm image is operable (UPGRADE image).
* Can be built for BOOT slot or UPGRADE slot of bootloader.
* Can be used to evaluate `swap` and `overwrite only` upgrade modes.

### Hardware Limitations

Since this application is created to demonstrate MCUBoot library features and not as reference examples some considerations are taken.

1. Port/pin `P5_0` and `P5_1` used to configure serial port for debug prints. These pins are the most commonly used for serial port connection among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definitions of `CY_DEBUG_UART_TX` and `CY_DEBUG_UART_RX` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.
2. Port `GPIO_PRT13` pin `7U` used to define user connection LED. This pin is the most commonly used for USER_LED connection among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definitions of `LED_PORT` and `LED_PIN` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.

### Pre-build Action

Pre-build action is implemented to define start address and size of flash, as well as RAM start address and size for BlinkyApp.

These values are set by specifing following macros (default values shown):
`SLOT_SIZE ?= 0x10000` - for slot located in internal flash 
`SLOT_SIZE ?= 0x40200` - for slot located in external flash 

For PSoC 6 2M devices:
`DEFINES_APP += -DRAM_START=0x08040000`
`DEFINES_APP += -DRAM_SIZE=0x10000`

For PSoC 6 1M and 512K devices:
`DEFINES_APP += -DRAM_START=0x08020000`
`DEFINES_APP += -DRAM_SIZE=0x10000`

For all devices:
`DEFINES_APP += -DUSER_APP_START=0x10018000`

in `boot/cypress/BlinkyApp.mk`.

Pre-build action calls GCC preprocessor which replaces defines to particular values in `BlinkyApp_template.ld`.

**Important**: make sure RAM areas of CM4-based BlinkyApp and CM0p-based MCUBootApp bootloader do not overlap.

Memory (stack) corruption of CM0p application can cause failure if SystemCall-served operations invoked from CM4.

### Building An Application

Supported platforms:

* PSOC_062_2M
* PSOC_062_1M
* PSOC_062_512K

Root directory for build is **boot/cypress.**

**Single image**

The following command will build regular HEX file of a BlinkyApp for primary (BOOT) slot:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT

_Note: HEADER_OFFSET=%SLOT_SIZE%_

To build image for secondary (UPGRADE) slot to use in `swap` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x10000 SWAP_UPGRADE=1

To build image for secondary (UPGRADE) slot to use in `overwrite only` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x10000 SWAP_UPGRADE=0

To build image for primary (BOOT) image for custom slot size `0x70000`:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT SLOT_SIZE=0x70000

To build image for secondary (UPGRADE) image for custom slot size `0x70000` to use in `swap` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE SLOT_SIZE=0x70000 HEADER_OFFSET=0x70000 SWAP_UPGRADE=1

**Multi image**

`BlinkyApp` can be built to use in multi image bootloader configuration.

To obtain appropriate hex files to use with multi image MCUBootApp, makefile flag `HEADER_OFFSET=` can be used.

Example usage:

Considering default config:

* first image BOOT (PRIMARY) slot starts `0x10018000`
* slot size `0x10000`
* second image BOOT (PRIMARY) slot starts at `0x10038000`

To obtain appropriate artifact for second image PRIMARY slot run this command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT HEADER_OFFSET=0x20000

*Note:* only 2 images are supported at the moment.

**Upgrade mode dependency**

`MCUBootApp` can upgrade image either by overwriting an image from a secondary slot to a primary slot or by swapping the two images.

To build `BlinkyApp` for different upgrade mode `SWAP_UPGRADE` flag is used.

`SWAP_UPGRADE=0` - for overwrite mode.
`SWAP_UPGRADE=1` - for swap upgrade mode (default).

**Upgrade image for external memory**

To prepare MCUBootApp for work with external memory refer to `MCUBootApp/ExternalMemory.md`.

To build `BlinkyApp` upgrade image for external memory use command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x7FE8000 ERASED_VALUE=0xff USE_EXTERNAL_FLASH=1

`HEADER_OFFSET` defines the offset from original boot image address. This one in line above suggests secondary slot will start from `0x18000000`, which is a start of external memory related addreses on PSoC 6 devices.

`ERASED_VALUE` defines the memory cell contents in erased state. It is `0x00` for PSoC 6 internal Flash and `0xff` for S25FL512S.

In case of using muti-image configuration, upgrade image for second application can be built using next command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x8228000 ERASED_VALUE=0xff USE_EXTERNAL_FLASH=1

Note: for S25FL512S block address should be mutiple of 0x40000.

**Encrypted upgrade image**

To prepare MCUBootApp for work with encrypted upgrade image please refer to `MCUBootApp/Readme.md`.

To obtain encrypted upgrade image of BlinkyApp extra flag ENC_IMG=1 should be passed in command line, for example:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x20000 ENC_IMG=1

This also suggests user already placed corresponing *.pem key in \keys folder. The key variables are defined in root Makefile as SIGN_KEY_FILE and ENC_KEY_FILE

### Complete Build Flags Description
- `BUILDCFG` - configuration type
    - Release 
    - Debug
- `MAKEINFO` - build verbosity level
    - 0 (default) - less build info
    - 1 - verbose output of compilation
- `PLATFORM`
    - `PSOC_062_2M` - only supported now
- `SLOT_SIZE` - size of primary/secondary slot of MCUBootApp this app will be used with
    - 0x%VALUE%
- `HEADER_OFFSET` - shift start address of image by value
    - 0 (default) - no offset of output hex file
    - 0x%VALUE% - offset for output hex file
- `IMG_TYPE` - for which slot of MCUBootApp image is build
    - `BOOT` (default) - build image for primary (BOOT) slot
    - `UPGRADE` - build image for secondary (UPGRADE) slot
- `SWAP_UPGRADE` - define upgrade mode type on `MCUBootApp` this app will be used with
    - `0` - for overwrite mode.
    - `1` - (default) for swap upgrade mode
- `ERASED_VALUE` - define memory cell contents in erased state
    - `0x0` - internal memory
    - `0xff` - external memory
- `TOOLCHAIN_PATH` - path to gcc compiler to use for build
    - Example: TOOLCHAIN_PATH=/home/fw-security/ModusToolbox/tools_2.0/gcc-7.2.1
    - Example: TOOLCHAIN_PATH=C:\gcc

### Post-Build

Post build action is executed at compile time for `BlinkyApp`. In case of build for `PSOC_062_2M`, `PSOC_062_1M`, `PSOC_062_512K` platforms it calls `imgtool` from `MCUBoot` scripts and adds signature to compiled image.

Flags passed to `imgtool` for signature are defined in `SIGN_ARGS` variable in BlinkyApp.mk.

### How To Program An Application

There are couple ways of programming BlinkyApp firmware. Following instructions assume usage of one of Cypress development kits, for example `CY8CPROTO_062_4343W`.

1. Direct usage of OpenOCD.

OpenOCD package is supplied with ModusToolbox IDE and can be found in installation folder under `./tools_2.1/openocd`.

Open terminal application -  and execute following command after substitution `PATH_TO_APPLICATION.hex` and `OPENOCD` paths.

Connect a board to your computer. Switch Kitprog3 to DAP-BULK mode by pressing `SW3 MODE` button until `LED2 STATUS` constantly shines.

        export OPENOCD=/Applications/ModusToolbox/tools_2.1/openocd 

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit" 

2. Using GUI tool `Cypress Programmer`

Follow [link](https://www.cypress.com/products/psoc-programming-solutions) to download.

Connect board to your computer. Switch Kitprog3 to DAP-BULK mode by pressing `SW3 MODE` button until `LED2 STATUS` constantly shines. Open `Cypress Programmer` and click `Connect`, then choose hex file: `MCUBootApp.hex` or `BlinkyApp.hex` and click `Program`.  Check log to ensure programming success. Reset board.

3. Using `DAPLINK`.

Connect board to your computer. Switch embeded  Kitprog3 to `DAPLINK` mode by pressing `SW3 MODE` button until `LED2 STATUS` blinks fast and mass storage device appeared in OS. Drag and drop `hex` files you wish to program to `DAPLINK` drive in your OS.

**Hex file names to use for programming**

`BlinkyApp` always produce build artifacts in 2 separate folders: `BlinkyApp/out/PSOC_062_2M/Debug/boot` and `BlinkyApp/out/PSOC_062_2M/Debug/upgrade`.

These files are ready to be flashed to the board:

* **BlinkyApp.hex** from `boot` folder
* **BlinkyApp_upgrade.hex** from `upgrade` folder 

`BlinkyApp_unsigned.hex` hex file is also preserved in both cases for possible troubleshooting.

**Important: When swap status upgrade mode used**

 In case of using this application in a system with `swap` type of upgrade refer first to `MCUBootApp.md` section **SWAP/Expected lifecycle**.

**BlinkyApp.hex** should be programmed to a device once. All firmware upgrades should be delivered using secondary (UPGRADE) slot thus **BlinkyApp_upgrade.hex** image.

If user for some reason tries to program **BlinkyApp.hex** to primary slot directly second time - **system state should be reset**.

To reset system state at least `swap status partition` area in flash should be erased - see addresses in `MCUBootApp.md` paragraph **Memory maps**.

To erase swap status partition area in MCUBootApp with a single image configuration with default memory map using `OpenOCD` execute command:

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10038000 0x1000" -c "reset; shutdown"

To erase swap status partition area in MCUBootApp with a multi image configuration with default memory map using `OpenOCD` execute command:

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10078000 0x2000" -c "reset; shutdown"

In both cases it is easier to erase all device flash or all flash after MCUBootApp. This command erases all flash after MCUBootApp including primary, secondary and swap status partiton.

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10018000" -c "reset; shutdown"

### Example Terminal Output

When user application programmed in BOOT slot:

    ===========================
    [BlinkyApp] BlinkyApp v1.0 [CM4]
    ===========================
    [BlinkyApp] GPIO initialized 
    [BlinkyApp] UART initialized 
    [BlinkyApp] Retarget I/O set to 115200 baudrate 
    [BlinkyApp] Red led blinks with 1 sec period
    [BlinkyApp] Update watchdog timer started in MCUBootApp to mark sucessful start of user app
    [BlinkyApp] Turn off watchdog timer

When user application programmed in UPRADE slot and upgrade procedure was successful:

    ===========================
    [BlinkyApp] BlinkyApp v2.0 [+]
    ===========================
    [BlinkyApp] GPIO initialized 
    [BlinkyApp] UART initialized 
    [BlinkyApp] Retarget I/O set to 115200 baudrate 
    [BlinkyApp] Red led blinks with 0.25 sec period
    [BlinkyApp] Update watchdog timer started in MCUBootApp to mark sucessful start of user app
    [BlinkyApp] Turn off watchdog timer
    [BlinkyApp] Try to set img_ok to confirm upgrade image
    [BlinkyApp] SWAP Status : Image OK was set at 0x10027fe8.
