// Copyright (c) 2017-2019 Linaro LTD
// Copyright (c) 2018-2019 JUUL Labs
// Copyright (c) 2019 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! Describe flash areas.

use simflash::{Flash, SimFlash, Sector};
use std::ptr;
use std::collections::HashMap;

/// Structure to build up the boot area table.
#[derive(Debug, Default, Clone)]
pub struct AreaDesc {
    areas: Vec<Vec<FlashArea>>,
    whole: Vec<FlashArea>,
    sectors: HashMap<u8, Vec<Sector>>,
}

impl AreaDesc {
    pub fn new() -> AreaDesc {
        AreaDesc {
            areas: vec![],
            whole: vec![],
            sectors: HashMap::new(),
        }
    }

    pub fn add_flash_sectors(&mut self, id: u8, flash: &SimFlash) {
        self.sectors.insert(id, flash.sector_iter().collect());
    }

    /// Add a slot to the image.  The slot must align with erasable units in the flash device.
    /// Panics if the description is not valid.  There are also bootloader assumptions that the
    /// slots are PRIMARY_SLOT, SECONDARY_SLOT, and SCRATCH in that order.
    pub fn add_image(&mut self, base: usize, len: usize, id: FlashId, dev_id: u8) {
        let nid = id as usize;
        let orig_base = base;
        let orig_len = len;
        let mut base = base;
        let mut len = len;

        while nid > self.areas.len() {
            self.areas.push(vec![]);
            self.whole.push(Default::default());
        }

        if nid != self.areas.len() {
            panic!("Flash areas not added in order");
        }

        let mut area = vec![];

        for sector in &self.sectors[&dev_id] {
            if len == 0 {
                break;
            };
            if base > sector.base + sector.size - 1 {
                continue;
            }
            if sector.base != base {
                panic!("Image does not start on a sector boundary");
            }

            area.push(FlashArea {
                flash_id: id,
                device_id: dev_id,
                pad16: 0,
                off: sector.base as u32,
                size: sector.size as u32,
            });

            base += sector.size;
            len -= sector.size;
        }

        if len != 0 {
            panic!("Image goes past end of device");
        }

        self.areas.push(area);
        self.whole.push(FlashArea {
            flash_id: id,
            device_id: dev_id,
            pad16: 0,
            off: orig_base as u32,
            size: orig_len as u32,
        });
    }

    // Add a simple slot to the image.  This ignores the device layout, and just adds the area as a
    // single unit.  It assumes that the image lines up with image boundaries.  This tests
    // configurations where the partition table uses larger sectors than the underlying flash
    // device.
    pub fn add_simple_image(&mut self, base: usize, len: usize, id: FlashId, dev_id: u8) {
        let area = vec![FlashArea {
            flash_id: id,
            device_id: dev_id,
            pad16: 0,
            off: base as u32,
            size: len as u32,
        }];

        self.areas.push(area);
        self.whole.push(FlashArea {
            flash_id: id,
            device_id: dev_id,
            pad16: 0,
            off: base as u32,
            size: len as u32,
        });
    }

    // Look for the image with the given ID, and return its offset, size and
    // device id. Returns None if the area is not present.
    pub fn find(&self, id: FlashId) -> Option<(usize, usize, u8)> {
        for area in &self.whole {
            // FIXME: should we ensure id is not duplicated over multiple devices?
            if area.flash_id == id {
                return Some((area.off as usize, area.size as usize, area.device_id));
            }
        }
        None
    }

    pub fn get_c(&self) -> CAreaDesc {
        let mut areas: CAreaDesc = Default::default();

        assert_eq!(self.areas.len(), self.whole.len());

        for (i, area) in self.areas.iter().enumerate() {
            if area.len() > 0 {
                areas.slots[i].areas = &area[0];
                areas.slots[i].whole = self.whole[i].clone();
                areas.slots[i].num_areas = area.len() as u32;
                areas.slots[i].id = area[0].flash_id;
            }
        }

        areas.num_slots = self.areas.len() as u32;

        areas
    }

    /// Return an iterator over all `FlashArea`s present.
    pub fn iter_areas(&self) -> impl Iterator<Item = &FlashArea> {
        self.whole.iter()
    }
}

/// The area descriptor, C format.
#[repr(C)]
#[derive(Debug, Default)]
pub struct CAreaDesc {
    slots: [CArea; 16],
    num_slots: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct CArea {
    whole: FlashArea,
    areas: *const FlashArea,
    num_areas: u32,
    // FIXME: is this not already available on whole/areas?
    id: FlashId,
}

impl Default for CArea {
    fn default() -> CArea {
        CArea {
            areas: ptr::null(),
            whole: Default::default(),
            id: FlashId::BootLoader,
            num_areas: 0,
        }
    }
}

/// Flash area map.
#[repr(u8)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[allow(dead_code)]
pub enum FlashId {
    BootLoader = 0,
    Image0 = 1,
    Image1 = 2,
    ImageScratch = 3,
    Image2 = 4,
    Image3 = 5,
}

impl Default for FlashId {
    fn default() -> FlashId {
        FlashId::BootLoader
    }
}

#[repr(C)]
#[derive(Debug, Clone, Default)]
pub struct FlashArea {
    pub flash_id: FlashId,
    pub device_id: u8,
    pad16: u16,
    pub off: u32,
    pub size: u32,
}
