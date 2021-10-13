# Introduction

This is an experimental self-hosted MCUBOOT session that runs on a standard PC.
It emulates flash via RAM and `memcpy`. It is for testing purposes.

# Building

First, build mbedtls. If you recursively clone the `ext/mbedtls` submodule it
will exist in `ext/` already, otherwise, update it:

```
% cd ext
% git submodule update mbedtls
% cd mbdetls
% mkdir build
% cd build
% cmake ..
% make
```

This will create `libmbedcrypto.a` under `build/library`, which is referenced
in `CMakeFiles.txt`.

Now just build the `selfhosted` example:

```
% mkdir build
% cd build
% cmake ..
% make
% ./bootme
```

