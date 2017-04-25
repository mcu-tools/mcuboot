//! HAL api for MyNewt applications

use flash::{Result, Flash};
use libc;
use std::slice;

// This isn't meant to call directly, but by a wrapper.

#[no_mangle]
pub extern fn sim_flash_erase(dev: *mut Flash, offset: u32, size: u32) -> libc::c_int {
    let mut dev: &mut Flash = unsafe { &mut *dev };
    map_err(dev.erase(offset as usize, size as usize))
}

#[no_mangle]
pub extern fn sim_flash_read(dev: *const Flash, offset: u32, dest: *mut u8, size: u32) -> libc::c_int {
    let dev: &Flash = unsafe { &*dev };
    let mut buf: &mut[u8] = unsafe { slice::from_raw_parts_mut(dest, size as usize) };
    map_err(dev.read(offset as usize, &mut buf))
}

#[no_mangle]
pub extern fn sim_flash_write(dev: *mut Flash, offset: u32, src: *const u8, size: u32) -> libc::c_int {
    let mut dev: &mut Flash = unsafe { &mut *dev };
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
