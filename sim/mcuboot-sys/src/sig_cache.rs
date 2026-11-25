// Copyright (c) 2026 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

//! Memo of verified signatures, for the simulator only.
//!
//! The power-fail tests boot the same few images many thousands of times, and every boot repeats
//! the same public-key signature checks, which dominate the run time with the slower algorithms
//! (ECDSA P-384 in particular).  `bootutil_img_validate()` asks this cache before calling
//! `bootutil_verify_sig()`, and records each signature that verifies.  The full check still runs
//! the first time a given (key, hash, signature) triple is seen, so every distinct input, and
//! every invalid one, goes through the real code.
//!
//! Only successful checks are recorded, and a hit requires the exact key, hash and signature
//! bytes, so a cached result can never accept anything that would not verify.  The cache is
//! shared by all test threads and lives only as long as the test process.
//!
//! Set `MCUBOOT_SIM_NO_SIG_CACHE` in the environment to disable it.

use std::{
    collections::HashSet,
    env,
    slice,
    sync::{LazyLock, Mutex, PoisonError},
};

/// A verified (key, hash, signature) triple.
type Triple = (Vec<u8>, Vec<u8>, Vec<u8>);

static VERIFIED: LazyLock<Mutex<HashSet<Triple>>> =
    LazyLock::new(|| Mutex::new(HashSet::new()));

static ENABLED: LazyLock<bool> =
    LazyLock::new(|| env::var_os("MCUBOOT_SIM_NO_SIG_CACHE").is_none());

/// Build an owned triple from the C buffers.
///
/// # Safety
///
/// Each pointer must be valid for reads of its length, or the length must be zero.
unsafe fn triple(key: *const u8, key_len: usize,
                 hash: *const u8, hash_len: usize,
                 sig: *const u8, sig_len: usize) -> Triple {
    unsafe fn bytes(p: *const u8, len: usize) -> Vec<u8> {
        if len == 0 {
            Vec::new()
        } else {
            slice::from_raw_parts(p, len).to_vec()
        }
    }

    (bytes(key, key_len), bytes(hash, hash_len), bytes(sig, sig_len))
}

#[no_mangle]
pub extern "C" fn sim_sig_cache_lookup(key: *const u8, key_len: usize,
                                       hash: *const u8, hash_len: usize,
                                       sig: *const u8, sig_len: usize) -> bool {
    if !*ENABLED {
        return false;
    }
    let entry = unsafe { triple(key, key_len, hash, hash_len, sig, sig_len) };
    VERIFIED.lock().unwrap_or_else(PoisonError::into_inner).contains(&entry)
}

#[no_mangle]
pub extern "C" fn sim_sig_cache_insert(key: *const u8, key_len: usize,
                                       hash: *const u8, hash_len: usize,
                                       sig: *const u8, sig_len: usize) {
    if !*ENABLED {
        return;
    }
    let entry = unsafe { triple(key, key_len, hash, hash_len, sig, sig_len) };
    VERIFIED.lock().unwrap_or_else(PoisonError::into_inner).insert(entry);
}
