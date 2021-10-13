# Introduction

This is an experimental self-hosted MCUBOOT session that runs on a standard PC. 
It emulates flash via RAM and `memcpy`. It is for testing purposes.

# Building

First, build mbedtls, e.g.:

```
% git clone git@github.com:armmbed/mbedtls
% cd mbdetls
% mkdir build
% cd build
% cmake ..
% make
```

This will create `libmbedcrypto.a` under `build/library`.

Next, modify `CMakeFiles.txt` in this foldter so that `MBEDTLS_ROOT` points to 
the mbedTLS repository we cloned above.

Standard `cmake` dance:

```
% mkdir build
% cd build
% cmake ..
% make
% ./bootme
```
