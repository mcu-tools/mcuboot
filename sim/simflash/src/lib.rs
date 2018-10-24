//! A flash simulator
//!
//! This module is capable of simulating the type of NOR flash commonly used in microcontrollers.
//! These generally can be written as individual bytes, but must be erased in larger units.

#[macro_use] extern crate log;
#[macro_use] extern crate error_chain;
extern crate rand;
mod pdump;

use rand::distributions::{IndependentSample, Range};
use std::fs::File;
use std::io::Write;
use std::iter::Enumerate;
use std::path::Path;
use std::slice;
use pdump::HexDump;

error_chain! {
    errors {
        OutOfBounds(t: String) {
            description("Offset is out of bounds")
            display("Offset out of bounds: {}", t)
        }
        Write(t: String) {
            description("Invalid write")
            display("Invalid write: {}", t)
        }
        SimulatedFail(t: String) {
            description("Write failed by chance")
            display("Failed write: {}", t)
        }
    }
}

pub trait Flash {
    fn erase(&mut self, offset: usize, len: usize) -> Result<()>;
    fn write(&mut self, offset: usize, payload: &[u8]) -> Result<()>;
    fn read(&self, offset: usize, data: &mut [u8]) -> Result<()>;

    fn add_bad_region(&mut self, offset: usize, len: usize, rate: f32) -> Result<()>;
    fn reset_bad_regions(&mut self);

    fn set_verify_writes(&mut self, enable: bool);

    fn sector_iter(&self) -> SectorIter;
    fn device_size(&self) -> usize;

    fn align(&self) -> usize;
    fn erased_val(&self) -> u8;
}

fn ebounds<T: AsRef<str>>(message: T) -> ErrorKind {
    ErrorKind::OutOfBounds(message.as_ref().to_owned())
}

#[allow(dead_code)]
fn ewrite<T: AsRef<str>>(message: T) -> ErrorKind {
    ErrorKind::Write(message.as_ref().to_owned())
}

#[allow(dead_code)]
fn esimulatedwrite<T: AsRef<str>>(message: T) -> ErrorKind {
    ErrorKind::SimulatedFail(message.as_ref().to_owned())
}

/// An emulated flash device.  It is represented as a block of bytes, and a list of the sector
/// mapings.
#[derive(Clone)]
pub struct SimFlash {
    data: Vec<u8>,
    write_safe: Vec<bool>,
    sectors: Vec<usize>,
    bad_region: Vec<(usize, usize, f32)>,
    // Alignment required for writes.
    align: usize,
    verify_writes: bool,
    erased_val: u8,
}

impl SimFlash {
    /// Given a sector size map, construct a flash device for that.
    pub fn new(sectors: Vec<usize>, align: usize, erased_val: u8) -> SimFlash {
        // Verify that the alignment is a positive power of two.
        assert!(align > 0);
        assert!(align & (align - 1) == 0);

        let total = sectors.iter().sum();
        SimFlash {
            data: vec![erased_val; total],
            write_safe: vec![true; total],
            sectors: sectors,
            bad_region: Vec::new(),
            align: align,
            verify_writes: true,
            erased_val: erased_val,
        }
    }

    #[allow(dead_code)]
    pub fn dump(&self) {
        self.data.dump();
    }

    /// Dump this image to the given file.
    #[allow(dead_code)]
    pub fn write_file<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let mut fd = File::create(path).chain_err(|| "Unable to write image file")?;
        fd.write_all(&self.data).chain_err(|| "Unable to write to image file")?;
        Ok(())
    }

    // Scan the sector map, and return the base and offset within a sector for this given byte.
    // Returns None if the value is outside of the device.
    fn get_sector(&self, offset: usize) -> Option<(usize, usize)> {
        let mut offset = offset;
        for (sector, &size) in self.sectors.iter().enumerate() {
            if offset < size {
                return Some((sector, offset));
            }
            offset -= size;
        }
        return None;
    }

}

impl Flash for SimFlash {
    /// The flash drivers tend to erase beyond the bounds of the given range.  Instead, we'll be
    /// strict, and make sure that the passed arguments are exactly at a sector boundary, otherwise
    /// return an error.
    fn erase(&mut self, offset: usize, len: usize) -> Result<()> {
        let (_start, slen) = self.get_sector(offset).ok_or_else(|| ebounds("start"))?;
        let (end, elen) = self.get_sector(offset + len - 1).ok_or_else(|| ebounds("end"))?;

        if slen != 0 {
            bail!(ebounds("offset not at start of sector"));
        }
        if elen != self.sectors[end] - 1 {
            bail!(ebounds("end not at start of sector"));
        }

        for x in &mut self.data[offset .. offset + len] {
            *x = self.erased_val;
        }

        for x in &mut self.write_safe[offset .. offset + len] {
            *x = true;
        }

        Ok(())
    }

    /// We restrict to only allowing writes of values that are:
    ///
    /// 1. being written to for the first time
    /// 2. being written to after being erased
    ///
    /// This emulates a flash device which starts out erased, with the
    /// added restriction that repeated writes to the same location
    /// are disallowed, even if they would be safe to do.
    fn write(&mut self, offset: usize, payload: &[u8]) -> Result<()> {
        for &(off, len, rate) in &self.bad_region {
            if offset >= off && (offset + payload.len()) <= (off + len) {
                let mut rng = rand::thread_rng();
                let between = Range::new(0., 1.);
                if between.ind_sample(&mut rng) < rate {
                    bail!(esimulatedwrite(
                        format!("Ignoring write to {:#x}-{:#x}", off, off + len)));
                }
            }
        }

        if offset + payload.len() > self.data.len() {
            panic!("Write outside of device");
        }

        // Verify the alignment (which must be a power of two).
        if offset & (self.align - 1) != 0 {
            panic!("Misaligned write address");
        }

        if payload.len() & (self.align - 1) != 0 {
            panic!("Write length not multiple of alignment");
        }

        for (i, x) in &mut self.write_safe[offset .. offset + payload.len()].iter_mut().enumerate() {
            if self.verify_writes && !(*x) {
                panic!("Write to unerased location at 0x{:x}", offset + i);
            }
            *x = false;
        }

        let sub = &mut self.data[offset .. offset + payload.len()];
        sub.copy_from_slice(payload);
        Ok(())
    }

    /// Read is simple.
    fn read(&self, offset: usize, data: &mut [u8]) -> Result<()> {
        if offset + data.len() > self.data.len() {
            bail!(ebounds("Read outside of device"));
        }

        let sub = &self.data[offset .. offset + data.len()];
        data.copy_from_slice(sub);
        Ok(())
    }

    /// Adds a new flash bad region. Writes to this area fail with a chance
    /// given by `rate`.
    fn add_bad_region(&mut self, offset: usize, len: usize, rate: f32) -> Result<()> {
        if rate < 0.0 || rate > 1.0 {
            bail!(ebounds("Invalid rate"));
        }

        info!("Adding new bad region {:#x}-{:#x}", offset, offset + len);
        self.bad_region.push((offset, len, rate));

        Ok(())
    }

    fn reset_bad_regions(&mut self) {
        self.bad_region.clear();
    }

    fn set_verify_writes(&mut self, enable: bool) {
        self.verify_writes = enable;
    }

    /// An iterator over each sector in the device.
    fn sector_iter(&self) -> SectorIter {
        SectorIter {
            iter: self.sectors.iter().enumerate(),
            base: 0,
        }
    }

    fn device_size(&self) -> usize {
        self.data.len()
    }

    fn align(&self) -> usize {
        self.align
    }

    fn erased_val(&self) -> u8 {
        self.erased_val
    }
}

/// It is possible to iterate over the sectors in the device, each element returning this.
#[derive(Debug, Clone)]
pub struct Sector {
    /// Which sector is this, starting from 0.
    pub num: usize,
    /// The offset, in bytes, of the start of this sector.
    pub base: usize,
    /// The length, in bytes, of this sector.
    pub size: usize,
}

pub struct SectorIter<'a> {
    iter: Enumerate<slice::Iter<'a, usize>>,
    base: usize,
}

impl<'a> Iterator for SectorIter<'a> {
    type Item = Sector;

    fn next(&mut self) -> Option<Sector> {
        match self.iter.next() {
            None => None,
            Some((num, &size)) => {
                let base = self.base;
                self.base += size;
                Some(Sector {
                    num: num,
                    base: base,
                    size: size,
                })
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::{Flash, SimFlash, Error, ErrorKind, Result, Sector};

    #[test]
    fn test_flash() {
        for &erased_val in &[0, 0xff] {
            // NXP-style, uniform sectors.
            let mut f1 = SimFlash::new(vec![4096usize; 256], 1, erased_val);
            test_device(&mut f1, erased_val);

            // STM style, non-uniform sectors.
            let mut f2 = SimFlash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 64 * 1024,
                                    128 * 1024, 128 * 1024, 128 * 1024], 1, erased_val);
            test_device(&mut f2, erased_val);
        }
    }

    fn test_device(flash: &mut Flash, erased_val: u8) {
        let sectors: Vec<Sector> = flash.sector_iter().collect();

        flash.erase(0, sectors[0].size).unwrap();
        let flash_size = flash.device_size();
        flash.erase(0, flash_size).unwrap();
        assert!(flash.erase(0, sectors[0].size - 1).is_bounds());

        // Verify that write and erase do something.
        flash.write(0, &[0x55]).unwrap();
        let mut buf = [0xAA; 4];
        flash.read(0, &mut buf).unwrap();
        assert_eq!(buf, [0x55, erased_val, erased_val, erased_val]);

        flash.erase(0, sectors[0].size).unwrap();
        flash.read(0, &mut buf).unwrap();
        assert_eq!(buf, [erased_val; 4]);

        // Program the first and last byte of each sector, verify that has been done, and then
        // erase to verify the erase boundaries.
        for sector in &sectors {
            let byte = [(sector.num & 127) as u8];
            flash.write(sector.base, &byte).unwrap();
            flash.write(sector.base + sector.size - 1, &byte).unwrap();
        }

        // Verify the above
        let mut buf = Vec::new();
        for sector in &sectors {
            let byte = (sector.num & 127) as u8;
            buf.resize(sector.size, 0);
            flash.read(sector.base, &mut buf).unwrap();
            assert_eq!(buf.first(), Some(&byte));
            assert_eq!(buf.last(), Some(&byte));
            assert!(buf[1..buf.len()-1].iter().all(|&x| x == erased_val));
        }
    }

    // Helper checks for the result type.
    trait EChecker {
        fn is_bounds(&self) -> bool;
    }

    impl<T> EChecker for Result<T> {

        fn is_bounds(&self) -> bool {
            match *self {
                Err(Error(ErrorKind::OutOfBounds(_), _)) => true,
                _ => false,
            }
        }
    }
}
