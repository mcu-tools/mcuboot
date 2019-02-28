use docopt::Docopt;
use log::{warn, error};
use std::{
    fmt,
    process,
};
use serde_derive::Deserialize;

mod caps;
mod image;
mod tlv;
pub mod testlog;

pub use crate::image::{
    ImagesBuilder,
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

        let run = ImagesBuilder::new(device, align, erased_val);

        let mut failed = false;

        // Creates a badly signed image in the secondary slot to check that
        // it is not upgraded to
        let bad_secondary_slot_image = run.clone().make_bad_secondary_slot_image();

        failed |= bad_secondary_slot_image.run_signfail_upgrade();

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

