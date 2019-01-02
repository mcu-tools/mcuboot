use docopt::Docopt;
use log::{info, warn, error};
use rand::{
    distributions::{IndependentSample, Range},
    Rng, SeedableRng, XorShiftRng,
};
use std::{
    fmt,
    mem,
    process,
    slice,
};
use aes_ctr::{
    Aes128Ctr,
    stream_cipher::{
        generic_array::GenericArray,
        NewFixStreamCipher,
        StreamCipherCore,
    },
};
use serde_derive::Deserialize;

mod caps;
mod tlv;
pub mod testlog;

use simflash::{Flash, SimFlash, SimFlashMap};
use mcuboot_sys::{c, AreaDesc, FlashId};
use crate::caps::Caps;
use crate::tlv::{TlvGen, TlvFlags, AES_SEC_KEY};

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

/// A test run, intended to be run from "cargo test", so panics on failure.
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
        where F: Fn(&mut Run)
    {
        for &dev in ALL_DEVICES {
            for &align in &[1, 2, 4, 8] {
                for &erased_val in &[0, 0xff] {
                    let mut run = Run::new(dev, align, erased_val);
                    f(&mut run);
                }
            }
        }
    }

    /// Construct an `Images` that doesn't expect an upgrade to happen.
    pub fn make_no_upgrade_image(&self) -> Images {
        let mut flashmap = self.flashmap.clone();
        let primaries = install_image(&mut flashmap, &self.slots, 0, 32784, false);
        let upgrades = install_image(&mut flashmap, &self.slots, 1, 41928, false);
        Images {
            flashmap: flashmap,
            areadesc: self.areadesc.clone(),
            slots: [self.slots[0].clone(), self.slots[1].clone()],
            primaries: primaries,
            upgrades: upgrades,
            total_count: None,
        }
    }

    /// Construct an `Images` for normal testing.
    pub fn make_image(&self) -> Images {
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

    pub fn make_bad_slot1_image(&self) -> Images {
        let mut bad_flashmap = self.flashmap.clone();
        let primaries = install_image(&mut bad_flashmap, &self.slots, 0, 32784, false);
        let upgrades = install_image(&mut bad_flashmap, &self.slots, 1, 41928, true);
        Images {
            flashmap: bad_flashmap,
            areadesc: self.areadesc.clone(),
            slots: [self.slots[0].clone(), self.slots[1].clone()],
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
        let bad_slot1_image = run.make_bad_slot1_image();

        failed |= bad_slot1_image.run_signfail_upgrade();

        let images = run.make_no_upgrade_image();
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

impl Images {
    /// A simple upgrade without forced failures.
    ///
    /// Returns the number of flash operations which can later be used to
    /// inject failures at chosen steps.
    pub fn run_basic_upgrade(&self) -> Result<i32, ()> {
        let (flashmap, total_count) = try_upgrade(&self.flashmap, &self, None);
        info!("Total flash operation count={}", total_count);

        if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
            warn!("Image mismatch after first boot");
            Err(())
        } else {
            Ok(total_count)
        }
    }

    #[cfg(feature = "overwrite-only")]
    pub fn run_basic_revert(&self) -> bool {
        false
    }

    #[cfg(not(feature = "overwrite-only"))]
    pub fn run_basic_revert(&self) -> bool {
        let mut fails = 0;

        // FIXME: this test would also pass if no swap is ever performed???
        if Caps::SwapUpgrade.present() {
            for count in 2 .. 5 {
                info!("Try revert: {}", count);
                let flashmap = try_revert(&self.flashmap, &self.areadesc, count);
                if !verify_image(&flashmap, &self.slots, 0, &self.primaries) {
                    error!("Revert failure on count {}", count);
                    fails += 1;
                }
            }
        }

        fails > 0
    }

    pub fn run_perm_with_fails(&self) -> bool {
        let mut fails = 0;
        let total_flash_ops = self.total_count.unwrap();

        // Let's try an image halfway through.
        for i in 1 .. total_flash_ops {
            info!("Try interruption at {}", i);
            let (flashmap, count) = try_upgrade(&self.flashmap, &self, Some(i));
            info!("Second boot, count={}", count);
            if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
                warn!("FAIL at step {} of {}", i, total_flash_ops);
                fails += 1;
            }

            if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                               BOOT_FLAG_SET, BOOT_FLAG_SET) {
                warn!("Mismatched trailer for Slot 0");
                fails += 1;
            }

            if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                               BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
                warn!("Mismatched trailer for Slot 1");
                fails += 1;
            }

            if Caps::SwapUpgrade.present() {
                if !verify_image(&flashmap, &self.slots, 1, &self.primaries) {
                    warn!("Slot 1 FAIL at step {} of {}", i, total_flash_ops);
                    fails += 1;
                }
            }
        }

        if fails > 0 {
            error!("{} out of {} failed {:.2}%", fails, total_flash_ops,
                   fails as f32 * 100.0 / total_flash_ops as f32);
        }

        fails > 0
    }

    pub fn run_perm_with_random_fails_5(&self) -> bool {
        self.run_perm_with_random_fails(5)
    }

    fn run_perm_with_random_fails(&self, total_fails: usize) -> bool {
        let mut fails = 0;
        let total_flash_ops = self.total_count.unwrap();
        let (flashmap, total_counts) = try_random_fails(&self.flashmap, &self,
                                                        total_flash_ops, total_fails);
        info!("Random interruptions at reset points={:?}", total_counts);

        let slot0_ok = verify_image(&flashmap, &self.slots, 0, &self.upgrades);
        let slot1_ok = if Caps::SwapUpgrade.present() {
            verify_image(&flashmap, &self.slots, 1, &self.primaries)
        } else {
            true
        };
        if !slot0_ok || !slot1_ok {
            error!("Image mismatch after random interrupts: slot0={} slot1={}",
                   if slot0_ok { "ok" } else { "fail" },
                   if slot1_ok { "ok" } else { "fail" });
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            error!("Mismatched trailer for Slot 0");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            error!("Mismatched trailer for Slot 1");
            fails += 1;
        }

        if fails > 0 {
            error!("Error testing perm upgrade with {} fails", total_fails);
        }

        fails > 0
    }

    #[cfg(feature = "overwrite-only")]
    pub fn run_revert_with_fails(&self) -> bool {
        false
    }

    #[cfg(not(feature = "overwrite-only"))]
    pub fn run_revert_with_fails(&self) -> bool {
        let mut fails = 0;

        if Caps::SwapUpgrade.present() {
            for i in 1 .. (self.total_count.unwrap() - 1) {
                info!("Try interruption at {}", i);
                if try_revert_with_fail_at(&self.flashmap, &self, i) {
                    error!("Revert failed at interruption {}", i);
                    fails += 1;
                }
            }
        }

        fails > 0
    }

    #[cfg(feature = "overwrite-only")]
    pub fn run_norevert(&self) -> bool {
        false
    }

    #[cfg(not(feature = "overwrite-only"))]
    pub fn run_norevert(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try norevert");

        // First do a normal upgrade...
        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        //FIXME: copy_done is written by boot_go, is it ok if no copy
        //       was ever done?

        if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
            warn!("Slot 0 image verification FAIL");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 1");
            fails += 1;
        }

        // Marks image in slot0 as permanent, no revert should happen...
        mark_permanent_upgrade(&mut flashmap, &self.slots[0]);

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed second boot");
            fails += 1;
        }

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }
        if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
            warn!("Failed image verification");
            fails += 1;
        }

        if fails > 0 {
            error!("Error running upgrade without revert");
        }

        fails > 0
    }

    // Tests a new image written to slot0 that already has magic and image_ok set
    // while there is no image on slot1, so no revert should ever happen...
    pub fn run_norevert_newimage(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try non-revert on imgtool generated image");

        mark_upgrade(&mut flashmap, &self.slots[0]);

        // This simulates writing an image created by imgtool to Slot 0
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        // Run the bootloader...
        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        // State should not have changed
        if !verify_image(&flashmap, &self.slots, 0, &self.primaries) {
            warn!("Failed image verification");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 1");
            fails += 1;
        }

        if fails > 0 {
            error!("Expected a non revert with new image");
        }

        fails > 0
    }

    // Tests a new image written to slot0 that already has magic and image_ok set
    // while there is no image on slot1, so no revert should ever happen...
    pub fn run_signfail_upgrade(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try upgrade image with bad signature");

        mark_upgrade(&mut flashmap, &self.slots[0]);
        mark_permanent_upgrade(&mut flashmap, &self.slots[0]);
        mark_upgrade(&mut flashmap, &self.slots[1]);

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        // Run the bootloader...
        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        // State should not have changed
        if !verify_image(&flashmap, &self.slots, 0, &self.primaries) {
            warn!("Failed image verification");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        if fails > 0 {
            error!("Expected an upgrade failure when image has bad signature");
        }

        fails > 0
    }

    #[cfg(not(feature = "overwrite-only"))]
    fn trailer_sz(&self, align: usize) -> usize {
        c::boot_trailer_sz(align as u8) as usize
    }

    // FIXME: could get status sz from bootloader
    #[cfg(not(feature = "overwrite-only"))]
    #[cfg(not(feature = "enc-rsa"))]
    #[cfg(not(feature = "enc-kw"))]
    fn status_sz(&self, align: usize) -> usize {
        self.trailer_sz(align) - (16 + 24)
    }

    #[cfg(feature = "enc-rsa")]
    #[cfg(not(feature = "overwrite-only"))]
    fn status_sz(&self, align: usize) -> usize {
        self.trailer_sz(align) - (16 + 24 + 32)
    }

    #[cfg(feature = "enc-kw")]
    #[cfg(not(feature = "overwrite-only"))]
    fn status_sz(&self, align: usize) -> usize {
        self.trailer_sz(align) - (16 + 24 + 32)
    }

    /// This test runs a simple upgrade with no fails in the images, but
    /// allowing for fails in the status area. This should run to the end
    /// and warn that write fails were detected...
    #[cfg(not(feature = "validate-slot0"))]
    pub fn run_with_status_fails_complete(&self) -> bool { false }

    #[cfg(feature = "validate-slot0")]
    pub fn run_with_status_fails_complete(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try swap with status fails");

        mark_permanent_upgrade(&mut flashmap, &self.slots[1]);
        self.mark_bad_status_with_rate(&mut flashmap, 0, 1.0);

        let (result, asserts) = c::boot_go(&mut flashmap, &self.areadesc, None, true);
        if result != 0 {
            warn!("Failed!");
            fails += 1;
        }

        // Failed writes to the marked "bad" region don't assert anymore.
        // Any detected assert() is happening in another part of the code.
        if asserts != 0 {
            warn!("At least one assert() was called");
            fails += 1;
        }

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
            warn!("Failed image verification");
            fails += 1;
        }

        info!("validate slot0 enabled; re-run of boot_go should just work");
        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed!");
            fails += 1;
        }

        if fails > 0 {
            error!("Error running upgrade with status write fails");
        }

        fails > 0
    }

    /// This test runs a simple upgrade with no fails in the images, but
    /// allowing for fails in the status area. This should run to the end
    /// and warn that write fails were detected...
    #[cfg(feature = "validate-slot0")]
    pub fn run_with_status_fails_with_reset(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;
        let mut count = self.total_count.unwrap() / 2;

        //info!("count={}\n", count);

        info!("Try interrupted swap with status fails");

        mark_permanent_upgrade(&mut flashmap, &self.slots[1]);
        self.mark_bad_status_with_rate(&mut flashmap, 0, 0.5);

        // Should not fail, writing to bad regions does not assert
        let (_, asserts) = c::boot_go(&mut flashmap, &self.areadesc, Some(&mut count), true);
        if asserts != 0 {
            warn!("At least one assert() was called");
            fails += 1;
        }

        self.reset_bad_status(&mut flashmap, 0);

        info!("Resuming an interrupted swap operation");
        let (_, asserts) = c::boot_go(&mut flashmap, &self.areadesc, None, true);

        // This might throw no asserts, for large sector devices, where
        // a single failure writing is indistinguishable from no failure,
        // or throw a single assert for small sector devices that fail
        // multiple times...
        if asserts > 1 {
            warn!("Expected single assert validating slot0, more detected {}", asserts);
            fails += 1;
        }

        if fails > 0 {
            error!("Error running upgrade with status write fails");
        }

        fails > 0
    }

    /// Adds a new flash area that fails statistically
    #[cfg(not(feature = "overwrite-only"))]
    fn mark_bad_status_with_rate(&self, flashmap: &mut SimFlashMap, slot: usize,
                                 rate: f32) {
        let dev_id = &self.slots[slot].dev_id;
        let flash = flashmap.get_mut(&dev_id).unwrap();
        let align = flash.align();
        let off = &self.slots[0].base_off;
        let len = &self.slots[0].len;
        let status_off = off + len - self.trailer_sz(align);

        // Mark the status area as a bad area
        let _ = flash.add_bad_region(status_off, self.status_sz(align), rate);
    }

    #[cfg(feature = "validate-slot0")]
    fn reset_bad_status(&self, flashmap: &mut SimFlashMap, slot: usize) {
        let dev_id = &self.slots[slot].dev_id;
        let flash = flashmap.get_mut(&dev_id).unwrap();
        flash.reset_bad_regions();

        // Disabling write verification the only assert triggered by
        // boot_go should be checking for integrity of status bytes.
        flash.set_verify_writes(false);
    }

    #[cfg(not(feature = "validate-slot0"))]
    #[cfg(not(feature = "overwrite-only"))]
    pub fn run_with_status_fails_with_reset(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try interrupted swap with status fails");

        mark_permanent_upgrade(&mut flashmap, &self.slots[1]);
        self.mark_bad_status_with_rate(&mut flashmap, 0, 1.0);

        // This is expected to fail while writing to bad regions...
        let (_, asserts) = c::boot_go(&mut flashmap, &self.areadesc, None, true);
        if asserts == 0 {
            warn!("No assert() detected");
            fails += 1;
        }

        fails > 0
    }

    #[cfg(feature = "overwrite-only")]
    pub fn run_with_status_fails_with_reset(&self) -> bool {
        false
    }
}

/// Test a boot, optionally stopping after 'n' flash options.  Returns a count
/// of the number of flash operations done total.
fn try_upgrade(flashmap: &SimFlashMap, images: &Images,
               stop: Option<i32>) -> (SimFlashMap, i32) {
    // Clone the flash to have a new copy.
    let mut flashmap = flashmap.clone();

    mark_permanent_upgrade(&mut flashmap, &images.slots[1]);

    let mut counter = stop.unwrap_or(0);

    let (first_interrupted, count) = match c::boot_go(&mut flashmap, &images.areadesc, Some(&mut counter), false) {
        (-0x13579, _) => (true, stop.unwrap()),
        (0, _) => (false, -counter),
        (x, _) => panic!("Unknown return: {}", x),
    };

    counter = 0;
    if first_interrupted {
        // fl.dump();
        match c::boot_go(&mut flashmap, &images.areadesc, Some(&mut counter), false) {
            (-0x13579, _) => panic!("Shouldn't stop again"),
            (0, _) => (),
            (x, _) => panic!("Unknown return: {}", x),
        }
    }

    (flashmap, count - counter)
}

#[cfg(not(feature = "overwrite-only"))]
fn try_revert(flashmap: &SimFlashMap, areadesc: &AreaDesc, count: usize) -> SimFlashMap {
    let mut flashmap = flashmap.clone();

    // fl.write_file("image0.bin").unwrap();
    for i in 0 .. count {
        info!("Running boot pass {}", i + 1);
        assert_eq!(c::boot_go(&mut flashmap, &areadesc, None, false), (0, 0));
    }
    flashmap
}

#[cfg(not(feature = "overwrite-only"))]
fn try_revert_with_fail_at(flashmap: &SimFlashMap, images: &Images,
                           stop: i32) -> bool {
    let mut flashmap = flashmap.clone();
    let mut fails = 0;

    let mut counter = stop;
    let (x, _) = c::boot_go(&mut flashmap, &images.areadesc, Some(&mut counter), false);
    if x != -0x13579 {
        warn!("Should have stopped at interruption point");
        fails += 1;
    }

    if !verify_trailer(&flashmap, &images.slots, 0, None, None, BOOT_FLAG_UNSET) {
        warn!("copy_done should be unset");
        fails += 1;
    }

    let (x, _) = c::boot_go(&mut flashmap, &images.areadesc, None, false);
    if x != 0 {
        warn!("Should have finished upgrade");
        fails += 1;
    }

    if !verify_image(&flashmap, &images.slots, 0, &images.upgrades) {
        warn!("Image in slot 0 before revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_image(&flashmap, &images.slots, 1, &images.primaries) {
        warn!("Image in slot 1 before revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 0, BOOT_MAGIC_GOOD,
                       BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
        warn!("Mismatched trailer for Slot 0 before revert");
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 1, BOOT_MAGIC_UNSET,
                       BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
        warn!("Mismatched trailer for Slot 1 before revert");
        fails += 1;
    }

    // Do Revert
    let (x, _) = c::boot_go(&mut flashmap, &images.areadesc, None, false);
    if x != 0 {
        warn!("Should have finished a revert");
        fails += 1;
    }

    if !verify_image(&flashmap, &images.slots, 0, &images.primaries) {
        warn!("Image in slot 0 after revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_image(&flashmap, &images.slots, 1, &images.upgrades) {
        warn!("Image in slot 1 after revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 0, BOOT_MAGIC_GOOD,
                       BOOT_FLAG_SET, BOOT_FLAG_SET) {
        warn!("Mismatched trailer for Slot 1 after revert");
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 1, BOOT_MAGIC_UNSET,
                       BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
        warn!("Mismatched trailer for Slot 1 after revert");
        fails += 1;
    }

    fails > 0
}

fn try_random_fails(flashmap: &SimFlashMap, images: &Images,
                    total_ops: i32,  count: usize) -> (SimFlashMap, Vec<i32>) {
    let mut flashmap = flashmap.clone();

    mark_permanent_upgrade(&mut flashmap, &images.slots[1]);

    let mut rng = rand::thread_rng();
    let mut resets = vec![0i32; count];
    let mut remaining_ops = total_ops;
    for i in 0 .. count {
        let ops = Range::new(1, remaining_ops / 2);
        let reset_counter = ops.ind_sample(&mut rng);
        let mut counter = reset_counter;
        match c::boot_go(&mut flashmap, &images.areadesc, Some(&mut counter), false) {
            (0, _) | (-0x13579, _) => (),
            (x, _) => panic!("Unknown return: {}", x),
        }
        remaining_ops -= reset_counter;
        resets[i] = reset_counter;
    }

    match c::boot_go(&mut flashmap, &images.areadesc, None, false) {
        (-0x13579, _) => panic!("Should not be have been interrupted!"),
        (0, _) => (),
        (x, _) => panic!("Unknown return: {}", x),
    }

    (flashmap, resets)
}

/// Show the flash layout.
#[allow(dead_code)]
fn show_flash(flash: &dyn Flash) {
    println!("---- Flash configuration ----");
    for sector in flash.sector_iter() {
        println!("    {:3}: 0x{:08x}, 0x{:08x}",
                 sector.num, sector.base, sector.size);
    }
    println!("");
}

/// Install a "program" into the given image.  This fakes the image header, or at least all of the
/// fields used by the given code.  Returns a copy of the image that was written.
fn install_image(flashmap: &mut SimFlashMap, slots: &[SlotInfo], slot: usize, len: usize,
                 bad_sig: bool) -> [Option<Vec<u8>>; 2] {
    let offset = slots[slot].base_off;
    let slot_len = slots[slot].len;
    let dev_id = slots[slot].dev_id;

    let mut tlv = make_tlv();

    const HDR_SIZE: usize = 32;

    // Generate a boot header.  Note that the size doesn't include the header.
    let header = ImageHeader {
        magic: 0x96f3b83d,
        load_addr: 0,
        hdr_size: HDR_SIZE as u16,
        _pad1: 0,
        img_size: len as u32,
        flags: tlv.get_flags(),
        ver: ImageVersion {
            major: (offset / (128 * 1024)) as u8,
            minor: 0,
            revision: 1,
            build_num: offset as u32,
        },
        _pad2: 0,
    };

    let mut b_header = [0; HDR_SIZE];
    b_header[..32].clone_from_slice(header.as_raw());
    assert_eq!(b_header.len(), HDR_SIZE);

    tlv.add_bytes(&b_header);

    // The core of the image itself is just pseudorandom data.
    let mut b_img = vec![0; len];
    splat(&mut b_img, offset);

    // TLV signatures work over plain image
    tlv.add_bytes(&b_img);

    // Generate encrypted images
    let flag = TlvFlags::ENCRYPTED as u32;
    let is_encrypted = (tlv.get_flags() & flag) == flag;
    let mut b_encimg = vec![];
    if is_encrypted {
        let key = GenericArray::from_slice(AES_SEC_KEY);
        let nonce = GenericArray::from_slice(&[0; 16]);
        let mut cipher = Aes128Ctr::new(&key, &nonce);
        b_encimg = b_img.clone();
        cipher.apply_keystream(&mut b_encimg);
    }

    // Build the TLV itself.
    let mut b_tlv = if bad_sig {
        let good_sig = &mut tlv.make_tlv();
        vec![0; good_sig.len()]
    } else {
        tlv.make_tlv()
    };

    // Pad the block to a flash alignment (8 bytes).
    while b_tlv.len() % 8 != 0 {
        //FIXME: should be erase_val?
        b_tlv.push(0xFF);
    }

    let mut buf = vec![];
    buf.append(&mut b_header.to_vec());
    buf.append(&mut b_img);
    buf.append(&mut b_tlv.clone());

    let mut encbuf = vec![];
    if is_encrypted {
        encbuf.append(&mut b_header.to_vec());
        encbuf.append(&mut b_encimg);
        encbuf.append(&mut b_tlv);
    }

    let result: [Option<Vec<u8>>; 2];

    // Since images are always non-encrypted in slot0, we first write an
    // encrypted image, re-read to use for verification, erase + flash
    // un-encrypted. In slot1 the image is written un-encrypted, and if
    // encryption is requested, it follows an erase + flash encrypted.

    let flash = flashmap.get_mut(&dev_id).unwrap();

    if slot == 0 {
        let enc_copy: Option<Vec<u8>>;

        if is_encrypted {
            flash.write(offset, &encbuf).unwrap();

            let mut enc = vec![0u8; encbuf.len()];
            flash.read(offset, &mut enc).unwrap();

            enc_copy = Some(enc);

            flash.erase(offset, slot_len).unwrap();
        } else {
            enc_copy = None;
        }

        flash.write(offset, &buf).unwrap();

        let mut copy = vec![0u8; buf.len()];
        flash.read(offset, &mut copy).unwrap();

        result = [Some(copy), enc_copy];
    } else {

        flash.write(offset, &buf).unwrap();

        let mut copy = vec![0u8; buf.len()];
        flash.read(offset, &mut copy).unwrap();

        let enc_copy: Option<Vec<u8>>;

        if is_encrypted {
            flash.erase(offset, slot_len).unwrap();

            flash.write(offset, &encbuf).unwrap();

            let mut enc = vec![0u8; encbuf.len()];
            flash.read(offset, &mut enc).unwrap();

            enc_copy = Some(enc);
        } else {
            enc_copy = None;
        }

        result = [Some(copy), enc_copy];
    }

    result
}

// The TLV in use depends on what kind of signature we are verifying.
#[cfg(feature = "sig-rsa")]
#[cfg(feature = "enc-kw")]
fn make_tlv() -> TlvGen {
    TlvGen::new_rsa_kw()
}

#[cfg(feature = "sig-rsa")]
#[cfg(not(feature = "enc-rsa"))]
#[cfg(not(feature = "enc-kw"))]
fn make_tlv() -> TlvGen {
    TlvGen::new_rsa_pss()
}

#[cfg(feature = "sig-ecdsa")]
#[cfg(feature = "enc-kw")]
fn make_tlv() -> TlvGen {
    TlvGen::new_ecdsa_kw()
}

#[cfg(feature = "sig-ecdsa")]
#[cfg(not(feature = "enc-kw"))]
fn make_tlv() -> TlvGen {
    TlvGen::new_ecdsa()
}

#[cfg(not(feature = "sig-rsa"))]
#[cfg(feature = "enc-rsa")]
fn make_tlv() -> TlvGen {
    TlvGen::new_enc_rsa()
}

#[cfg(feature = "sig-rsa")]
#[cfg(feature = "enc-rsa")]
fn make_tlv() -> TlvGen {
    TlvGen::new_sig_enc_rsa()
}

#[cfg(not(feature = "sig-rsa"))]
#[cfg(not(feature = "sig-ecdsa"))]
#[cfg(feature = "enc-kw")]
fn make_tlv() -> TlvGen {
    TlvGen::new_enc_kw()
}

#[cfg(not(feature = "sig-rsa"))]
#[cfg(not(feature = "sig-ecdsa"))]
#[cfg(not(feature = "enc-rsa"))]
#[cfg(not(feature = "enc-kw"))]
fn make_tlv() -> TlvGen {
    TlvGen::new_hash_only()
}

#[cfg(feature = "enc-rsa")]
fn find_image(images: &[Option<Vec<u8>>; 2], slot: usize) -> &Vec<u8> {
    match &images[slot] {
        Some(image) => return image,
        None => panic!("Invalid image"),
    }
}

#[cfg(feature = "enc-kw")]
fn find_image(images: &[Option<Vec<u8>>; 2], slot: usize) -> &Vec<u8> {
    match &images[slot] {
        Some(image) => return image,
        None => panic!("Invalid image"),
    }
}

#[cfg(not(feature = "enc-rsa"))]
#[cfg(not(feature = "enc-kw"))]
fn find_image(images: &[Option<Vec<u8>>; 2], _slot: usize) -> &Vec<u8> {
    match &images[0] {
        Some(image) => return image,
        None => panic!("Invalid image"),
    }
}

/// Verify that given image is present in the flash at the given offset.
fn verify_image(flashmap: &SimFlashMap, slots: &[SlotInfo], slot: usize,
                images: &[Option<Vec<u8>>; 2]) -> bool {
    let image = find_image(images, slot);
    let buf = image.as_slice();
    let dev_id = slots[slot].dev_id;

    let mut copy = vec![0u8; buf.len()];
    let offset = slots[slot].base_off;
    let flash = flashmap.get(&dev_id).unwrap();
    flash.read(offset, &mut copy).unwrap();

    if buf != &copy[..] {
        for i in 0 .. buf.len() {
            if buf[i] != copy[i] {
                info!("First failure for slot{} at {:#x} {:#x}!={:#x}",
                      slot, offset + i, buf[i], copy[i]);
                break;
            }
        }
        false
    } else {
        true
    }
}

#[cfg(feature = "overwrite-only")]
#[allow(unused_variables)]
// overwrite-only doesn't employ trailer management
fn verify_trailer(flashmap: &SimFlashMap, slots: &[SlotInfo], slot: usize,
                  magic: Option<u8>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    true
}

#[cfg(not(feature = "overwrite-only"))]
fn verify_trailer(flashmap: &SimFlashMap, slots: &[SlotInfo], slot: usize,
                  magic: Option<u8>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    let offset = slots[slot].trailer_off;
    let dev_id = slots[slot].dev_id;
    let mut copy = vec![0u8; c::boot_magic_sz() + c::boot_max_align() * 2];
    let mut failed = false;

    let flash = flashmap.get(&dev_id).unwrap();
    let erased_val = flash.erased_val();
    flash.read(offset, &mut copy).unwrap();

    failed |= match magic {
        Some(v) => {
            if v == 1 && &copy[16..] != MAGIC.unwrap() {
                warn!("\"magic\" mismatch at {:#x}", offset);
                true
            } else if v == 3 {
                let expected = [erased_val; 16];
                if &copy[16..] != expected {
                    warn!("\"magic\" mismatch at {:#x}", offset);
                    true
                } else {
                    false
                }
            } else {
                false
            }
        },
        None => false,
    };

    failed |= match image_ok {
        Some(v) => {
            if (v == 1 && copy[8] != v) || (v == 3 && copy[8] != erased_val) {
                warn!("\"image_ok\" mismatch at {:#x} v={} val={:#x}", offset, v, copy[8]);
                true
            } else {
                false
            }
        },
        None => false,
    };

    failed |= match copy_done {
        Some(v) => {
            if (v == 1 && copy[0] != v) || (v == 3 && copy[0] != erased_val) {
                warn!("\"copy_done\" mismatch at {:#x} v={} val={:#x}", offset, v, copy[0]);
                true
            } else {
                false
            }
        },
        None => false,
    };

    !failed
}

/// The image header
#[repr(C)]
pub struct ImageHeader {
    magic: u32,
    load_addr: u32,
    hdr_size: u16,
    _pad1: u16,
    img_size: u32,
    flags: u32,
    ver: ImageVersion,
    _pad2: u32,
}

impl AsRaw for ImageHeader {}

#[repr(C)]
pub struct ImageVersion {
    major: u8,
    minor: u8,
    revision: u16,
    build_num: u32,
}

#[derive(Clone)]
struct SlotInfo {
    base_off: usize,
    trailer_off: usize,
    len: usize,
    dev_id: u8,
}

pub struct Images {
    flashmap: SimFlashMap,
    areadesc: AreaDesc,
    slots: [SlotInfo; 2],
    primaries: [Option<Vec<u8>>; 2],
    upgrades: [Option<Vec<u8>>; 2],
    total_count: Option<i32>,
}

const MAGIC: Option<&[u8]> = Some(&[0x77, 0xc2, 0x95, 0xf3,
                                    0x60, 0xd2, 0xef, 0x7f,
                                    0x35, 0x52, 0x50, 0x0f,
                                    0x2c, 0xb6, 0x79, 0x80]);

// Replicates defines found in bootutil.h
const BOOT_MAGIC_GOOD: Option<u8> = Some(1);
const BOOT_MAGIC_UNSET: Option<u8> = Some(3);

const BOOT_FLAG_SET: Option<u8> = Some(1);
const BOOT_FLAG_UNSET: Option<u8> = Some(3);

/// Write out the magic so that the loader tries doing an upgrade.
fn mark_upgrade(flashmap: &mut SimFlashMap, slot: &SlotInfo) {
    let flash = flashmap.get_mut(&slot.dev_id).unwrap();
    let offset = slot.trailer_off + c::boot_max_align() * 2;
    flash.write(offset, MAGIC.unwrap()).unwrap();
}

/// Writes the image_ok flag which, guess what, tells the bootloader
/// the this image is ok (not a test, and no revert is to be performed).
fn mark_permanent_upgrade(flashmap: &mut SimFlashMap, slot: &SlotInfo) {
    let flash = flashmap.get_mut(&slot.dev_id).unwrap();
    let mut ok = [flash.erased_val(); 8];
    ok[0] = 1u8;
    let off = slot.trailer_off + c::boot_max_align();
    let align = flash.align();
    flash.write(off, &ok[..align]).unwrap();
}

// Drop some pseudo-random gibberish onto the data.
fn splat(data: &mut [u8], seed: usize) {
    let seed_block = [0x135782ea, 0x92184728, data.len() as u32, seed as u32];
    let mut rng: XorShiftRng = SeedableRng::from_seed(seed_block);
    rng.fill_bytes(data);
}

/// Return a read-only view into the raw bytes of this object
trait AsRaw : Sized {
    fn as_raw<'a>(&'a self) -> &'a [u8] {
        unsafe { slice::from_raw_parts(self as *const _ as *const u8,
                                       mem::size_of::<Self>()) }
    }
}

fn show_sizes() {
    // This isn't panic safe.
    for min in &[1, 2, 4, 8] {
        let msize = c::boot_trailer_sz(*min);
        println!("{:2}: {} (0x{:x})", min, msize, msize);
    }
}
