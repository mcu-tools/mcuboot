## PSoC™ 6 platform description

### MCUBootApp specifics

### Default memory map

The default values are set in PSOC6.mk. 

The actual values used by MCUBootApp for the primary and secondary slots, Scratch, and Status areas for swap upgrade are calculated at compile time by the preprocessor.

##### Single-image mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10028000 | 0x10000 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10028000 | 0x10038000 | 0x10000 | Secondary_1 (UPGRADE) slot for BlinkyApp; |

If the upgrade type is swap using scratch:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10038000 | 0x1800    | Start of swap status partition; |
| 0x10039800 | 0x1000    | Start of scratch area partition;|

##### Multi-image mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10028000 | 0x10000 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10028000 | 0x10038000 | 0x10000 | Secondary_1 (UPGRADE) slot for BlinkyApp; |
| 0x10038000 | 0x10058000 | 0x20000 | Primary_2 (BOOT) slot of Bootloader       |
| 0x10058000 | 0x10078000 | 0x20000 | Secondary_2 (UPGRADE) slot of Bootloader  |

If the upgrade type is swap:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10078000 | 0x2800    | Start of swap status partition; |
| 0x1007a800 | 0x1000    | Start of scratch area partition;|

**SWAP upgrade from external memory**

When MCUBootApp is configured to support upgrade images places in external memory, the following fixed addresses are predefined:

| SMIF base address | Offset      | Description                     |
|-------------------|-------------|---------------------------------|
| 0x18000000        | 0x0         | Start of Secondary_1 (UPGRADE) image;     |
| 0x18000000        | 0x240000    | Start of Secondary_2 (UPGRADE) image;     |
| 0x18000000        | 0x440000    | Start of scratch area partition;|

##### Single-image mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10058200 | 0x40200 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x18000000 | 0x18040200 | 0x40200 | Secondary_1 (UPGRADE) slot for BlinkyApp; |

If the upgrade type is swap:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10058200 | 0x3c00    | Start of swap status partition; |

##### Multi-image mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10058200 | 0x40200 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10058200 | 0x10098400 | 0x40200 | Primary_2 (BOOT) slot of Bootloader       |
| 0x18000000 | 0x18040200 | 0x40200 | Secondary_1 (UPGRADE) slot for BlinkyApp; |
| 0x18240000 | 0x18280200 | 0x40200 | Secondary_2 (UPGRADE) slot of Bootloader; |

If the upgrade type is swap using scratch:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10098400 | 0x6400    | Start of swap status partition; |

### Encrypted Image Support

To protect the user's image from unwanted read, Upgrade Image Encryption can be applied. The ECDH/HKDF with the EC256 scheme is used in a given solution as well as mbedTLS as a crypto provider.

To enable the image encryption support, use the `ENC_IMG=1` build flag (BlinkyApp should also be built with this flash set 1).

The user is also responsible for providing corresponding binary key data in `enc_priv_key[]` (file `\MCUBootApp\keys.c`). The public part will be used by `imgtool` when signing and encrypting upgrade image. Signing image with encryption is described in [BlinkyApp.md](../../BlinkyApp/BlinkyApp.md).

After MCUBootApp is built with these settings, unencrypted and encrypted images will be accepted in the secondary (upgrade) slot.

An example of the command:

    make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug MCUBOOT_IMAGE_NUMBER=1 ENC_IMG=1

NOTE: Debug configuration of MCUBootApp with Multi-image encrypted upgrades in external flash (built with flags `BUILDCFG=Debug` `MCUBOOT_IMG_NUMBER=2 USE_EXTERNAL_FLASH=1 ENC_IMG=1`) is set to use optimization level `-O2 -g3` to fit into `0x18000` allocated for `MCUBootApp`.

### Programming applications

#### Using OpenOCD from command line

The following instructions assume the usage of one of Cypress development kits `CY8CPROTO_062_4343W`.

Connect the board to your computer. Switch Kitprog3 to DAP-BULK mode by clicking the `SW3 MODE` button until `LED2 STATUS` constantly shines.

Open the terminal application and execute the following command after substitution of the `PATH_TO_APPLICATION.hex` and `OPENOCD` paths:

        export OPENOCD=/Applications/ModusToolbox/tools_2.4/openocd

        ${OPENOCD}/bin/openocd -s ${OPENOCD}/scripts \
                            -f ${OPENOCD}/scripts/interface/kitprog3.cfg \
                            -f ${OPENOCD}/scripts/target/psoc6_2m.cfg \
                            -c "init; reset init; program PATH_TO_APPLICATION.hex" \
                            -c "resume; reset; exit"
