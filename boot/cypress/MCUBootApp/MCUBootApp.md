## MCUBootApp - demo bootloader application to use with Cypress targets

### Solution description

This solution demonstrates the operation of MCUboot on Cypress PSoC™ 6 and CYW20829 devices.

* Single-/Multi-image operation modes
* Overwrite/Swap upgrade modes
* Interrupted upgrade recovery for swap upgrades
* Upgrade image confirmation
* Revert bad upgrade images
* Secondary slots located in external flash

This demo supports PSoC™ 6 chips with the 1M-, 2M-, and 512K-flash on board, and the CYW20829 chip with no internal flash.
The evaluation kits are:
* `CY8CPROTO-062-4343W`
* `CY8CKIT-062-WIFI-BT`
* `CY8CPROTO-062S3-4343W`
* `CYW920829M2EVB-01`
* `CYBLE-416045-EVAL`
* `CY8CPROTO-063-BLE`
* `CY8CKIT-062-BLE`
* `KIT_XMC72_EVK`

### Platfrom specifics

MCUBootApp can be built for different platforms. So, the main application makefile `MCUBootApp.mk` operates with common build variables and flags. Most of them can be passed to the build system as a `make` command parameter and each platform defines the default value prefixed with `PLATFORM_` in the corresponding makefile - `PSOC6.mk` or `CYW20829.mk`. The build flags and variables are described in detail in the following paragraphs.

### Memory maps

The MCUboot terminology names a slot from which **boot** occurs as **primary** and a slot where an **upgrade** image is placed as **secondary**. Some platforms support both internal and external flash and some only external flash.

The flash map of the bootloader is defined at compile-time and cannot be changed dynamically. Flash map is prepared in the industry-accepted JSON (JavaScript Object Notation) format. It should follow the rules described in section **How to modify the flash map**.

`MCUBootApp` contains JSON templates for flash maps with commonly used configurations. They can be found in `platforms/memory/flash_%platform_name%/flashmap` The slots' sizes are defined per platform to be compatible with all supported device families.

The actual addresses are provided in corresponding platform doc files:

- [PSOC6.md](../platforms/PSOC6.md)
- [CYW20289.md](../platforms/CYW20829.md)

#### How to modify the flash map

When modifying slots sizes, ensure aligning new values with the linker script files for appropriate applications.

##### Flash map definition
Flash map describes what flash memory areas are allocated and defines their addresses and sizes. Also, it specifies the type of external flash memory, if applicable.

To build `MCUBootApp` with the given flash map (e.g., `flash_map.json`), supply the following parameter to `make`:
`FLASH_MAP=flash_map.json`

###### Flash map format
Flash map must have the `"boot_and_upgrade"` section, define the location of `MCUBootApp` and at least one image. For instance:

```
{
    "boot_and_upgrade": 
    {
        "bootloader": 
        {
            "address": 
            {
                "description": "Address of the bootloader",
                "value": "0x10000000"
            },
            "size": 
            {
                "description": "Size of the bootloader",
                "value": "0x18000"
            }
        },
        "application_1": 
        {
            "address": 
            {
                "description": "Address of the application primary slot",
                "value": "0x10018000"
            },
            "size": 
            {
                "description": "Size of the application primary slot",
                "value": "0x10000"
            },
            "upgrade_address": 
            {
                "description": "Address of the application secondary slot",
                "value": "0x18030200"
            },
            "upgrade_size": 
            {
                "description": "Size of the application secondary slot",
                "value": "0x10000"
            }
        }
    }
}
```

Here an application identifier should follow the pattern, i.e., the 2nd image in the multi-image case is `"application_2"`, the 3rd is `"application_3"`, and so on. Up to four applications are supported at this moment.

For each image, the location and size of its primary slot are given in the `"address"` and `"size"` parameters. The location and size of the secondary slot are specified in the `"upgrade_address"` and `"upgrade_size"`. All four values described above are mandatory.

There also should be a mandatory `"bootloader"` section, describing the location and size of `MCUBootApp` in the `"address"` and `"size"` parameters, respectively.

###### Scratch area
The scratch area location and size are given in the `"scratch_address"` and `"scratch_size"` parameters of the `"bootloader"` subsection.
For example:

```
{
    "boot_and_upgrade": 
    {
        "bootloader": 
        {
            . . .
            "scratch_address": {
                "description": "Address of the scratch area",
                "value": "0x18440000"
            },
            "scratch_size": {
                "description": "Size of the scratch area",
                "value": "0x10000"
            },
        },
        . . .
```

###### Swap status partition
If the desired upgrade mode is `swap scratch with status partition`, one should define the `"status_address"` and `"status_size"` parameters in the `"bootloader"` subsection, e.g.:

```
{
    "boot_and_upgrade": 
    {
        "bootloader": 
        {
            . . .
            "status_address": {
                "description": "Address of the swap status partition",
                "value": "0x10038000"
            },
            "status_size": {
                "description": "Size of the swap status partition",
                "value": "0x3800"
            }
        },
        . . .
```
The required size of the status partition relies on many factors. If an insufficient size is given in the flash map, `make` will fail with a message such as:
```
Insufficient swap status area - suggested size 0x3800
```
To calculate the minimal correct size of the status partition, one could specify `"value": "0"` for the `"status_size"`. After the intentional `make` failure, copy the correct size from the error message.

###### External flash
If external flash memory is used, one should specify its parameters. The first way is to specify the exact model:

```
{
    "external_flash": [
        {
            "model": "S25HS256T"
        }
    ],
    "boot_and_upgrade": {
        . . .
```
However, the supported model list is incomplete. The known models are Infineon `S25HS256T`/`S25HS512T`/`S25HS01GT` SEMPER™ NOR Flash ICs, and a couple of SPI Flash ICs from other vendors. Another way is specifying the important parameters, like:

```
{
    "external_flash": [
        {
            "flash-size": "0x100000",
            "erase-size": "0x1000"
        }
    ],
    "boot_and_upgrade": {
        . . .
```
for a typical 8-Mbit SPI flash with uniform 4-KByte erase blocks. While JSON list syntax is used for the `"external_flash"` section, only a single instance is supported at this moment.

If the main application image is located in the external flash, `XIP` (eXecute In Place) mode should be turned on. To do so, supply the corresponding `"mode"` parameter:

```
{
    "external_flash": [
        {
            "model": "S25HS256T",
            "mode": "XIP"
        }
    ],
    . . .
```
###### Service RAM Application
The CYW20829 platform has a hardware-supported security counter. For more details on rollback protection support, refer to the [CYW20289.md](../platforms/CYW20829.md) file.
The mentioned feature requires a dedicated area in the flash memory to store the Service RAM Application and other required data. The layout of these areas is defined in the `"service_app"` JSON section:

```
    . . .
    "boot_and_upgrade":
    {
        "bootloader": {
            . . .
        },
        "service_app": 
        {
            "address": {
                "description": "Address of the service application",
                "value": "0x60070000"
            },
            "size": {
                "description": "Size of the service application",
                "value": "0x8000"
            },
            "params_address": {
                "description": "Address of the service application input parameters",
                "value": "0x60078000"
            },
            "params_size": {
                "description": "Size of the service application input parameters",
                "value": "0x400"
            },
            "desc_address": {
                "description": "Address of the service application descriptor",
                "value": "0x60078400"
            },
            "desc_size": {
                "description": "Size of the service application descriptor",
                "value": "0x20"
            }
        },
        "application_1": {
            . . .
```
###### Shared secondary slot
In the multi-image case, one can reduce the utilization of flash memory by placing secondary images in the same area. This area is referred to as **Shared secondary slot**. This is especially desirable if there are more than two images.

An important consideration is that this option assumes updates are performed in a sequential manner (consider the Swap upgrade method): place the 1st image into the shared slot, reset to MCUBoot, check the updated image and set the Image OK flag for the 1st image, reset to MCUBoot for permanent swap. Then place the 2nd image into the shared slot, reset to MCUBoot, check the updated image, and set the Image OK flag for the 2nd image, reset to MCUBoot for permanent swap, etc.

Take into account that it is possible to revert only the last updated image, as its previous version resides in the Shared secondary slot. There is no way to revert changes for previous images, as their backups are gone! That is the trade-off of the Shared secondary slot.

A shared secondary slot is rather a virtual concept, we still create individual flash areas for all secondary images. However, these areas are now overlapped (this is prohibited in the standard multi-image scenario). Moreover, the special placing of secondary slots is required, as described below. Consider the triple-image example:

```
|         |---------|         |\
|         |         |         | \
|---------|         |         |  \
|         |         |         |   \
| Image 1 | Image 2 |---------|    \
|         |         |         |     Shared
|---------|         |         |     Secondary
| Trailer |         | Image 3 |     Slot
|---------|---------|         |    /
|         | Trailer |         |   /
|         |---------|---------|  /
|         |         | Trailer | /
|         |         |---------|/
```
The purpose of such a layout is to allow MCUBoot to understand what image is placed in the shared secondary slot. While secondary images now can (and should) overlap, their trailers must under no circumstances share the same address!

One can declare all secondary slots as shared using the following JSON syntax:

```
    "boot_and_upgrade": {
        "bootloader": {
            . . .
            "shared_slot": {
                "description": "Using shared secondary slot",
                "value": true
            }
        },
```
Alternatively, this can be done for each application:

```
    "boot_and_upgrade": {
        "bootloader": {
            . . .
        },
        "application_1": {
            . . .
            "shared_slot": {
                "description": "Using shared secondary slot",
                "value": true
            }
        },
        "application_2": {
            . . .
            "shared_slot": {
                "description": "Using shared secondary slot",
                "value": true
            }
        },
        . . .
```
where `true` marks the shared slot, `false` marks the normal (non-shared) secondary slot. In theory, one can use a separate secondary slot for the 1st image and a shared secondary slot for all other images.

When the `shared_slot` flag is set, different checks are performed at the pre-build stage. For instance, the following error is reported if image trailers appear at the same address:
```
Same trailer address for application_3 (secondary slot) and application_2 (secondary slot)
```
As mentioned above, a shared secondary slot is a virtual concept, so overlapped flash areas are created for each image's secondary slot. No separate flash area is created for the shared slot itself.

**Upgrade process deviations**

The shared slot feature has some differences and limitations in the update algorithm when there are one or more invalid images in primary slots and an upgrade of these images is initiated through the shared upgrade slot (so-called **bootstrap** mode of bootloader). In this case, the bootloader allows to update the image even if other images are not valid (unlike the classic multi-image case). Bootloader however does not transfer control to these images until all primary slots become valid. ImageOK flag is set by updated images only after their successful validation and start.

Considering the above there is a certain limitation for the shared slot mode. For **swap mode**, an update of valid slots is not possible as long as there is at least one image with an invalid prime slot.

Attempting to upgrade a valid primary slot of one image with an invalid primary slot of another image may run a revert procedure the next time the bootloader is started (provided that the data of the shared slot has not been changed before). Therefore, for the shared slot, it is recommended to first make all invalid primary slots valid and only then update other images through the shared slot.

###### Running on the specific core
PSoC™ 6 platform has an option to select the core (i.e., either `Cortex-M4` or `Cortex-M0+`) on which the user application should be run. This is useful for multicore firmware. Selection is done in the `"core"` JSON section:

```
{
    "boot_and_upgrade":
    {
        "bootloader": {
            . . .
        },
        "application_1": {
            "core": {
                "description": "Run app on the specific core. PSoC6: CM0P or CM4",
                "value": "CM0P"
            },
            "address": {
                "description": "Address of the application primary slot",
                "value": "0x10018000"
            },
            "size": {
                "description": "Size of the application primary slot",
                "value": "0x10000"
            },
            . . .
        }
    }
}
```
If not specified, the default `CM4` core is assumed.

Note that in the multi-image case this option makes sense only for `application_1`, as MCUBoot always starts the 1st image. Specifying `core` for other images is an error.

###### JSON syntax rules
```
| Group              | Item              | Description                                              |
|--------------------|-------------------|----------------------------------------------------------|
| `external_flash`   | `model`           | External flash model (if supported), e.g. `S25HS256T`    |
| `external_flash`   | `flash-size`      | External flash size in bytes (if model is not supported) |
| `external_flash`   | `erase-size`      | Erase block size in bytes (if model is not supported)    |
| `external_flash`   | `mode`            | Set to `XIP` for eXecute In Place                        |
| `boot_and_upgrade` | `bootloader`      | Contains flash areas used by the `MCUBootApp`            |
| `bootloader`       | `address`         | Absolute address of the `MCUBootApp`                     |
| `bootloader`       | `size`            | Size of the `MCUBootApp` in bytes                        |
| `bootloader`       | `scratch_address` | Absolute address of the Scratch Area                     |
| `bootloader`       | `scratch_size`    | Size of the Scratch Area in bytes                        |
| `bootloader`       | `status_address`  | Absolute address of the Swap Status Partition            |
| `bootloader`       | `status_size`     | Size of the Swap Status Partition in bytes               |
| `bootloader`       | `shared_slot`     | Marking the shared secondary slot for all images         |
| `boot_and_upgrade` | `service_app`     | Reserves flash space for Service RAM Application         |
| `service_app`      | `address`         | Address of the Service RAM Application                   |
| `service_app`      | `size`            | Size of the Service RAM Application                      |
| `service_app`      | `params_address`  | Address of the input parameters (follows the app)        |
| `service_app`      | `params_size`     | Size of the service application input parameters         |
| `service_app`      | `desc_address`    | Address of the app descriptor (follows the parameters)   |
| `service_app`      | `desc_size`       | Size of the service application descriptor (always 0x20) |
| `boot_and_upgrade` | `application_1`   | Contains flash areas of the 1st application image        |
| `boot_and_upgrade` | `application_2`   | 2nd image, see the description of `application_1`        |
| `boot_and_upgrade` | `application_3`   | 3rd image, see the description of `application_1`        |
| `boot_and_upgrade` | `application_4`   | 4th image, see the description of `application_1`        |
| `application_1`    | `address`         | Absolute address of the Primary Slot of the 1st image    |
| `application_1`    | `size`            | Size (in bytes) of the Primary Slot of the 1st image     |
| `application_1`    | `upgrade_address` | Absolute address of the Secondary Slot of the 1st image  |
| `application_1`    | `upgrade_size`    | Size (in bytes) of the Secondary Slot of the 1st image   |
| `application_1`    | `shared_slot`     | Marking the shared secondary slot for the 1st image      |
| `application_1`    | `core`            | Specify the core to run an application (only on PSoC™ 6) |
| `address`          | `value`           | Value of the given address (hex or decimal)              |
| `scratch_address`  | `value`           | Value of the Scratch Area address (hex or decimal)       |
| `status_address`   | `value`           | Value of the Status Partition address (hex or decimal)   |
| `upgrade_address`  | `value`           | Value of the Secondary Slot address (hex or decimal)     |
| `size`             | `value`           | Value of the given size (hex or decimal)                 |
| `scratch_size`     | `value`           | Value of the Scratch Area size (hex or decimal)          |
| `status_size`      | `value`           | Value of the Status Partition size (hex or decimal)      |
| `upgrade_size`     | `value`           | Value of the Secondary Slot size (hex or decimal)        |
| `shared_slot`      | `value`           | Set to `true` for the Shared secondary slot              |
| `core`             | `value`           | Either `Cortex-M4` (default) or `Cortex-M0+`             |
```

###### Flash map internals
When the `FLASH_MAP=` option is supplied to `make`, it involves the Python script `boot/cypress/scripts/memorymap.py`. It takes the JSON file and converts flash map into the C header file `boot/cypress/platforms/memory/memory.h`.

At the same time it creates the `boot/cypress/MCUBootApp/memorymap.mk`, which is conditionally included from the `boot/cypress/MCUBootApp/MCUBootApp.mk`. The generated file contains various definitions derived from the flash map, such as `MCUBOOT_IMAGE_NUMBER`, `MAX_IMG_SECTORS`, `USE_EXTERNAL_FLASH`, and `USE_XIP`. So, there is no need to specify these and similar parameters manually.

Do not edit either `sysflash/memory.h` or `memorymap.mk`, as both files are overwritten on every build.

#### External flash

Some Cypress devices, for example, `CYW20829`, only have an external flash, so all memory areas are located in an external flash.

However, PSoC™ 6 chips have internal flash and, additionally, support the external memory connection. Thus, it is possible to place secondary (upgrade) slots in the external memory module and use most of the internal flash for the primary image.
For more details on External Memory usage, refer to the [ExternalMemory.md](ExternalMemory.md) file.

#### PSoC™ 6 RAM

RAM areas in the MCUBootApp bootloader application and BlinkyApp are defined as an example pair. If your user application requires a different RAM area, ensure that it is not overlapped with the MCUBootApp RAM area. The memory (stack) corruption of the bootloading application can cause a failure if SystemCall-served operations were invoked from the user app.

The MCUBootApp linker script also contains the special section `public_ram`, which serves as a shared RAM area between the CM0p and CM4 cores. When CM4 and CM0p cores perform operations with internal flash, this area is used for interprocessor data sharing.

#### CYW20829 RAM

Only one CM33 core is used in the CYW20829 chips, so there are no restrictions for the RAM usage by the layer1 and layer2 applications (i.e. MCUBootApp and BlinkyApp).

### Hardware cryptography acceleration

Cypress PSoC™ 6 MCU family supports hardware acceleration of the cryptography based on the mbedTLS Library via a shim layer. The implementation of this layer is supplied as the separate submodule `cy-mbedtls-acceleration`. The hardware acceleration of the cryptography shortens the boot time by more than four times compared to the software implementation (observation results).

The CYW20289 chip has hardware acceleration of the SHA256 algorithm only, and in other cases, uses pure software implementation of the cryptography based on MbedTLS.

To enable the hardware acceleration in `MCUBootApp`, pass flag `USE_CRYPTO_HW=1` to `make` during the build.

The hardware cryptographic acceleration is disabled for all devices at the moment. `USE_CRYPTO_HW` flag is set to 0 by default. This package will be updated in the next version.

__NOTE__: Hardware acceleration is not available in the current version of mcuboot since `cy-mbedtls-acceleration` does not support `mbedTLS 3.0` yet. 

__NOTE__: To reduce boot time for MCUBoot in `SWAP` mode, in the case when only **Primary slot** is programmed - disable `BOOTSTRAP` functionality. This happens because `BOOTSTRAP` uses additional slot validation, and it takes more time without hardware cryptography acceleration.

### Multi-image mode

Multi-image operation considers upgrading and verification of more than one image on a device.

Single or multi-image mode is dictated by the `MCUBOOT_IMAGE_NUMBER` `make` flag. This flag's value is set in an auto-generated `memorymap.mk` file per flash map used. There is no need to pass it manually.

In Multi-image operation, up to four images are supported. 

Consider MCUBootApp with 2 images supported. The operation is the following:

1. Verification of the Secondary_1 and Secondary_2 images.
2. Upgrades Secondary to Primary if valid images are found.
3. Verification of the Primary_1 and Primary_2 images.
4. Boots the image from the Primary_1 slot only.
5. Boots Primary_1 only if both - Primary_1 and Primary_2 are present and valid.

This ensures that two dependent applications can be accepted by the device only if both images are valid.

### Upgrade modes

There are two different types of upgrade processes supported by MCUBootApp. For the `overwrite only` type of upgrade - the secondary image is simply copied to the primary slot after successful validation. No way to revert the upgrade if the secondary image is inoperable.

For `swap` upgrade mode - images in the primary and secondary slots are swapped. The upgrade can be reverted if the secondary image did not confirm its operation.

Upgrade mode is the same for all images in the Multi-image mode.

#### Overwrite only

To build MCUBootApp for overwrite upgrades only, `MCUBootApp/config/mcuboot_config/mcuboot_config.h` must contain the following definition:

`#define MCUBOOT_OVERWRITE_ONLY 1`

This flag's value is set in an auto-generated `memorymap.mk` file per flash map used. There is no need to pass it manually.

In Overwrite-only mode, MCUBootApp first checks if any upgrade image is present in the secondary slot(s), then validates the digital signature of the upgrade image in the secondary slot(s). If validation is successful, MCUBootApp starts copying the secondary slot content to the primary slot. After the copy is done, MCUBootApp starts the upgrade image execution from the primary slot.

If the upgraded application does not work - there is no way to revert to the previous working version. Only the new upgrade firmware can fix the previously broken upgrade.

#### Swap mode

For devices with large minimum-erase size like PSoC™ 6 with 512 bytes and also for configurations, which use an external flash with an even bigger minimum-erase size, there is an additional option in MCUBoot to use the dedicated `status partition` for robust storage of swap-related information.

##### Why use swap with status partition

Originally, the MCUboot library has been designed with the consideration that the minimum write/erase size of the flash is always 8 bytes or less. This value is critical because the swap algorithms use it to align portions of data that contain the swap operation status of each flash sector in a slot before writing to flash. Data alignment is also performed before the writing of special-purpose data to the image trailer.

Writing of the flash sector status or image trailer data will be the `single cycle` operation to ensure that the power loss and unpredicted resets robustness of bootloading applications. This requirement eliminates the usage of the `read-modify-write` type of operations with flash.

`Swap with status partition` is implemented specifically to address devices with a large write/erase size. It is based on existing MCUboot swap algorithms but does not have restrictions on the 8-byte alignment. Instead, the minimum write/erase size can be specified by the user and the algorithm will calculate the sizes of the status partition considering this value. All write/erase operations are aligned to this minimum write/erase size as well.

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

The principle diagram of the status partition:

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
`D0`, `D1`, and `Dx` are duplicates of data described on the left. At least 2 duplicates are present in the system. This duplication is used to eliminate flash wear. Each of `Dx` contains valid data for `current swap step - 1`. Each swap operation for the flash sector updates the status for this sector in the current `Dx` and the value on `CNT` increases. The next operation checks the least value of `CNT` in the available `Dx`s, copies the data from `Dx` with `CNT+1`, and updates the status of the current sector. This continues until all sectors in the slot are moved and then swapped.  
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
The number of duplicates is 2. The total size is 1536 * 2 = 3072 bytes.
2 slots are used in the particular case PRIMARY and SECONDARY, each needs 3072 bytes to store swap status data. The total is 3072 * 2 = 6144 bytes.

The swap status partition occupies 6144 bytes of the flash area.

**Expected lifecycle**

The bootloading application uses the swap using the status partition, so Upgrade mode stores the system state in a separate flash area and the following product lifecycle is expected:

`Empty` - A fully-erased device.  
`Ready` - `Empty` -The device is programmed with the MCUboot-based bootloading application - MCUBootApp in this case.  
`Flashed` - Initial version v1.0 of the user application, BlinkyApp in this case, is flashed to the primary (BOOT) slot.  
`Upgraded` - The updated firmware image of the user application is delivered to the secondary slot (UPGRADE) and the bootloading application performs an upgrade.  

It is expected that the product stays in the `Upgraded` state until the end of its lifecycle.

If there is a need to wipe out the product and flash new firmware directly to the primary (BOOT) slot, the device is transferred to the `Empty` or `Ready` state and then walks through all the states again.

### Software limitation
For both internal and external flash memories, the slot address must be properly aligned, as the image trailer should occupy a separate sector.

Normally image trailer occupies the whole erase block (e.g. 512 bytes for PSoC™ 62 internal Flash, or 256 kilobytes for SEMPER™ Secure NOR Flash). There is a specific case when images are placed in both memory types, refer to the [PSOC6.md](../platforms/PSOC6.md) file.

When an improper address is specified, `make` will fail with a message like:
```
Misaligned application_1 (secondary slot) - suggested address 0x18030200
```
This gives the nearest larger address that satisfies the slot location requirements. Other errors, such as overlapping flash areas, are also checked and reported.
### Hardware limitations

This application is created to demonstrate the MCUboot library features and not as a reference example. So, some considerations are taken.

1. `SCB5` is used to configure a serial port for debug prints. This is the most commonly used Serial Communication Block number among available Cypress PSoC™ 6 kits. To use custom hardware with this application, set custom `SCB*` and pins in the  `cypress/MCUBootApp/custom_debug_uart_cfg.h` file and pass the `USE_CUSTOM_DEBUG_UART=1` parameter to the `make` command upon MCUBootApp build.

The `custom_debug_uart_cfg.h` file description:

`CUSTOM_UART_HW`           - Sets a custom SCB name used as the debug serial port. (e.g. `SCB1`, `SCB2`, ...)  
`CUSTOM_UART_SCB_NUMBER`   - Sets the number of SCB. It is `x` in the custom SCBx, which is set in `CUSTOM_UART_HW`.
                                 (e.g. `1` if `CUSTOM_UART_HW` is  set to  SCB1, `2` if `CUSTOM_UART_HW`is  set to  SCB2, ...)  
`CUSTOM_UART_PORT`         - Sets the GPIO port number whose pins are used as RX and TX of the debug serial port.  
`CUSTOM_UART_RX_PIN`       - Sets the pin number in the GPIO port used as RX of the debug serial port.  
`CUSTOM_UART_TX_PIN`       - Sets the pin number in the GPIO port used as TX of the debug serial port.  

The above-described applies to `PSoC™ 62` and `PSoC™ 63` platforms.

2. `CY_SMIF_SLAVE_SELECT_0` is used to define the chip select for the SMIF driver. This configuration is used on the evaluation kit for this example CY8CPROTO-062-4343W. To use custom hardware with this application, change the value of `smif_id` in `main.c` of MCUBootApp to a value that corresponds to your design.
__NOTE__: SMIF driver not supported with `PSoC™ 063` based kits.

### Downloading solution assets

The required set of assets:

* MCUBooot Library (root repository)
* HAL Library (submodule)
* Peripheral Drivers Library (PDL) (submodule)
* mbedTLS Cryptographic Library (submodule)

To get submodules - run the following command:

    git submodule update --init --recursive

### Configuring MCUBootApp bootloading application

1. Choose Upgrade mode and number of images.

`platforms/memory/flash_%platform_name%/flashmap` folder contains a set of predefined flash map JSON files with suffixes _overwrite_ or _swap_ for upgrade methods and _single_ or _multi_ for images number in its names. Depending on the file chosen upgrade method and images number are configured:

`USE_OVERWRITE` `make` flag is set to 1 or 0 for `overwrite` or `swap` mode;
`MCUBOOT_IMAGE_NUMBER` flag is set to a number of corresponding `application_#` sections in the flash map file.

These flag values are set in an auto-generated `memorymap.mk` file per flash map used. There is no need to pass them manually.

__NOTE__: Do not use flash map JSON files with suffixes xip or smif for `PSoC™ 063` kits.

2. Enable the hardware acceleration of the cryptography on devices that support this feature.

Pass `USE_CRYPTO_HW=1` to the `make` command. This option is temporarily disabled by default - see paragraph **Hardware cryptography acceleration**.

Additionally, users can configure hardware rollback protection on the supported platforms. To do this flash map file from `platforms/memory/flash_%platform_name%/flashmap/hw_rollback_prot` folder should be used.

`USE_HW_ROLLBACK_PROT` `make` flag is set to 1 in auto-generated `memorymap.mk`. 

The rollback protection feature is currently supported on CYW20829 devices in Secure mode only.

### Building solution

Folder `boot/cypress` contains make-files infrastructure for building MCUBootApp bootloader applications. Example build commands are provided later in this document for different build configurations.

Toolchain is set by default in `toolchains.mk` file, depending on `COMPILER` makefile variable. MCUBoot is currently support only `GCC_ARM` as compiler. Toolchain path can be redefined, by setting `TOOLCHAIN_PATH` build flag to desired toolchain path. Below is an example on how to set toolchain path from **ModusToolbox™ IDE 3.0**:

    make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json TOOLCHAIN_PATH=c:/Users/${USERNAME}/ModusToolbox/tools_3.0/gcc

* Build MCUBootApp in the `Debug` configuration for Single-image mode with swap upgrade.

    `PSoC™ 062`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json

    `PSoC™ 063`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_063_1M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single.json

    `XMC7200`

        make clean app APP_NAME=MCUBootApp PLATFORM=XMC7200 BUILDCFG=Debug FLASH_MAP=platforms/memory/XMC7000/flashmap/xmc7000_swap_single.json PLATFORM_CONFIG=platforms/memory/XMC7000/flashmap/xmc7200_platform.json CORE=CM0P APP_CORE=CM7 APP_CORE_ID=0

    `XMC7100`

        make clean app APP_NAME=MCUBootApp PLATFORM=XMC7100 BUILDCFG=Debug FLASH_MAP=platforms/memory/XMC7000/flashmap/xmc7000_swap_single.json PLATFORM_CONFIG=platforms/memory/XMC7000/flashmap/xmc7100_platform.json CORE=CM0P APP_CORE=CM7 APP_CORE_ID=0

* Build MCUBootApp in `Release` configuration for Multi-image mode with overwriting update.

    `PSoC™ 062`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Release FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_overwrite_multi.json

    `PSoC™ 063`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_063_1M BUILDCFG=Release FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_overwrite_multi.json


* Build MCUBootApp in `Debug` configuration for Single-image mode with swap upgrade and in `smif` mode.

    `PSoC™ 062`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single_smif.json

    `PSoC™ 063`

        Supported only for `PLATFORM=PSOC_063_1M DEVICE=CY8C6347BZI-BLD53`
        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_1M DEVICE=CY8C6347BZI-BLD53 BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_swap_single_smif.json
        `NOTE:` PSOC_062_1M platform is used here since kit, where particular MPN is installed is called CY8CKIT-062-BLE

* Build MCUBootApp in `Debug` configuration for Single-image mode with swap upgrade and in `xip` mode.

    `PSoC™ 062`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_2M BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_xip_swap.json

    `PSoC™ 063`

        make clean app APP_NAME=MCUBootApp PLATFORM=PSOC_062_1M DEVICE=CY8C6347BZI-BLD53 BUILDCFG=Debug FLASH_MAP=platforms/memory/PSOC6/flashmap/psoc6_xip_swap.json
    `NOTE:` PSOC_062_1M platform is used here since kit, where particular MPN is installed is called CY8CKIT-062-BLE

The root directory for the build is `boot/cypress`.

### Encrypted image support

To protect firmware content from reading, plain binary data can be encrypted. MCUBootApp supports the encrypted image in some implementations, depending on the platform.

On PSoC™ 6, an upgrade image can be encrypted and then programmed to the corresponding Secondary slot of MCUBootApp. It is then decrypted and transferred to the primary slot using the preferred upgrade method. For more details on the encrypted image implementation, refer to the [PSOC6.md](../platforms/PSOC6.md) file.

On CYW20829, an encrypted image is supported in both slots. The firmware here is located in external memory, so, the chip's SMIF block encrypted eXecution In Place (XIP) feature is used. Encrypted firmware is placed directly in the primary slot and is decrypted on the fly. The encrypted upgrade image is first validated by MCUBootApp in the secondary slot and then transferred to the primary slot as it is. For more details on the encrypted image implementation, refer to the [CYW20289.md](../platforms/CYW20829.md) file.

### Rollback protection

MCUboot supports the security counter implementation to provide downgrade prevention. This mechanism allows the user to explicitly restrict the possibility to execute/upgrade images whose security counters are less than the current firmware counter. So, it can be guaranteed, that obsolete firmware with possible vulnerabilities can not be executed on the device.

**Currently, only the CYW20829 platform supports the hardware rollback counter protection.**
For more details on the implementation, refer to the [CYW20289.md](../platforms/CYW20829.md) file.

### Complete build flags and parameters description
 
Can be passed to `make` or set in makefiles.

`MCUBOOT_LOG_LEVEL` - Can be set at `MCUBOOT_LOG_LEVEL_DEBUG` to enable the verbose output of MCUBootApp.   
`ENC_IMG` - When set to `1`, it enables the encrypted image support in MCUBootApp.   
`APP_DEFAULT_POLICY` - The path to a policy file to use for signing MCUBootApp and user application (BlinkyApp) on the CYW20829 platform.   
`USE_BOOTSTRAP` - When set to `1` and Swap mode is enabled, the application in the secondary slot will overwrite the primary slot if the primary slot application is invalid.   
`USE_CRYPTO_HW` - When set to `1`, uses the hardware-accelerated cryptography on the PSoC™ 6 platform, and SHA-256 HW acceleration for the CYW20289 platform.   
`LSC` - The lifecycle state of the chip. Possible options are `SECURE` and `NORMAL_NO_SECURE` (effective on CYW20829 chip only).   
`DEVICE` - is used to set a particular MPN for a platform since multiple MPNs are associated with one platform, for example:   
`PLATFORM=PSOC_062_1M DEVICE=CY8C6347BZI-BLD53`   

The next flags will be set by script in auto-generated makefile 'memorymap.mk':   
`MCUBOOT_IMAGE_NUMBER` - The number of images to be supported by the current build of MCUBootApp.    
`USE_OVERWRITE` - `0` - Use swap with Scratch upgrade mode, `1` - use Overwrite only upgrade.   
`USE_EXTERNAL_FLASH` - When set to `1`, it enables the external memory support on the PSoC™ 6 platform. This value is always set to `1` on CYW20829.   
`USE_HW_ROLLBACK_PROT` - When set to `1`, it enables the hardware rollback protection on the CYW20829 platform with Secure mode enabled.   

Adding `clean` to `make` will clean the build folder, and files boot/cypress/MCUBootApp/memorymap.mk and boot/cypress/platforms/memory/memorymap.h will be removed and re-generated.   

### Programming solution

The MCUBootApp firmware can be programmed in different ways.

1. The direct usage of OpenOCD.

The OpenOCD package is supplied with ModusToolbox™ IDE and can be found in the installation folder `ModusToolbox/tools_2.4/openocd`.

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

*Msys2* - To use the systems path, navigate to the msys2 folder, open `msys2_shell.cmd`, uncomment set `MSYS2_PATH_TYPE=inherit`, and restart the MSYS2 shell. This will inherit the system's path and find `python` installed in the regular way as well as `imgtool` and its dependencies.
