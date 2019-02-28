use docopt::Docopt;
use log::{warn, error};
use std::{
    fmt,
    process,
};
use serde_derive::Deserialize;

mod caps;
mod image;
mod suit;
mod tlv;
pub mod testlog;

use simflash::{SimFlash, SimFlashMap};
use mcuboot_sys::{c, AreaDesc, FlashId};

use crate::image::{
    Images,
    install_image,
    mark_upgrade,
    SlotInfo,
    show_sizes,
};

const USAGE: &'static str = "
Mcuboot simulator

Usage:
  bootsim sizes
  bootsim run --device TYPE [--align SIZE]
  bootsim runall
  bootsim (--help | --version)

Options:
  -h, --help         Show this message
  --version          Version
  --device TYPE      MCU to simulate
                     Valid values: stm32f4, k64f
  --align SIZE       Flash write alignment
";

#[derive(Debug, Deserialize)]
struct Args {
    flag_help: bool,
    flag_version: bool,
    flag_device: Option<DeviceName>,
    flag_align: Option<AlignArg>,
    cmd_sizes: bool,
    cmd_run: bool,
    cmd_runall: bool,
}

#[derive(Copy, Clone, Debug, Deserialize)]
pub enum DeviceName { Stm32f4, K64f, K64fBig, Nrf52840, Nrf52840SpiFlash, }

pub static ALL_DEVICES: &'static [DeviceName] = &[
    DeviceName::Stm32f4,
    DeviceName::K64f,
    DeviceName::K64fBig,
    DeviceName::Nrf52840,
    DeviceName::Nrf52840SpiFlash,
];

impl fmt::Display for DeviceName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match *self {
            DeviceName::Stm32f4 => "stm32f4",
            DeviceName::K64f => "k64f",
            DeviceName::K64fBig => "k64fbig",
            DeviceName::Nrf52840 => "nrf52840",
            DeviceName::Nrf52840SpiFlash => "Nrf52840SpiFlash",
        };
        f.write_str(name)
    }
}

#[derive(Debug)]
struct AlignArg(u8);

struct AlignArgVisitor;

impl<'de> serde::de::Visitor<'de> for AlignArgVisitor {
    type Value = AlignArg;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str("1, 2, 4 or 8")
    }

    fn visit_u8<E>(self, n: u8) -> Result<Self::Value, E>
        where E: serde::de::Error
    {
        Ok(match n {
            1 | 2 | 4 | 8 => AlignArg(n),
            n => {
                let err = format!("Could not deserialize '{}' as alignment", n);
                return Err(E::custom(err));
            }
        })
    }
}

impl<'de> serde::de::Deserialize<'de> for AlignArg {
    fn deserialize<D>(d: D) -> Result<AlignArg, D::Error>
        where D: serde::de::Deserializer<'de>
    {
        d.deserialize_u8(AlignArgVisitor)
    }
}

pub fn main() {
    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.deserialize())
        .unwrap_or_else(|e| e.exit());
    // println!("args: {:#?}", args);

    if args.cmd_sizes {
        show_sizes();
        return;
    }

    let mut status = RunStatus::new();
    if args.cmd_run {

        let align = args.flag_align.map(|x| x.0).unwrap_or(1);


        let device = match args.flag_device {
            None => panic!("Missing mandatory device argument"),
            Some(dev) => dev,
        };

        status.run_single(device, align, 0xff);
    }

    if args.cmd_runall {
        for &dev in ALL_DEVICES {
            for &align in &[1, 2, 4, 8] {
                for &erased_val in &[0, 0xff] {
                    status.run_single(dev, align, erased_val);
                }
            }
        }
    }

    if status.failures > 0 {
        error!("{} Tests ran with {} failures", status.failures + status.passes, status.failures);
        process::exit(1);
    } else {
        error!("{} Tests ran successfully", status.passes);
        process::exit(0);
    }
}

/// A Run describes a single run of the simulator.  It captures the
/// configuration of a particular device configuration, including the flash
/// devices and the information about the slots.  This can be thought of as
/// a builder for `Images`.
#[derive(Clone)]
pub struct Run {
    flashmap: SimFlashMap,
    areadesc: AreaDesc,
    slots: [SlotInfo; 2],
}

impl Run {
    pub fn new(device: DeviceName, align: u8, erased_val: u8) -> Run {
        let (flashmap, areadesc) = make_device(device, align, erased_val);

        let (slot0_base, slot0_len, slot0_dev_id) = areadesc.find(FlashId::Image0);
        let (slot1_base, slot1_len, slot1_dev_id) = areadesc.find(FlashId::Image1);

        // NOTE: not accounting "swap_size" because it is not used by sim...
        let offset_from_end = c::boot_magic_sz() + c::boot_max_align() * 2;

        // Construct a primary image.
        let slot0 = SlotInfo {
            base_off: slot0_base as usize,
            trailer_off: slot0_base + slot0_len - offset_from_end,
            len: slot0_len as usize,
            dev_id: slot0_dev_id,
        };

        // And an upgrade image.
        let slot1 = SlotInfo {
            base_off: slot1_base as usize,
            trailer_off: slot1_base + slot1_len - offset_from_end,
            len: slot1_len as usize,
            dev_id: slot1_dev_id,
        };

        Run {
            flashmap: flashmap,
            areadesc: areadesc,
            slots: [slot0, slot1],
        }
    }

    pub fn each_device<F>(f: F)
        where F: Fn(Run)
    {
        for &dev in ALL_DEVICES {
            for &align in &[1, 2, 4, 8] {
                for &erased_val in &[0, 0xff] {
                    let run = Run::new(dev, align, erased_val);
                    f(run);
                }
            }
        }
    }

    /// Construct an `Images` that doesn't expect an upgrade to happen.
    pub fn make_no_upgrade_image(self) -> Images {
        let mut flashmap = self.flashmap;
        let primaries = install_image(&mut flashmap, &self.slots, 0, 32784, false);
        let upgrades = install_image(&mut flashmap, &self.slots, 1, 41928, false);
        Images {
            flashmap: flashmap,
            areadesc: self.areadesc,
            slots: self.slots,
            primaries: primaries,
            upgrades: upgrades,
            total_count: None,
        }
    }

    /// Construct an `Images` for normal testing.
    pub fn make_image(self) -> Images {
        let mut images = self.make_no_upgrade_image();
        mark_upgrade(&mut images.flashmap, &images.slots[1]);

        // upgrades without fails, counts number of flash operations
        let total_count = match images.run_basic_upgrade() {
            Ok(v)  => v,
            Err(_) => {
                panic!("Unable to perform basic upgrade");
            },
        };

        images.total_count = Some(total_count);
        images
    }

    pub fn make_bad_slot1_image(self) -> Images {
        let mut bad_flashmap = self.flashmap;
        let primaries = install_image(&mut bad_flashmap, &self.slots, 0, 32784, false);
        let upgrades = install_image(&mut bad_flashmap, &self.slots, 1, 41928, true);
        Images {
            flashmap: bad_flashmap,
            areadesc: self.areadesc,
            slots: self.slots,
            primaries: primaries,
            upgrades: upgrades,
            total_count: None,
        }
    }

}

pub struct RunStatus {
    failures: usize,
    passes: usize,
}

impl RunStatus {
    pub fn new() -> RunStatus {
        RunStatus {
            failures: 0,
            passes: 0,
        }
    }

    pub fn run_single(&mut self, device: DeviceName, align: u8, erased_val: u8) {
        warn!("Running on device {} with alignment {}", device, align);

        let run = Run::new(device, align, erased_val);

        let mut failed = false;

        // Creates a badly signed image in slot1 to check that it is not
        // upgraded to
        let bad_slot1_image = run.clone().make_bad_slot1_image();

        failed |= bad_slot1_image.run_signfail_upgrade();

        let images = run.clone().make_no_upgrade_image();
        failed |= images.run_norevert_newimage();

        let images = run.make_image();

        failed |= images.run_basic_revert();
        failed |= images.run_revert_with_fails();
        failed |= images.run_perm_with_fails();
        failed |= images.run_perm_with_random_fails(5);
        failed |= images.run_norevert();

        failed |= images.run_with_status_fails_complete();
        failed |= images.run_with_status_fails_with_reset();

        //show_flash(&flash);

        if failed {
            self.failures += 1;
        } else {
            self.passes += 1;
        }
    }

    pub fn failures(&self) -> usize {
        self.failures
    }
}

/// Build the Flash and area descriptor for a given device.
pub fn make_device(device: DeviceName, align: u8, erased_val: u8) -> (SimFlashMap, AreaDesc) {
    match device {
        DeviceName::Stm32f4 => {
            // STM style flash.  Large sectors, with a large scratch area.
            let flash = SimFlash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024,
                                      64 * 1024,
                                      128 * 1024, 128 * 1024, 128 * 1024],
                                      align as usize, erased_val);
            let dev_id = 0;
            let mut areadesc = AreaDesc::new();
            areadesc.add_flash_sectors(dev_id, &flash);
            areadesc.add_image(0x020000, 0x020000, FlashId::Image0, dev_id);
            areadesc.add_image(0x040000, 0x020000, FlashId::Image1, dev_id);
            areadesc.add_image(0x060000, 0x020000, FlashId::ImageScratch, dev_id);

            let mut flashmap = SimFlashMap::new();
            flashmap.insert(dev_id, flash);
            (flashmap, areadesc)
        }
        DeviceName::K64f => {
            // NXP style flash.  Small sectors, one small sector for scratch.
            let flash = SimFlash::new(vec![4096; 128], align as usize, erased_val);

            let dev_id = 0;
            let mut areadesc = AreaDesc::new();
            areadesc.add_flash_sectors(dev_id, &flash);
            areadesc.add_image(0x020000, 0x020000, FlashId::Image0, dev_id);
            areadesc.add_image(0x040000, 0x020000, FlashId::Image1, dev_id);
            areadesc.add_image(0x060000, 0x001000, FlashId::ImageScratch, dev_id);

            let mut flashmap = SimFlashMap::new();
            flashmap.insert(dev_id, flash);
            (flashmap, areadesc)
        }
        DeviceName::K64fBig => {
            // Simulating an STM style flash on top of an NXP style flash.  Underlying flash device
            // uses small sectors, but we tell the bootloader they are large.
            let flash = SimFlash::new(vec![4096; 128], align as usize, erased_val);

            let dev_id = 0;
            let mut areadesc = AreaDesc::new();
            areadesc.add_flash_sectors(dev_id, &flash);
            areadesc.add_simple_image(0x020000, 0x020000, FlashId::Image0, dev_id);
            areadesc.add_simple_image(0x040000, 0x020000, FlashId::Image1, dev_id);
            areadesc.add_simple_image(0x060000, 0x020000, FlashId::ImageScratch, dev_id);

            let mut flashmap = SimFlashMap::new();
            flashmap.insert(dev_id, flash);
            (flashmap, areadesc)
        }
        DeviceName::Nrf52840 => {
            // Simulating the flash on the nrf52840 with partitions set up so that the scratch size
            // does not divide into the image size.
            let flash = SimFlash::new(vec![4096; 128], align as usize, erased_val);

            let dev_id = 0;
            let mut areadesc = AreaDesc::new();
            areadesc.add_flash_sectors(dev_id, &flash);
            areadesc.add_image(0x008000, 0x034000, FlashId::Image0, dev_id);
            areadesc.add_image(0x03c000, 0x034000, FlashId::Image1, dev_id);
            areadesc.add_image(0x070000, 0x00d000, FlashId::ImageScratch, dev_id);

            let mut flashmap = SimFlashMap::new();
            flashmap.insert(dev_id, flash);
            (flashmap, areadesc)
        }
        DeviceName::Nrf52840SpiFlash => {
            // Simulate nrf52840 with external SPI flash. The external SPI flash
            // has a larger sector size so for now store scratch on that flash.
            let flash0 = SimFlash::new(vec![4096; 128], align as usize, erased_val);
            let flash1 = SimFlash::new(vec![8192; 64], align as usize, erased_val);

            let mut areadesc = AreaDesc::new();
            areadesc.add_flash_sectors(0, &flash0);
            areadesc.add_flash_sectors(1, &flash1);

            areadesc.add_image(0x008000, 0x068000, FlashId::Image0, 0);
            areadesc.add_image(0x000000, 0x068000, FlashId::Image1, 1);
            areadesc.add_image(0x068000, 0x018000, FlashId::ImageScratch, 1);

            let mut flashmap = SimFlashMap::new();
            flashmap.insert(0, flash0);
            flashmap.insert(1, flash1);
            (flashmap, areadesc)
        }
    }
}
