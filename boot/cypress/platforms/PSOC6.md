## PSoC™ 6 platform description

### MCUBootApp specifics

### Default memory map

This repository provides a set of predefined memory maps in JSON files. They are located in `platforms/memory/PSOC6/flashmap`. One can use the predefined flash map or define its own using the predefined file as a template.

### JSON flash map
As absolute addresses are used in JSON flash maps, the placement of flash area in internal or external memory is derived from its address. For instance:

```
        "application_1": {
            "address": {
                "description": "Address of the application primary slot",
                "value": "0x10018000"
            },
            "size": {
                "description": "Size of the application primary slot",
                "value": "0x10000"
            },
            "upgrade_address": {
                "description": "Address of the application secondary slot",
                "value": "0x18030200"
            },
            "upgrade_size": {
                "description": "Size of the application secondary slot",
                "value": "0x10000"
            }
        }
```
declares primary slot in the internal Flash, and secondary slot in the external Flash.

##### Shared secondary slot
Some Flash ICs have large erase block. For SEMPER™ Secure NOR Flash it is 256 kilobytes, so placing each image trailer in a separate erase block seems a waste.

Specific technique is needed to place all trailers of the shared secondary slot in the single erase block. Since the whole erase block with trailer is occasionally cleared by MCUBoot, image padding is required to place trailers at different addresses and to avoid unintended erasing of image bytes.

```
              /|           |-----------|           |
             / |           |           |           |
            /  |-----------|           |           |
           /   |           |           |           |
          /    |           |  Image 2  |-----------|
         /     |  Image 1  |           |           |
        /      :           :           :  Image 3  :
       /       |           |           |           |
      /        |-----------|-----------|-----------|
   Shared      |  Trailer  |  Padding  |           |\
Secondary      |0x200 bytes|0x200 bytes|  Padding  | \
     Slot      |-----------|-----------|           |  \
      \        |           |  Trailer  |0x400 bytes|   \
       \       |           |0x200 bytes|           |  Erase
        \      |           |-----------|-----------|  block
         \     |           |           |  Trailer  |  256 K
          \    |           |           |0x200 bytes|   /
           \   |           |           |-----------|  /
            \  :           :           :           : /
             \ |           |           |           |/
              \|-----------|-----------|-----------|
```
The pre-build script issues messages, such as
```
Note: application_2 (secondary slot) requires 512 padding bytes before trailer
```
to remind about the necessary padding.

**Attention**: shared slot feature support two images only.

### Encrypted Image Support

To protect the user's image from unwanted read, Upgrade Image Encryption can be applied. The ECDH/HKDF with the EC256 scheme is used in a given solution as well as mbedTLS as a crypto provider.

To enable the image encryption support, use the `ENC_IMG=1` build flag (BlinkyApp should also be built with this flash set 1).

The user is also responsible for providing corresponding binary key data in `enc_priv_key[]` (file `\MCUBootApp\keys.c`). The public part will be used by `imgtool` when signing and encrypting upgrade image. Signing image with encryption is described in [BlinkyApp.md](../../BlinkyApp/BlinkyApp.md).

After MCUBootApp is built with these settings, unencrypted and encrypted images will be accepted in the secondary (upgrade) slot.

An example of the command:

    make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json ENC_IMG=1

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
