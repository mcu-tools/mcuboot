### External Memory support for Secondary Slot

**Description**

Given document describes the use of external memory module as a secondary (upgrade) slot with Cypress' PSoC6 devices.

The demonstration device is CY8CPROTO-062-4343W board which is PSoC6 device with 2M of Flash available.
The memory module present on board is S25FL512SAGMFI010 512-Mbit external Quad SPI NOR Flash.

Using external memory for secondary slot allows to nearly double the size of Boot Image.

**Operation Design and Flow**

The design is based on using SFDP command's auto-discovery functionality of memory module IC and Cypress' SMIF PDL driver.

It is assumed that user's design meets following:
* The memory-module used is SFDP-compliant;
* There only one module is being used for secondary slot;
* Only "OWERWRITE" bootloading scheme is used;
* The address for secondary slot should start from 0x18000000.
This corresponds to PSoC6's SMIF (Serial Memory InterFace) IP block mapping.
* The slot size for upgrade slot is even (or smaller) to erase size (0x40000) of given memory module.
This requirement is accepted for code simplicity.

The default flash map implemented is the following:
* [0x10000000, 0x10018000] - MCUBootApp (bootloader) area;
* [0x10018000, 0x10028000] - primary slot for BlinkyApp;
* [0x18000000, 0x18010000] - secondary slot for BlinkyApp;
* [0x10038000, 0x10039000] - scratch area (not used);

Size of slots `0x10000` - 64kB

**Note 1**: make sure primary, secondary slot and bootloader app sizes are appropriate and correspond to flash area size defined in Applications' linker files.

**Note 2**: make sure secondary slot start address is aligned (or smaller) to erase size (0x40000 - 256kB).

MCUBootApp's `main.c` contains the call to Init-SFDP API which performs required GPIO configurations, SMIF IP block configurations, SFDP protocol read and memory-config structure initialization.

After that MCUBootApp is ready to accept upgrade image from external memory module.

Once valid upgrade image was accepted the image in external memory will be erased.

**How to enable external memory support:**

1. Seek for `CY_BOOT_USE_EXTERNAL_FLASH` in sources and define it in any: MCUBootApp.mk or any suitable header file.
2. Navigate to `cy_flash_map.c` and check if secondary slot start address and size meet the application's needs.
3. Define which slave select is used for external memory on a board by setting `smif_id` value in `main.c`.
4. Build MCUBootApp as described in `Readme.md`.

**How to build upgrade image for external memory:**

    make app APP_NAME=BlinkyApp PLATFORM=PSOC_062_2M IMG_TYPE=UPGRADE HEADER_OFFSET=0x7FE8000 ERASED_VALUE=0xff

`HEADER_OFFSET` defines the offset from original boot image address. This one in line above suggests secondary slot will start from `0x18000000`.

`ERASED_VALUE` defines the memory cell contents in erased state. It is `0x00` for PSoC6's internal Flash and `0xff` for S25FL512S.

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
