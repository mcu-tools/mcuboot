// Copyright (c) 2019 Linaro LTD
// Copyright (c) 2019-2020 JUUL Labs
// Copyright (c) 2019-2021 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

use byteorder::{
    LittleEndian, WriteBytesExt,
};
use log::{
    Level::Info,
    error,
    info,
    log_enabled,
    warn,
};
use rand::{
    Rng, RngCore, SeedableRng,
    rngs::SmallRng,
};
use std::{
    collections::HashSet,
    io::{Cursor, Write},
    mem,
    slice,
};
use aes_ctr::{
    Aes128Ctr,
    Aes256Ctr,
    stream_cipher::{
        generic_array::GenericArray,
        NewStreamCipher,
        SyncStreamCipher,
    },
};

use simflash::{Flash, SimFlash, SimMultiFlash};
use mcuboot_sys::{c, AreaDesc, FlashId};
use crate::{
    ALL_DEVICES,
    DeviceName,
};
use crate::caps::Caps;
use crate::depends::{
    BoringDep,
    Depender,
    DepTest,
    DepType,
    NO_DEPS,
    PairDep,
    UpgradeInfo,
};
use crate::tlv::{ManifestGen, TlvGen, TlvFlags};
use typenum::{U32, U16};

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
    pub fn new(device: DeviceName, align: usize, erased_val: u8) -> Result<Self, String> {
        let (flash, areadesc, unsupported_caps) = Self::make_device(device, align, erased_val);

        for cap in unsupported_caps {
            if cap.present() {
                return Err(format!("unsupported {:?}", cap));
            }
        }

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
                None => return Err("insufficient partitions".to_string()),
            };
            let id1 = match image {
                0 => FlashId::Image1,
                1 => FlashId::Image3,
                _ => panic!("More than 2 images not supported"),
            };
            let (secondary_base, secondary_len, secondary_dev_id) = match areadesc.find(id1) {
                Some(info) => info,
                None => return Err("insufficient partitions".to_string()),
            };

            let offset_from_end = c::boot_magic_sz() + c::boot_max_align() * 4;

            // Construct a primary image.
            let primary = SlotInfo {
                base_off: primary_base as usize,
                trailer_off: primary_base + primary_len - offset_from_end,
                len: primary_len as usize,
                dev_id: primary_dev_id,
                index: 0,
            };

            // And an upgrade image.
            let secondary = SlotInfo {
                base_off: secondary_base as usize,
                trailer_off: secondary_base + secondary_len - offset_from_end,
                len: secondary_len as usize,
                dev_id: secondary_dev_id,
                index: 1,
            };

            slots.push([primary, secondary]);
        }

        Ok(ImagesBuilder {
            flash,
            areadesc,
            slots,
        })
    }

    pub fn each_device<F>(f: F)
        where F: Fn(Self)
    {
        for &dev in ALL_DEVICES {
            for &align in test_alignments() {
                for &erased_val in &[0, 0xff] {
                    match Self::new(dev, align, erased_val) {
                        Ok(run) => f(run),
                        Err(msg) => warn!("Skipping {}: {}", dev, msg),
                    }
                }
            }
        }
    }

    /// Construct an `Images` that doesn't expect an upgrade to happen.
    pub fn make_no_upgrade_image(self, deps: &DepTest) -> Images {
        let num_images = self.num_images();
        let mut flash = self.flash;
        let images = self.slots.into_iter().enumerate().map(|(image_num, slots)| {
            let dep: Box<dyn Depender> = if num_images > 1 {
                Box::new(PairDep::new(num_images, image_num, deps))
            } else {
                Box::new(BoringDep::new(image_num, deps))
            };
            let primaries = install_image(&mut flash, &slots[0], 42784, &*dep, false);
            let upgrades = match deps.depends[image_num] {
                DepType::NoUpgrade => install_no_image(),
                _ => install_image(&mut flash, &slots[1], 46928, &*dep, false)
            };
            OneImage {
                slots,
                primaries,
                upgrades,
            }}).collect();
        install_ptable(&mut flash, &self.areadesc);
        Images {
            flash,
            areadesc: self.areadesc,
            images,
            total_count: None,
        }
    }

    pub fn make_image(self, deps: &DepTest, permanent: bool) -> Images {
        let mut images = self.make_no_upgrade_image(deps);
        for image in &images.images {
            mark_upgrade(&mut images.flash, &image.slots[1]);
        }

        // upgrades without fails, counts number of flash operations
        let total_count = match images.run_basic_upgrade(permanent) {
            Some(v)  => v,
            None =>
                if deps.upgrades.iter().any(|u| *u == UpgradeInfo::Held) {
                    0
                } else {
                    panic!("Unable to perform basic upgrade");
                }
        };

        images.total_count = Some(total_count);
        images
    }

    pub fn make_bad_secondary_slot_image(self) -> Images {
        let mut bad_flash = self.flash;
        let images = self.slots.into_iter().enumerate().map(|(image_num, slots)| {
            let dep = BoringDep::new(image_num, &NO_DEPS);
            let primaries = install_image(&mut bad_flash, &slots[0], 32784, &dep, false);
            let upgrades = install_image(&mut bad_flash, &slots[1], 41928, &dep, true);
            OneImage {
                slots,
                primaries,
                upgrades,
            }}).collect();
        Images {
            flash: bad_flash,
            areadesc: self.areadesc,
            images,
            total_count: None,
        }
    }

    pub fn make_erased_secondary_image(self) -> Images {
        let mut flash = self.flash;
        let images = self.slots.into_iter().enumerate().map(|(image_num, slots)| {
            let dep = BoringDep::new(image_num, &NO_DEPS);
            let primaries = install_image(&mut flash, &slots[0], 32784, &dep, false);
            let upgrades = install_no_image();
            OneImage {
                slots,
                primaries,
                upgrades,
            }}).collect();
        Images {
            flash,
            areadesc: self.areadesc,
            images,
            total_count: None,
        }
    }

    pub fn make_bootstrap_image(self) -> Images {
        let mut flash = self.flash;
        let images = self.slots.into_iter().enumerate().map(|(image_num, slots)| {
            let dep = BoringDep::new(image_num, &NO_DEPS);
            let primaries = install_no_image();
            let upgrades = install_image(&mut flash, &slots[1], 32784, &dep, false);
            OneImage {
                slots,
                primaries,
                upgrades,
            }}).collect();
        Images {
            flash,
            areadesc: self.areadesc,
            images,
            total_count: None,
        }
    }

    /// Build the Flash and area descriptor for a given device.
    pub fn make_device(device: DeviceName, align: usize, erased_val: u8) -> (SimMultiFlash, AreaDesc, &'static [Caps]) {
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
                (flash, areadesc, &[Caps::SwapUsingMove])
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
                (flash, areadesc, &[])
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
                (flash, areadesc, &[Caps::SwapUsingMove])
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
                (flash, areadesc, &[])
            }
            DeviceName::Nrf52840UnequalSlots => {
                let dev = SimFlash::new(vec![4096; 128], align as usize, erased_val);

                let dev_id = 0;
                let mut areadesc = AreaDesc::new();
                areadesc.add_flash_sectors(dev_id, &dev);
                areadesc.add_image(0x008000, 0x03c000, FlashId::Image0, dev_id);
                areadesc.add_image(0x044000, 0x03b000, FlashId::Image1, dev_id);

                let mut flash = SimMultiFlash::new();
                flash.insert(dev_id, dev);
                (flash, areadesc, &[Caps::SwapUsingScratch, Caps::OverwriteUpgrade])
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
                (flash, areadesc, &[Caps::SwapUsingMove])
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
                (flash, areadesc, &[])
            }
        }
    }

    pub fn num_images(&self) -> usize {
        self.slots.len()
    }
}

impl Images {
    /// A simple upgrade without forced failures.
    ///
    /// Returns the number of flash operations which can later be used to
    /// inject failures at chosen steps.  Returns None if it was unable to
    /// count the operations in a basic upgrade.
    pub fn run_basic_upgrade(&self, permanent: bool) -> Option<i32> {
        let (flash, total_count) = self.try_upgrade(None, permanent);
        info!("Total flash operation count={}", total_count);

        if !self.verify_images(&flash, 0, 1) {
            warn!("Image mismatch after first boot");
            None
        } else {
            Some(total_count)
        }
    }

    pub fn run_bootstrap(&self) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        if Caps::Bootstrap.present() {
            info!("Try bootstraping image in the primary");

            if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
                warn!("Failed first boot");
                fails += 1;
            }

            if !self.verify_images(&flash, 0, 1) {
                warn!("Image in the first slot was not bootstrapped");
                fails += 1;
            }

            if !self.verify_trailers(&flash, 0, BOOT_MAGIC_GOOD,
                                     BOOT_FLAG_SET, BOOT_FLAG_SET) {
                warn!("Mismatched trailer for the primary slot");
                fails += 1;
            }
        }

        if fails > 0 {
            error!("Expected trailer on secondary slot to be erased");
        }

        fails > 0
    }


    /// Test a simple upgrade, with dependencies given, and verify that the
    /// image does as is described in the test.
    pub fn run_check_deps(&self, deps: &DepTest) -> bool {
        let (flash, _) = self.try_upgrade(None, true);

        self.verify_dep_images(&flash, deps)
    }

    fn is_swap_upgrade(&self) -> bool {
        Caps::SwapUsingScratch.present() || Caps::SwapUsingMove.present()
    }

    pub fn run_basic_revert(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            return false;
        }

        let mut fails = 0;

        // FIXME: this test would also pass if no swap is ever performed???
        if self.is_swap_upgrade() {
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
            let (flash, count) = self.try_upgrade(Some(i), true);
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

            if self.is_swap_upgrade() && !self.verify_images(&flash, 1, 0) {
                warn!("Secondary slot FAIL at step {} of {}",
                    i, total_flash_ops);
                fails += 1;
            }
        }

        if fails > 0 {
            error!("{} out of {} failed {:.2}%", fails, total_flash_ops,
                   fails as f32 * 100.0 / total_flash_ops as f32);
        }

        fails > 0
    }

    pub fn run_perm_with_random_fails(&self, total_fails: usize) -> bool {
        let mut fails = 0;
        let total_flash_ops = self.total_count.unwrap();
        let (flash, total_counts) = self.try_random_fails(total_flash_ops, total_fails);
        info!("Random interruptions at reset points={:?}", total_counts);

        let primary_slot_ok = self.verify_images(&flash, 0, 1);
        let secondary_slot_ok = if self.is_swap_upgrade() {
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

        if self.is_swap_upgrade() {
            for i in 1 .. self.total_count.unwrap() {
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
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
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

        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
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

    // Test that an upgrade is rejected.  Assumes that the image was build
    // such that the upgrade is instead a downgrade.
    pub fn run_nodowngrade(&self) -> bool {
        if !Caps::DowngradePrevention.present() {
            return false;
        }

        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try no downgrade");

        // First, do a normal upgrade.
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
            warn!("Failed first boot");
            fails += 1;
        }

        if !self.verify_images(&flash, 0, 0) {
            warn!("Failed verification after downgrade rejection");
            fails += 1;
        }

        if fails > 0 {
            error!("Error testing downgrade rejection");
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
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
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
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
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

    // Should detect there is a leftover trailer in an otherwise erased
    // secondary slot and erase its trailer.
    pub fn run_secondary_leftover_trailer(&self) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        info!("Try with a leftover trailer in the secondary; must be erased");

        // Add a trailer on the secondary slot
        self.mark_permanent_upgrades(&mut flash, 1);
        self.mark_upgrades(&mut flash, 1);

        // Run the bootloader...
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
            warn!("Failed first boot");
            fails += 1;
        }

        // State should not have changed
        if !self.verify_images(&flash, 0, 0) {
            warn!("Failed image verification");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot");
            fails += 1;
        }

        if fails > 0 {
            error!("Expected trailer on secondary slot to be erased");
        }

        fails > 0
    }

    fn trailer_sz(&self, align: usize) -> usize {
        c::boot_trailer_sz(align as u32) as usize
    }

    fn status_sz(&self, align: usize) -> usize {
        c::boot_status_sz(align as u32) as usize
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

        let result = c::boot_go(&mut flash, &self.areadesc, None, true);
        if !result.success() {
            warn!("Failed!");
            fails += 1;
        }

        // Failed writes to the marked "bad" region don't assert anymore.
        // Any detected assert() is happening in another part of the code.
        if result.asserts() != 0 {
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
        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
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
            let asserts = c::boot_go(&mut flash, &self.areadesc, Some(&mut count), true).asserts();
            if asserts != 0 {
                warn!("At least one assert() was called");
                fails += 1;
            }

            self.reset_bad_status(&mut flash, 0);

            info!("Resuming an interrupted swap operation");
            let asserts = c::boot_go(&mut flash, &self.areadesc, None, true).asserts();

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
            let asserts = c::boot_go(&mut flash, &self.areadesc, None, true).asserts();
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
    fn try_upgrade(&self, stop: Option<i32>, permanent: bool) -> (SimMultiFlash, i32) {
        // Clone the flash to have a new copy.
        let mut flash = self.flash.clone();

        if permanent {
            self.mark_permanent_upgrades(&mut flash, 1);
        }

        let mut counter = stop.unwrap_or(0);

        let (first_interrupted, count) = match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
            x if x.interrupted() => (true, stop.unwrap()),
            x if x.success() => (false, -counter),
            x => panic!("Unknown return: {:?}", x),
        };

        counter = 0;
        if first_interrupted {
            // fl.dump();
            match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
                x if x.interrupted() => panic!("Shouldn't stop again"),
                x if x.success() => (),
                x => panic!("Unknown return: {:?}", x),
            }
        }

        (flash, count - counter)
    }

    fn try_revert(&self, count: usize) -> SimMultiFlash {
        let mut flash = self.flash.clone();

        // fl.write_file("image0.bin").unwrap();
        for i in 0 .. count {
            info!("Running boot pass {}", i + 1);
            assert!(c::boot_go(&mut flash, &self.areadesc, None, false).success_no_asserts());
        }
        flash
    }

    fn try_revert_with_fail_at(&self, stop: i32) -> bool {
        let mut flash = self.flash.clone();
        let mut fails = 0;

        let mut counter = stop;
        if !c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false).interrupted() {
            warn!("Should have stopped test at interruption point");
            fails += 1;
        }

        // In a multi-image setup, copy done might be set if any number of
        // images was already successfully swapped.
        if !self.verify_trailers_loose(&flash, 0, None, None, BOOT_FLAG_UNSET) {
            warn!("copy_done should be unset");
            fails += 1;
        }

        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
            warn!("Should have finished test upgrade");
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
        let mut counter = stop;
        if !c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false).interrupted() {
            warn!("Should have stopped revert at interruption point");
            fails += 1;
        }

        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
            warn!("Should have finished revert upgrade");
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
            warn!("Mismatched trailer for the primary slot after revert");
            fails += 1;
        }
        if !self.verify_trailers(&flash, 1, BOOT_MAGIC_UNSET,
                                 BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot after revert");
            fails += 1;
        }

        if !c::boot_go(&mut flash, &self.areadesc, None, false).success() {
            warn!("Should have finished 3rd boot");
            fails += 1;
        }

        if !self.verify_images(&flash, 0, 0) {
            warn!("Image in the primary slot is invalid on 1st boot after revert");
            fails += 1;
        }
        if !self.verify_images(&flash, 1, 1) {
            warn!("Image in the secondary slot is invalid on 1st boot after revert");
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
        for reset in &mut resets {
            let reset_counter = rng.gen_range(1, remaining_ops / 2);
            let mut counter = reset_counter;
            match c::boot_go(&mut flash, &self.areadesc, Some(&mut counter), false) {
                x if x.interrupted() => (),
                x => panic!("Unknown return: {:?}", x),
            }
            remaining_ops -= reset_counter;
            *reset = reset_counter;
        }

        match c::boot_go(&mut flash, &self.areadesc, None, false) {
            x if x.interrupted() => panic!("Should not be have been interrupted!"),
            x if x.success() => (),
            x => panic!("Unknown return: {:?}", x),
        }

        (flash, resets)
    }

    /// Verify the image in the given flash device, the specified slot
    /// against the expected image.
    fn verify_images(&self, flash: &SimMultiFlash, slot: usize, against: usize) -> bool {
        self.images.iter().all(|image| {
            verify_image(flash, &image.slots[slot],
                         match against {
                             0 => &image.primaries,
                             1 => &image.upgrades,
                             _ => panic!("Invalid 'against'")
                             })
        })
    }

    /// Verify the images, according to the dependency test.
    fn verify_dep_images(&self, flash: &SimMultiFlash, deps: &DepTest) -> bool {
        for (image_num, (image, upgrade)) in self.images.iter().zip(deps.upgrades.iter()).enumerate() {
            info!("Upgrade: slot:{}, {:?}", image_num, upgrade);
            if !verify_image(flash, &image.slots[0],
                            match upgrade {
                                UpgradeInfo::Upgraded => &image.upgrades,
                                UpgradeInfo::Held => &image.primaries,
                            }) {
                error!("Failed to upgrade properly: image: {}, upgrade: {:?}", image_num, upgrade);
                return true;
            }
        }

        false
    }

    /// Verify that at least one of the trailers of the images have the
    /// specified values.
    fn verify_trailers_loose(&self, flash: &SimMultiFlash, slot: usize,
                             magic: Option<u8>, image_ok: Option<u8>,
                             copy_done: Option<u8>) -> bool {
        self.images.iter().any(|image| {
            verify_trailer(flash, &image.slots[slot],
                           magic, image_ok, copy_done)
        })
    }

    /// Verify that the trailers of the images have the specified
    /// values.
    fn verify_trailers(&self, flash: &SimMultiFlash, slot: usize,
                       magic: Option<u8>, image_ok: Option<u8>,
                       copy_done: Option<u8>) -> bool {
        self.images.iter().all(|image| {
            verify_trailer(flash, &image.slots[slot],
                           magic, image_ok, copy_done)
        })
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

    /// Dump out the flash image(s) to one or more files for debugging
    /// purposes.  The names will be written as either "{prefix}.mcubin" or
    /// "{prefix}-001.mcubin" depending on how many images there are.
    pub fn debug_dump(&self, prefix: &str) {
        for (id, fdev) in &self.flash {
            let name = if self.flash.len() == 1 {
                format!("{}.mcubin", prefix)
            } else {
                format!("{}-{:>0}.mcubin", prefix, id)
            };
            fdev.write_file(&name).unwrap();
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
    println!();
}

/// Install a "program" into the given image.  This fakes the image header, or at least all of the
/// fields used by the given code.  Returns a copy of the image that was written.
fn install_image(flash: &mut SimMultiFlash, slot: &SlotInfo, len: usize,
                 deps: &dyn Depender, bad_sig: bool) -> ImageData {
    let offset = slot.base_off;
    let slot_len = slot.len;
    let dev_id = slot.dev_id;

    let mut tlv: Box<dyn ManifestGen> = Box::new(make_tlv());

    // Add the dependencies early to the tlv.
    for dep in deps.my_deps(offset, slot.index) {
        tlv.add_dependency(deps.other_id(), &dep);
    }

    const HDR_SIZE: usize = 32;

    // Generate a boot header.  Note that the size doesn't include the header.
    let header = ImageHeader {
        magic: tlv.get_magic(),
        load_addr: 0,
        hdr_size: HDR_SIZE as u16,
        protect_tlv_size: tlv.protect_size(),
        img_size: len as u32,
        flags: tlv.get_flags(),
        ver: deps.my_version(offset, slot.index),
        _pad2: 0,
    };

    let mut b_header = [0; HDR_SIZE];
    b_header[..32].clone_from_slice(header.as_raw());
    assert_eq!(b_header.len(), HDR_SIZE);

    tlv.add_bytes(&b_header);

    // The core of the image itself is just pseudorandom data.
    let mut b_img = vec![0; len];
    splat(&mut b_img, offset);

    // Add some information at the start of the payload to make it easier
    // to see what it is.  This will fail if the image itself is too small.
    {
        let mut wr = Cursor::new(&mut b_img);
        writeln!(&mut wr, "offset: {:#x}, dev_id: {:#x}, slot_info: {:?}",
                 offset, dev_id, slot).unwrap();
        writeln!(&mut wr, "version: {:?}", deps.my_version(offset, slot.index)).unwrap();
    }

    // TLV signatures work over plain image
    tlv.add_bytes(&b_img);

    // Generate encrypted images
    let flag = TlvFlags::ENCRYPTED_AES128 as u32 | TlvFlags::ENCRYPTED_AES256 as u32;
    let is_encrypted = (tlv.get_flags() & flag) != 0;
    let mut b_encimg = vec![];
    if is_encrypted {
        let flag = TlvFlags::ENCRYPTED_AES256 as u32;
        let aes256 = (tlv.get_flags() & flag) == flag;
        tlv.generate_enc_key();
        let enc_key = tlv.get_enc_key();
        let nonce = GenericArray::from_slice(&[0; 16]);
        b_encimg = b_img.clone();
        if aes256 {
            let key: &GenericArray<u8, U32> = GenericArray::from_slice(enc_key.as_slice());
            let mut cipher = Aes256Ctr::new(&key, &nonce);
            cipher.apply_keystream(&mut b_encimg);
        } else {
            let key: &GenericArray<u8, U16> = GenericArray::from_slice(enc_key.as_slice());
            let mut cipher = Aes128Ctr::new(&key, &nonce);
            cipher.apply_keystream(&mut b_encimg);
        }
    }

    // Build the TLV itself.
    if bad_sig {
        tlv.corrupt_sig();
    }
    let mut b_tlv = tlv.make_tlv();

    let dev = flash.get_mut(&dev_id).unwrap();

    let mut buf = vec![];
    buf.append(&mut b_header.to_vec());
    buf.append(&mut b_img);
    buf.append(&mut b_tlv.clone());

    // Pad the buffer to a multiple of the flash alignment.
    let align = dev.align();
    while buf.len() % align != 0 {
        buf.push(dev.erased_val());
    }

    let mut encbuf = vec![];
    if is_encrypted {
        encbuf.append(&mut b_header.to_vec());
        encbuf.append(&mut b_encimg);
        encbuf.append(&mut b_tlv);

        while encbuf.len() % align != 0 {
            encbuf.push(dev.erased_val());
        }
    }

    // Since images are always non-encrypted in the primary slot, we first write
    // an encrypted image, re-read to use for verification, erase + flash
    // un-encrypted. In the secondary slot the image is written un-encrypted,
    // and if encryption is requested, it follows an erase + flash encrypted.

    if slot.index == 0 {
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

/// Install no image.  This is used when no upgrade happens.
fn install_no_image() -> ImageData {
    ImageData {
        plain: vec![],
        cipher: None,
    }
}

fn make_tlv() -> TlvGen {
    if Caps::EcdsaP224.present() {
        panic!("Ecdsa P224 not supported in Simulator");
    }
    let mut aes_key_size = 128;
    if Caps::Aes256.present() {
        aes_key_size = 256;
    }

    if Caps::EncKw.present() {
        if Caps::RSA2048.present() {
            TlvGen::new_rsa_kw(aes_key_size)
        } else if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa_kw(aes_key_size)
        } else {
            TlvGen::new_enc_kw(aes_key_size)
        }
    } else if Caps::EncRsa.present() {
        if Caps::RSA2048.present() {
            TlvGen::new_sig_enc_rsa(aes_key_size)
        } else {
            TlvGen::new_enc_rsa(aes_key_size)
        }
    } else if Caps::EncEc256.present() {
        if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa_ecies_p256(aes_key_size)
        } else {
            TlvGen::new_ecies_p256(aes_key_size)
        }
    } else if Caps::EncX25519.present() {
        if Caps::Ed25519.present() {
            TlvGen::new_ed25519_ecies_x25519(aes_key_size)
        } else {
            TlvGen::new_ecies_x25519(aes_key_size)
        }
    } else {
        // The non-encrypted configuration.
        if Caps::RSA2048.present() {
            TlvGen::new_rsa_pss()
        } else if Caps::RSA3072.present() {
            TlvGen::new_rsa3072_pss()
        } else if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa()
        } else if Caps::Ed25519.present() {
            TlvGen::new_ed25519()
        } else {
            TlvGen::new_hash_only()
        }
    }
}

impl ImageData {
    /// Find the image contents for the given slot.  This assumes that slot 0
    /// is unencrypted, and slot 1 is encrypted.
    fn find(&self, slot: usize) -> &Vec<u8> {
        let encrypted = Caps::EncRsa.present() || Caps::EncKw.present() ||
            Caps::EncEc256.present() || Caps::EncX25519.present();
        match (encrypted, slot) {
            (false, _) => &self.plain,
            (true, 0) => &self.plain,
            (true, 1) => self.cipher.as_ref().expect("Invalid image"),
            _ => panic!("Invalid slot requested"),
        }
    }
}

/// Verify that given image is present in the flash at the given offset.
fn verify_image(flash: &SimMultiFlash, slot: &SlotInfo, images: &ImageData) -> bool {
    let image = images.find(slot.index);
    let buf = image.as_slice();
    let dev_id = slot.dev_id;

    let mut copy = vec![0u8; buf.len()];
    let offset = slot.base_off;
    let dev = flash.get(&dev_id).unwrap();
    dev.read(offset, &mut copy).unwrap();

    if buf != &copy[..] {
        for i in 0 .. buf.len() {
            if buf[i] != copy[i] {
                info!("First failure for slot{} at {:#x} ({:#x} within) {:#x}!={:#x}",
                      slot.index, offset + i, i, buf[i], copy[i]);
                break;
            }
        }
        false
    } else {
        true
    }
}

fn verify_trailer(flash: &SimMultiFlash, slot: &SlotInfo,
                  magic: Option<u8>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    if Caps::OverwriteUpgrade.present() {
        return true;
    }

    let offset = slot.trailer_off + c::boot_max_align();
    let dev_id = slot.dev_id;
    let mut copy = vec![0u8; c::boot_magic_sz() + c::boot_max_align() * 3];
    let mut failed = false;

    let dev = flash.get(&dev_id).unwrap();
    let erased_val = dev.erased_val();
    dev.read(offset, &mut copy).unwrap();

    failed |= match magic {
        Some(v) => {
            if v == 1 && &copy[24..] != MAGIC {
                warn!("\"magic\" mismatch at {:#x}", offset);
                true
            } else if v == 3 {
                let expected = [erased_val; 16];
                if copy[24..] != expected {
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

/// Install a partition table.  This is a simplified partition table that
/// we write at the beginning of flash so make it easier for external tools
/// to analyze these images.
fn install_ptable(flash: &mut SimMultiFlash, areadesc: &AreaDesc) {
    let ids: HashSet<u8> = areadesc.iter_areas().map(|area| area.device_id).collect();
    for &id in &ids {
        // If there are any partitions in this device that start at 0, and
        // aren't marked as the BootLoader partition, avoid adding the
        // partition table.  This makes it harder to view the image, but
        // avoids messing up images already written.
        let skip_ptable = areadesc
            .iter_areas()
            .any(|area| {
                area.device_id == id &&
                    area.off == 0 &&
                    area.flash_id != FlashId::BootLoader
            });
        if skip_ptable {
            if log_enabled!(Info) {
                let special: Vec<FlashId> = areadesc.iter_areas()
                    .filter(|area| area.device_id == id && area.off == 0)
                    .map(|area| area.flash_id)
                    .collect();
                info!("Skipping partition table: {:?}", special);
            }
            break;
        }

        let mut buf: Vec<u8> = vec![];
        write!(&mut buf, "mcuboot\0").unwrap();

        // Iterate through all of the partitions in that device, and encode
        // into the table.
        let count = areadesc.iter_areas().filter(|area| area.device_id == id).count();
        buf.write_u32::<LittleEndian>(count as u32).unwrap();

        for area in areadesc.iter_areas().filter(|area| area.device_id == id) {
            buf.write_u32::<LittleEndian>(area.flash_id as u32).unwrap();
            buf.write_u32::<LittleEndian>(area.off).unwrap();
            buf.write_u32::<LittleEndian>(area.size).unwrap();
            buf.write_u32::<LittleEndian>(0).unwrap();
        }

        let dev = flash.get_mut(&id).unwrap();

        // Pad to alignment.
        while buf.len() % dev.align() != 0 {
            buf.push(0);
        }

        dev.write(0, &buf).unwrap();
    }
}

/// The image header
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

impl AsRaw for ImageHeader {}

#[repr(C)]
#[derive(Clone, Debug)]
pub struct ImageVersion {
    pub major: u8,
    pub minor: u8,
    pub revision: u16,
    pub build_num: u32,
}

#[derive(Clone, Debug)]
pub struct SlotInfo {
    pub base_off: usize,
    pub trailer_off: usize,
    pub len: usize,
    // Which slot within this device.
    pub index: usize,
    pub dev_id: u8,
}

const MAGIC: &[u8] = &[0x77, 0xc2, 0x95, 0xf3,
                       0x60, 0xd2, 0xef, 0x7f,
                       0x35, 0x52, 0x50, 0x0f,
                       0x2c, 0xb6, 0x79, 0x80];

// Replicates defines found in bootutil.h
const BOOT_MAGIC_GOOD: Option<u8> = Some(1);
const BOOT_MAGIC_UNSET: Option<u8> = Some(3);

const BOOT_FLAG_SET: Option<u8> = Some(1);
const BOOT_FLAG_UNSET: Option<u8> = Some(3);

/// Write out the magic so that the loader tries doing an upgrade.
pub fn mark_upgrade(flash: &mut SimMultiFlash, slot: &SlotInfo) {
    let dev = flash.get_mut(&slot.dev_id).unwrap();
    let align = dev.align();
    let offset = slot.trailer_off + c::boot_max_align() * 4;
    if offset % align != 0 || MAGIC.len() % align != 0 {
        // The write size is larger than the magic value.  Fill a buffer
        // with the erased value, put the MAGIC in it, and write it in its
        // entirety.
        let mut buf = vec![dev.erased_val(); align];
        buf[(offset % align)..].copy_from_slice(MAGIC);
        dev.write(offset - (offset % align), &buf).unwrap();
    } else {
        dev.write(offset, MAGIC).unwrap();
    }
}

/// Writes the image_ok flag which, guess what, tells the bootloader
/// the this image is ok (not a test, and no revert is to be performed).
fn mark_permanent_upgrade(flash: &mut SimMultiFlash, slot: &SlotInfo) {
    // Overwrite mode always is permanent, and only the magic is used in
    // the trailer.  To avoid problems with large write sizes, don't try to
    // set anything in this case.
    if Caps::OverwriteUpgrade.present() {
        return;
    }

    let dev = flash.get_mut(&slot.dev_id).unwrap();
    let mut ok = [dev.erased_val(); 8];
    ok[0] = 1u8;
    let off = slot.trailer_off + c::boot_max_align() * 3;
    let align = dev.align();
    dev.write(off, &ok[..align]).unwrap();
}

// Drop some pseudo-random gibberish onto the data.
fn splat(data: &mut [u8], seed: usize) {
    let mut seed_block = [0u8; 16];
    let mut buf = Cursor::new(&mut seed_block[..]);
    buf.write_u32::<LittleEndian>(0x135782ea).unwrap();
    buf.write_u32::<LittleEndian>(0x92184728).unwrap();
    buf.write_u32::<LittleEndian>(data.len() as u32).unwrap();
    buf.write_u32::<LittleEndian>(seed as u32).unwrap();
    let mut rng: SmallRng = SeedableRng::from_seed(seed_block);
    rng.fill_bytes(data);
}

/// Return a read-only view into the raw bytes of this object
trait AsRaw : Sized {
    fn as_raw(&self) -> &[u8] {
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

#[cfg(not(feature = "large-write"))]
fn test_alignments() -> &'static [usize] {
    &[1, 2, 4, 8]
}

#[cfg(feature = "large-write")]
fn test_alignments() -> &'static [usize] {
    &[1, 2, 4, 8, 128, 512]
}
