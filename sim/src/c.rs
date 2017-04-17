/// Interface wrappers to C API entering to the bootloader

use area::AreaDesc;
use flash::Flash;
use libc;

/// Invoke the bootloader on this flash device.
pub fn boot_go(flash: &mut Flash, areadesc: &AreaDesc) -> i32 {
    unsafe { raw::invoke_boot_go(flash as *mut _ as *mut libc::c_void,
                                 &areadesc.get_c() as *const _) as i32 }
}

/// Setter/getter for the flash counter.  This isn't thread safe.
pub fn get_flash_counter() -> i32 {
    unsafe { raw::flash_counter as i32 }
}

/// Set the flash counter.  Zero indicates the flash should not be interrupted.  The counter will
/// then go negative for each flash operation.
pub fn set_flash_counter(counter: i32) {
    unsafe { raw::flash_counter = counter as libc::c_int };
}

pub fn boot_trailer_sz() -> u32 {
    unsafe { raw::boot_trailer_sz(raw::sim_flash_align) }
}

pub fn get_sim_flash_align() -> u8 {
    unsafe { raw::sim_flash_align }
}

pub fn set_sim_flash_align(align: u8) {
    unsafe { raw::sim_flash_align = align };
}

pub fn boot_magic_sz() -> u32 {
    unsafe { raw::BOOT_MAGIC_SZ }
}

mod raw {
    use area::CAreaDesc;
    use libc;

    extern "C" {
        // This generates a warning about `CAreaDesc` not being foreign safe.  There doesn't appear to
        // be any way to get rid of this warning.  See https://github.com/rust-lang/rust/issues/34798
        // for information and tracking.
        pub fn invoke_boot_go(flash: *mut libc::c_void, areadesc: *const CAreaDesc) -> libc::c_int;
        pub static mut flash_counter: libc::c_int;

        pub static mut sim_flash_align: u8;
        pub fn boot_trailer_sz(min_write_sz: u8) -> u32;

        pub static mut BOOT_MAGIC_SZ: u32;
    }
}
