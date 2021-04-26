## MCUBootApp - demo bootloading application to be used with Cypress targets

### Solution Description

MCUBootApp is created to demonstrate operation of MCUBoot library on Cypress' PSoC 6 device. It supports various operation modes and features of MCUBoot library.

* single/multi image operation modes
* overwrite/swap upgrade modes
* interrupted upgrade recovery for swap upgrades
* upgrade image confirmation
* reverting of bad upgrade images
* secondary slots located in external flash

This demo supports PSoC 6 chips with 1M, 2M and 512K Flash on board.
Evaluation kits are:
* `CY8CPROTO-062-4343W`
* `CY8CKIT-062-WIFI-BT`
* `CY8CPROTO-062S3-4343W`.

### Memory Maps

MCUBoot terminology assumes a slot from which **boot** is happening to be named **primary**, and a slot where **upgrade** image is placed - **secondary**.

#### Internal Flash

The flash map is defined at compile time. It can be configured through makefiles and `MCUBootApp/sysflash/sysflash.h` and `cypress/cy_flash_pal/cy_flash_map.c`.

The default `MCUBootApp` flash map is defined for demonstration purpose. Sizes of slots are adjusted to be compatible with all supported device families: 1M, 2M and 512K.

Actual addresses provided below are calculated by preprocessor in `sysflash.h` and `cy_flash_map.c` per slot sizes set.

##### Single Image Mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10028000 | 0x10000 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10028000 | 0x10038000 | 0x10000 | Secondary_1 (UPGRADE) slot for BlinkyApp; |

If upgrade type is swap using scratch:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10038000 | 0x1800    | Start of swap status partition; |
| 0x10039800 | 0x1000    | Start of scratch area partition;|

##### Multi Image Mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10028000 | 0x10000 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10028000 | 0x10038000 | 0x10000 | Secondary_1 (UPGRADE) slot for BlinkyApp; |
| 0x10038000 | 0x10058000 | 0x20000 | Primary_2 (BOOT) slot of Bootloader       |
| 0x10058000 | 0x10078000 | 0x20000 | Secondary_2 (UPGRADE) slot of Bootloader  |

If upgrade type swap:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10078000 | 0x2800    | Start of swap status partition; |
| 0x1007a800 | 0x1000    | Start of scratch area partition;|

**SWAP upgrade from external memory**

When MCUBootApp is configured to support upgrade images places in external memory following fixed addresses are predefined:

| SMIF base address | Offset      | Description                     |
|-------------------|-------------|---------------------------------|
| 0x18000000        | 0x0         | Start of Secondary_1 (UPGRADE) image;     |
| 0x18000000        | 0x240000    | Start of Secondary_2 (UPGRADE) image;     |
| 0x18000000        | 0x440000    | Start of scratch area partition;|

##### Single Image Mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10058200 | 0x40200 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x18000000 | 0x18040200 | 0x40200 | Secondary_1 (UPGRADE) slot for BlinkyApp; |

If upgrade type swap:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10058200 | 0x3c00    | Start of swap status partition; |

##### Multi Image Mode

| Start addr | End addr   | Size    | Description                               |
|------------|------------|---------|-------------------------------------------|
| 0x10000000 | 0x10018000 | 0x18000 | MCUBootApp (bootloader) area;             |
| 0x10018000 | 0x10058200 | 0x40200 | Primary_1 (BOOT) slot for BlinkyApp;      |
| 0x10058200 | 0x10098400 | 0x40200 | Primary_2 (BOOT) slot of Bootloader       |
| 0x18000000 | 0x18040200 | 0x40200 | Secondary_1 (UPGRADE) slot for BlinkyApp; |
| 0x18240000 | 0x18280200 | 0x40200 | Secondary_2 (UPGRADE) slot of Bootloader; |

If upgrade type is swap using scratch:

| Start addr | Size      | Description                     |
|------------|-----------|---------------------------------|
| 0x10098400 | 0x6400    | Start of swap status partition; |

##### How To Modify Flash Map

When modifying slots sizes - make sure to align new values with linker script files for appropriate applications.

**Option 1**

Navigate to `sysflash.h` and modify slots sizes directly to meet your needs.

`CY_BOOT_BOOTLOADER_SIZE` defines size of MCUBootApp.
`CY_BOOT_IMAGE_1_SIZE` defines slot size for single image case.
`CY_BOOT_IMAGE_2_SIZE` defines slot size of second image in multi image case.

__Option 2.__

Navigate to `sysflash/sysflash.h` and uncomment `CY_FLASH_MAP_EXT_DESC` definition.
Now define and initialize `struct flash_area *boot_area_descs[]` in a code with flash memory addresses and sizes you need at the beginning of application, so flash APIs from `cy_flash_pal/cy_flash_map.c` will use it.

__Note:__ for both options make sure to use correct `MCUBOOT_MAX_IMG_SECTORS`. This should correspond to slot size used. Maximum value of sectors can be set by passing a flag `MAX_IMG_SECTORS=__number__` to `make`. By default it is set to 256 sectors, which corresponds to `0x20000` slot size in multi image use case. Sector size assumed to be 512 bytes, so 128 sectors needed to fill `0x10000`, 256 sectors for `0x20000` and so on.

###### How To Override The Flash Map Values During Build Process

It is possible to override MCUBootApp definitions from build system. Navigate to `MCUBootApp.mk`, find section `DEFINES_APP +=`
Using this construction macros can be defined and passed to compiler.

The full list of macros used to configure the custom multi image case with upgrade from external memory:

* MCUBOOT_MAX_IMG_SECTORS
* CY_FLASH_MAP_EXT_DESC
* CY_BOOT_SCRATCH_SIZE
* CY_BOOT_BOOTLOADER_SIZE
* CY_BOOT_IMAGE_1_SIZE
* CY_BOOT_IMAGE_2_SIZE
* CY_BOOT_EXTERNAL_FLASH_SECONDARY_1_OFFSET
* CY_BOOT_EXTERNAL_FLASH_SECONDARY_2_OFFSET
* CY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET

As an example in a makefile slots sizes redefinition should look like following:

`DEFINES_APP +=-DCY_BOOT_EXTERNAL_FLASH_SCRATCH_OFFSET=0x18780000`
`DEFINES_APP +=-DMCUBOOT_MAX_IMG_SECTORS=168`
`DEFINES_APP +=-DCY_BOOT_IMAGE_1_SIZE=0x15000`
`DEFINES_APP +=-DCY_BOOT_IMAGE_2_SIZE=0x15000`

#### External Flash

It is also possible to place secondary (upgrade) slots in external memory module and use most of internal for primary image.
Details about External Memory usage are described in separate guiding document `MCUBootApp/ExternalMemory.md`.

#### RAM

RAM areas in CM0p-based MCUBootApp bootloading application and CM4-based BlinkyApp are defined as an example pair. If your CM4 user application requires different RAM area make sure it is not overlap with MCUBootApp ram area. Memory (stack) corruption of CM0p application can cause failure if SystemCall-served operations invoked from CM4.

MCUBootApp linker script also contains special section `public_ram`. This section serves for shared ram area between CM0p and CM4 cores. When CM4 and CM0p cores perform operations with internal flash, this area is used for interprocessor connection data sharing.

### Hardware Cryptography Acceleration

Cypress PSoC 6 MCU family supports hardware acceleration of cryptography based on mbedTLS Library via shim layer. Implementation of this layer is supplied as separate submodule `cy-mbedtls-acceleration`. HW acceleration of cryptography shortens boot time in more then 4 times, comparing to software implementation (observation results).

To enable hardware acceleration in `MCUBootApp` pass flag `USE_CRYPTO_HW=1` to `make` while build.

Hardware acceleration of cryptography is enabled for PSoC 6 devices by default.

### Multi Image Mode

Multi image operation considers upgrading and verification of more then one image on the device.

By default MCUBootApp is configured for single image mode. To enable multi image operation pass `MCUBOOT_IMAGE_NUMBER=2` as parameter to `make`.

 `MCUBOOT_IMAGE_NUMBER` can also be changed permanently in `MCUBootApp/config/mcuboot_config.h` file. This value can only be set to 2 (only dual-image is supported at the moment).

In multi image operation (two images are considered for simplicity) MCUBootApp bootloading application operates as following:

1. Verifies Primary_1 and Primary_2 images;
2. Verifies Secondary_1 and Secondary_2 images;
3. Upgrades Secondary to Primary if valid images found;
4. Boots image from Primary_1 slot only;
5. Boots Primary_1 only if both - Primary_1 and Primary_2 are present and valid;

This ensures two dependent applications can be accepted by device only in case both images are valid.

### Upgrade Modes

There are two different types of upgrade process supported by MCUBootApp. In case of `overwrite only` type of upgrade - secondary image is simply copied to primary slot after successful validation. No way to revert upgrade in a case when secondary image is inoperable.

In case of `swap` upgrade mode - images in primary and secondary slots are swaped. Upgrade can be reverted if secondary image did not confirm its operation.

Upgrade mode is the same for all images in multi image mode.

#### Overwrite Only

To build MCUBootApp for overwrite upgrades only `MCUBootApp/config/mcuboot_config/mcuboot_config.h` should contain following define:

`#define MCUBOOT_OVERWRITE_ONLY 1`

This define can also be set in `MCUBootApp/MCUBootApp.mk`:

`DEFINES_APP +=-DMCUBOOT_OVERWRITE_ONLY=1`

In ovewrite only mode MCUBootApp first checks if any upgrade image is present in secondary slot(s), then validates digital signature of upgrade image in secondary slot(s). If validation is successful MCUBootApp starts copying secondary slot content to primary slot. After copy is done MCUBootApp starts upgrade image execution from primary slot.

If upgraded application does not work - there is no way no revert back to previous working version. In this case only new upgrade firmware can fix previous broken upgrade.

#### Swap Mode

There are 2 basic types of swap modes supported in MCUBoot:
* scratch
* move

For devices with large minimum erase size like PSoC 6 with 512 bytes and also for configurations which use external flash with even bigger minimum erase size there is an additional option in MCUBoot to use dedicated `status partition` for robust storage of swap related information.

##### Why use swap with status partition

Originally MCUBoot library has been designed with a consideration, that minimum write/erase size of flash would always be 8 bytes or less. This value is critical, because swap algorithms use it to align portions of data that contain swap operation status of each flash sector in slot before writing to flash. Data alignment is also performed before writes of special purpose data to image trailer.

Writing of flash sector status or image trailer data should be `single cycle` operation to ensure power loss and unpredicted resets robustness of bootloading applications. This requirement eliminates usage of `read-modify-write` type of operations with flash.

`Swap with status partition` is implemented specifically to address devices with large write/erase size. It is based on existing mcuboot swap algorithms, but does not have restriction of 8 bytes alignment. Instead minimum write/erase size can be specified by user and algorithm will calculate sizes of status partition, considering this value. All write/erase operations are aligned to this minimum write/erase size as well.

##### Swap Status Partition Description

The main distinction of `swap with status partition` is that separate flash area (partition) is used to store swap status values and image trailer instead of using free flash area at the end of primary/secondary image slot.

This partition consists of separate areas:
* area to store swap status values
  * swap_status_0
  * ...
  * swap_status_x
* area to store image trailer data:
  * Encryption key 0
  * Encryption key 1
  * Swap size
  * Swap info
  * Copy done value
  * Image ok
  * Boot image magic

Principal diagram of status partition:

```
+-+-+-+-+-+-+         +-+-+-+-+-+-+          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ \
|           |         |     D0    |    -->   |           swap_status_0         |  \
|           |         +-+-+-+-+-+-+          |           swap_status_1         |   \
|           |         |     D1    |          |           swap_status_2         |    \
|           |         +-+-+-+-+-+-+          |                ...              |     \
|  PRIMARY  |   -->   |           |          |           swap_status_max       |      min write/erase
|           |         |    ....   |          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+      size of flash hw
|           |         +-+-+-+-+-+-+          |            AREA_MAGIC           |     /
|           |         |     Dx    |          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    /
+-+-+-+-+-+-+         +-+-+-+-+-+-+          |               CRC               |   /
+-+-+-+-+-+-+                                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  /
|           |                                |               CNT               | /
|           |                                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ \
|           |                                |        swap_status_max+1        |  \
|           |                                |        swap_status_max+2        |   \
| SECONDARY |                                |        swap_status_max+3        |    \
|           |                                |                 ...             |     \
|           |                                |           swap_status_x         |       min write/erase
|           |                                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+       size of flash hw 
+-+-+-+-+-+-+                                |           AREA_MAGIC            |     /
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    /
                                             |               CRC               |   /
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  /
                                             |               CNT               | /
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ \
                                             |                                 |  \
                                             |          Image trailer          |   \
                                             |                                 |    \
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     min write/erase
                                             |            AREA_MAGIC           |     size of flash hw
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    /
                                             |               CRC               |   /
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  /
                                             |               CNT               | /
                                             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
**Scheme legend:**

`PRIMARY` and `SECONRADY` are areas in status partition to contain data regarding corresponding slot in mcuboot.
`D0`, `D1` and `Dx` are duplicates of data described on the left. At least 2 duplicates should be present in system. This duplication is used to eliminate flash wear. Each of `Dx` contains valid data for `current swap step - 1`. Each swap operation for flash sector updates status for this sector in current `Dx` and value on `CNT` inreases. Next operation checks least value of `CNT` in available `Dx`'s, copies there data from `Dx` with `CNT+1` and updates status of current sector. This continues until all sectors in slot are moved and then swaped.
`CRC` - 4 bytes value - checksum of data contaited in area.
`CNT` - 4 bytes value.
`swap_status_0`, `swap_status_1`- one byte values, that contain status for corresponding image sector.
`swap_status_x` - last sector of `BOOT_MAX_IMAGE_SECTORS`.
`swap_status_max` - maximum number of sectors that fits in min write/erase size for particular flash hardware. If `swap_status_max` is less then `swap_status_x` additional slice of min write/erase flash area is allocated to store  swap status data.
`Image trailer` - should be at least 64 bytes. Code calculates how many min write/erase sizes need to be allocated to store image trailer data.m.

**Calculation example for PSoC 6 with minimum write/erase size of 512 bytes is used.**

Following considered:
* Single image case
* Minimum write/erase size 512 bytes
* PRIMARY/SECONDARY slots size `0x50000`
* BOOT_MAX_IMG_SECTORS 0x50000 / 512 = 640
* Number of duplicates `Dx = 2`

One slice of `min write/erase` size can store data for maximum number of 500 sectors: 512 - 4 (CRC) - 4 (CNT) - 4 (area magic) = 500. Since BOOT_MAX_IMG_SECTORS is 640 - 2 slices of `min write/erase` is allocated. Total size is 1024 bytes. 
Image trailer data fits in 64 bytes, so one slice of `min write/erase` size is allocated. Total size is 1024 + 512 = 1536 bytes.
Duplicates number equals 2. Total size is 1536 * 2 = 3072 bytes.
2 slots are used in particular case PRIMARY and SECONDARY, each needs 3072 bytes to store swap status data. Tolal is 3072 * 2 = 6144 bytes.

Swap status partition occupies 6144 bytes of flash area in this case.

**Expected lifecycle**

Since bootloading application that uses swap using status partition upgrade mode stores system state in separate flash area following product lifecycle is expected:
`Empty` - Fully erased device
`Ready` - `Empty` device is programmed with MCUBoot based bootloading application - MCUBootApp in this case.
`Flashed` - Initial version v1.0 of user applicatio, BlinkyApp is this case, flashed to primary (BOOT) slot.
`Upgraded` - updated firmware image of user application is delivered to secondary slot (UPGRADE) and bootloading application performs upgrade.

It is expected that product stays in `Upgraded` state ultil end of its lifecycle.

In case there is a need to wipe out product and flash new firmware directly to primary (BOOT) slot - device should be transfered to `Empty` or `Ready` state and then walk through all states again.

### Hardware Limitations

Since this application is created to demonstrate MCUBoot library features and not as reference examples some considerations are taken.

1. `SCB5` used to configure serial port for debug prints. This is the most commonly used Serial Communication Block number among available Cypress PSoC 6 kits. If you try to use custom hardware with this application - change definition of `CYBSP_UART_HW` in `main.c` of MCUBootApp to SCB* that correspond to your design.

2. `CY_SMIF_SLAVE_SELECT_0` is used as definition SMIF driver API. This configuration is used on evaluation kit for this example CY8CPROTO-062-4343W. If you try to use custom hardware with this application - change value of `smif_id` in `main.c` of MCUBootApp to value that corresponds to your design.

### Downloading Solution's Assets

There is a set assets required:

* MCUBooot Library (root repository)
* PSoC 6 HAL Library (submodule)
* PSoC 6 Peripheral Drivers Library (PDL) (submodule)
* mbedTLS Cryptographic Library (submodule)

To get submodules - run the following command:

    git submodule update --init --recursive

### Configuring MCUBootApp Bootloading Application

1. Choose upgrade mode:
   
`SWAP` - this mode is set by default in `MCUBootApp/config/mcuboot_config/mcuboot_config.h`. `MCUBOOT_SWAP_USING_STATUS` preprocessor symbol is defined to enable this mode.

`Ovewrite only` - pass `USE_OVERWRITE=1` parameter to `make` for overwrite mode compilation.
    
2. Change memory map

Check paragraph **How to modify Flash map** above.

3. Enable hardware acceleration of cryptography

Pass `USE_CRYPTO_HW=1` to `make` command. This option is enabled by default.

4. Change number of images - single or multi image configuration
 
Pass `MCUBOOT_IMAGE_NUMBER=1` for single image configuration
Pass `MCUBOOT_IMAGE_NUMBER=2` for multi image configuration

### Building Solution

This folder `boot/cypress` contains make files infrastructure for building MCUBootApp bootloader application. Example build command are provided below for couple different build configurations.

* Build MCUBootApp in `Debug` configuration for single image use case.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug MCUBOOT_IMAGE_NUMBER=1

* Build MCUBootApp in `Release` configuration for multi image use case.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Release MCUBOOT_IMAGE_NUMBER=2

Root directory for build is `boot/cypress`.

### Encrypted Image Support

To protect user image from unwanted read - Upgrade Image Encryption can be applied. The ECDH/HKDF with EC256 scheme is used in a given solution as well as mbedTLS as a crypto provider.

To enable image encryption support use `ENC_IMG=1` build flag (BlinkyApp should also be built with this flash set 1).

User is also responsible for providing corresponding binary key data in `enc_priv_key[]` (file `\MCUBootApp\keys.c`). The public part will be used by imgtool when signing and encrypting upgrade image. Signing image with encryption is described in `\BlinkyApp\Readme.md`.

After MCUBootApp is built with these settings unencrypted and encrypted images will be accepted in secondary (upgrade) slot.

Example command:

    make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug MCUBOOT_IMAGE_NUMBER=1 ENC_IMG=1

__NOTE__: Debug configuration of MCUBootApp with multi image encrypted upgrades in external flash (built with flags `BUILDCFG=Debug` `MCUBOOT_IMG_NUMBER=2 USE_EXTERNAL_FLASH=1 ENC_IMG=1`) is set to use optimization level `-O2 -g3` to fit into `0x18000` allocated for `MCUBootApp`.

### Programming Solution

There are couple ways of programming MCUBootApp firmware. Following instructions assume usage of one of Cypress development kits `CY8CPROTO_062_4343W`.

1. Direct usage of OpenOCD.

OpenOCD package is supplied with ModuToolbox IDE and can be found in installation folder under `./tools_2.1/openocd`.

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

### Build Environment Troubleshooting

Regular shell/terminal combination on Linux and MacOS.

On Windows:

* Cygwin
* Msys2

Also IDE may be used:
* Eclipse / ModusToolbox ("makefile project from existing source")

*Make* - make sure it is added to system's `PATH` variable and correct path is first in the list;

*Python/Python3* - make sure you have correct path referenced in `PATH`;

*Msys2* - to use systems PATH navigate to msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart MSYS2 shell.

*Cygwin* - add following to build command `CURDIR=pwd | cygpath --mixed -f -` so that build command looks like that:

    make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M CURDIR=`pwd | cygpath --mixed -f -`

This will iherit system's PATH so should find `python3.7` installed in regular way as well as imgtool and its dependencies.

