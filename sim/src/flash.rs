//! A flash simulator
//!
//! This module is capable of simulating the type of NOR flash commonly used in microcontrollers.
//! These generally can be written as individual bytes, but must be erased in larger units.

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
    }
}

fn ebounds<T: AsRef<str>>(message: T) -> ErrorKind {
    ErrorKind::OutOfBounds(message.as_ref().to_owned())
}

fn ewrite<T: AsRef<str>>(message: T) -> ErrorKind {
    ErrorKind::Write(message.as_ref().to_owned())
}

/// An emulated flash device.  It is represented as a block of bytes, and a list of the sector
/// mapings.
#[derive(Clone)]
pub struct Flash {
    data: Vec<u8>,
    sectors: Vec<usize>,
    // Alignment required for writes.
    align: usize,
}

impl Flash {
    /// Given a sector size map, construct a flash device for that.
    pub fn new(sectors: Vec<usize>, align: usize) -> Flash {
        // Verify that the alignment is a positive power of two.
        assert!(align > 0);
        assert!(align & (align - 1) == 0);

        let total = sectors.iter().sum();
        Flash {
            data: vec![0xffu8; total],
            sectors: sectors,
            align: align,
        }
    }

    /// The flash drivers tend to erase beyond the bounds of the given range.  Instead, we'll be
    /// strict, and make sure that the passed arguments are exactly at a sector boundary, otherwise
    /// return an error.
    pub fn erase(&mut self, offset: usize, len: usize) -> Result<()> {
        let (_start, slen) = self.get_sector(offset).ok_or_else(|| ebounds("start"))?;
        let (end, elen) = self.get_sector(offset + len - 1).ok_or_else(|| ebounds("end"))?;

        if slen != 0 {
            bail!(ebounds("offset not at start of sector"));
        }
        if elen != self.sectors[end] - 1 {
            bail!(ebounds("end not at start of sector"));
        }

        for x in &mut self.data[offset .. offset + len] {
            *x = 0xff;
        }

        Ok(())
    }

    /// Writes are fairly unconstrained, but we restrict to only allowing writes of values that
    /// are entirely written as 0xFF.
    pub fn write(&mut self, offset: usize, payload: &[u8]) -> Result<()> {
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

        let mut sub = &mut self.data[offset .. offset + payload.len()];
        if sub.iter().any(|x| *x != 0xFF) {
            bail!(ewrite(format!("Write to non-FF location: offset: {:x}", offset)));
        }

        sub.copy_from_slice(payload);
        Ok(())
    }

    /// Read is simple.
    pub fn read(&self, offset: usize, data: &mut [u8]) -> Result<()> {
        if offset + data.len() > self.data.len() {
            bail!(ebounds("Read outside of device"));
        }

        let sub = &self.data[offset .. offset + data.len()];
        data.copy_from_slice(sub);
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

    /// An iterator over each sector in the device.
    pub fn sector_iter(&self) -> SectorIter {
        SectorIter {
            iter: self.sectors.iter().enumerate(),
            base: 0,
        }
    }

    pub fn device_size(&self) -> usize {
        self.data.len()
    }

    pub fn dump(&self) {
        self.data.dump();
    }

    /// Dump this image to the given file.
    pub fn write_file<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let mut fd = File::create(path).chain_err(|| "Unable to write image file")?;
        fd.write_all(&self.data).chain_err(|| "Unable to write to image file")?;
        Ok(())
    }
}

/// It is possible to iterate over the sectors in the device, each element returning this.
#[derive(Debug)]
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
    use super::{Flash, Error, ErrorKind, Result, Sector};

    #[test]
    fn test_flash() {
        // NXP-style, uniform sectors.
        let mut f1 = Flash::new(vec![4096usize; 256]);
        test_device(&mut f1);

        // STM style, non-uniform sectors
        let mut f2 = Flash::new(vec![16 * 1024, 16 * 1024, 16 * 1024, 64 * 1024,
                                128 * 1024, 128 * 1024, 128 * 1024]);
        test_device(&mut f2);
    }

    fn test_device(flash: &mut Flash) {
        let sectors: Vec<Sector> = flash.sector_iter().collect();

        flash.erase(0, sectors[0].size).unwrap();
        let flash_size = flash.device_size();
        flash.erase(0, flash_size).unwrap();
        assert!(flash.erase(0, sectors[0].size - 1).is_bounds());

        // Verify that write and erase do something.
        flash.write(0, &[0]).unwrap();
        let mut buf = [0; 4];
        flash.read(0, &mut buf).unwrap();
        assert_eq!(buf, [0, 0xff, 0xff, 0xff]);

        flash.erase(0, sectors[0].size).unwrap();
        flash.read(0, &mut buf).unwrap();
        assert_eq!(buf, [0xff; 4]);

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
            assert!(buf[1..buf.len()-1].iter().all(|&x| x == 0xff));
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
