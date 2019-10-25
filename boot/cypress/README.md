### Port of MCUBoot library to be used with Cypress targets

**Downloading Solution's Assets**

There is a set assets required:

* MCUBooot Library (root repository)
* PSoC6 BSP Library
* PSoC6 Peripheral Drivers Library (PDL)
* mbedTLS Cryptographic Library

Those are represented as submodules.

To retrieve source code with subsequent submodules pull:

`git clone --recursive http://git-ore.aus.cypress.com/repo/cy_mcuboot_project/cy_mcuboot.git`

Submodules can also be updated and initialized separately:

`cd cy_mcuboot`
`git submodule update --init`

MbedTLS library also relies on submodule crypto:

`cd boot/cypress/libs/mbedtls/crypto`
`git submodule update`


**Building Solutions**

This folder contains make files infrastructure for building both MCUBoot Bootloader and sample BlinkyLed application used to with Bootloader.

Instructions on how to build and upload Bootloader and sample image are located is Readme.md files in corresponding folders.

Root directory for build is **boot/cypress.**

**Currently supportred targets:**
* CY8CPROTO_062_4343W

