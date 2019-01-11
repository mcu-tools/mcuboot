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

use simflash::{Flash, SimFlashMap};
use mcuboot_sys::{c, AreaDesc};
use crate::caps::Caps;
use crate::tlv::{ManifestGen, TlvGen, TlvFlags, AES_SEC_KEY};

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

    pub fn run_basic_revert(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            return false;
        }

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
                warn!("Mismatched trailer for the primary slot");
                fails += 1;
            }

            if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                               BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
                warn!("Mismatched trailer for the secondary slot");
                fails += 1;
            }

            if Caps::SwapUpgrade.present() {
                if !verify_image(&flashmap, &self.slots, 1, &self.primaries) {
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
        let (flashmap, total_counts) = try_random_fails(&self.flashmap, &self,
                                                        total_flash_ops, total_fails);
        info!("Random interruptions at reset points={:?}", total_counts);

        let primary_slot_ok = verify_image(&flashmap, &self.slots,
                                           0, &self.upgrades);
        let secondary_slot_ok = if Caps::SwapUpgrade.present() {
            verify_image(&flashmap, &self.slots, 1, &self.primaries)
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
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            error!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
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
                if try_revert_with_fail_at(&self.flashmap, &self, i) {
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
            warn!("Primary slot image verification FAIL");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the secondary slot");
            fails += 1;
        }

        // Marks image in the primary slot as permanent,
        // no revert should happen...
        mark_permanent_upgrade(&mut flashmap, &self.slots[0]);

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        let (result, _) = c::boot_go(&mut flashmap, &self.areadesc, None, false);
        if result != 0 {
            warn!("Failed second boot");
            fails += 1;
        }

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_SET) {
            warn!("Mismatched trailer for the primary slot");
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

    // Tests a new image written to the primary slot that already has magic and
    // image_ok set while there is no image on the secondary slot, so no revert
    // should ever happen...
    pub fn run_norevert_newimage(&self) -> bool {
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try non-revert on imgtool generated image");

        mark_upgrade(&mut flashmap, &self.slots[0]);

        // This simulates writing an image created by imgtool to
        // the primary slot
        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
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
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }
        if !verify_trailer(&flashmap, &self.slots, 1, BOOT_MAGIC_UNSET,
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
        let mut flashmap = self.flashmap.clone();
        let mut fails = 0;

        info!("Try upgrade image with bad signature");

        mark_upgrade(&mut flashmap, &self.slots[0]);
        mark_permanent_upgrade(&mut flashmap, &self.slots[0]);
        mark_upgrade(&mut flashmap, &self.slots[1]);

        if !verify_trailer(&flashmap, &self.slots, 0, BOOT_MAGIC_GOOD,
                           BOOT_FLAG_SET, BOOT_FLAG_UNSET) {
            warn!("Mismatched trailer for the primary slot");
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

        self.trailer_sz(align) - (16 + 24 + bias)
    }

    /// This test runs a simple upgrade with no fails in the images, but
    /// allowing for fails in the status area. This should run to the end
    /// and warn that write fails were detected...
    pub fn run_with_status_fails_complete(&self) -> bool {
        if !Caps::ValidatePrimarySlot.present() {
            return false;
        }

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
            warn!("Mismatched trailer for the primary slot");
            fails += 1;
        }

        if !verify_image(&flashmap, &self.slots, 0, &self.upgrades) {
            warn!("Failed image verification");
            fails += 1;
        }

        info!("validate primary slot enabled; \
               re-run of boot_go should just work");
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
    pub fn run_with_status_fails_with_reset(&self) -> bool {
        if Caps::OverwriteUpgrade.present() {
            false
        } else if Caps::ValidatePrimarySlot.present() {

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
                warn!("Expected single assert validating the primary slot, \
                       more detected {}", asserts);
                fails += 1;
            }

            if fails > 0 {
                error!("Error running upgrade with status write fails");
            }

            fails > 0
        } else {
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
    }

    /// Adds a new flash area that fails statistically
    fn mark_bad_status_with_rate(&self, flashmap: &mut SimFlashMap, slot: usize,
                                 rate: f32) {
        if Caps::OverwriteUpgrade.present() {
            return;
        }

        let dev_id = &self.slots[slot].dev_id;
        let flash = flashmap.get_mut(&dev_id).unwrap();
        let align = flash.align();
        let off = &self.slots[0].base_off;
        let len = &self.slots[0].len;
        let status_off = off + len - self.trailer_sz(align);

        // Mark the status area as a bad area
        let _ = flash.add_bad_region(status_off, self.status_sz(align), rate);
    }

    fn reset_bad_status(&self, flashmap: &mut SimFlashMap, slot: usize) {
        if !Caps::ValidatePrimarySlot.present() {
            return;
        }

        let dev_id = &self.slots[slot].dev_id;
        let flash = flashmap.get_mut(&dev_id).unwrap();
        flash.reset_bad_regions();

        // Disabling write verification the only assert triggered by
        // boot_go should be checking for integrity of status bytes.
        flash.set_verify_writes(false);
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

fn try_revert(flashmap: &SimFlashMap, areadesc: &AreaDesc, count: usize) -> SimFlashMap {
    let mut flashmap = flashmap.clone();

    // fl.write_file("image0.bin").unwrap();
    for i in 0 .. count {
        info!("Running boot pass {}", i + 1);
        assert_eq!(c::boot_go(&mut flashmap, &areadesc, None, false), (0, 0));
    }
    flashmap
}

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
        warn!("Image in the primary slot before revert is invalid at stop={}",
              stop);
        fails += 1;
    }
    if !verify_image(&flashmap, &images.slots, 1, &images.primaries) {
        warn!("Image in the secondary slot before revert is invalid at stop={}",
              stop);
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 0, BOOT_MAGIC_GOOD,
                       BOOT_FLAG_UNSET, BOOT_FLAG_SET) {
        warn!("Mismatched trailer for the primary slot before revert");
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 1, BOOT_MAGIC_UNSET,
                       BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
        warn!("Mismatched trailer for the secondary slot before revert");
        fails += 1;
    }

    // Do Revert
    let (x, _) = c::boot_go(&mut flashmap, &images.areadesc, None, false);
    if x != 0 {
        warn!("Should have finished a revert");
        fails += 1;
    }

    if !verify_image(&flashmap, &images.slots, 0, &images.primaries) {
        warn!("Image in the primary slot after revert is invalid at stop={}",
              stop);
        fails += 1;
    }
    if !verify_image(&flashmap, &images.slots, 1, &images.upgrades) {
        warn!("Image in the secondary slot after revert is invalid at stop={}",
              stop);
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 0, BOOT_MAGIC_GOOD,
                       BOOT_FLAG_SET, BOOT_FLAG_SET) {
        warn!("Mismatched trailer for the secondary slot after revert");
        fails += 1;
    }
    if !verify_trailer(&flashmap, &images.slots, 1, BOOT_MAGIC_UNSET,
                       BOOT_FLAG_UNSET, BOOT_FLAG_UNSET) {
        warn!("Mismatched trailer for the secondary slot after revert");
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
pub fn install_image(flashmap: &mut SimFlashMap, slots: &[SlotInfo], slot: usize, len: usize,
                 bad_sig: bool) -> [Option<Vec<u8>>; 2] {
    let offset = slots[slot].base_off;
    let slot_len = slots[slot].len;
    let dev_id = slots[slot].dev_id;

    let mut tlv: Box<dyn ManifestGen> = Box::new(make_tlv());

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

    // Since images are always non-encrypted in the primary slot, we first write
    // an encrypted image, re-read to use for verification, erase + flash
    // un-encrypted. In the secondary slot the image is written un-encrypted,
    // and if encryption is requested, it follows an erase + flash encrypted.

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
        } else if Caps::EcdsaP256.present() {
            TlvGen::new_ecdsa()
        } else {
            TlvGen::new_hash_only()
        }
    }
}

fn find_image(images: &[Option<Vec<u8>>; 2], slot: usize) -> &Vec<u8> {
    let slot = if Caps::EncRsa.present() || Caps::EncKw.present() {
        slot
    } else {
        0
    };

    match &images[slot] {
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

fn verify_trailer(flashmap: &SimFlashMap, slots: &[SlotInfo], slot: usize,
                  magic: Option<u8>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    if Caps::OverwriteUpgrade.present() {
        return true;
    }

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
pub struct SlotInfo {
    pub base_off: usize,
    pub trailer_off: usize,
    pub len: usize,
    pub dev_id: u8,
}

pub struct Images {
    pub flashmap: SimFlashMap,
    pub areadesc: AreaDesc,
    pub slots: [SlotInfo; 2],
    pub primaries: [Option<Vec<u8>>; 2],
    pub upgrades: [Option<Vec<u8>>; 2],
    pub total_count: Option<i32>,
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
pub fn mark_upgrade(flashmap: &mut SimFlashMap, slot: &SlotInfo) {
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

pub fn show_sizes() {
    // This isn't panic safe.
    for min in &[1, 2, 4, 8] {
        let msize = c::boot_trailer_sz(*min);
        println!("{:2}: {} (0x{:x})", min, msize, msize);
    }
}
