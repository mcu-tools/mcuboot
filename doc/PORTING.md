# Porting How-To

This document describes the requirements and necessary steps required to port
`mcuboot` to a new target `OS`.

# Requirements

* `mcuboot` requires that the target provides a `flash` API with ability to
  get flash alignment and read/write/erase individual sectors.

* `mcuboot` doesn't bundle a cryptographic library, which means the target
  OS must already have it bundled. The supported libraries at the moment are
  `tinycrypt` and `mbed TLS`.

* TODO: what more?

# Steps to port

## Configuration options macro

Configuration options are checked with the macro `MYNEWT_VAL(x)` where `x`
is the configuration to get the value for. This enables support for OSes with
configuration systems that don't take the `#define ...` format. If the target
OS uses `#define ...` one can simply use the following definition:

```c
#define MYNEWT_VAL(x) (x)
```

If your system uses another method, like build config files, a proper façade
must be defined.

## Main app and calling the bootloader

From the perspective of the target OS, the bootloader can be seen as a library,
so an entry point must be provided. This is likely a typical `app` for the
target OS, and it must call the following function to run the bootloader:

```c
int boot_go(struct boot_rsp *rsp);
```

This function is located at `boot/bootutil/loader.c` and receives a `struct
boot_rsp` pointer. The `struct boot_rsp` is defined as:

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

After running the management functions of the bootloader, `boot_go` returns
an initialized `boot_rsp` which has pointers to the location of the image
where the target firmware is located which can be used to jump to.

## Flash access and flash Map

* Regarding flash access the bootloader has two requirements:

### hal_flash_align

`mcuboot` needs to know the alignment of the flash. To get this information it
calls `hal_flash_align`.

TODO: this needs to die and move to flash_map...

### flash_map

The bootloader requires a `flash_map` to be able to know how the flash is
"partitioned". A `flash_map` consists of `struct flash_area` entries
specifying the "partitions", where a `flash_area` defined as follows:

```c
struct flash_area {
    uint8_t  fa_id;         /** The slot/scratch identification */
    uint8_t  fa_device_id;  /** The device id (usually there's only one) */
    uint16_t pad16;
    uint32_t fa_off;        /** The flash offset from the beginning */
    uint32_t fa_size;       /** The size of this sector */
};
```

`fa_id` is can be one of the following options:

```c
#define FLASH_AREA_IMAGE_0 1
#define FLASH_AREA_IMAGE_1 2
#define FLASH_AREA_IMAGE_SCRATCH 3
```

The functions that must be defined for working with the `flash_area`s are:

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
/*< TODO */
int     flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret);
/*< Returns the `fa_id` for slot, where slot is 0 or 1 */
int     flash_area_id_from_image_slot(int slot);
/*< Returns the slot, for the `fa_id` supplied */
int     flash_area_id_to_image_slot(int area_id);
```

## Memory management for mbed TLS

`mbed TLS` employs dynamic allocation of memory, making use of the pair
`calloc/free`. If `mbed TLS` is to be used for crypto, your target RTOS
needs to provide this pair of function.

To configure the what functions are called when allocating/deallocating
memory `mbed TLS` uses the following call [^cite1]:

```
int mbedtls_platform_set_calloc_free (void *(*calloc_func)(size_t, size_t),
                                      void (*free_func)(void *));
```

If your system already provides functions with compatible signatures, those
can be used directly here, otherwise create new functions that glue to
your `calloc/free` implementations.

[^cite1]```https://tls.mbed.org/api/platform_8h.html```
