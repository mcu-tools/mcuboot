# Zephyr test plan

This page describes how MCUboot is tested on Zephyr.

The testing is performed with the code in `samples/zephyr`. These examples
were written using the FRDM-K64F, but other boards should be similar.
Currently, however, the partitions are hardcoded in the Makefile targets
to the flash memory.

---
***Note***

*The script `run-tests.sh` in the `samples/zephyr` directory helps*
*automate the process and provides simple `y or n` prompts for each test*
*case and expected result.*

---

## Building and running

The tests are built using the various `test-*` targets in
`samples/zephyr/Makefile`. To build and run the tests, do the following:

1. For each test, run `make` with the intended test target:

   ```
       make test-good-rsa
   ```

2. Execute a full erase, and program the bootloader itself:

   ```
       pyocd erase --chip
       make flash_boot
   ```

3. After it resets, observe a block of text in the terminal looking as
   follows:

   ```
      main: Starting bootloader

      # (...)a few debug messages(...)

      main: Unable to find bootable image
   ```

4. Load `hello1`:

   ```
       make flash_hello1
   ```

   This prints the following message:

   ```
       main: Jumping to the first image slot
   ```
   You will also get the `hello1` image running.

   There are comments within each test target describing the intended
   behavior for each of these steps, also indicating if an upgrade is
   either meant to happen or not.

5. Load `hello2`:

   ```
       make flash_hello2
   ```

   This prints the following message:

   ```
       boot_swap_type: Swap type: test
   ```
   You will also get the `hello2` image running.

6. Reset the target:

   ```
       pyocd commander -c reset
   ```

7. Observe a revert and the `hello1` image running.

## Testing that "mark as OK" works

To make sure we can mark the image as OK, and that a revert does not
happen, input the following commands:

```
    make flash_hello1
    make flash_hello2
```

This boots the `hello2` image.
To mark this image as OK, input the following commands:

```
    pyocd flash -a 0x7ffe8 image_ok.bin
    pyocd commander -c reset
```

Also, make sure this stays in the `hello2` image.

This step does not make sense on the tests where the upgrade does not
happen.

## Testing all configurations

Repeat these steps for each one of the `test-*` targets in the Makefile.
