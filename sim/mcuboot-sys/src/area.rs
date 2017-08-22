//! Describe flash areas.

use simflash::{Flash, SimFlash, Sector};
use std::ptr;

/// Structure to build up the boot area table.
#[derive(Debug)]
pub struct AreaDesc {
    bootloader_area: Vec<FlashArea>,
    image_areas: Vec<Vec<FlashArea>>,
    nffs_area: Vec<FlashArea>,
    core_area: Vec<FlashArea>,
    reboot_log_area: Vec<FlashArea>,
    whole: Vec<FlashArea>,
    sectors: Vec<Sector>,
}

/// Get the area vector from the provided areas vector to put the next FlashArea in.
/// In most of the cases this is the last area, but if we start a new area,
/// we have to create a new vector.
fn get_area_vector<'a>(areas: &'a mut Vec<Vec<FlashArea>>, first_area: bool) -> & 'a mut Vec<FlashArea> {
    if first_area {
        areas.push(vec![])
    }
    let area_index = areas.len() - 1;
    &mut(areas [area_index]) // return the last
}

/// Add a new FlashArea to the area provided.
fn add_area(area: &mut Vec<FlashArea>, id: FlashId, sec_base: usize, sec_size: usize) {
    area.push(FlashArea {
        flash_id: id,
        device_id: 42,
        pad16: 0,
        off: sec_base as u32,
        size: sec_size as u32,
    });
}

fn add_application_area(image_areas: &mut Vec<Vec<FlashArea>>, first_area: bool, id: FlashId, sec_base: usize, sec_size: usize) {
    assert!(
        id == FlashId::Image0 ||
        id == FlashId::Image1 ||
        id == FlashId::ImageScratch
    );
    let mut area_vector =  get_area_vector(image_areas, first_area);
    add_area(area_vector, id, sec_base, sec_size)
}

impl AreaDesc {
    pub fn new(flash: &SimFlash) -> AreaDesc {
        AreaDesc {
            bootloader_area: vec![],
            image_areas: vec![],
            nffs_area: vec![],
            core_area: vec![],
            reboot_log_area: vec![],
            whole: vec![],
            sectors: flash.sector_iter().collect(),
        }
    }

    /// Add a slot to the image.  The slot must align with erasable units in the flash device.
    /// Panics if the description is not valid.  There are also bootloader assumptions that the
    /// slots are SLOT0, SLOT1, and SCRATCH in that order.
    pub fn add_image(&mut self, base: usize, len: usize, id: FlashId) {
        let orig_base = base;
        let orig_len = len;
        let mut base = base;
        let mut len = len;

        let images_in_order = match id {
            FlashId::BootLoader => { self.bootloader_area.len() == 0}
            FlashId::Image0 => { self.image_areas.len()%3 == 0 }
            FlashId::Image1 => { self.image_areas.len()%3 == 1 }
            FlashId::ImageScratch => { self.image_areas.len()%3 == 2 }
            FlashId::Nffs => { self.nffs_area.len() == 0  }
            FlashId::Core => { self.core_area.len() == 0 }
            FlashId::RebootLog => { self.reboot_log_area.len() ==0 }
        };

        if !images_in_order {
            panic!("Flash areas not added in order");
        }

        let mut first_area = true;

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

            match id {
                FlashId::BootLoader => { add_area(&mut self.bootloader_area, id, sector.base, sector.size) },
                FlashId::Nffs => { add_area(&mut self.nffs_area, id, sector.base, sector.size) },
                FlashId::Core => { add_area(&mut self.core_area, id, sector.base, sector.size) },
                FlashId::RebootLog => { add_area(&mut self.reboot_log_area, id, sector.base, sector.size) },
                _ => { add_application_area(&mut self.image_areas, first_area, id, sector.base, sector.size) },
            }

            base += sector.size;
            len -= sector.size;
            first_area = false;
        }

        if len != 0 {
            panic!("Image goes past end of device");
        }

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

        match id {
            FlashId::BootLoader => { add_area(&mut self.bootloader_area, id, base, len) },
            FlashId::Nffs => { add_area(&mut self.nffs_area, id, base, len) },
            FlashId::Core => { add_area(&mut self.core_area, id, base, len) },
            FlashId::RebootLog => { add_area(&mut self.reboot_log_area, id, base, len) },
            _ => { add_application_area(&mut self.image_areas, true, id, base, len) },
        }

        self.whole.push(FlashArea {
            flash_id: id,
            device_id: 42,
            pad16: 0,
            off: base as u32,
            size: len as u32,
        });
    }

    // Look for the image nth with the given ID, and return its base and size.  Images of a
    // certain type are numbered from 0.  Panics if the area is not present.
    pub fn find(&self, id: FlashId, n: usize) -> (usize, usize) {
        let mut current_n = n;
        for area in &self.whole {
            if area.flash_id == id {
                if current_n == 0
                {
                    return (area.off as usize, area.size as usize);
                } else {
                    current_n -= 1;
                }
            }
        }
        panic!("Requesting area that is not present in flash");
    }

    fn fill_careadesc (&self, areas: &mut CAreaDesc, area: & Vec<FlashArea>, i: usize) {
        if area.len() > 0 {
            areas.slots[i].areas = &area[0];
            areas.slots[i].whole = self.whole[i].clone();
            areas.slots[i].num_areas = area.len() as u32;
            areas.slots[i].id = area[0].flash_id;
        }
    }

    pub fn get_c(&self) -> CAreaDesc {
        let mut areas: CAreaDesc = Default::default();

        // the number of boot image
        let mut areas_len = self.image_areas.len();
        if self.bootloader_area.len() > 0 { areas_len += 1; }
        if self.nffs_area.len()       > 0 { areas_len += 1; }
        if self.core_area.len()       > 0 { areas_len += 1; }
        if self.reboot_log_area.len() > 0 { areas_len += 1; }
        assert_eq!(areas_len, self.whole.len());

        let mut current_index: usize = 0;
        if self.bootloader_area.len() > 0 {
            self.fill_careadesc (&mut areas, &self.bootloader_area, current_index);
            current_index += 1;
        }
        for area in self.image_areas.iter() {
            self.fill_careadesc (&mut areas, &area, current_index);
            current_index += 1;
        }
        if self.nffs_area.len() > 0 {
            self.fill_careadesc (&mut areas, &self.nffs_area, current_index);
            current_index += 1;
        }
        if self.core_area.len() > 0 {
            self.fill_careadesc (&mut areas, &self.core_area, current_index);
            current_index += 1;
        }
        if self.reboot_log_area.len() > 0 {
            self.fill_careadesc (&mut areas, &self.reboot_log_area, current_index);
            // current_index += 1;
        }

        areas.num_slots = areas_len as u32;

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

