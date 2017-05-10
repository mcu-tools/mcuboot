#[macro_use] extern crate log;
extern crate env_logger;
extern crate docopt;
extern crate libc;
extern crate rand;
extern crate rustc_serialize;

#[macro_use]
extern crate error_chain;

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
mod flash;
pub mod api;
mod pdump;

use flash::Flash;
use area::{AreaDesc, FlashId};

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
        let mut failed = false;

        warn!("Running on device {} with alignment {}", device, align);

        let (mut flash, areadesc) = match device {
            DeviceName::Stm32f4 => {
                // STM style flash.  Large sectors, with a large scratch area.
                let flash = Flash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 16 * 1024,
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
                let flash = Flash::new(vec![4096; 128], align as usize);

                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_image(0x020000, 0x020000, FlashId::Image0);
                areadesc.add_image(0x040000, 0x020000, FlashId::Image1);
                areadesc.add_image(0x060000, 0x001000, FlashId::ImageScratch);
                (flash, areadesc)
            }
            DeviceName::K64fBig => {
                // Simulating an STM style flash on top of an NXP style flash.  Underlying flash device
                // uses small sectors, but we tell the bootloader they are large.
                let flash = Flash::new(vec![4096; 128], align as usize);

                let mut areadesc = AreaDesc::new(&flash);
                areadesc.add_simple_image(0x020000, 0x020000, FlashId::Image0);
                areadesc.add_simple_image(0x040000, 0x020000, FlashId::Image1);
                areadesc.add_simple_image(0x060000, 0x020000, FlashId::ImageScratch);
                (flash, areadesc)
            }
            DeviceName::Nrf52840 => {
                // Simulating the flash on the nrf52840 with partitions set up so that the scratch size
                // does not divide into the image size.
                let flash = Flash::new(vec![4096; 128], align as usize);

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

        // println!("Areas: {:#?}", areadesc.get_c());

        // Install the boot trailer signature, so that the code will start an upgrade.
        // TODO: This must be a multiple of flash alignment, add support for an image that is smaller,
        // and just gets padded.
        let primary = install_image(&mut flash, slot0_base, 32784);

        // Install an upgrade image.
        let upgrade = install_image(&mut flash, slot1_base, 41928);

        // Set an alignment, and position the magic value.
        c::set_sim_flash_align(align);
        let trailer_size = c::boot_trailer_sz();

        // Mark the upgrade as ready to install.  (This looks like it might be a bug in the code,
        // however.)
        mark_upgrade(&mut flash, scratch_base - trailer_size as usize);

        let (fl2, total_count) = try_upgrade(&flash, &areadesc, None);
        info!("First boot, count={}", total_count);
        if !verify_image(&fl2, slot0_base, &upgrade) {
            error!("Image mismatch after first boot");
            // This isn't really recoverable, and more tests aren't likely to reveal much.
            self.failures += 1;
            return;
        }

        let mut bad = 0;
        // Let's try an image halfway through.
        for i in 1 .. total_count {
            info!("Try interruption at {}", i);
            let (fl3, count) = try_upgrade(&flash, &areadesc, Some(i));
            info!("Second boot, count={}", count);
            if !verify_image(&fl3, slot0_base, &upgrade) {
                warn!("FAIL at step {} of {}", i, total_count);
                bad += 1;
            }
            if !verify_image(&fl3, slot1_base, &primary) {
                warn!("Slot 1 FAIL at step {} of {}", i, total_count);
                bad += 1;
            }
        }
        error!("{} out of {} failed {:.2}%",
               bad, total_count,
               bad as f32 * 100.0 / total_count as f32);
        if bad > 0 {
            failed = true;
        }

        let (fl4, total_counts) = try_random_fails(&flash, &areadesc, total_count, 5);
        info!("Random fails at reset points={:?}", total_counts);
        let slot0_ok = verify_image(&fl4, slot0_base, &upgrade);
        let slot1_ok = verify_image(&fl4, slot1_base, &primary);
        if !slot0_ok || !slot1_ok {
            error!("Image mismatch after random fails: slot0={} slot1={}",
                   if slot0_ok { "ok" } else { "fail" },
                   if slot1_ok { "ok" } else { "fail" });
            self.failures += 1;
            return;
        }

        for count in 2 .. 5 {
            info!("Try revert: {}", count);
            let fl2 = try_revert(&flash, &areadesc, count);
            if !verify_image(&fl2, slot0_base, &primary) {
                warn!("Revert failure on count {}", count);
                failed = true;
            }
        }

        info!("Try norevert");
        let fl2 = try_norevert(&flash, &areadesc);
        if !verify_image(&fl2, slot0_base, &upgrade) {
            warn!("No revert failed");
            failed = true;
        }

        /*
        // show_flash(&flash);

        println!("First boot for upgrade");
        // c::set_flash_counter(570);
        c::boot_go(&mut flash, &areadesc);
        // println!("{} flash ops", c::get_flash_counter());

        verify_image(&flash, slot0_base, &upgrade);

        println!("\n------------------\nSecond boot");
        c::boot_go(&mut flash, &areadesc);
        */
        if failed {
            self.failures += 1;
        } else {
            self.passes += 1;
        }
    }
}

/// Test a boot, optionally stopping after 'n' flash options.  Returns a count of the number of
/// flash operations done total.
fn try_upgrade(flash: &Flash, areadesc: &AreaDesc, stop: Option<i32>) -> (Flash, i32) {
    // Clone the flash to have a new copy.
    let mut fl = flash.clone();

    // mark permanent upgrade
    let ok = [1u8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    let (base, _) = areadesc.find(FlashId::ImageScratch);
    let align = c::get_sim_flash_align() as usize;
    fl.write(base - align, &ok[..align]).unwrap();

    c::set_flash_counter(stop.unwrap_or(0));
    let (first_interrupted, cnt1) = match c::boot_go(&mut fl, &areadesc) {
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

    let cnt2 = cnt1 - c::get_flash_counter();

    (fl, cnt2)
}

fn try_revert(flash: &Flash, areadesc: &AreaDesc, count: usize) -> Flash {
    let mut fl = flash.clone();
    c::set_flash_counter(0);

    // fl.write_file("image0.bin").unwrap();
    for i in 0 .. count {
        info!("Running boot pass {}", i + 1);
        assert_eq!(c::boot_go(&mut fl, &areadesc), 0);
    }
    fl
}

fn try_norevert(flash: &Flash, areadesc: &AreaDesc) -> Flash {
    let mut fl = flash.clone();
    c::set_flash_counter(0);
    let align = c::get_sim_flash_align() as usize;

    assert_eq!(c::boot_go(&mut fl, &areadesc), 0);
    // Write boot_ok
    let ok = [1u8, 0, 0, 0, 0, 0, 0, 0];
    let (slot0_base, slot0_len) = areadesc.find(FlashId::Image0);
    fl.write(slot0_base + slot0_len - align, &ok[..align]).unwrap();
    assert_eq!(c::boot_go(&mut fl, &areadesc), 0);
    fl
}

fn try_random_fails(flash: &Flash, areadesc: &AreaDesc, total_ops: i32, 
                    count: usize) -> (Flash, Vec<i32>) {
    let mut fl = flash.clone();

    // mark permanent upgrade
    let ok = [1u8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    let (base, _) = areadesc.find(FlashId::ImageScratch);
    let align = c::get_sim_flash_align() as usize;
    fl.write(base - align, &ok[..align]).unwrap();

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
        println!("    {:2}: 0x{:08x}, 0x{:08x}",
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

/// Write out the magic so that the loader tries doing an upgrade.
fn mark_upgrade(flash: &mut Flash, offset: usize) {
    let magic = vec![0x77, 0xc2, 0x95, 0xf3,
                     0x60, 0xd2, 0xef, 0x7f,
                     0x35, 0x52, 0x50, 0x0f,
                     0x2c, 0xb6, 0x79, 0x80];
    flash.write(offset, &magic).unwrap();
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
