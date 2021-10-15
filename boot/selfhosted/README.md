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


# Status

Failing.

```
secondary area (1):
  version: 2.0.0+0
  image size: 0
  magic: good
  swap type: test
  copy done: unset
  image ok: unset
[ERR] Image in the secondary slot is not valid!
swap type: none
confirmed: 0
```

recreated images don't make sense, they are smaller.
why -S = 4048? shouldn't be 4096?
