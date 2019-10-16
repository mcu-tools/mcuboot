### Test application for MCUBoot based Bootloader smoke test.

**Properties:**

1. Built for CM4 core.
2. Starts by MCUBoot Application which is running on CM0p.
3. Prints debug info about itself to terminal at 115200.
4. Blinks RED led with 2 frequencies, depending on type of image - BOOT or UPGRADE. Also prints version of image to terminal.
5. Can be built for BOOT slot or UPGRADE slot of MCUBoot Bootloader.

**How build:**

```make app APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W BUILDCFG=Debug MAKEINFO=1 SIGN=1 HEADER_OFFSET=0x20400 IMG_TYPE=BOOT_IMG```

**Flags:**
- **MAKEINFO** - 0 (default) - silent build, 1 - verbose output of complilation, .
- **SIGN** - 0 (default) - do not add signature, 1 - add signature to output file (needed for User Application).
- **HEADER_OFFSET** - 0 (default) - no offset of output hex file, 0x%VALUE% - offset for output hex file. Value 0x20400 includes 0x400 offset for MCUBoot header and 0x20000 for MCUBoot Bootloader (until appropriate linker files would be established)
- **IMG_TYPE** - BOOT_IMG(default) - build image for BOOT slot of MCUBoot Bootloader, UPGRADE_IMG - build image for UPGRADE slot of MCUBoot Bootloader.
  **NOTE**: In case of UPGRADE image HEADER_OFFSET should be set to 0x40400
