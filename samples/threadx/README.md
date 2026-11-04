# Eclipse ThreadX MCUboot Samples and Tests

This directory contains sample applications and automated test suites demonstrating MCUboot integration with Eclipse ThreadX.

## Subdirectories

- **`sample_app/`**: A sample ThreadX application demonstrating:
  - Booting from the primary slot with vector table relocation.
  - Image self-confirmation using `boot_set_confirmed_multi()`.
  - Reading active slot swap state via `boot_read_swap_state_by_id()`.
  - Staging and requesting firmware updates using `boot_set_pending_multi()`.

- **`tests/`**: Automated verification test runner verifying:
  - Flash map bounds checking, alignment, and sector enumeration.
  - ECDSA P-256 signature verification of genuine signed firmware images.
  - Rejection of tampered / corrupted images.
  - Dual-slot swap-move upgrade workflow (`MCUBOOT_SWAP_USING_MOVE`).
  - Automatic rollback on unconfirmed images.
  - Interrupted swap recovery under simulated sudden power loss.

## Building and Running

Configure and build with CMake and Ninja:

```bash
cmake -B build -S samples/threadx -G Ninja
cmake --build build
```

Run the automated test suite:

```bash
ctest --test-dir build --output-on-failure
# Or directly run the test binary:
./build/tests/threadx_mcuboot_test
```
