## Blinking LED test application for MCUBootApp bootloader application

### Description

Implements a simple Blinky LED application to demonstrate the MCUBootApp bootloader application operation for the boot and upgrade processes.

It is validated and started by MCUBootApp, which is running on the CM0p core of PSoC™ 6 devices, or CM33 core for the CYW20829 device.

Functionality:

* Blinks red LED with 2 different rates, depending on the image type - BOOT or UPGRADE.
* Prints debug info and the application version to the terminal at baud rate 115200.
* Manages the watchdog-timer start in MCUBootApp as one of the confirmation mechanisms.
* Sets a special bit in flash to confirm that the image is operable (UPGRADE image).
* Can be built for BOOT slot or UPGRADE slot of the bootloader.
* Can be used to evaluate `swap` and `overwrite only` upgrade modes.

### Hardware limitations

This application is created to demonstrate the MCUboot library features.

1. Port/pin `P5_0` and `P5_1` are used to configure a serial port for debug prints. These pins are the most commonly used for serial port connection among available Cypress PSoC™ 6 kits. To use custom hardware with this application, change the definitions of `CY_DEBUG_UART_TX` and `CY_DEBUG_UART_RX` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.
2. Port `GPIO_PRT13` pin `7U` is used to define the user connection-LED. This pin is the most commonly used for USER_LED connection among available Cypress PSoC™ 6 kits. To use custom hardware with this application, change the definitions of `LED_PORT` and `LED_PIN` in `main.c` of BlinkyApp to port/pin pairs corresponding to your design.

### Pre-build action

Pre-build action is implemented to define the start address and flash size, as well as the RAM start address and BlinkyApp size.

`FLASH_MAP` `make` parameter is used to provide an input file for pre-build action. Refer to `MCUBootApp.md` for details.

The result of the pre-build script is an auto-generated `flashmap.mk` file with a set of makefile flags:

`PRIMARY_IMG_START` - start address of the primary image in flash, this value is defined in the JSON flash map as the `"value"` field of the address section for `"application_#"`.

`SECONDARY_IMG_START`- start address of the secondary image in flash, this value is defined in the JSON flash map as the `"upgrade_address"` field of the `"address"` section for `"application_#"`.

`SLOT_SIZE` - slot size for the primary and the secondary images, this value is taken from `"value"` field of `"size"` section of `"application_#"` from JSON file.

`BOOTLOADER_SIZE` - size of the Bootloader application, this value is defined in the JSON flash map as the `"size"` field of the address section for `"bootloader"`.

`USE_EXTERNAL_FLASH` - is set to 1 if a flash map with the `_smif` suffix is chosen.

`USE_XIP` - is set to 1 if the "external_flash" section with "mode": "XIP" is present in the flash map file.

These values are set by specifying the following macros (default values shown):
`SLOT_SIZE ?= 0x10000` - for slot located in internal flash of PSoC™ 6 chips
`SLOT_SIZE ?= 0x40200` - for slot located in external flash of PsoC™ 6 kits
`SLOT_SIZE ?= 0x20000` - for slot located in external flash of CYW20829 kits

During pre-build action, the GCC preprocessor is used to generate the target linker script from a template `BlinkyApp_template.ld`.

**Important (PSoC™ 6)**: ensure that the RAM areas of CM4-based BlinkyApp and CM0p-based MCUBootApp bootloader do not overlap.

Memory (stack) corruption of the CM0p application can cause a failure if SystemCall-served operations were invoked from CM4.

### Building an application

Toolchain is set by default in `toolchains.mk` file, depending on `COMPILER` makefile variable. MCUBoot is currently support only `GCC_ARM` as compiler. Toolchain path can be redefined, by setting `TOOLCHAIN_PATH` build flag to desired toolchain path. Below is an example on how to set toolchain path from **ModusToolbox™ IDE 3.0**:

    make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_swap_single.json TOOLCHAIN_PATH=c:/Users/$(USERNAME)/ModusToolbox/tools_3.0/gcc

The supported platforms:

* PSOC_062_2M
* PSOC_062_1M
* PSOC_062_512K
* PSOC_063_1M
* CYW20829

The root directory is boot/cypress.
Since BlinkyApp built for BOOT or UPGRADE slot has its own folder BlinkyApp/out/boot or BlinkyApp/out/upgrade consider using following jobs to clear build folder before build.

The root directory for is **boot/cypress.**   
Since BlinkyApp built for BOOT or UPGRADE slot has its own folder `BlinkyApp/out/boot` or `BlinkyApp/out/upgrade` consider using following jobs to clear build folder before build:   
 - **clean_boot** - to clean the BOOT image directory
 - **clean_upgrade** - to clean the UPGRADE image directory.   
 
These jobs also remove auto-generated files 'flashmap.mk' and 'cy_flash_map.h', which is required to eliminate possible errors.   

**Upgrade mode dependency**

`MCUBootApp` can upgrade an image either by overwriting the image from a secondary slot to a primary slot or by swapping the two images.  
To build `BlinkyApp` for different upgrade modes choose flash map JSON file with the corresponding suffix - either `_swap_` or `_overwrite_`.  
But hold in the mind, that `MCUBootApp` and `BlinkyApp` should use the same flash map file!  
For example: to building `MCUBootApp` and `BlinkyApp` in the 'single overwride' mode use the flash map file:   
`FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_single.json`  

**Single-image**

The following command will build BlinkyApp as a regular HEX file for the primary (BOOT) slot to be used in a single image case with `swap` upgrade type of Bootloader:

    make clean_boot app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=BOOT FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_swap_single.json IMG_ID=1

To build an image for the secondary (UPGRADE) slot to be used in a single image case with `swap` upgrade type of Bootloader:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_swap_single.json IMG_ID=1

To build an image for the secondary (UPGRADE) slot to be used in a single image case with `overwrite` upgrade type of Bootloader:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_single.json IMG_ID=1

**Multi-image**

**NOTE!** `MCUBootApp` should be build with multi-image support as well. Refer to the appropriated section of [MCUBootApp.md](../MCUBootApp/MCUBootApp.md).   

`BlinkyApp` can be built in multi-image bootloader configuration for PSoC™ 6 chips only.

To obtain the appropriate hex files to use with multi-image MCUBootApp, the makefile flag `IMG_ID` is used.

`IMG_ID` flag value should correspond to the `application_#` number of JSON flash map file used for the build. For example, to build `BlinkyApp` for the UPGRADE slot of the second image following command is used:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_single.json IMG_ID=2

When this option is omitted, `IMG_ID=1` is assumed.    

**Upgrade image for external memory (PSoC™ 6)**
__NOTE__: Not supported with `PSoC™ 063` kits.

To prepare MCUBootApp for work with external memory, refer to [ExternalMemory.md](../MCUBootApp/ExternalMemory.md).

To build a `BlinkyApp` upgrade image for external memory to be used in a single image configuration with overwrite upgrade mode, use the command:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_single_smif.json IMG_ID=1

`ERASED_VALUE` defines the memory cell contents in the erased state. It is `0x00` for PSoC™ 6 internal flash and `0xff` for S25FL512S. For `CYW20289` default value is `0xff` since it only uses an external flash.

In the multi-image configuration, an upgrade image for the second application is built using the command:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_multi_smif.json IMG_ID=2

**Encrypted upgrade image**

To prepare MCUBootApp for work with an encrypted upgrade image, refer to [MCUBootApp.md](../MCUBootApp/MCUBootApp.md).

To obtain an encrypted upgrade image of BlinkyApp, pass extra flag `ENC_IMG=1` in the command line, for example:

    make clean_upgrade app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE FLASH_MAP=platforms/cy_flash_pal/flash_psoc6/flashmap/psoc6_overwrite_single.json IMG_ID=1 ENC_IMG=1

This also suggests that the user has already placed a corresponding *.pem key in the \keys folder. The key variables are defined in the root Makefile as SIGN_KEY_FILE and ENC_KEY_FILE

Refer to [CYW20829.md](../platforms/CYW20829.md) for details of encrypted image build for the CYW20289 platfrom.

### Complete build flags description
- `BUILDCFG` - The configuration type
    - Release 
    - Debug
- `VERBOSE` - The build verbosity level
    - 0 (default) - less build info
    - 1 - verbose output of compilation
- `PLATFORM`
    - `PSOC_062_2M`
    - `PSOC_062_1M`
    - `PSOC_062_512K`
    - `CYW20289`
- `SLOT_SIZE` - The size of the primary/secondary slot of MCUBootApp. This app will be used with
    - 0x%VALUE%
- `IMG_TYPE` - The slot of MCUBootApp, for which the image is being built.
    - `BOOT (default)` - to build an image for the primary (BOOT) slot.
    - `UPGRADE` - to build an image for the secondary (UPGRADE) slot.
- `ERASED_VALUE` - Define memory cell contents in the erased state.
    - `0x0` - Internal memory.
    - `0xff` - External memory.
- `TOOLCHAIN_PATH` - The path to the GCC compiler to use for the build.
    - Example: TOOLCHAIN_PATH=/home/user/ModusToolbox/tools_2.4/gcc
    - Example: TOOLCHAIN_PATH=C:/ModusToolbox/tools_2.4/gcc

Flags are set by pre-build action. Result of pre-build can be found in autogenerated file `BlinkyApp/flashmap.mk`.   

- `USE_OVERWRITE` - Define the Upgrade mode type of `MCUBootApp` to use with this app.
    - `1` - For Overwrite mode.
    - `0` - (default) For Swap Upgrade mode.
- `USE_EXTERNAL_FLASH` - Define support of external flash.
    - `1` - external flash is present.
    - `0` - external flash is absent.
- `USE_XIP` - Define support of eXecute in Place mode.
    - `0` - Not used
    - `1` - Used

### Post-build

The post-build action is executed at the compile time for `BlinkyApp`. For the `PSOC_062_2M`, `PSOC_062_1M`, `PSOC_062_512K` platforms, it calls `imgtool` from `MCUboot` scripts and adds a signature to the compiled image.

Flags passed to `imgtool` for a signature are defined in the `SIGN_ARGS` variable in BlinkyApp.mk.

For `CYW20829`, `cysecuretools` is used for the image signing.

### How to program an application

BlinkyApp firmware can be programmed in different ways. The following instructions assume the usage of one of Cypress development kits, for example, `CY8CPROTO_062_4343W`.

1. Direct usage of OpenOCD.

Connect a board to your computer. Switch Kitprog3 to DAP-BULK mode by clicking the `SW3 MODE` button until `LED2 STATUS` constantly shines.

The OpenOCD package is supplied with ModusToolbox™ IDE and can be found in the ModusToolbox™ installation folder `ModusToolbox/tools_2.4/openocd`.

Open the terminal application and execute the following command after substitution of the `PATH_TO_APPLICATION.hex` and `OPENOCD_PATH` paths:

        export OPENOCD_PATH=/Applications/ModusToolbox/tools_2.4/openocd 

        ${OPENOCD_PATH}/bin/openocd -s ${OPENOCD_PATH}/scripts \
                            -f ${OPENOCD_PATH}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD_PATH}/scripts/target/psoc6_2m.cfg \
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
    [BlinkyApp] Update watchdog timer started in MCUBootApp to mark the successful start of the user app
    [BlinkyApp] Turn off the watchdog timer

When the user application is programmed in the upgrade slot and the upgrade procedure was successful:

    ===========================
    [BlinkyApp] BlinkyApp v2.0 [+]
    ===========================
    [BlinkyApp] GPIO initialized 
    [BlinkyApp] UART initialized 
    [BlinkyApp] Retarget I/O set to 115200 baudrate 
    [BlinkyApp] Red led blinks with 0.25 sec period
    [BlinkyApp] Update watchdog timer started in MCUBootApp to mark the successful start of the user app
    [BlinkyApp] Turn off the watchdog timer
    [BlinkyApp] Try to set img_ok to confirm the upgrade image
    [BlinkyApp] SWAP Status : Image OK was set at 0x10027fe8.
