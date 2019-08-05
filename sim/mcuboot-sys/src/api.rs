//! HAL api for MyNewt applications

use crate::area::CAreaDesc;
use libc;
use log::{Level, log_enabled, warn};
use simflash::{Result, Flash, FlashPtr};
use std::{
    cell::RefCell,
    collections::HashMap,
    mem,
    ptr,
    slice,
};

/// A FlashMap maintain a table of [device_id -> Flash trait]
pub type FlashMap = HashMap<u8, FlashPtr>;

pub struct FlashParamsStruct {
    align: u8,
    erased_val: u8,
}

pub type FlashParams = HashMap<u8, FlashParamsStruct>;

pub struct CAreaDescPtr {
   pub ptr: *const CAreaDesc,
}

pub struct FlashContext {
    flash_map: FlashMap,
    flash_params: FlashParams,
    flash_areas: CAreaDescPtr,
}

impl FlashContext {
    pub fn new() -> FlashContext {
        FlashContext {
            flash_map: HashMap::new(),
            flash_params: HashMap::new(),
            flash_areas: CAreaDescPtr{ptr: ptr::null()},
        }
    }
}

#[repr(C)]
#[derive(Debug, Default)]
pub struct CSimContext {
    pub flash_counter: libc::c_int,
    pub jumped: libc::c_int,
    pub c_asserts: u8,
    pub c_catch_asserts: u8,
    // NOTE: Always leave boot_jmpbuf declaration at the end; this should
    // store a "jmp_buf" which is arch specific and not defined by libc crate.
    // The size below is enough to store data on a x86_64 machine.
    pub boot_jmpbuf: [u64; 16],
}

pub struct CSimContextPtr {
   pub ptr: *const CSimContext,
}

impl CSimContextPtr {
    pub fn new() -> CSimContextPtr {
        CSimContextPtr {
            ptr: ptr::null(),
        }
    }
}

thread_local! {
    pub static THREAD_CTX: RefCell<FlashContext> = RefCell::new(FlashContext::new());
    pub static SIM_CTX: RefCell<CSimContextPtr> = RefCell::new(CSimContextPtr::new());
}

// Set the flash device to be used by the simulation.  The pointer is unsafely stashed away.
pub unsafe fn set_flash(dev_id: u8, dev: &mut dyn Flash) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_params.insert(dev_id, FlashParamsStruct {
            align: dev.align() as u8,
            erased_val: dev.erased_val(),
        });
        let dev: &'static mut dyn Flash = mem::transmute(dev);
        ctx.borrow_mut().flash_map.insert(
            dev_id, FlashPtr{ptr: dev as *mut dyn Flash});
    });
}

pub unsafe fn clear_flash(dev_id: u8) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_map.remove(&dev_id);
    });
}

// This isn't meant to call directly, but by a wrapper.

#[no_mangle]
pub extern fn sim_get_flash_areas() -> *const CAreaDesc {
    THREAD_CTX.with(|ctx| {
        ctx.borrow().flash_areas.ptr
    })
}

#[no_mangle]
pub extern fn sim_set_flash_areas(areas: *const CAreaDesc) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_areas.ptr = areas;
    });
}

#[no_mangle]
pub extern fn sim_reset_flash_areas() {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_areas.ptr = ptr::null();
    });
}

#[no_mangle]
pub extern fn sim_get_context() -> *const CSimContext {
    SIM_CTX.with(|ctx| {
        ctx.borrow().ptr
    })
}

#[no_mangle]
pub extern fn sim_set_context(ptr: *const CSimContext) {
    SIM_CTX.with(|ctx| {
        ctx.borrow_mut().ptr = ptr;
    });
}

#[no_mangle]
pub extern fn sim_reset_context() {
    SIM_CTX.with(|ctx| {
        ctx.borrow_mut().ptr = ptr::null();
    });
}

#[no_mangle]
pub extern fn sim_flash_erase(dev_id: u8, offset: u32, size: u32) -> libc::c_int {
    let mut rc: libc::c_int = -19;
    THREAD_CTX.with(|ctx| {
        if let Some(flash) = ctx.borrow().flash_map.get(&dev_id) {
            let dev = unsafe { &mut *(flash.ptr) };
            rc = map_err(dev.erase(offset as usize, size as usize));
        }
    });
    rc
}

#[no_mangle]
pub extern fn sim_flash_read(dev_id: u8, offset: u32, dest: *mut u8, size: u32) -> libc::c_int {
    let mut rc: libc::c_int = -19;
    THREAD_CTX.with(|ctx| {
        if let Some(flash) = ctx.borrow().flash_map.get(&dev_id) {
            let mut buf: &mut[u8] = unsafe { slice::from_raw_parts_mut(dest, size as usize) };
            let dev = unsafe { &mut *(flash.ptr) };
            rc = map_err(dev.read(offset as usize, &mut buf));
        }
    });
    rc
}

#[no_mangle]
pub extern fn sim_flash_write(dev_id: u8, offset: u32, src: *const u8, size: u32) -> libc::c_int {
    let mut rc: libc::c_int = -19;
    THREAD_CTX.with(|ctx| {
        if let Some(flash) = ctx.borrow().flash_map.get(&dev_id) {
            let buf: &[u8] = unsafe { slice::from_raw_parts(src, size as usize) };
            let dev = unsafe { &mut *(flash.ptr) };
            rc = map_err(dev.write(offset as usize, &buf));
        }
    });
    rc
}

#[no_mangle]
pub extern fn sim_flash_align(id: u8) -> u8 {
    THREAD_CTX.with(|ctx| {
        ctx.borrow().flash_params.get(&id).unwrap().align
    })
}

#[no_mangle]
pub extern fn sim_flash_erased_val(id: u8) -> u8 {
    THREAD_CTX.with(|ctx| {
        ctx.borrow().flash_params.get(&id).unwrap().erased_val
    })
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
