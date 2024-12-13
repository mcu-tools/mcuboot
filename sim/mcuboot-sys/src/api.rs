// Copyright (c) 2017-2021 Linaro LTD
// Copyright (c) 2018-2019 JUUL Labs
// Copyright (c) 2023 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! HAL api for MyNewt applications

use crate::area::CAreaDesc;
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
    align: u32,
    erased_val: u8,
}

pub type FlashParams = HashMap<u8, FlashParamsStruct>;

/// The `boot_rsp` structure used by boot_go.
#[repr(C)]
#[derive(Debug)]
pub struct BootRsp {
    pub br_hdr: *const ImageHeader,
    pub flash_dev_id: u8,
    pub image_off: u32,
}

// TODO: Don't duplicate this image header declaration.
#[repr(C)]
#[derive(Debug)]
pub struct ImageHeader {
    magic: u32,
    load_addr: u32,
    hdr_size: u16,
    protect_tlv_size: u16,
    img_size: u32,
    flags: u32,
    ver: ImageVersion,
    _pad2: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct ImageVersion {
    pub major: u8,
    pub minor: u8,
    pub revision: u16,
    pub build_num: u32,
}

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

impl Default for FlashContext {
    fn default() -> FlashContext {
        FlashContext {
            flash_map: HashMap::new(),
            flash_params: HashMap::new(),
            flash_areas: CAreaDescPtr{ptr: ptr::null()},
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct CSimContext {
    pub flash_counter: libc::c_int,
    pub jumped: libc::c_int,
    pub c_asserts: u8,
    pub c_catch_asserts: u8,
    // NOTE: Always leave boot_jmpbuf declaration at the end; this should
    // store a "jmp_buf" which is arch specific and not defined by libc crate.
    // The size below is enough to store data on a x86_64 machine.
    pub boot_jmpbuf: [u64; 48],
}

impl Default for CSimContext {
    fn default() -> Self {
        CSimContext {
            flash_counter: 0,
            jumped: 0,
            c_asserts: 0,
            c_catch_asserts: 0,
            boot_jmpbuf: [0; 48],
        }
    }
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

impl Default for CSimContextPtr {
    fn default() -> CSimContextPtr {
        CSimContextPtr {
            ptr: ptr::null(),
        }
    }
}

/// This struct describes the RAM layout of the current device.  It will be stashed, per test
/// thread, and queried by the C code.
#[repr(C)]
#[derive(Debug, Default)]
pub struct BootsimRamInfo {
    pub start: u32,
    pub size: u32,
    pub base: usize,
}

/// This struct stores the non-volatile security counter per image. It will be stored per test thread,
/// and the C code will set / get the values here.
#[repr(C)]
#[derive(Debug, Default)]
pub struct NvCounterStorage {
    pub storage: Vec<u32>,
}

impl NvCounterStorage {
    pub fn new() -> Self {
        let count = if cfg!(feature = "multiimage") {
            2
        } else {
            1
        };
        Self {
            storage: vec![0; count]
        }
    }
}

thread_local! {
    pub static THREAD_CTX: RefCell<FlashContext> = RefCell::new(FlashContext::new());
    pub static SIM_CTX: RefCell<CSimContextPtr> = RefCell::new(CSimContextPtr::new());
    pub static RAM_CTX: RefCell<BootsimRamInfo> = RefCell::new(BootsimRamInfo::default());
    pub static NV_COUNTER_CTX: RefCell<NvCounterStorage> = RefCell::new(NvCounterStorage::new());
}

/// Set the flash device to be used by the simulation.  The pointer is unsafely stashed away.
///
/// # Safety
///
/// This uses mem::transmute to stash a Rust pointer into a C value to
/// retrieve later.  It should be safe to use this.
pub fn set_flash(dev_id: u8, dev: &mut dyn Flash) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_params.insert(dev_id, FlashParamsStruct {
            align: dev.align() as u32,
            erased_val: dev.erased_val(),
        });
        unsafe {
            let dev: &'static mut dyn Flash = mem::transmute(dev);
            ctx.borrow_mut().flash_map.insert(
                dev_id, FlashPtr{ptr: dev as *mut dyn Flash});
        }
    });
}

pub fn clear_flash(dev_id: u8) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_map.remove(&dev_id);
    });
}

// This isn't meant to call directly, but by a wrapper.

#[no_mangle]
pub extern "C" fn sim_get_flash_areas() -> *const CAreaDesc {
    THREAD_CTX.with(|ctx| {
        ctx.borrow().flash_areas.ptr
    })
}

#[no_mangle]
pub extern "C" fn sim_set_flash_areas(areas: *const CAreaDesc) {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_areas.ptr = areas;
    });
}

#[no_mangle]
pub extern "C" fn sim_reset_flash_areas() {
    THREAD_CTX.with(|ctx| {
        ctx.borrow_mut().flash_areas.ptr = ptr::null();
    });
}

#[no_mangle]
pub extern "C" fn sim_get_context() -> *const CSimContext {
    SIM_CTX.with(|ctx| {
        ctx.borrow().ptr
    })
}

#[no_mangle]
pub extern "C" fn sim_set_context(ptr: *const CSimContext) {
    SIM_CTX.with(|ctx| {
        ctx.borrow_mut().ptr = ptr;
    });
}

#[no_mangle]
pub extern "C" fn sim_reset_context() {
    SIM_CTX.with(|ctx| {
        ctx.borrow_mut().ptr = ptr::null();
    });
}

#[no_mangle]
pub extern "C" fn bootsim_get_ram_info() -> *const BootsimRamInfo {
    RAM_CTX.with(|ctx| {
        if ctx.borrow().base == 0 {
            // Option is messier to get a pointer out of, so just check if the base has been set to
            // anything.
            panic!("ram info not set, but being used");
        }
        ctx.as_ptr()
    })
}

/// Store a copy of this RAM info.
pub fn set_ram_info(info: BootsimRamInfo) {
    RAM_CTX.with(|ctx| {
        ctx.replace(info);
    });
}

/// Clear out the ram info.
pub fn clear_ram_info() {
    RAM_CTX.with(|ctx| {
        ctx.borrow_mut().base = 0;
    });
}

#[no_mangle]
pub extern "C" fn sim_flash_erase(dev_id: u8, offset: u32, size: u32) -> libc::c_int {
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
pub extern "C" fn sim_flash_read(dev_id: u8, offset: u32, dest: *mut u8, size: u32) -> libc::c_int {
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
pub extern "C" fn sim_flash_write(dev_id: u8, offset: u32, src: *const u8, size: u32) -> libc::c_int {
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
pub extern "C" fn sim_flash_align(id: u8) -> u32 {
    THREAD_CTX.with(|ctx| {
        ctx.borrow().flash_params.get(&id).unwrap().align
    })
}

#[no_mangle]
pub extern "C" fn sim_flash_erased_val(id: u8) -> u8 {
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
pub extern "C" fn sim_log_enabled(level: libc::c_int) -> libc::c_int {
    let res = match level {
        1 => log_enabled!(Level::Error),
        2 => log_enabled!(Level::Warn),
        3 => log_enabled!(Level::Info),
        4 => log_enabled!(Level::Debug),
        5 => log_enabled!(Level::Trace), // log level == SIM
        _ => false,
    };
    if res {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn sim_set_nv_counter_for_image(image_index: u32, security_counter_value: u32) -> libc::c_int {
    let mut rc = 0;
    NV_COUNTER_CTX.with(|ctx| {
        let mut counter_storage = ctx.borrow_mut();
        if image_index as usize >= counter_storage.storage.len() {
            rc = -1;
            return;
        }
        if counter_storage.storage[image_index as usize] > security_counter_value {
            rc = -2;
            warn!("Failed to set security counter value ({}) for image index {}", security_counter_value, image_index);
            return;
        }

        counter_storage.storage[image_index as usize] = security_counter_value;
    });

    return rc;
}

#[no_mangle]
pub extern "C" fn sim_get_nv_counter_for_image(image_index: u32, security_counter_value: *mut u32) -> libc::c_int {
    let mut rc = 0;
    NV_COUNTER_CTX.with(|ctx| {
        let counter_storage = ctx.borrow();
        if image_index as usize >= counter_storage.storage.len() {
            rc = -1;
            return;
        }
        unsafe { *security_counter_value = counter_storage.storage[image_index as usize] };

    });
    return rc;
}
