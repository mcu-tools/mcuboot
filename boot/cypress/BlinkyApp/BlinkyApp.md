## Blinking LED test application for MCUBootApp bootloading application

### Description

Implements a simple Blinky LED application to demonstrate the MCUBootApp bootloading application operation for the boot and upgrade processes.

It is validated and started by MCUBootApp, which is running on the CM0p core of PSoC™ 6 devices.

Functionality:

* Blinks red LED with 2 different rates, depending on the image type - BOOT or UPGRADE.
* Prints debug info and the appplication version to the terminal at baud rate 115200.
* Manages the watchdog-timer start in MCUBootApp as one of the confirmation mechanisms.
* Sets a special bit in flash to confirm that the image is operable (UPGRADE image).
* Can be built for boot slot or UPGRADE slot of the bootloader.
* Can be used to evaluate `swap` and `overwrite only` upgrade modes.

### Hardware limitations

This application is created to demonstrate the MCUboot library features.

1. Port/pin `P5_0` and `P5_1` are used to configure a serial port for debug prints. These pins are the most commonly used for serial port connection among available Cypress PSoC™ 6 kits. To use custom hardware with this application, change the definitions of `CY_DEBUG_UART_TX` and `CY_DEBUG_UART_RX` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.
2. Port `GPIO_PRT13` pin `7U` is used to define the user connection-LED. This pin is the most commonly used for USER_LED connection among available Cypress PSoC™ 6 kits. To use custom hardware with this application, change the definitions of `LED_PORT` and `LED_PIN` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.

### Pre-build action

Pre-build action is implemented to define the start address and flash size, as well as the RAM start address and BlinkyApp size.

These values are set by specifying the following macros (default values shown):
`SLOT_SIZE ?= 0x10000` - for slot located in internal flash of PSoC™ 6 chips
`SLOT_SIZE ?= 0x40200` - for slot located in external flash of PsoC™ 6 kits

For PSoC™ 6 2M devices:
`DEFINES_APP += -DRAM_START=0x08040000`
`DEFINES_APP += -DRAM_SIZE=0x10000`

For PSoC™ 6 1M and 512K devices:
`DEFINES_APP += -DRAM_START=0x08020000`
`DEFINES_APP += -DRAM_SIZE=0x10000`

For PSoC™ 6 devices:
`DEFINES_APP += -DUSER_APP_START=0x10018000`

in `boot/cypress/BlinkyApp.mk`.

The pre-build action calls the GCC preprocessor, which replaces the defines to particular values in `BlinkyApp_template.ld`.

**Important (PSoC™ 6)**: ensure that the RAM areas of CM4-based BlinkyApp and CM0p-based MCUBootApp bootloader do not overlap.

Memory (stack) corruption of the CM0p application can cause a failure if SystemCall-served operations were invoked from CM4.

### Building an application

The supported platforms:

* PSOC_062_2M
* PSOC_062_1M
* PSOC_062_512K

The root directory for build is **boot/cypress.**

**Single-image**

The following command will build a regular HEX file of a BlinkyApp for the primary (BOOT) slot:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT

_Note: HEADER_OFFSET=%SLOT_SIZE%_

To build an image for the secondary (UPGRADE) slot to use in `swap` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x10000 USE_OVEWRITE=0

To build an image for the secondary (UPGRADE) slot to use in `overwrite only` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x10000 USE_OVEWRITE=1

To build an image for the primary (BOOT) image for custom slot size `0x70000`:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT SLOT_SIZE=0x70000

To build an image for the secondary (UPGRADE) image for custom slot size `0x70000` to use in `swap` upgrade:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE SLOT_SIZE=0x70000 HEADER_OFFSET=0x70000 USE_OVEWRITE=0

**Multi-image**

`BlinkyApp` can be built in multi-image bootloader configuration for PSoC™ 6 chips only.

To obtain appropriate hex files to use with multi-image MCUBootApp, makefile flag `HEADER_OFFSET=` can be used.

Refer to the default memory map for each platform to find slots' addresses.

To obtain an appropriate artifact for the second image PRIMARY slot, run this type of a command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT HEADER_OFFSET=0x20000

*Note:* only 2 images are supported at the moment.

**Upgrade mode dependency**

`MCUBootApp` can upgrade an image either by overwriting the image from a secondary slot to a primary slot or by swapping the two images.

To build `BlinkyApp` for different upgrade mode, use the `USE_OVERWRITE` flag.

`USE_OVERWRITE=1` - for Overwrite mode.
`USE_OVERWRITE=0` - for Swap upgrade mode (default).

**Upgrade image for external memory (PSoC™ 6)**

To prepare MCUBootApp for work with external memory, refer to [ExternalMemory.md](../MCUBootApp/ExternalMemory.md).

To build a `BlinkyApp` upgrade image for external memory, use command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x7FE8000 ERASED_VALUE=0xff USE_EXTERNAL_FLASH=1

`HEADER_OFFSET` defines the offset from the original boot image address. The address in the above line suggests the secondary slot will start from `0x18000000`, which is a start of external-memory-related addreses on PSoC™ 6 devices.

`ERASED_VALUE` defines the memory cell contents in the erased state. It is `0x00` for PSoC™ 6 internal flash and `0xff` for S25FL512S.

In multi-image configuration, an upgrade image for the second application is built using command:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x8228000 ERASED_VALUE=0xff USE_EXTERNAL_FLASH=1

Note: for the S25FL512S block, the address will be multiple of 0x40000.

**Encrypted upgrade image**

To prepare MCUBootApp for work with an encrypted upgrade image, refer to [MCUBootApp.md](../MCUBootApp/MCUBootApp.md).

To obtain an encrypted upgrade image of BlinkyApp, pass extra flag ENC_IMG=1 in the command line, for example:

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x20000 ENC_IMG=1

This also suggests that the user has already placed a corresponding *.pem key in the \keys folder. The key variables are defined in root Makefile as SIGN_KEY_FILE and ENC_KEY_FILE

### Complete build flags description
- `BUILDCFG` - The configuration type
    - Release 
    - Debug
- `MAKEINFO` - The build verbosity level
    - 0 (default) - less build info
    - 1 - verbose output of compilation
- `PLATFORM`
    - `PSOC_062_2M`
    - `PSOC_062_1M`
    - `PSOC_062_512K`
- `SLOT_SIZE` - The size of the primary/secondary slot of MCUBootApp. This app will be used with
    - 0x%VALUE%
- `HEADER_OFFSET` - The displacement value of the image start address.
    - 0 (default) - No offset of the output hex file.
    - 0x%VALUE% - The offset for the output hex file.
- `IMG_TYPE` - The slot of MCUBootApp, for which the image is being built.
    - `BOOT` (default) - A build image for the primary (BOOT) slot.
    - `UPGRADE` - A build image for the secondary (UPGRADE) slot.
- `USE_OVERWRITE` - Define the Upgrade mode type of `MCUBootApp` to use with this app.
    - `1` - For Overwrite mode.
    - `0` - (default) For Swap Upgrade mode.
- `ERASED_VALUE` - Define memory cell contents in the erased state.
    - `0x0` - Internal memory.
    - `0xff` - External memory.
- `TOOLCHAIN_PATH` - The path to the GCC compiler to use for build.
    - Example: TOOLCHAIN_PATH=/home/user/ModusToolbox/tools_2.4/gcc
    - Example: TOOLCHAIN_PATH=C:/ModusToolbox/tools_2.4/gcc

### Post-build

The post-build action is executed at the compile time for `BlinkyApp`. For the `PSOC_062_2M`, `PSOC_062_1M`, `PSOC_062_512K` platforms, it calls `imgtool` from `MCUboot` scripts and adds a signature to the compiled image.

Flags passed to `imgtool` for a signature are defined in the `SIGN_ARGS` variable in BlinkyApp.mk.

### How to program an application

BlinkyApp firmware can be programmed in different ways. The following instructions assume the usage of one of Cypress development kits, for example `CY8CPROTO_062_4343W`.

1. Direct usage of OpenOCD.

Connect a board to your computer. Switch Kitprog3 to DAP-BULK mode by clicking the `SW3 MODE` button until `LED2 STATUS` constantly shines.

The OpenOCD package is supplied with ModusToolbox™ IDE and can be found in the ModusToolbox™ installation folder `ModusToolbox/tools_2.4/openocd`.

Open the terminal application and execute the following command after substitution of the `PATH_TO_APPLICATION.hex` and `OPENOCD` paths:

        export OPENOCD=/Applications/ModusToolbox/tools_2.4/openocd 

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit" 

2. Using GUI tool `Cypress Programmer`

Follow [link](https://www.cypress.com/products/psoc-programming-solutions) to download.

Connect the board to your computer. Switch Kitprog3 to DAP-BULK mode by clicking the `SW3 MODE` button until `LED2 STATUS` constantly shines. Open `Cypress Programmer` and click `Connect`, then choose the hex file `MCUBootApp.hex` or `BlinkyApp.hex`, and finally click `Program`.  Check the log to ensure that the programming is successful. Reset the board.

3. Using `DAPLINK`.

Connect the board to your computer. Switch embedded  Kitprog3 to `DAPLINK` mode by clicking the `SW3 MODE` button until `LED2 STATUS` blinks fast and the USB mass storage device displays in the OS. Drag and drop the `hex` files to be programmed to the `DAPLINK` drive in your OS.

**Hex file names to use for programming**

`BlinkyApp` always produces build artifacts in two separate folders: `BlinkyApp/out/PSOC_062_2M/Debug/boot` and `BlinkyApp/out/PSOC_062_2M/Debug/upgrade`.

These files are ready to be flashed to the board:

* **BlinkyApp.hex** from the `boot` folder
* **BlinkyApp_upgrade.hex** from the `upgrade` folder 

The `BlinkyApp_unsigned.hex` hex file is also preserved in both cases for possible troubleshooting.

**Important: When Swap status Upgrade mode is used**

 While using this application in a system with the `swap` type of upgrade, refer first to the [MCUBootApp.md](../MCUBootApp/MCUBootApp.md) section **SWAP/Expected lifecycle**.

**BlinkyApp.hex** can be programmed to a device once. All firmware upgrades are delivered using the secondary (UPGRADE) slot, thus a **BlinkyApp_upgrade.hex** image.

If the user tries to program **BlinkyApp.hex** to the primary slot directly for the second time - **system state should be reset**.

To reset the system state, erase at least the `swap status partition` area in flash - see the addresses in the [MCUBootApp.md](../MCUBootApp/MCUBootApp.md) paragraph **Memory maps**.

To erase the swap status partition area in MCUBootApp with single-image configuration with the default memory map, use the `OpenOCD` command:

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10038000 0x1000" -c "reset; shutdown"

To erase the swap status partition area in MCUBootApp with multi-image configuration with the default memory map, use the `OpenOCD` command:

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10078000 0x2000" -c "reset; shutdown"

In both cases, it is easier to erase the whole device flash or all flash after MCUBootApp. This command erases all flash after MCUBootApp, including the primary, secondary, and swap status partiton:

    $OPENOCD_PATH/bin/openocd -s "$OPENOCD_PATH/scripts" -f "$OPENOCD_PATH/ scripts/interface/kitprog3.cfg" -f "$OPENOCD_PATH/scripts/target/psoc6_2m.cfg" -c "init; reset init" -c "flash erase_address 0x10018000 0x1E8000" -c "reset; shutdown"

### Example terminal output

When the user application is programmed in the boot slot:

    ===========================
    [BlinkyApp] BlinkyApp v1.0 [CM4]
    ===========================
    [BlinkyApp] GPIO initialized 
    [BlinkyApp] UART initialized 
    [BlinkyApp] Retarget I/O set to 115200 baudrate 
    [BlinkyApp] Red led blinks with 1 sec period
    [BlinkyApp] Update watchdog timer started in MCUBootApp to mark sucessful start of user app
    [BlinkyApp] Turn off watchdog timer

When the user application is programmed in the upgrade slot and the upgrade procedure was successful:

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
