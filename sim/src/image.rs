use log::{info, warn, error};
use rand::{
    distributions::{IndependentSample, Range},
    Rng, SeedableRng, XorShiftRng,
};
use std::{
    mem,
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

use simflash::{Flash, SimFlash, SimMultiFlash};
use mcuboot_sys::{c, AreaDesc, FlashId};
use crate::{
    ALL_DEVICES,
    DeviceName,
};
use crate::caps::Caps;
use crate::tlv::{ManifestGen, TlvGen, TlvFlags, AES_SEC_KEY};

/// A builder for Images.  This describes a single run of the simulator,
/// capturing the configuration of a particular set of devices, including
/// the flash simulator(s) and the information about the slots.
#[derive(Clone)]
pub struct ImagesBuilder {
    flash: SimMultiFlash,
    areadesc: AreaDesc,
    slots: Vec<[SlotInfo; 2]>,
}

/// Images represents the state of a simulation for a given set of images.
/// The flash holds the state of the simulated flash, whereas primaries
/// and upgrades hold the expected contents of these images.
pub struct Images {
    flash: SimMultiFlash,
    areadesc: AreaDesc,
    images: Vec<OneImage>,
    total_count: Option<i32>,
}

/// When doing multi-image, there is an instance of this information for
/// each of the images.  Single image there will be one of these.
struct OneImage {
    slots: [SlotInfo; 2],
    primaries: ImageData,
    upgrades: ImageData,
}

/// The Rust-side representation of an image.  For unencrypted images, this
/// is just the unencrypted payload.  For encrypted images, we store both
/// the encrypted and the plaintext.
struct ImageData {
    plain: Vec<u8>,
    cipher: Option<Vec<u8>>,
}

impl ImagesBuilder {
    /// Construct a new image builder for the given device.  Returns
    /// Some(builder) if is possible to test this configuration, or None if
    /// not possible (for example, if there aren't enough image slots).
    pub fn new(device: DeviceName, align: u8, erased_val: u8) -> Option<Self> {
        let (flash, areadesc) = Self::make_device(device, align, erased_val);

        let num_images = Caps::get_num_images();

        let mut slots = Vec::with_capacity(num_images);
        for image in 0..num_images {
            // This mapping must match that defined in
            // `boot/zephyr/include/sysflash/sysflash.h`.
            let id0 = match image {
                0 => FlashId::Image0,
                1 => FlashId::Image2,
                _ => panic!("More than 2 images not supported"),
            };
            let (primary_base, primary_len, primary_dev_id) = match areadesc.find(id0) {
                Some(info) => info,
                None => return None,
            };
            let id1 = match image {
                0 => FlashId::Image1,
                1 => FlashId::Image3,
                _ => panic!("More than 2 images not supported"),
            };
            let (secondary_base, secondary_len, secondary_dev_id) = match areadesc.find(id1) {
                Some(info) => info,
                None => return None,
            };

            let offset_from_end = c::boot_magic_sz() + c::boot_max_align() * 4;

            // Construct a primary image.
            let primary = SlotInfo {
                base_off: primary_base as usize,
                trailer_off: primary_base + primary_len - offset_from_end,
                len: primary_len as usize,
                dev_id: primary_dev_id,
            };

            // And an upgrade image.
            let secondary = SlotInfo {
                base_off: secondary_base as usize,
                trailer_off: secondary_base + secondary_len - offset_from_end,
                len: secondary_len as usize,
                dev_id: secondary_dev_id,
            };

            slots.push([primary, secondary]);
        }

        Some(ImagesBuilder {
            flash: flash,
            areadesc: areadesc,
            slots: slots,
        })
    }

    pub fn each_device<F>(f: F)
        where F: Fn(Self)
    {
        for &dev in ALL_DEVICES {
            for &align in &[1, 2, 4, 8] {
                for &erased_val in &[0, 0xff] {
                    match Self::new(dev, align, erased_val) {
                        Some(run) => f(run),
                        None => warn!("Skipping {:?}, insufficient partitions", dev),
                    }
                }
            }
        }
    }

    /// Construct an `Images` that doesn't expect an upgrade to happen.
    pub fn make_no_upgrade_image(self) -> Images {
        let mut flash = self.flash;
        let images = self.slots.into_iter().map(|slots| {
            let primaries = install_image(&mut flash, &slots, 0, 32784, false);
            let upgrades = install_image(&mut flash, &slots, 1, 41928, false);
            OneImage {
                slots: slots,
                primaries: primaries,
                upgrades: upgrades,
            }}).collect();
        Images {
            flash: flash,
            areadesc: self.areadesc,
            images: images,
            total_count: None,
        }
    }

    /// Construct an `Images` for normal testing.
    pub fn make_image(self) -> Images {
        let mut images = self.make_no_upgrade_image();
        for image in &images.images {
            mark_upgrade(&mut images.flash, &image.slots[1]);
        }

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

    pub fn make_bad_secondary_slot_image(self) -> Images {
        let mut bad_flash = self.flash;
        let images = self.slots.into_iter().map(|slots| {
            let primaries = install_image(&mut bad_flash, &slots, 0, 32784, false);
            let upgrades = install_image(&mut bad_flash, &slots, 1, 41928, true);
            OneImage {
                slots: slots,
                primaries: primaries,
                upgrades: upgrades,
            }}).collect();
        Images {
            flash: bad_flash,
            areadesc: self.areadesc,
            images: images,
            total_count: None,
        }
    }

    /// Build the Flash and area descriptor for a given device.
    pub fn make_device(device: DeviceName, align: u8, erased_val: u8) -> (SimMultiFlash, AreaDesc) {
        match device {
            DeviceName::Stm32f4 => {
                // STM style flash.  Large sectors, with a large scratch area.
                let dev = SimFlash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024,
                                        64 * 1024,
                                        128 * 1024, 128 * 1024, 128 * 1024],
                                        align as usize, erased_val);
                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0, dev_id);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1, dev_id);
                areadesc.add_image(0x060000, 0x020000, FlashId::ImageScratch, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc)
            }
            DeviceName::K64f => {
                // NXP style flash.  Small sectors, one small sector for scratch.
                let dev = SimFlash::new(vec![4096; 128], align as usize, erased_val);

                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0, dev_id);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1, dev_id);
                areadesc.add_image(0x060000, 0x001000, FlashId::ImageScratch, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc)
            }
            DeviceName::K64fBig => {
                // Simulating an STM style flash on top of an NXP style flash.  Underlying flash device
                // uses small sectors, but we tell the bootloader they are large.
                let dev = SimFlash::new(vec![4096; 128], align as usize, erased_val);

                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_simple_image(0x020000, 0x020000, FlashId::Image0, dev_id);
                areadesc.add_simple_image(0x040000, 0x020000, FlashId::Image1, dev_id);
                areadesc.add_simple_image(0x060000, 0x020000, FlashId::ImageScratch, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc)
            }
            DeviceName::Nrf52840 => {
                // Simulating the flash on the nrf52840 with partitions set up so that the scratch size
                // does not divide into the image size.
                let dev = SimFlash::new(vec![4096; 128], align as usize, erased_val);

                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_image(0x008000, 0x034000, FlashId::Image0, dev_id);
                areadesc.add_image(0x03c000, 0x034000, FlashId::Image1, dev_id);
                areadesc.add_image(0x070000, 0x00d000, FlashId::ImageScratch, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc)
            }
            DeviceName::Nrf52840SpiFlash => {
                // Simulate nrf52840 with external SPI flash. The external SPI flash
                // has a larger sector size so for now store scratch on that flash.
                let dev0 = SimFlash::new(vec![4096; 128], align as usize, erased_val);
                let dev1 = SimFlash::new(vec![8192; 64], align as usize, erased_val);

                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(0, &dev0);
                areadesc.add_flash_sectors(1, &dev1);

                areadesc.add_image(0x008000, 0x068000, FlashId::Image0, 0);
                areadesc.add_image(0x000000, 0x068000, FlashId::Image1, 1);
                areadesc.add_image(0x068000, 0x018000, FlashId::ImageScratch, 1);

                let mut flash = SimMultiFlash::new();
                flash.insert(0, dev0);
                flash.insert(1, dev1);
                (flash, areadesc)
            }
            DeviceName::K64fMulti => {
                // NXP style flash, but larger, to support multiple images.
                let dev = SimFlash::new(vec![4096; 256], align as usize, erased_val);

                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0, dev_id);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1, dev_id);
                areadesc.add_image(0x060000, 0x001000, FlashId::ImageScratch, dev_id);
                areadesc.add_image(0x080000, 0x020000, FlashId::Image2, dev_id);
                areadesc.add_image(0x0a0000, 0x020000, FlashId::Image3, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc)
            }
        }
    }
}

impl Images {
    /// A simple upgrade without forced failures.
    ///
    /// Returns the number of flash operations which can later be used to
    /// inject failures at chosen steps.
    pub fn run_basic_upgrade(&self) -> Result<i32, ()> {
        let (flash, total_count) = self.try_upgrade(None);
        info!("Total flash operation count={}", total_count);

        if !self.verify_images(&flash, 0, 1) {
            warn!("Image mismatch after first boot");
            Err(())
        } else {
            Ok(total_count)
        }
    }

    pub fn run_basic_revert(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            return false;
        }

        let mut fails = 0;

        // FIXME: this test would also pass if no swap is ever performed???
        if Caps::SwapUpgrade.present() {
            for count in 2 .. 5 {
                info!("Try revert: {}", count);
                let flash = self.try_revert(count);
                if !self.verify_images(&flash, 0, 0) {
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
            let (flash, count) = self.try_upgrade(Some(i));
            info!("Second boot, count={}", count);
            if !self.verify_images(&flash, 0, 1) {
                warn!("FAIL at step {} of {}", i, total_flash_ops);
                fails += 1;
            }

            if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                     BOOT_FLAG_SET, BOOT_FLAG_SET) {
                warn!("Mismatched trailer for the primary slot");
                fails += 1;
            }

            if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                     BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
                warn!("Mismatched trailer for the secondary slot");
                fails += 1;
            }

            if Caps::SwapUpgrade.present() {
                if !self.verify_images(&flash, 1, 0) {
                    warn!("Secondary slot FAIL at step {} of {}",
                          i, total_flash_ops);
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

    pub fn run_perm_with_random_fails(&self, total_fails: usize) -> bool {
        let mut fails = 0;
        let total_flash_ops = self.total_count.unwrap();
        let (flash, total_counts) = self.try_random_fails(total_flash_ops, total_fails);
        info!("Random interruptions at reset points={:?}", total_counts);

        let primary_slot_ok = self.verify_images(&flash, 0, 1);
        let secondary_slot_ok = if Caps::SwapUpgrade.present() {
            // TODO: This result is ignored.
            self.verify_images(&flash, 1, 0)
        } else {
            true
        };
        if !primary_slot_ok || !secondary_slot_ok {
            error!("Image mismatch after random interrupts: primary slot={} \
                    secondary slot={}",
                   if primary_slot_ok { "ok" } else { "fail" },
                   if secondary_slot_ok { "ok" } else { "fail" });
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_SET) {
            error!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            error!("Mismatched trailer for the secondary slot");
            fails += 1;
        }

        if fails > 0 {
            error!("Error testing perm upgrade with {} fails", total_fails);
        }

        fails > 0
    }

    pub fn run_revert_with_fails(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            return false;
        }

        let mut fails = 0;

        if Caps::SwapUpgrade.present() {
            for i in 1 .. (self.total_count.unwrap() - 1) {
                info!("Try interruption at {}", i);
                if self.try_revert_with_fail_at(i) {
                    error!("Revert failed at interruption {}", i);
                    fails += 1;
                }
            }
        }

        fails > 0
    }

    pub fn run_norevert(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            return false;
        }

        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try norevert");

        // First do a normal upgrade...
        let (result, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        //FIXME: copy_done is written by boot_go, is it ok if no copy
        //       was ever done?

        if !self.verify_images(&flash, 0, 1) {
            warn!("Primary slot image verification FAIL");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot");
            fails += 1;
        }

        // Marks image in the primary slot as permanent,
        // no revert should happen...
        self.mark_permanent_upgrades(&mut flash, 0);

        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        let (result, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed second boot");
            fails += 1;
        }

        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !self.verify_images(&flash, 0, 1) {
            warn!("Failed image verification");
            fails += 1;
        }

        if fails > 0 {
            error!("Error running upgrade without revert");
        }

        fails > 0
    }

    // Tests a new image written to the primary slot that already has magic and
    // image_ok set while there is no image on the secondary slot, so no revert
    // should ever happen...
    pub fn run_norevert_newimage(&self) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try non-revert on imgtool generated image");

        self.mark_upgrades(&mut flash, 0);

        // This simulates writing an image created by imgtool to
        // the primary slot
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        // Run the bootloader...
        let (result, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        // State should not have changed
        if !self.verify_images(&flash, 0, 0) {
            warn!("Failed image verification");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot");
            fails += 1;
        }

        if fails > 0 {
            error!("Expected a non revert with new image");
        }

        fails > 0
    }

    // Tests a new image written to the primary slot that already has magic and
    // image_ok set while there is no image on the secondary slot, so no revert
    // should ever happen...
    pub fn run_signfail_upgrade(&self) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try upgrade image with bad signature");

        self.mark_upgrades(&mut flash, 0);
        self.mark_permanent_upgrades(&mut flash, 0);
        self.mark_upgrades(&mut flash, 1);

        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        // Run the bootloader...
        let (result, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed first boot");
            fails += 1;
        }

        // State should not have changed
        if !self.verify_images(&flash, 0, 0) {
            warn!("Failed image verification");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        if fails > 0 {
            error!("Expected an upgrade failure when image has bad signature");
        }

        fails > 0
    }

    fn trailer_sz(&self, align: usize) -> usize {
        c::boot_trailer_sz(align as u8) as usize
    }

    // FIXME: could get status sz from bootloader
    fn status_sz(&self, align: usize) -> usize {
        let bias = if Caps::EncRsa.present() || Caps::EncKw.present() {
            32
        } else {
            0
        };

        self.trailer_sz(align) - (16 + 32 + bias)
    }

    /// This test runs a simple upgrade with no fails in the images, but
    /// allowing for fails in the status area. This should run to the end
    /// and warn that write fails were detected...
    pub fn run_with_status_fails_complete(&self) -> bool {
        if !Caps::ValidatePrimarySlot.present() {
            return false;
        }

        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try swap with status fails");

        self.mark_permanent_upgrades(&mut flash, 1);
        self.mark_bad_status_with_rate(&mut flash, 0, 1.0);

        let (result, asserts) = c::boot_go(&mut flash, &self.areadesc, None, true);
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

        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        if !self.verify_images(&flash, 0, 1) {
            warn!("Failed image verification");
            fails += 1;
        }

        info!("validate primary slot enabled; \
               re-run of boot_go should just work");
        let (result, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
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
    pub fn run_with_status_fails_with_reset(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            false
        } else if Caps::ValidatePrimarySlot.present() {

            let mut flash = self.flash.clone();
            let mut fails = 0;
            let mut count = self.total_count.unwrap() / 2;

            //info!("count={}\n", count);

            info!("Try interrupted swap with status fails");

            self.mark_permanent_upgrades(&mut flash, 1);
            self.mark_bad_status_with_rate(&mut flash, 0, 0.5);

            // Should not fail, writing to bad regions does not assert
            let (_, asserts) = c::boot_go(&mut flash, &self.areadesc, Some(&mut count), true);
            if asserts != 0 {
                warn!("At least one assert() was called");
                fails += 1;
            }

            self.reset_bad_status(&mut flash, 0);

            info!("Resuming an interrupted swap operation");
            let (_, asserts) = c::boot_go(&mut flash, &self.areadesc, None, true);

            // This might throw no asserts, for large sector devices, where
            // a single failure writing is indistinguishable from no failure,
            // or throw a single assert for small sector devices that fail
            // multiple times...
            if asserts > 1 {
                warn!("Expected single assert validating the primary slot, \
                       more detected {}", asserts);
                fails += 1;
            }

            if fails > 0 {
                error!("Error running upgrade with status write fails");
            }

            fails > 0
        } else {
            let mut flash = self.flash.clone();
            let mut fails = 0;

            info!("Try interrupted swap with status fails");

            self.mark_permanent_upgrades(&mut flash, 1);
            self.mark_bad_status_with_rate(&mut flash, 0, 1.0);

            // This is expected to fail while writing to bad regions...
            let (_, asserts) = c::boot_go(&mut flash, &self.areadesc, None, true);
            if asserts == 0 {
                warn!("No assert() detected");
                fails += 1;
            }

            fails > 0
        }
    }

    /// Adds a new flash area that fails statistically
    fn mark_bad_status_with_rate(&self, flash: &mut SimMultiFlash, slot: usize,
                                 rate: f32) {
        if Caps::OverwriteUpgrade.present() {
            return;
        }

        // Set this for each image.
        for image in &self.images {
            let dev_id = &image.slots[slot].dev_id;
            let dev = flash.get_mut(&dev_id).unwrap();
            let align = dev.align();
            let off = &image.slots[slot].base_off;
            let len = &image.slots[slot].len;
            let status_off = off + len - self.trailer_sz(align);

            // Mark the status area as a bad area
            let _ = dev.add_bad_region(status_off, self.status_sz(align), rate);
        }
    }

    fn reset_bad_status(&self, flash: &mut SimMultiFlash, slot: usize) {
        if !Caps::ValidatePrimarySlot.present() {
            return;
        }

        for image in &self.images {
            let dev_id = &image.slots[slot].dev_id;
            let dev = flash.get_mut(&dev_id).unwrap();
            dev.reset_bad_regions();

            // Disabling write verification the only assert triggered by
            // boot_go should be checking for integrity of status bytes.
            dev.set_verify_writes(false);
        }
    }

    /// Test a boot, optionally stopping after 'n' flash options.  Returns a count
    /// of the number of flash operations done total.
    fn try_upgrade(&self, stop: Option<i32>) -> (SimMultiFlash, i32) {
        // Clone the flash to have a new copy.
        let mut flash = self.flash.clone();

        self.mark_permanent_upgrades(&mut flash, 1);

        let mut counter = stop.unwrap_or(0);

        let (first_interrupted, count) = match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
            (-0x13579, _) => (true, stop.unwrap()),
            (0, _) => (false, -counter),
            (x, _) => panic!("Unknown return: {}", x),
        };

        counter = 0;
        if first_interrupted {
            // fl.dump();
            match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
                (-0x13579, _) => panic!("Shouldn't stop again"),
                (0, _) => (),
                (x, _) => panic!("Unknown return: {}", x),
            }
        }

        (flash, count - counter)
    }

    fn try_revert(&self, count: usize) -> SimMultiFlash {
        let mut flash = self.flash.clone();

        // fl.write_file("image0.bin").unwrap();
        for i in 0 .. count {
            info!("Running boot pass {}", i + 1);
            assert_eq!(c::boot_go(&mut flash, &self.areadesc, None, false), (0, 0));
        }
        flash
    }

    fn try_revert_with_fail_at(&self, stop: i32) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        let mut counter = stop;
        let (x, _) = c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false);
        if x != -0x13579 {
            warn!("Should have stopped at interruption point");
            fails += 1;
        }

        if !self.verify_trailers(&flash, 0, None, None, BOOT_FLAG_UNSET) {
            warn!("copy_done should be unset");
            fails += 1;
        }

        let (x, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if x != 0 {
            warn!("Should have finished upgrade");
            fails += 1;
        }

        if !self.verify_images(&flash, 0, 1) {
            warn!("Image in the primary slot before revert is invalid at stop={}",
                  stop);
            fails += 1;
        }
        if !self.verify_images(&flash, 1, 0) {
            warn!("Image in the secondary slot before revert is invalid at stop={}",
                  stop);
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot before revert");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot before revert");
            fails += 1;
        }

        // Do Revert
        let (x, _) = c::boot_go(&mut flash, &self.areadesc, None, false);
        if x != 0 {
            warn!("Should have finished a revert");
            fails += 1;
        }

        if !self.verify_images(&flash, 0, 0) {
            warn!("Image in the primary slot after revert is invalid at stop={}",
                  stop);
            fails += 1;
        }
        if !self.verify_images(&flash, 1, 1) {
            warn!("Image in the secondary slot after revert is invalid at stop={}",
                  stop);
            fails += 1;
        }
        if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                 BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the secondary slot after revert");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot after revert");
            fails += 1;
        }

        fails > 0
    }

    fn try_random_fails(&self, total_ops: i32, count: usize) -> (SimMultiFlash, Vec<i32>) {
        let mut flash = self.flash.clone();

        self.mark_permanent_upgrades(&mut flash, 1);

        let mut rng = rand::thread_rng();
        let mut resets = vec![0i32; count];
        let mut remaining_ops = total_ops;
        for i in 0 .. count {
            let ops = Range::new(1, remaining_ops / 2);
            let reset_counter = ops.ind_sample(&mut rng);
            let mut counter = reset_counter;
            match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
                (0, _) | (-0x13579, _) => (),
                (x, _) => panic!("Unknown return: {}", x),
            }
            remaining_ops -= reset_counter;
            resets[i] = reset_counter;
        }

        match c::boot_go(&mut flash, &self.areadesc, None, false) {
            (-0x13579, _) => panic!("Should not be have been interrupted!"),
            (0, _) => (),
            (x, _) => panic!("Unknown return: {}", x),
        }

        (flash, resets)
    }

    /// Verify the image in the given flash device, the specified slot
    /// against the expected image.
    fn verify_images(&self, flash: &SimMultiFlash, slot: usize, against: usize) -> bool {
        for image in &self.images {
            if !verify_image(flash, &image.slots, slot,
                             match against {
                                 0 => &image.primaries,
                                 1 => &image.upgrades,
                                 _ => panic!("Invalid 'against'"),
                             }) {
                return false;
            }
        }
        true
    }

    /// Verify that the trailers of the images have the specified
    /// values.
    fn verify_trailers(&self, flash: &SimMultiFlash, slot: usize,
                       magic: Option<u8>, image_ok: Option<u8>,
                       copy_done: Option<u8>) -> bool {
        for image in &self.images {
            if !verify_trailer(flash, &image.slots, slot,
                               magic, image_ok, copy_done) {
                return false;
            }
        }
        true
    }

    /// Mark each of the images for permanent upgrade.
    fn mark_permanent_upgrades(&self, flash: &mut SimMultiFlash, slot: usize) {
        for image in &self.images {
            mark_permanent_upgrade(flash, &image.slots[slot]);
        }
    }

    /// Mark each of the images for permanent upgrade.
    fn mark_upgrades(&self, flash: &mut SimMultiFlash, slot: usize) {
        for image in &self.images {
            mark_upgrade(flash, &image.slots[slot]);
        }
    }
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
fn install_image(flash: &mut SimMultiFlash, slots: &[SlotInfo], slot: usize, len: usize,
                 bad_sig: bool) -> ImageData {
    let offset = slots[slot].base_off;
    let slot_len = slots[slot].len;
    let dev_id = slots[slot].dev_id;

    let mut tlv: Box<dyn ManifestGen> = Box::new(make_tlv());

    const HDR_SIZE: usize = 32;

    // Generate a boot header.  Note that the size doesn't include the header.
    let header = ImageHeader {
        magic: tlv.get_magic(),
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

    // Since images are always non-encrypted in the primary slot, we first write
    // an encrypted image, re-read to use for verification, erase + flash
    // un-encrypted. In the secondary slot the image is written un-encrypted,
    // and if encryption is requested, it follows an erase + flash encrypted.

    let dev = flash.get_mut(&dev_id).unwrap();

    if slot == 0 {
        let enc_copy: Option<Vec<u8>>;

        if is_encrypted {
            dev.write(offset, &encbuf).unwrap();

            let mut enc = vec![0u8; encbuf.len()];
            dev.read(offset, &mut enc).unwrap();

            enc_copy = Some(enc);

            dev.erase(offset, slot_len).unwrap();
        } else {
            enc_copy = None;
        }

        dev.write(offset, &buf).unwrap();

        let mut copy = vec![0u8; buf.len()];
        dev.read(offset, &mut copy).unwrap();

        ImageData {
            plain: copy,
            cipher: enc_copy,
        }
    } else {

        dev.write(offset, &buf).unwrap();

        let mut copy = vec![0u8; buf.len()];
        dev.read(offset, &mut copy).unwrap();

        let enc_copy: Option<Vec<u8>>;

        if is_encrypted {
            dev.erase(offset, slot_len).unwrap();

            dev.write(offset, &encbuf).unwrap();

            let mut enc = vec![0u8; encbuf.len()];
            dev.read(offset, &mut enc).unwrap();

            enc_copy = Some(enc);
        } else {
            enc_copy = None;
        }

        ImageData {
            plain: copy,
            cipher: enc_copy,
        }
    }
}

fn make_tlv() -> TlvGen {
    if Caps::EcdsaP224.present() {
        panic!("Ecdsa P224 not supported in Simulator");
    }

    if Caps::EncKw.present() {
        if Caps::RSA2048.present() {
            TlvGen::new_rsa_kw()
        } else if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa_kw()
        } else {
            TlvGen::new_enc_kw()
        }
    } else if Caps::EncRsa.present() {
        if Caps::RSA2048.present() {
            TlvGen::new_sig_enc_rsa()
        } else {
            TlvGen::new_enc_rsa()
        }
    } else {
        // The non-encrypted configuration.
        if Caps::RSA2048.present() {
            TlvGen::new_rsa_pss()
        } else if Caps::RSA3072.present() {
            TlvGen::new_rsa3072_pss()
        } else if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa()
        } else {
            TlvGen::new_hash_only()
        }
    }
}

impl ImageData {
    /// Find the image contents for the given slot.  This assumes that slot 0
    /// is unencrypted, and slot 1 is encrypted.
    fn find(&self, slot: usize) -> &Vec<u8> {
        let encrypted = Caps::EncRsa.present() || Caps::EncKw.present();
        match (encrypted, slot) {
            (false, _) => &self.plain,
            (true, 0) => &self.plain,
            (true, 1) => self.cipher.as_ref().expect("Invalid image"),
            _ => panic!("Invalid slot requested"),
        }
    }
}

/// Verify that given image is present in the flash at the given offset.
fn verify_image(flash: &SimMultiFlash, slots: &[SlotInfo], slot: usize,
                images: &ImageData) -> bool {
    let image = images.find(slot);
    let buf = image.as_slice();
    let dev_id = slots[slot].dev_id;

    let mut copy = vec![0u8; buf.len()];
    let offset = slots[slot].base_off;
    let dev = flash.get(&dev_id).unwrap();
    dev.read(offset, &mut copy).unwrap();

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

fn verify_trailer(flash: &SimMultiFlash, slots: &[SlotInfo], slot: usize,
                  magic: Option<u8>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    if Caps::OverwriteUpgrade.present() {
        return true;
    }

    let offset = slots[slot].trailer_off + c::boot_max_align();
    let dev_id = slots[slot].dev_id;
    let mut copy = vec![0u8; c::boot_magic_sz() + c::boot_max_align() * 3];
    let mut failed = false;

    let dev = flash.get(&dev_id).unwrap();
    let erased_val = dev.erased_val();
    dev.read(offset, &mut copy).unwrap();

    failed |= match magic {
        Some(v) => {
            if v == 1 && &copy[24..] != MAGIC.unwrap() {
                warn!("\"magic\" mismatch at {:#x}", offset);
                true
            } else if v == 3 {
                let expected = [erased_val; 16];
                if &copy[24..] != expected {
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
            if (v == 1 && copy[16] != v) || (v == 3 && copy[16] != erased_val) {
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
            if (v == 1 && copy[8] != v) || (v == 3 && copy[8] != erased_val) {
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
pub struct SlotInfo {
    pub base_off: usize,
    pub trailer_off: usize,
    pub len: usize,
    pub dev_id: u8,
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
pub fn mark_upgrade(flash: &mut SimMultiFlash, slot: &SlotInfo) {
    let dev = flash.get_mut(&slot.dev_id).unwrap();
    let offset = slot.trailer_off + c::boot_max_align() * 4;
    dev.write(offset, MAGIC.unwrap()).unwrap();
}

/// Writes the image_ok flag which, guess what, tells the bootloader
/// the this image is ok (not a test, and no revert is to be performed).
fn mark_permanent_upgrade(flash: &mut SimMultiFlash, slot: &SlotInfo) {
    let dev = flash.get_mut(&slot.dev_id).unwrap();
    let mut ok = [dev.erased_val(); 8];
    ok[0] = 1u8;
    let off = slot.trailer_off + c::boot_max_align() * 3;
    let align = dev.align();
    dev.write(off, &ok[..align]).unwrap();
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

pub fn show_sizes() {
    // This isn't panic safe.
    for min in &[1, 2, 4, 8] {
        let msize = c::boot_trailer_sz(*min);
        println!("{:2}: {} (0x{:x})", min, msize, msize);
    }
}
