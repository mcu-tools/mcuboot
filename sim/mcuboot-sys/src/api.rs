//! HAL api for MyNewt applications

use simflash::{Result, Flash, FlashPtr};
use lazy_static::lazy_static;
use libc;
use log::{Level, log_enabled, warn};
use std::{
    collections::HashMap,
    mem,
    ops::Deref,
    slice,
    sync::Mutex,
};

/// A FlashMap maintain a table of [device_id -> Flash trait]
pub type FlashMap = HashMap<u8, FlashPtr>;

lazy_static! {
    static ref FLASH: Mutex<FlashMap> = {
        Mutex::new(HashMap::new())
    };
}

struct FlashParams {
    align: u8,
    erased_val: u8,
}

lazy_static! {
    static ref FLASH_PARAMS: Mutex<HashMap<u8, FlashParams>> = {
        Mutex::new(HashMap::new())
    };
}

// Set the flash device to be used by the simulation.  The pointer is unsafely stashed away.
pub unsafe fn set_flash(dev_id: u8, dev: &mut dyn Flash) {
    let mut flash_params = FLASH_PARAMS.lock().unwrap();
    flash_params.insert(dev_id, FlashParams {
        align: dev.align() as u8,
        erased_val: dev.erased_val(),
    });

    let dev: &'static mut dyn Flash = mem::transmute(dev);
    let mut flash = FLASH.lock().unwrap();
    flash.insert(dev_id, FlashPtr{ptr: dev as *mut dyn Flash});
}

pub unsafe fn clear_flash(dev_id: u8) {
    let mut flash = FLASH.lock().unwrap();
    flash.remove(&dev_id);
}

// This isn't meant to call directly, but by a wrapper.

#[no_mangle]
pub extern fn sim_flash_erase(dev_id: u8, offset: u32, size: u32) -> libc::c_int {
    if let Ok(guard) = FLASH.lock() {
        if let Some(flash) = guard.deref().get(&dev_id) {
            let dev = unsafe { &mut *(flash.ptr) };
            return map_err(dev.erase(offset as usize, size as usize));
        }
    }
    -19
}

#[no_mangle]
pub extern fn sim_flash_read(dev_id: u8, offset: u32, dest: *mut u8, size: u32) -> libc::c_int {
    if let Ok(guard) = FLASH.lock() {
        if let Some(flash) = guard.deref().get(&dev_id) {
            let mut buf: &mut[u8] = unsafe { slice::from_raw_parts_mut(dest, size as usize) };
            let dev = unsafe { &mut *(flash.ptr) };
            return map_err(dev.read(offset as usize, &mut buf));
        }
    }
    -19
}

#[no_mangle]
pub extern fn sim_flash_write(dev_id: u8, offset: u32, src: *const u8, size: u32) -> libc::c_int {
    if let Ok(guard) = FLASH.lock() {
        if let Some(flash) = guard.deref().get(&dev_id) {
            let buf: &[u8] = unsafe { slice::from_raw_parts(src, size as usize) };
            let dev = unsafe { &mut *(flash.ptr) };
            return map_err(dev.write(offset as usize, &buf));
        }
    }
    -19
}

#[no_mangle]
pub extern fn sim_flash_align(id: u8) -> u8 {
    let flash_params = FLASH_PARAMS.lock().unwrap();
    let params = flash_params.get(&id).unwrap();
    params.align
}

#[no_mangle]
pub extern fn sim_flash_erased_val(id: u8) -> u8 {
    let flash_params = FLASH_PARAMS.lock().unwrap();
    let params = flash_params.get(&id).unwrap();
    params.erased_val
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
        1 => log_enabled!(Level::Error),
        2 => log_enabled!(Level::Warn),
        3 => log_enabled!(Level::Info),
        4 => log_enabled!(Level::Trace),
        _ => false,
    };
    if res {
        1
    } else {
        0
    }
}
