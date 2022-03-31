### Support of secondary slot in external memory for PSoC™ 6 devices

* For the CYW20829 external memory support, see the [CYW20829.md](../platforms/CYW20829/CYW20829.md) file.

#### Description

This document describes the use of the external memory module as a secondary (upgrade) slot with Cypress PSoC™ 6 devices.

The demonstration device is the `CY8CPROTO-062-4343W` board, which is a PSoC™ 6 device with 2M-flash, but other kits with 1M (CY8CKIT-062-WIFI-BT) or 512K (CY8CPROTO-062S3-4343W) chips can be used as well.
The memory module on boards is S25FL512SAGMFI010 512-Mbit external Quad SPI NOR flash.

Using external memory for secondary slots allows nearly doubling the Boot Image size.

#### Operation design and flow

The design is based on using the SFDP command's auto-discovery functionality of memory module IC and Cypress SMIF PDL driver.

A user's design example:
* The memory-module is SFDP-compliant.
* Only one module is used for the secondary slot.
* The address for the secondary slot starts from 0x18000000.
This corresponds to PSoC™ 6 SMIF (Serial Memory InterFace) IP block mapping.
* The slot size and start address for the upgrade slot meet the requirements, when using swap upgrade.

The default flash map can be found in the [MCUBootApp.md](MCUBootApp.md) file.

MCUBootApp's `main.c` contains the call to Init-SFDP API, which performs the required GPIO configurations, SMIF IP block configuration, SFDP protocol read and memory-config structure initialization.

Now, MCUBootApp is ready to accept an upgrade image from the external memory module.

Upgrades from external memory are supported for both `overwrite only` and `swap with status partition` modes of MCUBootApp. 

##### Requirements to size and start address of upgrade slot when using Swap mode

Due to the MCUboot image structure, some restrictions apply when using upgrades from external flash. The main requirement:

**The trailer portion of an upgrade image can be erased separately.**

To meet this requirement, the image trailer is placed separately on a full flash page, which equals 0x40200 for S25FL512SAGMFI010. 
Considering the default slot size for the external memory case described in the [MCUBootApp.md](MCUBootApp.md) file, occupied external flash looks as follows:

    0x18000000 [xxxxxxxxxxxxxxxx][ttfffffffffffff][fffffffffffffff]

Here:
`0x18000000` - The start address of external memory.
`[xxxxxxxxxxxxxxxx]` - The first flash page of minimum erase size 0x40000 occupied by the firmware.
`[tt]` - The trailer portion (last 0x200 of image) of the upgrade slot placed on a separate flash page.
`[fffff]` - The remained portion of the flash page, used to store the image trailer - this area cannot be used for anything else.

When using slots sizes other than default, consider the above-described `0x40200`.

When the slot size is not aligned to `0x40200`, the start address of the upgrade image in the external flash is calculated starting from the image trailer location. Consider the following example:

The primary slot size required is 590336 bytes (576k + 512b).

Four flash pages are required to fit the secondary slot (P1-P4):

    0x1800 0000 - 0x1804 0000 - P1
    0x1804 0000 - 0x1808 0000 - P2
    0x1808 0000 - 0x180C 0000 - P3
    0x1808 0000 - 0x180C 0000 - P4

The primary slot consists of 512 bytes of the image trailer, it goes to P4, 2 full sectors of 256k goes in P3 and P2, the remainder of 64k is resided in P1.

Thus, the start address of the secondary slot is: 0x1804 0000 - 0x10000 (64k) = 0x1803 0000. The size occupied is 4 * 256k = 786k

#### Execute in place (XIP) mode

In the XIP mode firmware image can be placed in the external memory and executed from there directly. This mode is useful for devices with small internal flash or when one wishes to reserve internal flash for other purposes.

On CYW20829 platform XIP mode is always used due to absence of internal memory.

This is optional for PSoC™ 6 devices. The JSON flash map should contain `"mode": "XIP"` in the `"external_flash" section`. `USE_XIP` flag is added to auto-generated `flashmap.mk` on pre-build action.

When XIP mode is used primary slot of an image can be placed in external memory.

This repository provides default flash map files with suffix _xip_ to be used for XIP mode in `cy_flash_pal/flash_%platform_name%/flashmap`.

#### How to enable external memory support

External memory is enabled when `make` flag `USE_EXTERNAL_FLASH` is set to `1`. Value of this flag is set in auto-generated `flashmap.mk` files when field `"external_flash"` is present in JSON file. 

Default flash maps with suffix _smif_ are provided in `cy_flash_pal/flash_psoc6/flashmap` folder for PSoC™ 6 devices, where presense of external memory in system is optional.

Build MCUBootApp as described in the [MCUBootApp.md](MCUBootApp.md) file.

**Building an upgrade image for external memory:**

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE ERASED_VALUE=0xff FLASH_MAP=cy_flash_pal/flash_psoc6/flashmap/psoc62_swap_single_smif.json IMG_ID=1

`ERASED_VALUE` - Defines the memory cell contents in the erased state. It is `0x00` for PSoC™ 6 internal flash and `0xff` for S25FL512S.

**Programming external memory**

Programming tools require configuration of SMIF block to debug/program external memory. When `MCUBootApp` is built with `BUILDCFG=Debug` flag SMIF configuration structures are added to the `MCUBootApp.hex` image. Additional sections:

At SFlash address `0x16000800` address of SMIF configuration structure is placed.

At SFlash address `0x16007c00` updated content of TOC2 structure is placed.

The MCUBootApp can be programmed similarly to described in the [MCUBootApp.md](MCUBootApp.md) file:

        export OPENOCD=/Applications/ModusToolbox/tools_2.4/openocd

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; psoc6 sflash_restrictions 1" \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit" 

There is a NULL-pointer placed for the SMIF configuration pointer in TOC2 (Table Of Contents, `cy_serial_flash_prog.c`).
This is done to force the CY8PROTO-062-4343W DAP Link firmware to program external memory with hardcoded values.

1. Click the SW3 Mode button on the board to switch the board to DAP Link mode.
2. Once DAP Link removable disk displays, drop (copy) the upgrade image HEX file to it.
This will invoke the firmware to program external memory.

**Note :** the programming of external memory is limited to S25FL512S p/n only at this moment.
