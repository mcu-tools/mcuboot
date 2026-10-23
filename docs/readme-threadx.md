# MCUboot Port for Eclipse ThreadX

## Overview

This directory provides first-class support for booting and upgrading [Eclipse ThreadX](https://github.com/eclipse-threadx/threadx) applications with [MCUboot](https://github.com/mcu-tools/mcuboot).

The porting layer implements all standard MCUboot bootloader services, including:
- Secure boot validation with cryptographic signature verification (ECDSA P-256 via TinyCrypt).
- Dual-slot swap upgrade mechanism with scratch partition (`MCUBOOT_SWAP_USING_SCRATCH`).
- Automatic rollback to the previous stable firmware version if an update fails to self-confirm.
- Power-loss resilience: atomic flash swap recovery after abrupt power interruptions.
- Target hardware reference configuration for STM32F429ZI-Nucleo (Arm Cortex-M4, 2MB Flash).
- Complete test runner and validation test suite.

---

## Flash Memory Layout

The reference configuration maps onto a 2 MB internal NOR flash (e.g. STM32F429ZI):

| Partition Name | Flash Area ID | Start Offset | Size | Flash Address Range |
| :--- | :--- | :--- | :--- | :--- |
| **MCUboot Bootloader** | N/A | `0x00000000` | 128 KB | `0x08000000` - `0x0801FFFF` |
| **Primary Slot (Active)** | `FLASH_AREA_IMAGE_0` (1) | `0x00020000` | 384 KB | `0x08020000` - `0x0807FFFF` |
| **Secondary Slot (Staging)** | `FLASH_AREA_IMAGE_1` (2) | `0x00080000` | 384 KB | `0x08080000` - `0x080DFFFF` |
| **Scratch Slot** | `FLASH_AREA_IMAGE_SCRATCH` (3) | `0x000E0000` | 128 KB | `0x080E0000` - `0x080FFFFF` |
| **Application Data / Storage** | N/A | `0x00100000` | 1024 KB | `0x08100000` - `0x081FFFFF` |

### Flash Geometry and Alignment
- **Write Alignment**: 8 bytes (`MCUBOOT_BOOT_MAX_ALIGN = 8`), matching Cortex-M flash controller requirements.
- **Erased Value**: `0xFF`.
- **Sector Sizes**: 16 KB (sectors 0-3), 64 KB (sector 4), and 128 KB (sectors 5-11).

---

## Architecture and Interfaces

The port implements all required MCUboot abstraction interfaces in `boot/threadx/`:

1. `<flash_map_backend/flash_map_backend.h>`:
   - Provides partition lookups (`flash_area_open`, `flash_area_close`).
   - Provides flash I/O operations (`flash_area_read`, `flash_area_write`, `flash_area_erase`).
   - Implements sector enumeration (`flash_area_to_sectors`, `flash_area_get_sectors`).
   - Implements alignment and erased value querying (`flash_area_align`, `flash_area_erased_val`).

2. `<mcuboot_config/mcuboot_config.h>`:
   - Enables ECDSA P-256 verification (`MCUBOOT_SIGN_EC256 = 1`).
   - Configures TinyCrypt crypto backend (`MCUBOOT_USE_TINYCRYPT = 1`).
   - Enables scratch swap algorithm (`MCUBOOT_SWAP_USING_SCRATCH = 1`).
   - Sets maximum image sectors to 128 (`MCUBOOT_MAX_IMG_SECTORS = 128`).
   - Enables `MCUBOOT_USE_FLASH_AREA_GET_SECTORS = 1`.

3. `<mcuboot_config/mcuboot_logging.h>` & `<mcuboot_config/mcuboot_assert.h>`:
   - Maps MCUboot log levels (`MCUBOOT_LOG_INF`, `MCUBOOT_LOG_WRN`, `MCUBOOT_LOG_ERR`) and asserts to platform diagnostics.

4. `<sysflash/sysflash.h>`:
   - Defines flash area identifiers: `FLASH_AREA_IMAGE_0`, `FLASH_AREA_IMAGE_1`, `FLASH_AREA_IMAGE_SCRATCH`.

5. `<os/os_malloc.h>`:
   - Maps memory allocation macros (`os_malloc`, `os_free`) to the platform heap allocator.

---

## Cryptographic Signature Verification

All firmware images booted by MCUboot must be signed with a valid private key. The public key is embedded in `boot/threadx/src/keys.c`.

### Generating an ECDSA P-256 Keypair

Generate an ECDSA P-256 keypair using `imgtool`:

```bash
python scripts/imgtool.py keygen -k root-ec-p256.pem -t ecdsa-p256
```

### Exporting the Public Key to C

Export the public key for compilation into the bootloader:

```bash
python scripts/imgtool.py getpub -k root-ec-p256.pem > keys.c
```

---

## Building the Bootloader

The bootloader can be built using CMake and Ninja (or standard Make):

```bash
cd boot/threadx
mkdir build && cd build
cmake -G Ninja ..
ninja
```

This produces:
- `bootutil_threadx`: The compiled MCUboot core and ThreadX port library.
- `mcuboot_threadx`: The standalone bootloader binary.
- `mcuboot_threadx_test`: The self-contained unit and functional test runner.

---

## Signing ThreadX Application Images

ThreadX application binaries must be packaged with MCUboot headers, TLVs, and cryptographic signatures before being flashed.

Use `imgtool.py` to sign raw application binaries:

```bash
python scripts/imgtool.py sign \
    --key root-ec-p256.pem \
    --header-size 0x200 \
    --align 8 \
    --version 1.0.0+0 \
    --slot-size 0x60000 \
    --pad-header \
    app_raw.bin \
    app_signed_v1.bin
```

---

## Firmware Upgrade and Confirmation Workflow

MCUboot uses a two-phase commit protocol to prevent bricking devices with bad firmware:

```
+-----------------------------------------------------------+
| 1. Download candidate image to Secondary Slot (0x08080000)|
+-----------------------------------------------------------+
                              |
                              v
+-----------------------------------------------------------+
| 2. Call boot_set_pending_multi(0, 0) for test swap        |
+-----------------------------------------------------------+
                              |
                              v
+-----------------------------------------------------------+
| 3. System reboots; MCUboot verifies and swaps slots       |
+-----------------------------------------------------------+
                              |
                              v
+-----------------------------------------------------------+
| 4. New ThreadX image starts and runs self-tests           |
+-----------------------------------------------------------+
         |                                         |
    [Tests Pass]                              [Tests Fail]
         v                                         v
+-------------------------------+        +-------------------------------+
| Call boot_set_confirmed()     |        | Reset or let watchdog trigger |
| Image marked permanent        |        | MCUboot rolls back on reboot  |
+-------------------------------+        +-------------------------------+
```

### Example Application Integration

A ThreadX application can link `bootutil_threadx` to manage updates:

```c
#include "tx_api.h"
#include "bootutil/bootutil.h"

void application_entry(ULONG parameter)
{
    /* Run self-diagnostics */
    if (diagnostics_pass()) {
        /* Confirm the new image so MCUboot does not roll back */
        boot_set_confirmed_multi(0);
    }

    /* Normal application execution */
}
```

A complete reference application is provided in `boot/threadx/sample_app/main.c`.

---

## Testing and Verification

The port includes a comprehensive unit and regression test suite covering all critical bootloader operations:

```bash
cd boot/threadx/build
ctest --output-on-failure
```

Or run the test binary directly:
```bash
./mcuboot_threadx_test
```

### Test Suite Matrix

| Test Case | Description | Pass Criteria |
| :--- | :--- | :--- |
| `test_flash_map_bounds_and_alignment` | Validates sector layout, partition boundaries, and 8-byte write alignment. | All partitions match STM32F429 geometry; writes reject unaligned data. |
| `test_signed_boot_valid` | Validates signature verification on a genuine signed ThreadX image. | `boot_go()` returns `0`, vectors point to slot 0 image. |
| `test_invalid_signature_rejection` | Validates rejection when signature bytes are corrupted. | `boot_go()` fails with non-zero exit code. |
| `test_confirmed_update_workflow` | Validates swap upgrade from v1.0.0 to v2.0.0 and image confirmation. | Candidate swapped to primary slot and marked permanent. |
| `test_rollback_after_unconfirmed_update` | Validates automatic rollback when an update reboots without confirmation. | Previous version v1.0.0 restored to primary slot. |
| `test_interrupted_swap_recovery` | Validates power-loss resilience during swap sectors copying. | MCUboot resumes swap after abrupt reset without data corruption. |
