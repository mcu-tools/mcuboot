### External Memory Support For Secondary Slot

#### Description

Given document describes the use of external memory module as a secondary (upgrade) slot with Cypress' PSoC 6 devices.

The demonstration device is `CY8CPROTO-062-4343W` board which is PSoC 6 device with 2M of Flash available, but other kits with 1M (CY8CKIT-062-WIFI-BT) or 512K (CY8CPROTO-062S3-4343W) chips can be used as well.
The memory module present on boards is S25FL512SAGMFI010 512-Mbit external Quad SPI NOR Flash.

Using external memory for secondary slot allows to nearly double the size of Boot Image.

#### Operation Design and Flow

The design is based on using SFDP command's auto-discovery functionality of memory module IC and Cypress' SMIF PDL driver.

It is assumed that user's design meets following:
* The memory-module used is SFDP-compliant;
* There only one module is being used for secondary slot;
* The address for secondary slot should start from 0x18000000.
This corresponds to PSoC 6's SMIF (Serial Memory InterFace) IP block mapping.
* The slot size and start address for upgrade slot meets requirements, when using swap upgrade.

The default flash map can be foung in MCUBootApp.md.

MCUBootApp's `main.c` contains the call to Init-SFDP API which performs required GPIO configurations, SMIF IP block configurations, SFDP protocol read and memory-config structure initialization.

After that MCUBootApp is ready to accept upgrade image from external memory module.

Upgrades from external memory are supported for both `overwrite only` and `swap with status partition` modes of MCUBootApp. 

##### Requirements to size and start address of upgrade slot when using swap mode.

Due to mcuboot image structure some restrictions applies when using upgrades from external flash. The main requirement is the following:

**Trailer portion of UPGRADE image should be possible to erase separately.**

To achive this requirement image trailer should be placed separately on full flash page, which equls 0x40200 in case of S25FL512SAGMFI010. Considering default slot size for external memory case described in MCUBootApp.md, occupied external flash would look as follows:

    0x18000000 [xxxxxxxxxxxxxxxx][ttfffffffffffff][fffffffffffffff]

Here:
`0x18000000` - start address of external memory
`[xxxxxxxxxxxxxxxx]` - first flash page of minimum erase size 0x40000 occupied by firmware.
`[tt]` - trailer portion (last 0x200 of image) of upgrade slot placed on separate flash page.
`[fffff]` - remained portion of flash page, used to store image trailer - this area should not be used for anything else.

When using slots sizes other, then default `0x40200` described above shoulb be considered.

When slot size does not aligned to `0x40000`, start address of UPGRADE image in external flash should be calculated starting from image trailer location. Consider example below.

Primary slot size required is 590336 bytes (576k + 512b).

4 flash pages are required to fit secondary slot (P1-P4):

    0x1800 0000 - 0x1804 0000 - P1
    0x1804 0000 - 0x1808 0000 - P2
    0x1808 0000 - 0x180C 0000 - P3
    0x1808 0000 - 0x180C 0000 - P4

Primary slot consist of 512 bytes of image trailer, it goes to P4, 2 full sectors of 256k goes in P3 and P2, reminded 64k is resided in P1.

Thus start address of secondary slot is: 0x1804 0000 - 0x10000 (64k) = 0x1803 0000. Size occupied is 4 * 256k = 786k    

##### How to enable external memory support:

1. Pass `USE_EXTERNAL_FLASH=1` flag to `make` command when building MCUBootApp.
2. Navigate to `cy_flash_map.c` and check if secondary slot start address and size meet the application's needs.
3. Define which slave select is used for external memory on a board by setting `smif_id` value in `main.c`.
4. Build MCUBootApp as described in `Readme.md`.

**Note 3**: External memory code is developed basing on PDL and can be run on CM0p core only. It may require modifications if used on CM4.

**How to build upgrade image for external memory:**

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x7FE8000 ERASED_VALUE=0xff

`HEADER_OFFSET` defines the offset from original boot image address. This one in line above suggests secondary slot will start from `0x18000000`.

`ERASED_VALUE` defines the memory cell contents in erased state. It is `0x00` for PSoC 6's internal Flash and `0xff` for S25FL512S.

**Programming to external memory**

The MCUBootApp programming can be done similarly to described in `Readme.md`:

        export OPENOCD=/Applications/ModusToolbox/tools_2.1/openocd

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; psoc6 sflash_restrictions 1" \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit" 

There is a NULL-pointer placed for SMIF configuration pointer in TOC2 (Table Of Contents, `cy_serial_flash_prog.c`).
This is done to force CY8PROTO-062-4343W DAP Link firmware to program external memory with hardcoded values.

1. Press SW3 Mode button on a board to switch the board into DAP Link mode.
2. Once DAP Link removable disk appeared drop (copy) the upgrade image HEX file to it.
This will invoke firmware to program external memory.

**Note 3:** the programming of external memory is limited to S25FL512S p/n only at this moment.
