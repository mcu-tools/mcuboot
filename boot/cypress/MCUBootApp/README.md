### MCUBoot Bootloader

**Description:**

This application is based on upstream MCUBoot library. It is designed be first application started on CM0p.

Features implemented:
* Run on CM0p core
* Has debug prints to terminal on 115200
* Can validate image hash of 2 slots in flash memory BOOT and UPGRADE
* Starts image located in BOOT slot after validating signature
* Performs upgrade operation from UPGRADE slot to BOOT slot after UPGRADE image hash verification

**Flash map used for MCUBoot Bootloader:**

`0x10000 0000 - 0x1002 0000` - MCUBoot Bootloader
`0x10002 0000 - 0x1003 0000` - BOOT slot of Bootloader
`0x10003 0000 - 0x1004 0000` - UPGRADE slot of Bootloader

Size of slots `0x10000` - 64kb

**How to build MCUBoot Bootloader:**

Root directory for build is **boot/cypress**.

The following command will build MCUBoot Bootloader HEX file:

`make app APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W-M0`

Flags by defalt:

`BUILDCFG=Debug`
`MAKEINFO=0`

**How to program MCUBoot Bootloader:**

Use any preffered tool for programming hex files.

Currently implemented makefile jobs use DAPLINK interface for programming.

To program Bootloader image use following command:

`make load APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W-M0`

**Example terminal output:**

When user application programmed in BOOT slot:

`[INF] MCUBoot Bootloader Started`

`[INF] User Application validated successfully`

`[INF] Starting User Application on CM4 (wait)…`