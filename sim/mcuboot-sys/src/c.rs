// Copyright (c) 2017-2021 Linaro LTD
// Copyright (c) 2017-2019 JUUL Labs
// Copyright (c) 2019-2023 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! Interface wrappers to C API entering to the bootloader

use crate::area::AreaDesc;
use simflash::SimMultiFlash;
use crate::api;

#[allow(unused)]
use std::sync::Once;

/// The result of an invocation of `boot_go`.  This is intentionally opaque so that we can provide
/// accessors for everything we need from this.
#[derive(Debug)]
pub enum BootGoResult {
    /// This run was stopped by the flash simulation mechanism.
    Stopped,
    /// The bootloader ran to completion with the following data.
    Normal {
        result: i32,
        asserts: u8,

        resp: api::BootRsp,
    },
}

impl BootGoResult {
    /// Was this run interrupted.
    pub fn interrupted(&self) -> bool {
        matches!(self, BootGoResult::Stopped)
    }

    /// Was this boot run successful (returned 0)
    pub fn success(&self) -> bool {
        matches!(self, BootGoResult::Normal { result: 0, .. })
    }

    /// Success, but also no asserts.
    pub fn success_no_asserts(&self) -> bool {
        matches!(self, BootGoResult::Normal {
            result: 0,
            asserts: 0,
            ..
        })
    }

    /// Get the asserts count.  An interrupted run will be considered to have no asserts.
    pub fn asserts(&self) -> u8 {
        match self {
            BootGoResult::Normal { asserts, .. } => *asserts,
            _ => 0,
        }
    }

    /// Retrieve the 'resp' field that is filled in.
    pub fn resp(&self) -> Option<&api::BootRsp> {
        match self {
            BootGoResult::Normal { resp, .. } => Some(resp),
            _ => None,
        }
    }
}

/// Invoke the bootloader on this flash device.
pub fn boot_go(multiflash: &mut SimMultiFlash, areadesc: &AreaDesc,
               counter: Option<&mut i32>, image_index: Option<i32>,
               catch_asserts: bool) -> BootGoResult {
    init_crypto();

    for (&dev_id, flash) in multiflash.iter_mut() {
        api::set_flash(dev_id, flash);
    }
    let mut sim_ctx = api::CSimContext {
        flash_counter: match counter {
            None => 0,
            Some(ref c) => **c as libc::c_int
        },
        jumped: 0,
        c_asserts: 0,
        c_catch_asserts: if catch_asserts { 1 } else { 0 },
        boot_jmpbuf: [0; 16],
    };
    let mut rsp = api::BootRsp {
        br_hdr: std::ptr::null(),
        flash_dev_id: 0,
        image_off: 0,
    };
    let result: i32 = unsafe {
        match image_index {
            None => raw::invoke_boot_go(&mut sim_ctx as *mut _,
                                        &areadesc.get_c() as *const _,
                                        &mut rsp as *mut _, -1) as i32,
            Some(i) => raw::invoke_boot_go(&mut sim_ctx as *mut _,
                                           &areadesc.get_c() as *const _,
                                           &mut rsp as *mut _,
                                           i as i32) as i32
        }
    };
    let asserts = sim_ctx.c_asserts;
    if let Some(c) = counter {
        *c = sim_ctx.flash_counter;
    }
    for &dev_id in multiflash.keys() {
        api::clear_flash(dev_id);
    }
    if result == -0x13579 {
        BootGoResult::Stopped
    } else {
        BootGoResult::Normal { result, asserts, resp: rsp }
    }
}

pub fn boot_trailer_sz(align: u32) -> u32 {
    unsafe { raw::boot_trailer_sz(align) }
}

pub fn boot_status_sz(align: u32) -> u32 {
    unsafe { raw::boot_status_sz(align) }
}

pub fn boot_magic_sz() -> usize {
    unsafe { raw::boot_magic_sz() as usize }
}

pub fn boot_max_align() -> usize {
    unsafe { raw::boot_max_align() as usize }
}

pub fn rsa_oaep_encrypt(pubkey: &[u8], seckey: &[u8]) -> Result<[u8; 256], &'static str> {
    unsafe {
        let mut encbuf: [u8; 256] = [0; 256];
        if raw::rsa_oaep_encrypt_(pubkey.as_ptr(), pubkey.len() as u32,
                                  seckey.as_ptr(), seckey.len() as u32,
                                  encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        Err("Failed to encrypt buffer")
    }
}

pub fn kw_encrypt(kek: &[u8], seckey: &[u8], keylen: u32) -> Result<Vec<u8>, &'static str> {
    unsafe {
        let mut encbuf = vec![0u8; 24];
        if keylen == 32 {
            encbuf = vec![0u8; 40];
        }
        if raw::kw_encrypt_(kek.as_ptr(), seckey.as_ptr(), encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        Err("Failed to encrypt buffer")
    }
}

pub fn set_security_counter(image_index: u32, security_counter_value: u32) {
    api::sim_set_nv_counter_for_image(image_index, security_counter_value);
}

pub fn get_security_counter(image_index: u32) -> u32 {
    let mut counter_val: u32 = 0;
    api::sim_get_nv_counter_for_image(image_index, &mut counter_val as *mut u32);
    return counter_val;
}

mod raw {
    use crate::area::CAreaDesc;
    use crate::api::{BootRsp, CSimContext};

    extern "C" {
        // This generates a warning about `CAreaDesc` not being foreign safe.  There doesn't appear to
        // be any way to get rid of this warning.  See https://github.com/rust-lang/rust/issues/34798
        // for information and tracking.
        pub fn invoke_boot_go(sim_ctx: *mut CSimContext, areadesc: *const CAreaDesc,
            rsp: *mut BootRsp, image_index: libc::c_int) -> libc::c_int;

        pub fn boot_trailer_sz(min_write_sz: u32) -> u32;
        pub fn boot_status_sz(min_write_sz: u32) -> u32;

        pub fn boot_magic_sz() -> u32;
        pub fn boot_max_align() -> u32;

        pub fn rsa_oaep_encrypt_(pubkey: *const u8, pubkey_len: libc::c_uint,
                                 seckey: *const u8, seckey_len: libc::c_uint,
                                 encbuf: *mut u8) -> libc::c_int;

        pub fn kw_encrypt_(kek: *const u8, seckey: *const u8,
                           encbuf: *mut u8) -> libc::c_int;

        #[allow(unused)]
        pub fn psa_crypto_init() -> u32;

        #[allow(unused)]
        pub fn mbedtls_test_enable_insecure_external_rng();
    }
}

#[allow(unused)]
static PSA_INIT_SYNC: Once = Once::new();

#[allow(unused)]
static MBEDTLS_EXTERNAL_RNG_ENABLE_SYNC: Once = Once::new();

#[cfg(feature = "psa-crypto-api")]
fn init_crypto() {
    PSA_INIT_SYNC.call_once(|| {
        assert_eq!(unsafe { raw::psa_crypto_init() }, 0);
    });

    /* The PSA APIs require properly initialisation of the entropy subsystem
     * The configuration adds the option MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG when the
     * psa-crypto-api feature is enabled. As a result the tests use the implementation
     * of the test external rng that needs to be initialised before being able to use it
     */
    MBEDTLS_EXTERNAL_RNG_ENABLE_SYNC.call_once(|| {
        unsafe { raw::mbedtls_test_enable_insecure_external_rng() }
    });
}

#[cfg(not(feature = "psa-crypto-api"))]
fn init_crypto() {
   // When the feature is not enabled, the init is just empty
}
