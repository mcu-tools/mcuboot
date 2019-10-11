# Downloading Solution's Assets

There are four assets required:

* MCUBooot Library (root repository)
* PSoC6 BSP Library
* PSoC6 Peripheral Drivers Library (PDL)
* mbedTLS Cryptographic Library

Those are present as submodules.

To retrieve working environment, root repo has to be cloned recursively:

__git clone --recursive https://github.com/JuulLabs-OSS/mcuboot.git__

As next step submodules have to be updated:

__git submodule update --init__


# Building Solution

Appplication name "MCUBootApp"

Board/device target "CY8CPROTO-062-4343W-M0" (CM0p core)

Build config - optimized for "Debug"

__make app APP_NAME=MCUBootApp TARGET=CY8CPROTO-062-4343W-M0 BUILDCFG=Debug__

Defaults are following:
APP_NAME=MCUBootApp
TARGET=CY8CPROTO-062-4343W-M0
BUILDCFG=Debug