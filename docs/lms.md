# LMS signatures

## Overview

LMS (Leighton-Micali Signatures, RFC 8554) is a stateful hash-based
signature scheme, recommended by NIST SP 800-208 and CNSA 2.0 for
post-quantum authenticity of firmware images. Several of its
properties make it a strong fit for a bootloader, even though they
are limitations elsewhere:

- Public keys are very small (32 bytes when expressed as a hash; 56
  bytes in the RFC 8554 wire form actually carried by mcuboot — see
  [Image format](#image-format)).
- Signatures are moderate in size — larger than ECDSA or RSA, but
  smaller than ML-DSA.
- Verification is fast, on the order of tens of milliseconds, and is
  composed entirely of repeated SHA-256 invocations.
- Most importantly, LMS is built around a **one-time signature**
  primitive (LM-OTS). Each underlying OTS leaf must sign at most
  once — reuse breaks security. LMS turns LM-OTS into a usable
  many-time signature by hanging OTS leaves off a Merkle tree, with
  the tree height fixed at key-generation time.

The stateful-key constraint is operational, not cryptographic: it
shifts a hard problem from the verifier to the signer's key-storage
discipline. For a typical OTA signing pipeline, where the signing
key lives in a controlled environment and is consulted serially by a
release process, this is a tractable trade in exchange for PQC
security.

## Algorithm parameters

LMS keys are parameterized at generation time. The parameters split
into two groups.

### LM-OTS (Winternitz)

LM-OTS uses a Winternitz one-time-signature construction. The two
relevant parameters are:

- `n` — size of the hash function output. 32 bytes for SHA-256.
- `w` — width in bits of each Winternitz coefficient. The primary
  size/speed trade-off:

| w | p (chains) | Chain length (2^w) | Hash ops to verify (worst case) | Signature size (p·n bytes) |
|---|------------|--------------------|---------------------------------|----------------------------|
| 1 | 265        | 2                  | 265                             | 8,480                      |
| 2 | 133        | 4                  | 399                             | 4,256                      |
| 4 | 67         | 16                 | 1,072                           | 2,144                      |
| 8 | 34         | 256                | 8,704                           | 1,088                      |

### LMS (Merkle tree)

- `m` — bytes per Merkle node. 32, matching SHA-256.
- `h` — height of the Merkle tree. Determines the total number of
  signatures the key can ever produce, and adds `h` hash values to
  each signature:

| h  | Total signatures | Additional signature bytes |
|----|------------------|----------------------------|
| 5  | 32               | 168                        |
| 10 | 1,024            | 328                        |
| 15 | 32,768           | 488                        |
| 20 | 1,048,576        | 648                        |
| 25 | 33,554,432       | 808                        |

## Mbed TLS 4.x constraints

mcuboot's LMS verifier currently goes through Mbed TLS 4.x's
implementation in `tf-psa-crypto/extras/lms.c`. That implementation
imposes the parameters in practice:

- **Only one parameter set is supported**:
  `LMS_SHA256_M32_H10` + `LMOTS_SHA256_N32_W8` — h=10 (1024 signatures
  over the lifetime of one key), w=8 (1088-byte LM-OTS signature
  proper, 1452-byte total LMS signature, ~8,700 SHA-256 ops to
  verify). The w/h trade-offs above are theoretical until upstream
  extends the implementation.
- **Only verification is shipped by default** (`MBEDTLS_LMS_C`); LMS
  signing requires `MBEDTLS_LMS_PRIVATE`. mcuboot does not enable it
  — signing happens out of tree in imgtool, using a separate Python
  library.
- The PSA Crypto API does not yet expose an LMS interface. mcuboot
  calls `mbedtls_lms_*` directly, which Mbed TLS calls out as an
  intentional public exception until PSA grows `PSA_ALG_LMS` (see
  `1.0-migration-guide.md`, `psa-transition.md`, and
  `architecture/0e-plans.md` in the Mbed TLS 4.1 source tree). This
  is the only interface upstream offers today; we will move to the
  PSA API when it lands.

## Image format

The wire format of `IMAGE_TLV_LMS` (0x26), the LMS public key carried
in `IMAGE_TLV_PUBKEY` / `IMAGE_TLV_KEYHASH`, and the rationale for
the 56-byte public key and 1452-byte signature lengths, are
documented in the bootloader design under
[LMS signatures](design.md#lms-signatures). This document covers the
algorithm, the imgtool/sim/bootloader plumbing, and the operational
implications.

## Signing with imgtool

LMS signing is implemented in `scripts/imgtool/keys/lms.py`, which
wraps the [`pyhsslms`](https://pypi.org/project/pyhsslms/) Pure-Python
LMS library (Russ Housley, RFC 8554 author; BSD-3-Clause). pyhsslms
was chosen because no pip-installable Python wrapper around a C LMS
library exists today: `pyca/cryptography` does not implement LMS,
and `liboqs-python` requires a system `liboqs` install which would
break a vanilla `pip install imgtool` from PyPI.

### Key file

LMS private keys are not PEM/PKCS8 (the PEM ASN.1 universe has no
agreed encoding for stateful-hash keys). imgtool stores them in a
custom PEM-style envelope:

```
-----BEGIN MCUBOOT LMS PRIVATE KEY-----
<base64 of: lms_type || lmots_type || SEED || I || q>
-----END MCUBOOT LMS PRIVATE KEY-----
```

Total payload is 60 bytes: 4 bytes `lms_type`, 4 bytes `lmots_type`,
32 bytes seed, 16 bytes key id `I`, 4 bytes leaf counter `q`. The
`keys/__init__.py` loader recognises the `BEGIN MCUBOOT LMS` marker
and dispatches before falling through to the `cryptography` PEM
loader.

### Sign-time behaviour

Each call to `sign_digest()`:

1. Advances `q` in memory.
2. Atomically rewrites the key file (tmpfile + fsync + rename)
   before returning the signature.
3. If the rewrite fails, no signature is returned.

The hazard the user must manage is **never re-signing from a stale
backup of the key file**: doing so reuses LM-OTS leaves and breaks
LMS's security argument. This is documented in the `lms.py` module
docstring; a production signing-policy document is likely warranted
before any release uses this feature.

### Public key

The 56-byte RFC 8554 public key is exported as raw bytes or
PEM-wrapped. `image.py` uses it directly for both `IMAGE_TLV_PUBKEY`
and `IMAGE_TLV_KEYHASH` (SHA-256 over the same 56 bytes) — the
public key is small enough that there is no real difference between
"carry the key" and "carry the hash and look the key up".

### Cost

On a contemporary development laptop (CPython 3.13, single-thread):

- h=5: keygen 0.12 s, load 0.12 s, sign/verify ~2 ms.
- h=10: keygen 3.9 s, load 3.9 s, sign/verify ~2 ms.

Each `imgtool sign` invocation pays the full load cost because
pyhsslms only persists the 60-byte state, not the 1024-leaf
precomputed tree. A long-lived signing daemon would amortize this;
for a CLI invocation per build, ~4 s on top of the existing build is
tolerable but worth flagging.

## Bootloader integration

The verifier is `boot/bootutil/src/image_lms.c`. It uses
`mbedtls_lms_import_public_key` + `mbedtls_lms_verify` directly on
the 56-byte serialized public key from `bootutil_keys[key_id]`.

### Hash-and-sign

mcuboot signs `SHA-256(image || protected_TLVs)` rather than the
image bytes themselves. RFC 8554 defines LMS as a sign-the-message
scheme (no "ph" / pre-hash variant exists), but a bootloader cannot
in general load a full image into RAM — images may live behind a
controller on external QSPI flash — and the Mbed TLS LMS verify API
takes a single contiguous buffer. The hash-and-sign envelope keeps
imgtool, the simulator, and the bootloader in agreement on what the
"message" is.

The security argument is unchanged. A SHA-256 collision attack would
break the hash-and-sign envelope, but LMS's own internal construction
is also SHA-256-based — an effective break of SHA-256 already breaks
LMS. This is the same trade-off that prevents the use of Ed25519 on
this class of device.

### Buffer sizing

The LMS signature is 1452 bytes — much larger than ECDSA or RSA.
`image_validate.c` was previously sharing one buffer for the hash,
the public key, and the signature; this is now split into
purpose-specific buffers so adding LMS does not push the hash and
key buffers up to 1452 bytes apiece.

## Simulator (the `sig-lms` feature)

The `sig-lms` Cargo feature wires LMS through the simulator stack:

- `TlvGen` signs test images via the
  [`lms-signature`](https://crates.io/crates/lms-signature) crate,
  matching the `LMS_SHA256_M32_H10 + LMOTS_SHA256_N32_W8` parameter
  set the bootloader supports.
- `sig-lms` automatically builds Mbed TLS 4.x for the bootloader side
  via `add_mbedtls_v4_psa_lms()` in `sim/mcuboot-sys/build.rs` —
  there is no separate `mbedtls-v4` Cargo feature to enable.
- An `lms_compat` integration test cross-checks wire-format
  compatibility between imgtool's pyhsslms wrapper and the
  `lms-signature` crate, in both directions (Python signs / Rust
  verifies, and vice versa).

### H10 exhaustion handling

The full sim test matrix easily exceeds the 1024-signature cap of a
single H10 key. The simulator side-steps this by saving the 16-byte
key identifier and 32-byte seed alongside the signing key and
regenerating from `(id, seed)` whenever the key exhausts. RFC 8554's
public-key derivation is deterministic in `(id, seed)`, so the
regenerated key produces the same Merkle root and previously-signed
images still verify against the same `bootutil_keys[]` entry.

This deliberately re-uses LM-OTS leaves across the regen — a real
security violation in production. It is acceptable in the sim
because the images never leave the test harness and the goal is
wire-format round-trip, not forgery resistance. The mechanism is
loudly documented at the singleton's definition in
`sim/src/tlv.rs`.

### Process-wide singleton

LMS private keys are stateful, so a fresh keypair is generated once
per process and shared across all tests. The 56-byte public key is
pushed into a writable buffer in `keys.c` via the
`mcuboot_sim_set_lms_pubkey` FFI hook, and `bootutil_keys[].key`
points into that buffer — the bootloader-side verifier sees the
same key the simulator just signed with.

### Threading

Mbed TLS's PSA core is only thread-safe with `MBEDTLS_THREADING_C`
enabled, which our v4 sim configs do not set; the LMS verifier sits
on top of `psa_hash_*` and inherits the constraint. The simulator's
external-RNG stub also uses libc's non-thread-safe `rand()`. CI
runs sig-lms test combinations with `--test-threads=1` — see
`ci/sim_run.sh`.

## Operational considerations

For anyone deploying LMS in a real signing pipeline:

- **Never re-sign from a stale backup of the LMS private-key file.**
  Reusing LM-OTS leaves leaks enough hash-chain values for an
  adversary to forge — restoring a backup *after* signing has
  already advanced `q` will leak the key.
- **Treat the key file as a source of truth.** imgtool's atomic
  rewrite (tmpfile + fsync + rename) is necessary but not sufficient:
  if a crash happens after the rewrite but before the caller
  persists the signed image, an LMS index is wasted (never reused),
  which is the safe outcome. Crashes that leave the key in any
  other state must not silently roll the file back.
- **Plan for key exhaustion.** H10 caps a key at 1024 signatures.
  This is plenty for a single product line over many releases, but
  the signing infrastructure should track usage and rotate the
  signing key well before exhaustion, including the verifier-side
  key rollover required to accept a new public key.
- **A signing-policy document is likely warranted** before LMS is
  used to sign release images. The single biggest operational risk
  is the stateful-private-key hazard above; it is not mitigated by
  the bootloader, only by procedure.

## TODO

The remaining work to reach a complete, mergeable LMS feature:

- **Zephyr Kconfig + sample.** Add `BOOT_SIGNATURE_TYPE_LMS` to
  `boot/zephyr/Kconfig`, wire it through `mcuboot_config.h` and
  `keys.c`, add `image_lms.c` to `boot/zephyr/CMakeLists.txt`, and
  add a build-only sample at `boot/zephyr/sample.yaml` so the
  bootloader is exercised in Zephyr's CI under LMS.
- **Generate the LMS test key at build time, not commit it.**
  Stateful keys cannot be safely committed (each test run would
  re-use indices). The Zephyr sample should run `imgtool keygen` as
  a CMake step. This is a natural pilot for the broader goal of
  removing checked-in test keys for *all* signature algorithms — the
  build-time generation pattern designed for LMS should generalize
  to RSA / ECDSA / Ed25519.
- **Move to the PSA API when upstream lands it.** When TF-PSA-Crypto
  exposes LMS through `psa_verify_hash` (or whatever interface the
  ongoing API design produces), `image_lms.c` should be ported off
  `mbedtls_lms_*` — the mbedtls-side APIs are public today only as
  a transitional measure for mechanisms with no PSA equivalent.

## Notes for future work

- LMS is verification-only in the bootloader. Signing remains in
  imgtool. Moving signing into the bootloader is not currently
  envisioned and would require either deep operational care or
  hardware-rooted state.
- ML-DSA (Dilithium) source is already sitting in the Mbed TLS 4.1
  submodule at `tf-psa-crypto/drivers/pqcp/mldsa-native/`. With the
  CMake-driven 4.x build path in place for LMS, adding a second PQC
  algorithm is mostly a matter of extending the config header and
  wiring the TLV / bootloader plumbing.
