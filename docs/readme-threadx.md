# MCUboot Port for Eclipse ThreadX

## Overview

This directory provides first-class support for booting and upgrading [Eclipse ThreadX](https://github.com/eclipse-threadx/threadx) applications with [MCUboot](https://github.com/mcu-tools/mcuboot).

The porting layer implements all standard MCUboot bootloader services, including:
- Secure boot validation with cryptographic signature verification (ECDSA P-256 via mbedTLS).
- Dual-slot swap upgrade mechanism using the move algorithm (`MCUBOOT_SWAP_USING_MOVE`).
- Automatic rollback to the previous stable firmware version if an update fails to self-confirm.
- Power-loss resilience: atomic flash swap recovery after abrupt power interruptions.
- Target hardware reference configuration for STM32F429ZI-Nucleo (Arm Cortex-M4, 2MB Flash).
- Standalone, configurable bootloader application under `boot/threadx/`.
- Sample application and automated verification test runner under `samples/threadx/`.

---

## Flash Memory Layout

The reference configuration maps onto a 2 MB internal NOR flash (e.g. STM32F429ZI):

| Partition Name | Flash Area ID | Start Offset | Size | Flash Address Range |
| :--- | :--- | :--- | :--- | :--- |
| **MCUboot Bootloader** | N/A | `0x00000000` | 128 KB | `0x08000000` - `0x0801FFFF` |
| **Primary Slot (Active)** | `FLASH_AREA_IMAGE_0` (1) | `0x00020000` | 384 KB | `0x08020000` - `0x0807FFFF` |
| **Secondary Slot (Staging)** | `FLASH_AREA_IMAGE_1` (2) | `0x00080000` | 384 KB | `0x08080000` - `0x080DFFFF` |
| **Application Data / Storage** | N/A | `0x000E0000` | 1152 KB | `0x080E0000` - `0x081FFFFF` |

> Note: With the `MCUBOOT_SWAP_USING_MOVE` algorithm, a dedicated scratch area is not required, maximizing available flash for user applications and file storage.

### Flash Geometry and Alignment
- **Write Alignment**: 8 bytes (`MCUBOOT_BOOT_MAX_ALIGN = 8`), matching Cortex-M flash controller requirements.
- **Erased Value**: `0xFF`.
- **Sector Sizes**: 128 KB per slot sector (3 sectors x 128 KB per slot).

---

## Architecture and Interfaces

The port implements all required MCUboot abstraction interfaces:

1. `<flash_map_backend/flash_map_backend.h>`:
   - Provides partition lookups (`flash_area_open`, `flash_area_close`).
   - Provides flash I/O operations (`flash_area_read`, `flash_area_write`, `flash_area_erase`).
   - Implements sector enumeration (`flash_area_to_sectors`, `flash_area_get_sectors`).
   - Implements alignment and erased value querying (`flash_area_align`, `flash_area_erased_val`).

2. `<mcuboot_config/mcuboot_config.h>`:
   - Enables ECDSA P-256 verification (`MCUBOOT_SIGN_EC256 = 1`).
   - Configures mbedTLS crypto backend (`MCUBOOT_USE_MBED_TLS = 1`).
   - Enables swap move algorithm (`MCUBOOT_SWAP_USING_MOVE = 1`).
   - Sets maximum image sectors to 128 (`MCUBOOT_MAX_IMG_SECTORS = 128`).
   - Enables primary slot validation (`MCUBOOT_VALIDATE_PRIMARY_SLOT = 1`).

3. `<mcuboot_config/mcuboot_logging.h>` & `<mcuboot_config/mcuboot_assert.h>`:
   - Maps standard MCUboot logging macros (`BOOT_LOG_INF`, `BOOT_LOG_WRN`, `BOOT_LOG_ERR`, `BOOT_LOG_DBG`) and asserts to platform diagnostics.

4. `<sysflash/sysflash.h>`:
   - Defines flash area identifiers: `FLASH_AREA_IMAGE_0`, `FLASH_AREA_IMAGE_1`.

5. `<os/os_malloc.h>`:
   - Maps memory allocation macros (`os_malloc`, `os_free`) to the platform heap allocator.

---

## Cryptographic Key Configuration

MCUboot verifies images using an embedded public key. The build system automatically extracts the public key from the configured PEM key file at build time using `imgtool.py getpub`.

You can configure the key file path during CMake generation:

```bash
cmake -B build -S boot/threadx -Dmcuboot_sign_key_file=path/to/my_key.pem
```

By default, `${mcuboot_root}/root-ec-p256.pem` is used for development.

---

## Building the Bootloader

The standalone bootloader application can be built using CMake and Ninja:

```bash
cd boot/threadx
cmake -B build -S . -G Ninja
cmake --build build
```

This produces:
- `libbootutil_threadx.a`: The static MCUboot core library and ThreadX port backend.
- `mcuboot_threadx`: The standalone bootloader executable.

---

## Building and Running Samples & Tests

Samples and automated tests are located under `samples/threadx/`:

```bash
cmake -B build_samples -S samples/threadx -G Ninja
cmake --build build_samples
```

This builds:
- `threadx_mcuboot_sample_app`: The sample ThreadX application demonstrating image confirmation.
- `threadx_mcuboot_test`: The complete automated test suite.

Run the test suite:

```bash
ctest --test-dir build_samples --output-on-failure
```

### Test Suite Matrix

| Test Case | Description | Pass Criteria |
| :--- | :--- | :--- |
| `test_flash_map_bounds_and_alignment` | Validates sector layout, partition boundaries, and 8-byte write alignment. | All partitions match STM32F429 geometry; writes reject unaligned data. |
| `test_signed_boot_valid` | Validates signature verification on a genuine signed ThreadX image. | `boot_go()` returns `0`, vectors point to slot 0 image. |
| `test_invalid_signature_rejection` | Validates rejection when signature bytes are corrupted. | `boot_go()` fails with non-zero exit code. |
| `test_confirmed_update_workflow` | Validates swap-move upgrade from v1.0.0 to v2.0.0 and image confirmation. | Candidate swapped to primary slot and marked permanent. |
| `test_rollback_after_unconfirmed_update` | Validates automatic rollback when an update reboots without confirmation. | Previous version v1.0.0 restored to primary slot. |
| `test_interrupted_swap_recovery` | Validates power-loss resilience during swap sectors copying. | MCUboot resumes swap after abrupt reset without data corruption. |
