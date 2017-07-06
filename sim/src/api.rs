//! HAL api for MyNewt applications

use simflash::{Result, Flash};
use libc;
use log::LogLevel;
use mem;
use std::slice;

// The current active flash device.  The 'static is a lie, and we manage the lifetime ourselves.
static mut FLASH: Option<*mut Flash> = None;

// Set the flash device to be used by the simulation.  The pointer is unsafely stashed away.
pub unsafe fn set_flash(dev: &mut Flash) {
    let dev: &'static mut Flash = mem::transmute(dev);
    FLASH = Some(dev as *mut Flash);
}

pub unsafe fn clear_flash() {
    FLASH = None;
}

// Retrieve the flash, returning an error from the enclosing function.  We can't panic here because
// we've called through C and unwinding is prohibited (it seems to just exit the program).
macro_rules! get_flash {
    () => {
        match FLASH {
            Some(x) => &mut *x,
            None => return -19,
        }
    }
}

// This isn't meant to call directly, but by a wrapper.

#[no_mangle]
pub extern fn sim_flash_erase(offset: u32, size: u32) -> libc::c_int {
    let dev = unsafe { get_flash!() };
    map_err(dev.erase(offset as usize, size as usize))
}

#[no_mangle]
pub extern fn sim_flash_read(offset: u32, dest: *mut u8, size: u32) -> libc::c_int {
    let dev = unsafe { get_flash!() };
    let mut buf: &mut[u8] = unsafe { slice::from_raw_parts_mut(dest, size as usize) };
    map_err(dev.read(offset as usize, &mut buf))
}

#[no_mangle]
pub extern fn sim_flash_write(offset: u32, src: *const u8, size: u32) -> libc::c_int {
    let dev = unsafe { get_flash!() };
    let buf: &[u8] = unsafe { slice::from_raw_parts(src, size as usize) };
    map_err(dev.write(offset as usize, &buf))
}

fn map_err(err: Result<()>) -> libc::c_int {
    match err {
        Ok(()) => 0,
        Err(e) => {
            warn!("{}", e);
            -1
        },
    }
}

/// Called by C code to determine if we should log at this level.  Levels are defined in
/// bootutil/bootutil_log.h.  This makes the logging from the C code controlled by bootsim::api, so
/// for example, it can be enabled with something like:
///     RUST_LOG=bootsim::api=info cargo run --release runall
/// or
///     RUST_LOG=bootsim=info cargo run --release runall
#[no_mangle]
pub extern fn sim_log_enabled(level: libc::c_int) -> libc::c_int {
    let res = match level {
        1 => log_enabled!(LogLevel::Error),
        2 => log_enabled!(LogLevel::Warn),
        3 => log_enabled!(LogLevel::Info),
        4 => log_enabled!(LogLevel::Trace),
        _ => false,
    };
    if res {
        1
    } else {
        0
    }
}
