// Copyright (c) 2017-2019 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

mod area;
pub mod c;

// The API needs to be public, even though it isn't intended to be called by Rust code, but the
// functions are exported to C code.
pub mod api;

pub use crate::area::{AreaDesc, FlashId};

/// For testing the ram load feature, we need to emulate a block of RAM and be able to pass that
/// down to the C code.  The call down to boot_go should go through this object so that the buffer
/// itself is managed properly.
pub struct RamBlock {
    ram: Vec<u8>,
    offset: u32, // 32-bit offset.
}

impl RamBlock {
    pub fn new(size: u32, offset: u32) -> RamBlock {
        RamBlock {
            ram: vec![0; size as usize],
            offset: offset,
        }
    }

    /// Borrow the RAM buffer, with 'offset' being the beginning of the buffer.
    pub fn borrow(&self) -> &[u8] {
        &self.ram
    }

    /// Borrow a piece of the ram, with 'offset' being the beginning of the buffer.
    pub fn borrow_part(&self, base: usize, size: usize) -> &[u8] {
        &self.ram[base..base+size]
    }

    pub fn invoke<F, R>(&self, act: F) -> R
        where F: FnOnce() -> R
    {
        api::set_ram_info(api::BootsimRamInfo {
            start: self.offset,
            size: self.ram.len() as u32,
            base: &self.ram[0] as *const u8 as usize - self.offset as usize,
        });
        let result = act();
        api::clear_ram_info();
        result
    }
}
