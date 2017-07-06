#[macro_use] extern crate log;
extern crate env_logger;
extern crate docopt;
extern crate libc;
extern crate rand;
extern crate rustc_serialize;
extern crate simflash;

use docopt::Docopt;
use rand::{Rng, SeedableRng, XorShiftRng};
use rand::distributions::{IndependentSample, Range};
use rustc_serialize::{Decodable, Decoder};
use std::fmt;
use std::mem;
use std::process;
use std::slice;

mod area;
mod c;
pub mod api;
mod caps;

use simflash::{Flash, SimFlash};
use area::{AreaDesc, FlashId};
use caps::Caps;

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

#[derive(Debug, RustcDecodable)]
struct Args {
    flag_help: bool,
    flag_version: bool,
    flag_device: Option<DeviceName>,
    flag_align: Option<AlignArg>,
    cmd_sizes: bool,
    cmd_run: bool,
    cmd_runall: bool,
}

#[derive(Copy, Clone, Debug, RustcDecodable)]
enum DeviceName { Stm32f4, K64f, K64fBig, Nrf52840 }

static ALL_DEVICES: &'static [DeviceName] = &[
    DeviceName::Stm32f4,
    DeviceName::K64f,
    DeviceName::K64fBig,
    DeviceName::Nrf52840,
];

impl fmt::Display for DeviceName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match *self {
            DeviceName::Stm32f4 => "stm32f4",
            DeviceName::K64f => "k64f",
            DeviceName::K64fBig => "k64fbig",
            DeviceName::Nrf52840 => "nrf52840",
        };
        f.write_str(name)
    }
}

#[derive(Debug)]
struct AlignArg(u8);

impl Decodable for AlignArg {
    // Decode the alignment ourselves, to restrict it to the valid possible alignments.
    fn decode<D: Decoder>(d: &mut D) -> Result<AlignArg, D::Error> {
        let m = d.read_u8()?;
        match m {
            1 | 2 | 4 | 8 => Ok(AlignArg(m)),
            _ => Err(d.error("Invalid alignment")),
        }
    }
}

fn main() {
    env_logger::init().unwrap();

    let args: Args = Docopt::new(USAGE)
        .and_then(|d| d.decode())
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

        status.run_single(device, align);
    }

    if args.cmd_runall {
        for &dev in ALL_DEVICES {
            for &align in &[1, 2, 4, 8] {
                status.run_single(dev, align);
            }
        }
    }

    if status.failures > 0 {
        warn!("{} Tests ran with {} failures", status.failures + status.passes, status.failures);
        process::exit(1);
    } else {
        warn!("{} Tests ran successfully", status.passes);
        process::exit(0);
    }
}

struct RunStatus {
    failures: usize,
    passes: usize,
}

impl RunStatus {
    fn new() -> RunStatus {
        RunStatus {
            failures: 0,
            passes: 0,
        }
    }

    fn run_single(&mut self, device: DeviceName, align: u8) {
        warn!("Running on device {} with alignment {}", device, align);

        let (mut flash, areadesc) = match device {
            DeviceName::Stm32f4 => {
                // STM style flash.  Large sectors, with a large scratch area.
                let flash = SimFlash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024,
                                          64 * 1024,
                                          128 * 1024, 128 * 1024, 128 * 1024],
                                          align as usize);
                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1);
                areadesc.add_image(0x060000, 0x020000, FlashId::ImageScratch);
                (flash, areadesc)
            }
            DeviceName::K64f => {
                // NXP style flash.  Small sectors, one small sector for scratch.
                let flash = SimFlash::new(vec![4096; 128], align as usize);

                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1);
                areadesc.add_image(0x060000, 0x001000, FlashId::ImageScratch);
                (flash, areadesc)
            }
            DeviceName::K64fBig => {
                // Simulating an STM style flash on top of an NXP style flash.  Underlying flash device
                // uses small sectors, but we tell the bootloader they are large.
                let flash = SimFlash::new(vec![4096; 128], align as usize);

                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_simple_image(0x020000, 0x020000, FlashId::Image0);
                areadesc.add_simple_image(0x040000, 0x020000, FlashId::Image1);
                areadesc.add_simple_image(0x060000, 0x020000, FlashId::ImageScratch);
                (flash, areadesc)
            }
            DeviceName::Nrf52840 => {
                // Simulating the flash on the nrf52840 with partitions set up so that the scratch size
                // does not divide into the image size.
                let flash = SimFlash::new(vec![4096; 128], align as usize);

                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_image(0x008000, 0x034000, FlashId::Image0);
                areadesc.add_image(0x03c000, 0x034000, FlashId::Image1);
                areadesc.add_image(0x070000, 0x00d000, FlashId::ImageScratch);
                (flash, areadesc)
            }
        };

        let (slot0_base, slot0_len) = areadesc.find(FlashId::Image0);
        let (slot1_base, slot1_len) = areadesc.find(FlashId::Image1);
        let (scratch_base, _) = areadesc.find(FlashId::ImageScratch);

        // Code below assumes that the slots are consecutive.
        assert_eq!(slot1_base, slot0_base + slot0_len);
        assert_eq!(scratch_base, slot1_base + slot1_len);

        let offset_from_end = c::boot_magic_sz() + c::boot_max_align() * 2;

        // println!("Areas: {:#?}", areadesc.get_c());

        // Install the boot trailer signature, so that the code will start an upgrade.
        // TODO: This must be a multiple of flash alignment, add support for an image that is smaller,
        // and just gets padded.

        // Create original and upgrade images
        let slot0 = SlotInfo {
            base_off: slot0_base as usize,
            trailer_off: slot1_base - offset_from_end,
        };

        let slot1 = SlotInfo {
            base_off: slot1_base as usize,
            trailer_off: scratch_base - offset_from_end,
        };

        let images = Images {
            slot0: slot0,
            slot1: slot1,
            primary: install_image(&mut flash, slot0_base, 32784),
            upgrade: install_image(&mut flash, slot1_base, 41928),
        };

        let mut failed = false;

        // Set an alignment, and position the magic value.
        c::set_sim_flash_align(align);

        mark_upgrade(&mut flash, &images.slot1);

        // upgrades without fails, counts number of flash operations
        let total_count = match run_basic_upgrade(&flash, &areadesc, &images) {
            Ok(v)  => v,
            Err(_) => {
                self.failures += 1;
                return;
            },
        };

        failed |= run_basic_revert(&flash, &areadesc, &images);
        failed |= run_revert_with_fails(&flash, &areadesc, &images, total_count);
        failed |= run_perm_with_fails(&flash, &areadesc, &images, total_count);
        failed |= run_perm_with_random_fails(&flash, &areadesc, &images,
                                                    total_count, 5);
        failed |= run_norevert(&flash, &areadesc, &images);

        //show_flash(&flash);

        if failed {
            self.failures += 1;
        } else {
            self.passes += 1;
        }
    }
}

/// A simple upgrade without forced failures.
///
/// Returns the number of flash operations which can later be used to
/// inject failures at chosen steps.
fn run_basic_upgrade(flash: &SimFlash, areadesc: &AreaDesc, images: &Images)
                     -> Result<i32, ()> {
    let (fl, total_count) = try_upgrade(&flash, &areadesc, &images, None);
    info!("Total flash operation count={}", total_count);

    if !verify_image(&fl, images.slot0.base_off, &images.upgrade) {
        warn!("Image mismatch after first boot");
        Err(())
    } else {
        Ok(total_count)
    }
}

fn run_basic_revert(flash: &SimFlash, areadesc: &AreaDesc, images: &Images) -> bool {
    let mut fails = 0;

    if Caps::SwapUpgrade.present() {
        for count in 2 .. 5 {
            info!("Try revert: {}", count);
            let fl = try_revert(&flash, &areadesc, count);
            if !verify_image(&fl, images.slot0.base_off, &images.primary) {
                warn!("Revert failure on count {}", count);
                fails += 1;
            }
        }
    }

    fails > 0
}

fn run_perm_with_fails(flash: &SimFlash, areadesc: &AreaDesc, images: &Images,
                       total_flash_ops: i32) -> bool {
    let mut fails = 0;

    // Let's try an image halfway through.
    for i in 1 .. total_flash_ops {
        info!("Try interruption at {}", i);
        let (fl, count) = try_upgrade(&flash, &areadesc, &images, Some(i));
        info!("Second boot, count={}", count);
        if !verify_image(&fl, images.slot0.base_off, &images.upgrade) {
            warn!("FAIL at step {} of {}", i, total_flash_ops);
            fails += 1;
        }

        if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, IMAGE_OK,
                           COPY_DONE) {
            warn!("Mismatched trailer for Slot 0");
            fails += 1;
        }

        if !verify_trailer(&fl, images.slot1.trailer_off, MAGIC_UNSET, UNSET,
                           UNSET) {
            warn!("Mismatched trailer for Slot 1");
            fails += 1;
        }

        if Caps::SwapUpgrade.present() {
            if !verify_image(&fl, images.slot1.base_off, &images.primary) {
                warn!("Slot 1 FAIL at step {} of {}", i, total_flash_ops);
                fails += 1;
            }
        }
    }

    info!("{} out of {} failed {:.2}%", fails, total_flash_ops,
           fails as f32 * 100.0 / total_flash_ops as f32);

    fails > 0
}

fn run_perm_with_random_fails(flash: &SimFlash, areadesc: &AreaDesc,
                              images: &Images, total_flash_ops: i32,
                              total_fails: usize) -> bool {
    let mut fails = 0;
    let (fl, total_counts) = try_random_fails(&flash, &areadesc, &images,
                                              total_flash_ops, total_fails);
    info!("Random interruptions at reset points={:?}", total_counts);

    let slot0_ok = verify_image(&fl, images.slot0.base_off, &images.upgrade);
    let slot1_ok = if Caps::SwapUpgrade.present() {
        verify_image(&fl, images.slot1.base_off, &images.primary)
    } else {
        true
    };
    if !slot0_ok || !slot1_ok {
        error!("Image mismatch after random interrupts: slot0={} slot1={}",
               if slot0_ok { "ok" } else { "fail" },
               if slot1_ok { "ok" } else { "fail" });
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, IMAGE_OK,
                       COPY_DONE) {
        error!("Mismatched trailer for Slot 0");
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot1.trailer_off, MAGIC_UNSET, UNSET,
                       UNSET) {
        error!("Mismatched trailer for Slot 1");
        fails += 1;
    }

    fails > 0
}

fn run_revert_with_fails(flash: &SimFlash, areadesc: &AreaDesc, images: &Images,
                         total_count: i32) -> bool {
    let mut fails = 0;

    if Caps::SwapUpgrade.present() {
        for i in 1 .. (total_count - 1) {
            info!("Try interruption at {}", i);
            if try_revert_with_fail_at(&flash, &areadesc, &images, i) {
                fails += 1;
            }
        }
    }

    fails > 0
}

fn run_norevert(flash: &SimFlash, areadesc: &AreaDesc, images: &Images) -> bool {
    let mut fl = flash.clone();
    let mut fails = 0;

    info!("Try norevert");
    c::set_flash_counter(0);

    // First do a normal upgrade...
    if c::boot_go(&mut fl, &areadesc) != 0 {
        warn!("Failed first boot");
        fails += 1;
    }

    if !verify_image(&fl, images.slot0.base_off, &images.upgrade) {
        warn!("Slot 0 image verification FAIL");
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, UNSET,
                       COPY_DONE) {
        warn!("Mismatched trailer for Slot 0");
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot1.trailer_off, MAGIC_UNSET, UNSET,
                       UNSET) {
        warn!("Mismatched trailer for Slot 1");
        fails += 1;
    }

    // Marks image in slot0 as permanent, no revert should happen...
    mark_permanent_upgrade(&mut fl, &images.slot0);

    if c::boot_go(&mut fl, &areadesc) != 0 {
        warn!("Failed second boot");
        fails += 1;
    }

    if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, IMAGE_OK,
                       COPY_DONE) {
        warn!("Mismatched trailer for Slot 0");
        fails += 1;
    }
    if !verify_image(&fl, images.slot0.base_off, &images.upgrade) {
        warn!("Failed image verification");
        fails += 1;
    }

    fails > 0
}

/// Test a boot, optionally stopping after 'n' flash options.  Returns a count
/// of the number of flash operations done total.
fn try_upgrade(flash: &SimFlash, areadesc: &AreaDesc, images: &Images,
               stop: Option<i32>) -> (SimFlash, i32) {
    // Clone the flash to have a new copy.
    let mut fl = flash.clone();

    mark_permanent_upgrade(&mut fl, &images.slot1);

    c::set_flash_counter(stop.unwrap_or(0));
    let (first_interrupted, count) = match c::boot_go(&mut fl, &areadesc) {
        -0x13579 => (true, stop.unwrap()),
        0 => (false, -c::get_flash_counter()),
        x => panic!("Unknown return: {}", x),
    };
    c::set_flash_counter(0);

    if first_interrupted {
        // fl.dump();
        match c::boot_go(&mut fl, &areadesc) {
            -0x13579 => panic!("Shouldn't stop again"),
            0 => (),
            x => panic!("Unknown return: {}", x),
        }
    }

    (fl, count - c::get_flash_counter())
}

fn try_revert(flash: &SimFlash, areadesc: &AreaDesc, count: usize) -> SimFlash {
    let mut fl = flash.clone();
    c::set_flash_counter(0);

    // fl.write_file("image0.bin").unwrap();
    for i in 0 .. count {
        info!("Running boot pass {}", i + 1);
        assert_eq!(c::boot_go(&mut fl, &areadesc), 0);
    }
    fl
}

fn try_revert_with_fail_at(flash: &SimFlash, areadesc: &AreaDesc, images: &Images,
                           stop: i32) -> bool {
    let mut fl = flash.clone();
    let mut x: i32;
    let mut fails = 0;

    c::set_flash_counter(stop);
    x = c::boot_go(&mut fl, &areadesc);
    if x != -0x13579 {
        warn!("Should have stopped at interruption point");
        fails += 1;
    }

    if !verify_trailer(&fl, images.slot0.trailer_off, None, None, UNSET) {
        warn!("copy_done should be unset");
        fails += 1;
    }

    c::set_flash_counter(0);
    x = c::boot_go(&mut fl, &areadesc);
    if x != 0 {
        warn!("Should have finished upgrade");
        fails += 1;
    }

    if !verify_image(&fl, images.slot0.base_off, &images.upgrade) {
        warn!("Image in slot 0 before revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_image(&fl, images.slot1.base_off, &images.primary) {
        warn!("Image in slot 1 before revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, UNSET,
                       COPY_DONE) {
        warn!("Mismatched trailer for Slot 0 before revert");
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot1.trailer_off, MAGIC_UNSET, UNSET,
                       UNSET) {
        warn!("Mismatched trailer for Slot 1 before revert");
        fails += 1;
    }

    // Do Revert
    c::set_flash_counter(0);
    x = c::boot_go(&mut fl, &areadesc);
    if x != 0 {
        warn!("Should have finished a revert");
        fails += 1;
    }

    if !verify_image(&fl, images.slot0.base_off, &images.primary) {
        warn!("Image in slot 0 after revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_image(&fl, images.slot1.base_off, &images.upgrade) {
        warn!("Image in slot 1 after revert is invalid at stop={}", stop);
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot0.trailer_off, MAGIC_VALID, IMAGE_OK,
                       COPY_DONE) {
        warn!("Mismatched trailer for Slot 1 after revert");
        fails += 1;
    }
    if !verify_trailer(&fl, images.slot1.trailer_off, MAGIC_UNSET, UNSET,
                       UNSET) {
        warn!("Mismatched trailer for Slot 1 after revert");
        fails += 1;
    }

    fails > 0
}

fn try_random_fails(flash: &SimFlash, areadesc: &AreaDesc, images: &Images,
                    total_ops: i32,  count: usize) -> (SimFlash, Vec<i32>) {
    let mut fl = flash.clone();

    mark_permanent_upgrade(&mut fl, &images.slot1);

    let mut rng = rand::thread_rng();
    let mut resets = vec![0i32; count];
    let mut remaining_ops = total_ops;
    for i in 0 .. count {
        let ops = Range::new(1, remaining_ops / 2);
        let reset_counter = ops.ind_sample(&mut rng);
        c::set_flash_counter(reset_counter);
        match c::boot_go(&mut fl, &areadesc) {
            0 | -0x13579 => (),
            x => panic!("Unknown return: {}", x),
        }
        remaining_ops -= reset_counter;
        resets[i] = reset_counter;
    }

    c::set_flash_counter(0);
    match c::boot_go(&mut fl, &areadesc) {
        -0x13579 => panic!("Should not be have been interrupted!"),
        0 => (),
        x => panic!("Unknown return: {}", x),
    }

    (fl, resets)
}

/// Show the flash layout.
#[allow(dead_code)]
fn show_flash(flash: &Flash) {
    println!("---- Flash configuration ----");
    for sector in flash.sector_iter() {
        println!("    {:3}: 0x{:08x}, 0x{:08x}",
                 sector.num, sector.base, sector.size);
    }
    println!("");
}

/// Install a "program" into the given image.  This fakes the image header, or at least all of the
/// fields used by the given code.  Returns a copy of the image that was written.
fn install_image(flash: &mut Flash, offset: usize, len: usize) -> Vec<u8> {
    let offset0 = offset;

    // Generate a boot header.  Note that the size doesn't include the header.
    let header = ImageHeader {
        magic: 0x96f3b83c,
        tlv_size: 0,
        _pad1: 0,
        hdr_size: 32,
        key_id: 0,
        _pad2: 0,
        img_size: len as u32,
        flags: 0,
        ver: ImageVersion {
            major: (offset / (128 * 1024)) as u8,
            minor: 0,
            revision: 1,
            build_num: offset as u32,
        },
        _pad3: 0,
    };

    let b_header = header.as_raw();
    /*
    let b_header = unsafe { slice::from_raw_parts(&header as *const _ as *const u8,
                                                  mem::size_of::<ImageHeader>()) };
                                                  */
    assert_eq!(b_header.len(), 32);
    flash.write(offset, &b_header).unwrap();
    let offset = offset + b_header.len();

    // The core of the image itself is just pseudorandom data.
    let mut buf = vec![0; len];
    splat(&mut buf, offset);
    flash.write(offset, &buf).unwrap();
    let offset = offset + buf.len();

    // Copy out the image so that we can verify that the image was installed correctly later.
    let mut copy = vec![0u8; offset - offset0];
    flash.read(offset0, &mut copy).unwrap();

    copy
}

/// Verify that given image is present in the flash at the given offset.
fn verify_image(flash: &Flash, offset: usize, buf: &[u8]) -> bool {
    let mut copy = vec![0u8; buf.len()];
    flash.read(offset, &mut copy).unwrap();

    if buf != &copy[..] {
        for i in 0 .. buf.len() {
            if buf[i] != copy[i] {
                info!("First failure at {:#x}", offset + i);
                break;
            }
        }
        false
    } else {
        true
    }
}

fn verify_trailer(flash: &Flash, offset: usize,
                  magic: Option<&[u8]>, image_ok: Option<u8>,
                  copy_done: Option<u8>) -> bool {
    let mut copy = vec![0u8; c::boot_magic_sz() + c::boot_max_align() * 2];
    let mut failed = false;

    flash.read(offset, &mut copy).unwrap();

    failed |= match magic {
        Some(v) => {
            if &copy[16..] != v  {
                warn!("\"magic\" mismatch at {:#x}", offset);
                true
            } else {
                false
            }
        },
        None => false,
    };

    failed |= match image_ok {
        Some(v) => {
            if copy[8] != v {
                warn!("\"image_ok\" mismatch at {:#x}", offset);
                true
            } else {
                false
            }
        },
        None => false,
    };

    failed |= match copy_done {
        Some(v) => {
            if copy[0] != v {
                warn!("\"copy_done\" mismatch at {:#x}", offset);
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
    tlv_size: u16,
    key_id: u8,
    _pad1: u8,
    hdr_size: u16,
    _pad2: u16,
    img_size: u32,
    flags: u32,
    ver: ImageVersion,
    _pad3: u32,
}

impl AsRaw for ImageHeader {}

#[repr(C)]
pub struct ImageVersion {
    major: u8,
    minor: u8,
    revision: u16,
    build_num: u32,
}

struct SlotInfo {
    base_off: usize,
    trailer_off: usize,
}

struct Images {
    slot0: SlotInfo,
    slot1: SlotInfo,
    primary: Vec<u8>,
    upgrade: Vec<u8>,
}

const MAGIC_VALID: Option<&[u8]> = Some(&[0x77, 0xc2, 0x95, 0xf3,
                                          0x60, 0xd2, 0xef, 0x7f,
                                          0x35, 0x52, 0x50, 0x0f,
                                          0x2c, 0xb6, 0x79, 0x80]);
const MAGIC_UNSET: Option<&[u8]> = Some(&[0xff; 16]);

const COPY_DONE: Option<u8> = Some(1);
const IMAGE_OK: Option<u8> = Some(1);
const UNSET: Option<u8> = Some(0xff);

/// Write out the magic so that the loader tries doing an upgrade.
fn mark_upgrade(flash: &mut Flash, slot: &SlotInfo) {
    let offset = slot.trailer_off + c::boot_max_align() * 2;
    flash.write(offset, MAGIC_VALID.unwrap()).unwrap();
}

/// Writes the image_ok flag which, guess what, tells the bootloader
/// the this image is ok (not a test, and no revert is to be performed).
fn mark_permanent_upgrade(flash: &mut Flash, slot: &SlotInfo) {
    let ok = [1u8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    let align = c::get_sim_flash_align() as usize;
    let off = slot.trailer_off + c::boot_max_align();
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
    let old_align = c::get_sim_flash_align();
    for min in &[1, 2, 4, 8] {
        c::set_sim_flash_align(*min);
        let msize = c::boot_trailer_sz();
        println!("{:2}: {} (0x{:x})", min, msize, msize);
    }
    c::set_sim_flash_align(old_align);
}
