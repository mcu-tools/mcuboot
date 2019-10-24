### Blinking LED test application for MCUBoot Bootloader.

**Description:**

Implements simple Blinky LED CM4 application to demonstrate MCUBoot Application operation in terms of BOOT and UPGRADE process.

It is started by MCUBoot Application which is running on CM0p.

* Blinks RED led with 2 different rates, depending on type of image - BOOT or UPGRADE.
* Prints debug info and version of itself to terminal at 115200 baud.
* Can be built for BOOT slot or UPGRADE slot of MCUBoot Bootloader.

**How to build an application:**

The following command will build regular HEX file of a Blinky Application, BOOT slot:

`make app APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W IMG_TYPE=BOOT`

This have following defaults suggested:

`BUILDCFG=Debug`
`IMG_TYPE=BOOT`

To build UPGRADE image use following command:

`make app APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W IMG_TYPE=UPGRADE`

**How to sign an image:**

To sign obtained image use following command:

`make sign APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W`

Flags defaults:

`BUILDCFG=Debug`
`IMG_TYPE=BOOT`

**How to program an application:**

To program BOOT image:

`make boot APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W`

To program UPGRADE image:

`make upgrade APP_NAME=BlinkyApp TARGET=CY8CPROTO-062-4343W`

Flags defaults:

`BUILDCFG=Debug`

**Flags:**
- `MAKEINFO` - 0 (default) - silent build, 1 - verbose output of complilation, .
- `HEADER_OFFSET` - 0 (default) - no offset of output hex file, 0x%VALUE% - offset for output hex file. Value 0x10000 is slot size of MCUBoot Bootloader is this example
- `IMG_TYPE` - `BOOT` (default) - build image for BOOT slot of MCUBoot Bootloader, `UPGRADE` - build image for `UPGRADE` slot of MCUBoot Bootloader.

**NOTE**: In case of `UPGRADE` image `HEADER_OFFSET` should be set to MCUBoot Bootloader slot size
