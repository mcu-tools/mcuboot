# Building MCUBoot with nRF52840 CC310 enabled

## Pre-prerequisites

Install west from Zephyr's pip package. This requires Python 3 or greater. 
```
pip install --user west
```

Go outside the root folder and clone the [nRF Connect SDK](https://github.com/NordicPlayground/fw-nrfconnect-nrf) next to the mcuboot folder.

```
git clone https://github.com/NordicPlayground/fw-nrfconnect-nrf nrf
west init -l nrf 
west update
```

The reason this is done this way is to get the `nrfxlib` and having west discover it as a module in Zephyr's build system.

## Building

make sure `root-ec-p256.pem` is set as the certificate and that `CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256` is selected not `CONFIG_BOOT_SIGNATURE_TYPE_RSA` in `prj.conf` of `boot/zephyr`.
Since it defaults to tinycrypt you'll have to go into `menuconfig` and change the implementation selection to `cc310` or also set this in `prj.conf`.

```
mkdir build && cd build
cmake -GNinja -DBOARD=nrf52840_pca10056
ninja flash
```

Build a hello world example in zephyr and sign it with imgtool.py with the `root-ec-p256.pem` and flash it at `FLASH_AREA_IMAGE_0`.
