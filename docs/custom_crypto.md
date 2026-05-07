<!--
  - SPDX-License-Identifier: Apache-2.0
-->

# Custom Crypto Backend

MCUboot's crypto abstraction layer supports several open source backends like
`MCUBOOT_USE_MBED_TLS`, `MCUBOOT_USE_TINYCRYPT`, etc. The
`MCUBOOT_USE_CUSTOM_CRYPTO` option allows to implement a custom backend that
lets users plug in any crypto library —
hardware accelerator, proprietary SDK, or another software implementation —
without modifying MCUboot's own source.

## How it works

When `MCUBOOT_USE_CUSTOM_CRYPTO` is defined, the crypto abstraction headers
(`sha.h`, `ecdsa.h`, `hmac_sha256.h`, `aes_ctr.h`, `ecdh_p256.h`) do not
define any types or functions for the custom crypto path. Instead, you must
provide them via your platform's custom crypto header.

Your custom header must be included **before** any bootutil crypto headers.
The simplest approach is to add your header's directory to the compiler include
path and have every translation unit include your custom header first (e.g.,
via a force-include compiler flag or by including it at the top of
`mcuboot_config.h`).

## Enabling the custom backend

Define exactly one crypto backend macro in your `mcuboot_config.h` (or via a
compiler flag):

```c
#define MCUBOOT_USE_CUSTOM_CRYPTO
```

Do **not** define any other `MCUBOOT_USE_*` crypto backend at the same time —
that is a compile-time error.

## Implementing `mcuboot_custom_crypto.h`

Your custom crypto header must define the context types and functions before
any bootutil crypto headers are included. The sections below cover the symbols
required by each MCUboot feature; if a required symbol is missing, the
compiler will fail at the first use.

### Required always: image hash

MCUboot uses the `bootutil_sha_*` interface for image hashing. Your custom
implementation must match the hash algorithm selected by your MCUboot
configuration.

```c
/* Context type */
typedef <your_sha_ctx_type> bootutil_sha_context;

static inline int bootutil_sha_init(bootutil_sha_context *ctx);
static inline int bootutil_sha_drop(bootutil_sha_context *ctx);
static inline int bootutil_sha_update(bootutil_sha_context *ctx,
                                      const void *data, uint32_t data_len);
static inline int bootutil_sha_finish(bootutil_sha_context *ctx,
                                      uint8_t *output);
```

### Required for signature verification: ECDSA

Your custom ECDSA implementation must match the curve selected by your
MCUboot signing configuration. For `MCUBOOT_SIGN_EC256`, implement P-256.
For `MCUBOOT_SIGN_EC384`, implement P-384, including parsing `secp384r1`
public keys and verifying signatures against the selected hash.

```c
/* Context type */
typedef <your_ecdsa_ctx_type> bootutil_ecdsa_context;

static inline void bootutil_ecdsa_init(bootutil_ecdsa_context *ctx);
static inline void bootutil_ecdsa_drop(bootutil_ecdsa_context *ctx);

/* Parse the SubjectPublicKeyInfo blob; advance *cp past the wrapper. */
static inline int bootutil_ecdsa_parse_public_key(bootutil_ecdsa_context *ctx,
                                                  uint8_t **cp, uint8_t *end);

/* Verify a signature over hash[0..hash_len). Returns 0 on success. */
static inline int bootutil_ecdsa_verify(bootutil_ecdsa_context *ctx,
                                        uint8_t *pk, size_t pk_len,
                                        uint8_t *hash, size_t hash_len,
                                        uint8_t *sig, size_t sig_len);
```

The helper interfaces below are not direct MCUboot entry points. They are only
needed if your custom encrypted-image implementation reuses the same helper
abstractions as MCUboot's stock `boot/bootutil/src/encrypted.c` logic.

### Helpers used by the stock image encryption implementation: HMAC-SHA256

```c
typedef <your_hmac_ctx_type> bootutil_hmac_sha256_context;

static inline void bootutil_hmac_sha256_init(
        bootutil_hmac_sha256_context *ctx);
static inline void bootutil_hmac_sha256_drop(
        bootutil_hmac_sha256_context *ctx);
static inline int  bootutil_hmac_sha256_set_key(
        bootutil_hmac_sha256_context *ctx,
        const uint8_t *key, uint32_t key_len);
static inline int  bootutil_hmac_sha256_update(
        bootutil_hmac_sha256_context *ctx,
        const void *data, uint32_t data_len);
static inline int  bootutil_hmac_sha256_finish(
        bootutil_hmac_sha256_context *ctx,
        uint8_t *output, uint32_t output_len);
```

### Helpers used by the stock image encryption implementation: AES-CTR

```c
typedef <your_aes_ctx_type> bootutil_aes_ctr_context;

#define BOOT_ENC_BLOCK_SIZE  16  /* must equal 16 */

static inline void bootutil_aes_ctr_init(bootutil_aes_ctr_context *ctx);
static inline void bootutil_aes_ctr_drop(bootutil_aes_ctr_context *ctx);
static inline int  bootutil_aes_ctr_set_key(bootutil_aes_ctr_context *ctx,
                                             const uint8_t *key);
static inline int  bootutil_aes_ctr_encrypt(bootutil_aes_ctr_context *ctx,
                                             uint8_t *counter,
                                             const uint8_t *m, uint32_t mlen,
                                             size_t blk_off, uint8_t *output);
static inline int  bootutil_aes_ctr_decrypt(bootutil_aes_ctr_context *ctx,
                                             uint8_t *counter,
                                             const uint8_t *c, uint32_t clen,
                                             size_t blk_off, uint8_t *output);
```

### Helpers used by the stock image encryption implementation: ECDH-P256

```c
typedef <your_ecdh_ctx_type> bootutil_ecdh_p256_context;
typedef bootutil_ecdh_p256_context bootutil_key_exchange_ctx;

static inline void bootutil_ecdh_p256_init(bootutil_ecdh_p256_context *ctx);
static inline void bootutil_ecdh_p256_drop(bootutil_ecdh_p256_context *ctx);

/* Compute the shared secret z = ECDH(sk, pk). Returns 0 on success. */
static inline int bootutil_ecdh_p256_shared_secret(
        bootutil_ecdh_p256_context *ctx,
        const uint8_t *pk,  /* 65 bytes, uncompressed */
        const uint8_t *sk,  /* 32 bytes */
        uint8_t *z);        /* 32 bytes out */
```

### Image encryption entry points

When `MCUBOOT_ENC_IMAGES` is defined and you supply a custom backend, you are
also responsible for providing the `boot_enc_*` symbols and
`boot_decrypt_key()` that MCUboot normally gets from
`boot/bootutil/src/encrypted.c`. That file guards its entire body with
`#if !defined(MCUBOOT_USE_CUSTOM_CRYPTO)` so it will not be compiled, and no
manual exclusion from the build is required. Your translation unit must
export:

```c
int  boot_decrypt_key(const uint8_t *buf, uint8_t *enckey);
int  boot_enc_init(struct enc_key_data *enc_state);
int  boot_enc_drop(struct enc_key_data *enc_state);
int  boot_enc_set_key(struct enc_key_data *enc_state, const uint8_t *key);
int  boot_enc_load(struct boot_loader_state *state, int slot,
                   const struct image_header *hdr,
                   const struct flash_area *fap,
                   struct boot_status *bs);
bool boot_enc_valid(const struct enc_key_data *enc_state);
void boot_enc_encrypt(struct enc_key_data *enc_state,
                      uint32_t off, uint32_t sz, uint32_t blk_off,
                      uint8_t *buf);
void boot_enc_decrypt(struct enc_key_data *enc_state,
                      uint32_t off, uint32_t sz, uint32_t blk_off,
                      uint8_t *buf);
void boot_enc_zeroize(struct enc_key_data *enc_state);
```

## Build requirements

| Step | Action |
|------|--------|
| 1. Compiler define | `-DMCUBOOT_USE_CUSTOM_CRYPTO` |
| 2. Include order | Ensure your custom header is included before `bootutil/crypto/*.h` |
| 3. Source files | Compile `.c` files that back your custom implementations |
| 4. Link | Link any required libraries from your crypto SDK |

**Note:** The simplest way to ensure correct include order is to use a
force-include compiler flag (e.g., `-include mcuboot_custom_crypto.h` with GCC)
or include your custom header at the top of `mcuboot_config.h`.

## Minimal example structure

```
my_board/
  crypto/
    mcuboot_custom_crypto.h   ← dispatcher — includes the headers below
    my_sha.h                  ← SHA-256 context + functions
    my_ecdsa.h                ← ECDSA-P256 context + functions
    my_enc.c                  ← boot_enc_* symbols (if MCUBOOT_ENC_IMAGES)
```

`mcuboot_custom_crypto.h`:

```c
#ifndef MCUBOOT_CUSTOM_CRYPTO_H
#define MCUBOOT_CUSTOM_CRYPTO_H

#include "my_sha.h"
#include "my_ecdsa.h"
#if defined(MCUBOOT_ENC_IMAGES)
#include "my_hmac_sha256.h"
#include "my_aes_ctr.h"
#include "my_ecdh_p256.h"
#endif

#endif /* MCUBOOT_CUSTOM_CRYPTO_H */
```

Add `my_board/crypto/` to the include path and compile `my_enc.c` when
`MCUBOOT_ENC_IMAGES` is enabled.  MCUboot will resolve all crypto calls
through the types and functions you define.
