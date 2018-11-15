/// Interface wrappers to C API entering to the bootloader

use area::AreaDesc;
use simflash::Flash;
use libc;
use api;
use std::sync::Mutex;

lazy_static! {
    /// Mutex to lock the simulation.  The C code for the bootloader uses
    /// global variables, and is therefore non-reentrant.
    static ref BOOT_LOCK: Mutex<()> = Mutex::new(());
}

/// Invoke the bootloader on this flash device.
pub fn boot_go(flash: &mut Flash, areadesc: &AreaDesc, counter: Option<&mut i32>,
               catch_asserts: bool) -> (i32, u8) {
    let _lock = BOOT_LOCK.lock().unwrap();

    unsafe {
        api::set_flash(0, flash);
        raw::c_catch_asserts = if catch_asserts { 1 } else { 0 };
        raw::c_asserts = 0u8;
        raw::flash_counter = match counter {
            None => 0,
            Some(ref c) => **c as libc::c_int
        };
    }
    let result = unsafe { raw::invoke_boot_go(&areadesc.get_c() as *const _) as i32 };
    let asserts = unsafe { raw::c_asserts };
    unsafe {
        counter.map(|c| *c = raw::flash_counter as i32);
        api::clear_flash(0);
    };
    (result, asserts)
}

pub fn boot_trailer_sz(align: u8) -> u32 {
    unsafe { raw::boot_slots_trailer_sz(align) }
}

pub fn boot_magic_sz() -> usize {
    unsafe { raw::BOOT_MAGIC_SZ as usize }
}

pub fn boot_max_align() -> usize {
    unsafe { raw::BOOT_MAX_ALIGN as usize }
}

pub fn ecdsa256_sign(privkey: &[u8], hash: &[u8]) -> Result<[u8; 64], &'static str> {
    unsafe {
        let mut signature: [u8; 64] = [0; 64];
        if raw::ecdsa256_sign_(privkey.as_ptr(), hash.as_ptr(),
                               hash.len() as u32, signature.as_mut_ptr()) == 1 {
            return Ok(signature);
        }
        return Err("Failed signature generation");
    }
}

pub fn rsa_oaep_encrypt(pubkey: &[u8], seckey: &[u8]) -> Result<[u8; 256], &'static str> {
    unsafe {
        let mut encbuf: [u8; 256] = [0; 256];
        if raw::rsa_oaep_encrypt_(pubkey.as_ptr(), pubkey.len() as u32,
                                  seckey.as_ptr(), seckey.len() as u32,
                                  encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        return Err("Failed to encrypt buffer");
    }
}

pub fn kw_encrypt(kek: &[u8], seckey: &[u8]) -> Result<[u8; 24], &'static str> {
    unsafe {
        let mut encbuf = [0u8; 24];
        if raw::kw_encrypt_(kek.as_ptr(), seckey.as_ptr(), encbuf.as_mut_ptr()) == 0 {
            return Ok(encbuf);
        }
        return Err("Failed to encrypt buffer");
    }
}

mod raw {
    use area::CAreaDesc;
    use libc;

    extern "C" {
        // This generates a warning about `CAreaDesc` not being foreign safe.  There doesn't appear to
        // be any way to get rid of this warning.  See https://github.com/rust-lang/rust/issues/34798
        // for information and tracking.
        pub fn invoke_boot_go(areadesc: *const CAreaDesc) -> libc::c_int;
        pub static mut flash_counter: libc::c_int;
        pub static mut c_asserts: u8;
        pub static mut c_catch_asserts: u8;

        pub fn boot_slots_trailer_sz(min_write_sz: u8) -> u32;

        pub static BOOT_MAGIC_SZ: u32;
        pub static BOOT_MAX_ALIGN: u32;

        pub fn ecdsa256_sign_(privkey: *const u8, hash: *const u8,
                              hash_len: libc::c_uint,
                              signature: *mut u8) -> libc::c_int;

        pub fn rsa_oaep_encrypt_(pubkey: *const u8, pubkey_len: libc::c_uint,
                                 seckey: *const u8, seckey_len: libc::c_uint,
                                 encbuf: *mut u8) -> libc::c_int;

        pub fn kw_encrypt_(kek: *const u8, seckey: *const u8,
                           encbuf: *mut u8) -> libc::c_int;
    }
}
