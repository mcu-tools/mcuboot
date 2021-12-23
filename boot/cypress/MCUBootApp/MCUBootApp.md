## MCUBootApp - demo bootloading application to use with Cypress targets

### Solution description

This solution demonstrates operation of MCUboot on Cypress PSoC™ 6 devices.

* Single-/Multi-image operation modes
* Overwrite/Swap upgrade modes
* Interrupted upgrade recovery for swap upgrades
* Upgrade image confirmation
* Reverting of bad upgrade images
* Secondary slots located in external flash

This demo supports PSoC™ 6 chips with the 1M-, 2M-, and 512K-flash on board.
The evaluation kits:
* `CY8CPROTO-062-4343W`
* `CY8CKIT-062-WIFI-BT`
* `CY8CPROTO-062S3-4343W``

### Platfrom specifics

MCUBootApp can be built for different platforms. So, the main application makefile `MCUBootApp.mk` operates with common build variables and flags. Most of them can be passed to build system as a `make` command parameter and each platform defines the default value prefixed with `PLATFORM_` in the corresponding makefile - `PSOC6.mk`. The build flags and variables are described in detail in the following paragraphs.

### Memory maps

The MCUboot terminology names a slot from which **boot** occurs as **primary** and a slot where an **upgrade** image is placed as **secondary**. Some platforms support both internal and external flash, some only external flash.

#### Internal flash

The flash map is defined at compile time. It can be configured through makefiles and `MCUBootApp/sysflash/sysflash.h` and `cypress/cy_flash_pal/flash_psoc6/cy_flash_map.c`.

The default `MCUBootApp` flash map is defined for demonstration purpose. The slots' sizes are defined per platform to be compatible with all supported device families.

The actual addresses are provided in corresponding platform doc files:

- [PSOC6.md](../platforms/PSOC6/PSOC6.md)

##### How to modify flash map

When modifying slots sizes, ensure aligning new values with the linker script files for appropriate applications.

**Option 1 - Use the build system variables**

The following parameters can be passed to build system with the `make` command, along with build flag `USE_CUSTOM_MEMORY_MAP=1`:

`BOOTLOADER_SIZE` - Defines the MCUBootApp size.  
`MAX_IMG_SECTORS` - Defines the maximum number in slot flash sectors.  
`IMAGE_1_SLOT_SIZE` - Defines the slot size for Single-image mode.  
`IMAGE_2_SLOT_SIZE` - Defines the slot size of the second image for Multi-image mode.  
`STATUS_PARTITION_OFFSET` - The start of the swap status partition.  
`SCRATCH_SIZE` - The size of the scratch area.  
`EXTERNAL_FLASH_SCRATCH_OFFSET` - The offset in external memory, where the scratch area starts.  
`EXTERNAL_FLASH_SECONDARY_1_OFFSET` - The offset in external memory, where Secondary slot of Image 1 is located.  
`EXTERNAL_FLASH_SECONDARY_2_OFFSET` - The offset in external memory, where Secondary slot of Image 2 is located.  

These parameters can also be changed directly in corresponding platform makefiles prefixed with `PLATFORM_`.

**Option 2.**

Navigate to `sysflash/sysflash.h` and uncomment the `CY_FLASH_MAP_EXT_DESC` definition.
Now define and initialize `struct flash_area *boot_area_descs[]` in the code with flash memory addresses and sizes you need at the beginning of application, so flash APIs from `cy_flash_pal/flash_psoc6/cy_flash_map.c` will use it.

Note: for both options, ensure using the correct `MCUBOOT_MAX_IMG_SECTORS` according to the slot size used. The maximum value of sectors can be set by passing flag `MAX_IMG_SECTORS=__number__` to `make`. For example, on PSoC™ 6 by default, it is set to 128 sectors, which corresponds to the `0x10000` slot size in Multi-image mode. For the sector size of 512 bytes, 128 sectors are needed to fill `0x10000`, 256 sectors for `0x20000`, and so on.

#### External flash

`PSoC™ 6` chips has internal flash and, additionally, supports the external memory connection. Thus, it is possible to place secondary (upgrade) slots in the external memory module and use most of internal for the primary image.
For more details on External Memory usage, refer to the [ExternalMemory.md](ExternalMemory.md) file.

#### PSoC™ 6 RAM

RAM areas in the MCUBootApp bootloading application and BlinkyApp are defined as an example pair. If your user application requires a different RAM area, ensure that it is not overlapped with the MCUBootApp RAM area. The memory (stack) corruption of the bootloading application can cause a failure if SystemCall-served operations were invoked from the user app.

The MCUBootApp linker script also contains the special section `public_ram`, which serves as a shared RAM area between the CM0p and CM4 cores. When CM4 and CM0p cores perform operations with internal flash, this area is used for the interprocessor data sharing.

### Hardware cryptography acceleration

Cypress PSoC™ 6 MCU family supports hardware acceleration of the cryptography based on the mbedTLS Library via a shim layer. The implementation of this layer is supplied as the separate submodule `cy-mbedtls-acceleration`. The hardware acceleration of the cryptography shortens the boot time by more than four times compared to the software implementation (observation results).

To enable the hardware acceleration in `MCUBootApp`, pass flag `USE_CRYPTO_HW=1` to `make` during build.

The hardware acceleration of the cryptography is disabled by default for all devices.

### Multi-image mode

Multi-image operation considers upgrading and verification of more than one image on a device.

By default, MCUBootApp is configured for Single-image mode. To enable Multi-image operation, pass `MCUBOOT_IMAGE_NUMBER=2` as a parameter to `make`.

 `MCUBOOT_IMAGE_NUMBER` can also be changed permanently in the `MCUBootApp/config/mcuboot_config.h` file.

In Multi-image operation (only two images are supported at this moment), the MCUBootApp bootloading application operates as follows:

1. Verifies the Primary_1 and Primary_2 images.
2. Verifies the Secondary_1 and Secondary_2 images.
3. Upgrades Secondary to Primary if valid images found.
4. Boots the image from the Primary_1 slot only.
5. Boots Primary_1 only if both - Primary_1 and Primary_2 are present and valid.

This ensures that two dependent applications can be accepted by the device only if both images are valid.

### Upgrade modes

There are two different types of the upgrade process supported by MCUBootApp. For the `overwrite only` type of upgrade - the secondary image is simply copied to the primary slot after successful validation. No way to revert upgrade if the secondary image is inoperable.

For `swap` upgrade mode - images in the primary and secondary slots are swapped. Upgrade can be reverted if the secondary image did not confirm its operation.

Upgrade mode is the same for all images in Multi-image mode.

#### Overwrite only

To build MCUBootApp for overwrite upgrades only, `MCUBootApp/config/mcuboot_config/mcuboot_config.h` must contain the following definition:

`#define MCUBOOT_OVERWRITE_ONLY 1`

This define can also be set in `MCUBootApp/MCUBootApp.mk`:

`DEFINES_APP +=-DMCUBOOT_OVERWRITE_ONLY=1`

In Overwrite-only mode, MCUBootApp first checks if any upgrade image is present in the secondary slot(s), then validates the digital signature of the upgrade image in the secondary slot(s). If validation is successful, MCUBootApp starts copying the secondary slot content to the primary slot. After the copy is done, MCUBootApp starts the upgrade image execution from the primary slot.

If the upgraded application does not work - there is no way to revert back to the previous working version. Only the new upgrade firmware can fix the previous broken upgrade.

#### Swap mode

For devices with a large minimum-erase size like PSoC™ 6 with 512 bytes and also for configurations, which use external flash with an even bigger minimum-erase size, there is an additional option in MCUBoot to use the dedicated `status partition` for robust storage of swap-related information.

##### Why use swap with status partition

Originally, the MCUboot library has been designed with a consideration that the minimum write/erase size of flash is always 8 bytes or less. This value is critical, because the swap algorithms use it to align portions of data that contain the swap operation status of each flash sector in a slot before writing to flash. Data alignment is also performed before writes of special-purpose data to the image trailer.

Writing of the flash sector status or image trailer data will be the `single cycle` operation to ensure that the power loss and unpredicted resets robustness of bootloading applications. This requirement eliminates the usage of the `read-modify-write` type of operations with flash.

`Swap with status partition` is implemented specifically to address devices with a large write/erase size. It is based on existing MCUboot swap algorithms, but does not have restriction of the 8-byte alignment. Instead, the minimum write/erase size can be specified by the user and the algorithm will calculate sizes of the status partition considering this value. All write/erase operations are aligned to this minimum write/erase size as well.

##### Swap status partition description

The main distinction of `swap with status partition` is that a separate flash area (partition) is used to store the swap status values and image trailer instead of using the free flash area at the end of the primary/secondary image slot.

This partition consists of separate areas:
* the area to store swap status values
  * swap_status_0
  * ...
  * swap_status_x
* the area to store image trailer data:
  * Encryption key 0
  * Encryption key 1
  * Swap size
  * Swap info
  * Copy done value
  * Image ok
  * Boot image magic

The principal diagram of the status partition:

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

`PRIMARY` and `SECONDARY` are areas in the status partition to contain data regarding a corresponding slot in MCUboot.  
`D0`, `D1`, and `Dx` are duplicates of data described on the left. At least 2 duplicates are present in the system. This duplication is used to eliminate flash wear. Each of `Dx` contains valid data for `current swap step - 1`. Each swap operation for the flash sector updates the status for this sector in the current `Dx` and the value on `CNT` increases. The next operation checks the least value of `CNT` in the available `Dx`s, copies the data from `Dx` with `CNT+1` and updates the status of the current sector. This continues until all sectors in the slot are moved and then swapped.  
`CRC` - A 4-byte value - the checksum of data contained in the area.  
`CNT` - A  4-byte value.  
`swap_status_0`, `swap_status_1` - 1-byte values that contain the status for a corresponding image sector.  
`swap_status_x` - The last sector of `BOOT_MAX_IMAGE_SECTORS`.  
`swap_status_max` - The maximum number of sectors that fits in the min write/erase size for particular flash hardware. If `swap_status_max` is less than `swap_status_x`, an additional slice of the min write/erase flash area is allocated to store swap status data.  
`Image trailer` - No less than 64 bytes. The code calculates how many min write/erase sizes to allocate for storing image trailer data.  

**A calculation example for PSoC™ 6 with the minimum write/erase size of 512 bytes.**

The following are considered:
* Single-image case
* Minimum write/erase size 512 bytes
* PRIMARY/SECONDARY slots size `0x50000`
* BOOT_MAX_IMG_SECTORS 0x50000 / 512 = 640
* Number of duplicates `Dx = 2`

One slice of the `min write/erase` size can store data for the maximum number of 500 sectors: 512 - 4 (CRC) - 4 (CNT) - 4 (area magic) = 500.  BOOT_MAX_IMG_SECTORS is 640, so 2 slices of `min write/erase` are allocated. The total size is 1024 bytes. 
Image trailer data fits in 64 bytes, so one slice of the `min write/erase` size is allocated. The total size is 1024 + 512 = 1536 bytes.
The number of duplicates 2. The total size is 1536 * 2 = 3072 bytes.
2 slots are used in the particular case PRIMARY and SECONDARY, each needs 3072 bytes to store swap status data. The total is 3072 * 2 = 6144 bytes.

The swap status partition occupies 6144 bytes of the flash area.

**Expected lifecycle**

The bootloading application uses the swap using the status partition, so Upgrade mode stores the system state in a separate flash area and the following product lifecycle is expected:

`Empty` - A fully-erased device.  
`Ready` - `Empty` -The device is programmed with the MCUboot-based bootloading application - MCUBootApp in this case.  
`Flashed` - Initial version v1.0 of the user application, BlinkyApp in this case, is flashed to the primary (BOOT) slot.  
`Upgraded` - The updated firmware image of the user application is delivered to the secondary slot (UPGRADE) and the bootloading application performs upgrade.  

It is expected that the product stays in the `Upgraded` state until the end of its lifecycle.

If there is a need to wipe out product and flash new firmware directly to the primary (BOOT) slot, the device is transferred to the `Empty` or `Ready` state and then walks through all the states again.

### Hardware limitations

This application is created to demonstrate the MCUboot library features and not as a reference examples. So, some considerations are taken.

1. `SCB5` is used to configure a serial port for debug prints. This is the most commonly used Serial Communication Block number among available Cypress PSoC™ 6 kits. To use custom hardware with this application, set custom `SCB*` and pins in the  `cypress/MCUBootApp/custom_debug_uart_cfg.h` file and pass the `USE_CUSTOM_DEBUG_UART=1` parameter to the `make` command upon MCUBootApp build.

The `custom_debug_uart_cfg.h` file description:

`CUSTOM_UART_HW`           - Sets a custom SCB name used as the debug serial port. (e.g. `SCB1`, `SCB2`, ...)  
`CUSTOM_UART_SCB_NUMBER`   - Sets the number of SCB. It is `x` in the custom SCBx, which is set in `CUSTOM_UART_HW`.
                                 (e.g. `1` if `CUSTOM_UART_HW` is  set to  SCB1, `2` if `CUSTOM_UART_HW`is  set to  SCB2, ...)  
`CUSTOM_UART_PORT`         - Sets the GPIO port number whose pins are used as RX and TX of the debug serial port.  
`CUSTOM_UART_RX_PIN`       - Sets the pin number in the GPIO port used as RX of the debug serial port.  
`CUSTOM_UART_TX_PIN`       - Sets the pin number in the GPIO port used as TX of the debug serial port.  


2. `CY_SMIF_SLAVE_SELECT_0` is used to define the chip select for the SMIF driver. This configuration is used on the evaluation kit for this example CY8CPROTO-062-4343W. To use custom hardware with this application, change the value of `smif_id` in `main.c` of MCUBootApp to a value that corresponds to your design.

### Downloading solution assets

The required set of assets:

* MCUBooot Library (root repository)
* HAL Library (submodule)
* Peripheral Drivers Library (PDL) (submodule)
* mbedTLS Cryptographic Library (submodule)

To get submodules - run the following command:

    git submodule update --init --recursive

### Configuring MCUBootApp bootloading application

1. Choose Upgrade mode.
   
`SWAP` - This mode is set by default in `MCUBootApp/config/mcuboot_config/mcuboot_config.h`. `MCUBOOT_SWAP_USING_STATUS` and `MCUBOOT_SWAP_USING_SCRATCH` are the preprocessor symbols to enable Swap mode.

`Overwrite only` - pass the `USE_OVERWRITE=1` parameter to `make` for Overwrite mode compilation.
    
2. Change the memory map.

Check the section **How to modify flash map** earlier in this document.

3. Enable the hardware acceleration of the cryptography on devices that support this feature.

Pass `USE_CRYPTO_HW=1` to the `make` command. This option is enabled by default on the supported platforms.

4. Change the number of images - Single- or Multi-image configuration.
 
Pass `MCUBOOT_IMAGE_NUMBER=1` for Single-image configuration.
Pass `MCUBOOT_IMAGE_NUMBER=2` for Multi-image configuration.

### Building solution

Folder `boot/cypress` contains a make-files infrastructure for building MCUBootApp bootloader applications. Example build commands are provided later in this document for different build configurations.

* Build MCUBootApp in the `Debug` configuration for Single-image mode.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug MCUBOOT_IMAGE_NUMBER=1

* Build MCUBootApp in `Release` configuration for Multi-image mode.

        make app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Release MCUBOOT_IMAGE_NUMBER=2

The root directory for build is `boot/cypress`.

### Encrypted image support

To protect firmware content from read, plain binary data can be encrypted. MCUBootApp supports the encrypted image in some implementations, depending on the platform.

On PSoC™ 6, an upgrade image can be encrypted and then programmed to corresponding Secondary slot of MCUBootApp. It is then decrypted and transferred to the primary slot using the preferred upgrade method. For more details on the encrypted image implementation, refer to the [PSOC6.md](../platforms/PSOC6/PSOC6.md) file.

### Complete build flags and parameters description

`MCUBOOT_IMAGE_NUMBER` - The number of images to be supported by the current build of MCUBootApp.  
`MCUBOOT_LOG_LEVEL` - Can be set at `MCUBOOT_LOG_LEVEL_DEBUG` to enable the verbose output of MCUBootApp.  
`ENC_IMG` - When set to `1`, enables the encrypted image support in MCUBootApp.  
`USE_OVERWRITE` - `0` - Use swap with Scratch upgrade mode, `1` - use Overwrite only upgrade.  
`USE_BOOTSTRAP` - When set to `1` and Swap mode is enabled, the application in the secondary slot will overwrite the primary slot, if the primary slot application is invalid.  
`USE_EXTERNAL_FLASH` - When set to `1`, enables the external memory support.  
`USE_CRYPTO_HW` - When set to `1`, uses the hardware accelerated cryptography on the PSoC™ 6 platform.  
`USE_CUSTOM_MEMORY_MAP` - When set to 1, the below parameters can be passed to the `make` command for the custom memory map definition.  
`BOOTLOADER_SIZE` - Defines the size of MCUBootApp.  
`MAX_IMG_SECTORS` - Defines the maximum number in the slot flash sectors.  
`IMAGE_1_SLOT_SIZE` - Defines the slot size for Single-image mode.  
`IMAGE_2_SLOT_SIZE` - Defines the slot size of the second image in Multi-image mode.  
`STATUS_PARTITION_OFFSET` start of swap status partition.  
`SCRATCH_SIZE` - The size of the scratch area.  
`EXTERNAL_FLASH_SCRATCH_OFFSET` - The offset in external memory where the scratch area starts.  
`EXTERNAL_FLASH_SECONDARY_1_OFFSET` - The offset in external memory where the secondary slot of Image 1 is located.  
`EXTERNAL_FLASH_SECONDARY_2_OFFSET` - The offset in external memory where the secondary slot of Image 2 is located.  

### Programming solution

The MCUBootApp firmware can be programmed in different ways.

1. The direct usage of OpenOCD.

The OpenOCD package is supplied with ModusToolbox™ IDE and can be found in installation folder `ModusToolbox/tools_2.4/openocd`.

Set environment variable `OPENOCD` to the path to the openocd folder in ModusToolbox™. Exact commands for programming images are provided in the corresponding platform readme files.

2. Using the GUI tool `Cypress Programmer`

Follow [link](https://www.cypress.com/products/psoc-programming-solutions) to download.

Connect the board to your computer. Switch Kitprog3 to DAP-BULK mode by clicking the `SW3 MODE` button until `LED2 STATUS` constantly shines. Open `Cypress Programmer` and click `Connect`, then choose hex file: `MCUBootApp.hex` or `BlinkyApp.hex` and click `Program`. Check the log to ensure the programming is successful. Reset the board.

3. Using `DAPLINK`.

This mode is currently supported only on PSoC™ 6 development kits.

Connect the board to your computer. Switch the embedded Kitprog3 to `DAPLINK` mode by clicking the `SW3 MODE` button until `LED2 STATUS` blinks fast and the mass storage device displays on the OS. Drag and drop `hex` files you wish to program to the `DAPLINK` drive in your OS.

### Build environment troubleshooting

Regular shell/terminal combination on Linux and MacOS.

On Windows:

* Cygwin
* Msys2

Also, an IDE can be used:
* Eclipse / ModusToolbox™ ("makefile project from existing source")

*Make* - ensure that it is added to the system's `PATH` variable and the correct path is the first on the list.

*Python/Python3* - ensure that you have the correct path referenced in `PATH`.

*Msys2* - To use the systems path, navigate to the msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, restart the MSYS2 shell. This will inherit the system's path and find `python` installed in the regular way as well as `imgtool` and its dependencies.
