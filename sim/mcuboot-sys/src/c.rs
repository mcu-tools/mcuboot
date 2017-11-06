/// Interface wrappers to C API entering to the bootloader

use area::AreaDesc;
use simflash::Flash;
use libc;
use api;

/// Invoke the bootloader on this flash device.
pub fn boot_go(flash: &mut Flash, areadesc: &AreaDesc, counter: Option<&mut i32>, align: u8) -> i32 {
    unsafe {
        api::set_flash(flash);
        raw::sim_flash_align = align;
        raw::flash_counter = match counter {
            None => 0,
            Some(ref c) => **c as libc::c_int
        };
    }
    let result = unsafe { raw::invoke_boot_go(&areadesc.get_c() as *const _) as i32 };
    unsafe {
        counter.map(|c| *c = raw::flash_counter as i32);
        api::clear_flash();
    };
    result
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

mod raw {
    use area::CAreaDesc;
    use libc;

    extern "C" {
        // This generates a warning about `CAreaDesc` not being foreign safe.  There doesn't appear to
        // be any way to get rid of this warning.  See https://github.com/rust-lang/rust/issues/34798
        // for information and tracking.
        pub fn invoke_boot_go(areadesc: *const CAreaDesc) -> libc::c_int;
        pub static mut flash_counter: libc::c_int;

        pub static mut sim_flash_align: u8;
        pub fn boot_slots_trailer_sz(min_write_sz: u8) -> u32;

        pub static BOOT_MAGIC_SZ: u32;
        pub static BOOT_MAX_ALIGN: u32;
    }
}
