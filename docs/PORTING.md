# Porting how-to

This document describes the requirements and necessary steps needed to port `MCUboot` to a new target `OS`.

# Requirements

These are the requirements for porting `MCUboot`:
  * a configuration file, which can be included as `mcuboot_config/mcuboot_config.h`, which configures various options (that begin with `MCUBOOT_`).
  * a target that provides a `flash` API with the ability to get the flash's minimum write size, and read/write/erase individual sectors.
  * a cryptographic library.
    As `MCUboot` doesn't bundle one, the target OS must already have it bundled.
    The supported libraries at the moment are either `Mbed TLS` or the combination of `tinycrypt` and `Mbed TLS` (where `Mbed TLS` is used to provide functionality not existing in `tinycrypt`).

# Steps to port

These are the steps needed to port `MCUboot`:
  * [Main app and calling the bootloader](#main-app-and-calling-the-bootloader)
  * [Configuration file](#configuration-file)
  * [Flash map](#flash-map)
  * [Memory management for Mbed TLS](#memory-management-for-mbed-tls)

## Main app and calling the bootloader

From the perspective of the target OS, the bootloader can be seen as a library, so an entry point must be provided.
This is likely a typical `app` for the target OS, and it must call the following function to run the bootloader:

```c
int boot_go(struct boot_rsp *rsp);
```

This function is located in the `boot/bootutil/loader.c` file and receives a `struct boot_rsp` pointer.
The `struct boot_rsp` is defined as follows:

```c
struct boot_rsp {
    /** A pointer to the header of the image to be executed. */
    const struct image_header *br_hdr;

    /**
     * The flash offset of the image to execute.  Indicates the position of
     * the image header.
     */
    uint8_t br_flash_id;
    uint32_t br_image_addr;
};
```

After running the management functions of the bootloader, `boot_go` returns an initialized `boot_rsp` which has pointers to the location of the image where the target firmware is located and which can be used to jump to.

## Configuration file

You must provide the `mcuboot_config/mcuboot_config.h` configuration file.
This is included by several files in the `library` portion of MCUboot.
It provides preprocessor definitions that configure the library's build.

See the file `samples/mcuboot_config/mcuboot_config.template.h` for a starting point and more information.
This is a good place to convert settings in your environment's configuration system to those required by MCUboot.
For example, Mynewt uses `MYNEWT_VAL()` and Zephyr uses Kconfig.
These configuration systems are converted to `MCUBOOT_` options in the following files:

- `boot/zephyr/include/mcuboot_config/mcuboot_config.h`
- `boot/mynewt/mcuboot_config/include/mcuboot_config/mcuboot_config.h`

## Flash map

The bootloader must be able to address flash memory regions where the code for MCUboot and the images of applications are stored, in a system-agnostic way.
For that purpose, MCUboot uses the ID, which is an integer number (uint8_t) that uniquely identifies each flash memory region.
Such flash memory regions are served by objects of the `const struct flash_area` type while the layout of these objects is gathered under the `flash_map`.
The common code of MCUboot, which is non-system specific, does not directly access the content of that object and never modifies it.
Instead, it calls the `flash_area_` API to perform any actions on that object, so systems are free to implement the internal logic of the flash memory map or define `struct flash_area` as they wish.
The only restriction is that the ID should be uniquely tied to regions characterized by device, offset, and size.

Changes to the common MCUboot code should not affect the system-specific internals of the flash memory map.
Also, system-specific code within MCUboot is not restricted from directly accessing `struct flash_area` elements.

See the following example for an implementation of `struct flash_area`:

```c
struct flash_area {
    uint8_t  fa_id;         /** The slot/scratch identification */
    uint8_t  fa_device_id;  /** The device id (usually there's only one) */
    uint16_t pad16;
    uint32_t fa_off;        /** The flash offset from the beginning */
    uint32_t fa_size;       /** The size of this sector */
};
```
The above example of a structure holds all the information that is currently required by MCUboot.
MCUboot does not try to access them directly.
Instead, a system is required to provide the following mandatory getter functions:

```c
/*< Obtains ID of the flash area characterized by `fa` */
int     flash_area_get_id(const struct flash_area *fa);
/*< Obtains ID of a device the flash area `fa` described region resides on */
int     flash_area_get_device_id(const struct flash_area *fa)
/*< Obtains offset, from the beginning of a device, the flash area described
 * region starts at */
uint32_t flash_area_get_off(const struct flash_area *fa)
/*< Obtains size, from the offset, of the flash area `fa` characterized region */
uint32_t flash_area_get_size(const struct flash_area *fa)

```

The MCUboot common code uses the defines listed below.
These defines must be defined by system-specific header files and are used to identify the destination of the flash memory area by ID:

```c
/* Independent from multiple image boot */
#define FLASH_AREA_BOOTLOADER         0
#define FLASH_AREA_IMAGE_SCRATCH      3
```
```c
/* Flash area IDs of the first image in case of multiple images */
#define FLASH_AREA_IMAGE_PRIMARY      1
#define FLASH_AREA_IMAGE_SECONDARY    2
```
```c
/* Flash area IDs of the second image in case of multiple images */
#define FLASH_AREA_IMAGE_PRIMARY      5
#define FLASH_AREA_IMAGE_SECONDARY    6
```

The numbers, given above, are provided as an example and depend on system implementation.

The main set of API functions, also required, that perform operations on the flash memory characterized by `struct flash_area` objects is as follows:

```c
/*< Opens the area for use. id is one of the `fa_id`s */
int     flash_area_open(uint8_t id, const struct flash_area **);
void    flash_area_close(const struct flash_area *);
/*< Reads `len` bytes of flash memory at `off` to the buffer at `dst` */
int     flash_area_read(const struct flash_area *, uint32_t off, void *dst,
                     uint32_t len);
/*< Writes `len` bytes of flash memory at `off` from the buffer at `src` */
int     flash_area_write(const struct flash_area *, uint32_t off,
                     const void *src, uint32_t len);
/*< Erases `len` bytes of flash memory at `off` */
int     flash_area_erase(const struct flash_area *, uint32_t off, uint32_t len);
/*< Returns this `flash_area`s alignment */
uint8_t flash_area_align(const struct flash_area *);
/*< What is value is read from erased flash bytes. */
uint8_t flash_area_erased_val(const struct flash_area *);
/*< Given flash area ID, return info about sectors within the area. */
int     flash_area_get_sectors(int fa_id, uint32_t *count,
                     struct flash_sector *sectors);
/*< Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary).
    `image_index` (0 or 1) is the index of the image. Image index is
    relevant only when multi-image support support is enabled */
int     flash_area_id_from_multi_image_slot(int image_index, int slot);
/*< Returns the slot (0 for primary or 1 for secondary), for the supplied
    `image_index` and `area_id`. `area_id` is unique and is represented by
    `fa_id` in the `flash_area` struct. */
int     flash_area_id_to_multi_image_slot(int image_index, int area_id);
```

---
***Note***

*Currently, MCUboot may open a flash area multiple times simultaneously (through nested calls to `flash_area_open`).*
*As a result, MCUboot may call `flash_area_close` on a flash area that is still opened by another part of MCUboot.*
*As a workaround when porting, it may be necessary to implement a counter of the number of times a given flash area has been opened by MCUboot.*
*The `flash_area_close` implementation should only fully deinitialize the underlying flash area when the open counter is decremented to 0.*
*See [this GitHub PR](https://github.com/mcu-tools/mcuboot/pull/894/) for more details.*

---

## Memory management for Mbed TLS

`Mbed TLS` employs dynamic allocation of memory, making use of the pair `calloc/free`.
If `Mbed TLS` is to be used for crypto, your target RTOS needs to provide this pair of functions.

To configure which functions are called when allocating and deallocating memory, `Mbed TLS` uses the following call:

```
int mbedtls_platform_set_calloc_free (void *(*calloc_func)(size_t, size_t),
                                      void (*free_func)(void *));
```

For reference, see [Mbed TLS platform.h](https://tls.mbed.org/api/platform_8h.html).
If your system already provides functions with compatible signatures, those can be used directly here.
Otherwise, create new functions that glue to your `calloc/free` implementations.
