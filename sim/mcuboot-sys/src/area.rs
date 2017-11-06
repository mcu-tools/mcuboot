//! Describe flash areas.

use simflash::{Flash, SimFlash, Sector};
use std::ptr;

/// Structure to build up the boot area table.
#[derive(Debug, Clone)]
pub struct AreaDesc {
    areas: Vec<Vec<FlashArea>>,
    whole: Vec<FlashArea>,
    sectors: Vec<Sector>,
}

impl AreaDesc {
    pub fn new(flash: &SimFlash) -> AreaDesc {
        AreaDesc {
            areas: vec![],
            whole: vec![],
            sectors: flash.sector_iter().collect(),
        }
    }

    /// Add a slot to the image.  The slot must align with erasable units in the flash device.
    /// Panics if the description is not valid.  There are also bootloader assumptions that the
    /// slots are SLOT0, SLOT1, and SCRATCH in that order.
    pub fn add_image(&mut self, base: usize, len: usize, id: FlashId) {
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

        for sector in &self.sectors {
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
                device_id: 42,
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
            device_id: 42,
            pad16: 0,
            off: orig_base as u32,
            size: orig_len as u32,
        });
    }

    // Add a simple slot to the image.  This ignores the device layout, and just adds the area as a
    // single unit.  It assumes that the image lines up with image boundaries.  This tests
    // configurations where the partition table uses larger sectors than the underlying flash
    // device.
    pub fn add_simple_image(&mut self, base: usize, len: usize, id: FlashId) {
        let area = vec![FlashArea {
            flash_id: id,
            device_id: 42,
            pad16: 0,
            off: base as u32,
            size: len as u32,
        }];

        self.areas.push(area);
        self.whole.push(FlashArea {
            flash_id: id,
            device_id: 42,
            pad16: 0,
            off: base as u32,
            size: len as u32,
        });
    }

    // Look for the image with the given ID, and return its base and size.  Panics if the area is
    // not present.
    pub fn find(&self, id: FlashId) -> (usize, usize) {
        for area in &self.whole {
            if area.flash_id == id {
                return (area.off as usize, area.size as usize);
            }
        }
        panic!("Requesting area that is not present in flash");
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
    Nffs = 4,
    Core = 5,
    RebootLog = 6
}

impl Default for FlashId {
    fn default() -> FlashId {
        FlashId::BootLoader
    }
}

#[repr(C)]
#[derive(Debug, Clone, Default)]
pub struct FlashArea {
    flash_id: FlashId,
    device_id: u8,
    pad16: u16,
    off: u32,
    size: u32,
}

