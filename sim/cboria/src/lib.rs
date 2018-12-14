//! Simplistic CBOR encoder
//!
//! Rather than attempt to integrate with Serde, or have a data type it
//! represents, provide a simple "push" interface to a cbor encoder.
//! Because we always want to generate definite encoding, the array and map
//! types will require pushing a known length.  The encoder will fail if
//! the correct number of items isn't then pushed.

use byteorder::{
    BigEndian,
    WriteBytesExt,
};
use std::{
    io::{Write},
    result,
};

pub use crate::dump::{
    CborDump,
    ContextItem,
    Context,
    SubDecode,
};

pub mod pdump;
mod dump;

type Result<T> = result::Result<T, failure::Error>;

/// A CBOR Encoder.  Data is written to the given writer.  This tracks
/// nested items to ensure that the resulting data is coherent, but it is
/// up to the caller to open and close everything correctly.
pub struct Encoder<W> {
    writer: W,
    nested: Vec<Nested>,
}

/// This enum tracks what nested item we are currently on.  The count is
/// the number of expected items to write.  The map will be twice the count
/// given by the user.
#[derive(Debug)]
enum Nested {
    Single,
    Map(usize),
    Array(usize),
}

impl<W: Write> Encoder<W> {
    /// Construct a new encoder that writes to the given writer.
    pub fn new(w: W) -> Encoder<W> {
        Encoder {
            writer: w,
            nested: vec![Nested::Single],
        }
    }

    /// Adjust the nested value.  Panics if we aren't supposed to be
    /// writing anything.
    fn account_item(&mut self) {
        match self.nested.pop() {
            // If we aren't expecting to write anything, panic.
            None => panic!("Attempt to encode CBOR item out of context"),

            // Expecting to write a single item, so discard the write.
            Some(Nested::Single) => {}

            // For maps and array, decrement the counter, until zero, where
            // it will end the map or array.
            Some(Nested::Map(0)) => {}
            Some(Nested::Array(0)) => {}

            Some(Nested::Map(n)) => self.nested.push(Nested::Map(n-1)),
            Some(Nested::Array(n)) => self.nested.push(Nested::Array(n-1)),
        }

        // Now pop off any zero-arity items.
        loop {
            match self.nested.pop() {
                Some(Nested::Map(0)) => {}
                Some(Nested::Array(0)) => {}
                Some(item) => {
                    self.nested.push(item);
                    break;
                }
                None => break,
            }
        }
    }

    /// Write an unsigned integer.
    pub fn add_unsigned(&mut self, value: usize) -> Result<()> {
        self.account_item();
        self.add_code(0, value as u64)
    }

    pub fn add_signed(&mut self, value: isize) -> Result<()> {
        self.account_item();
        if value < 0 {
            self.add_code(1, (-1 - value) as u64)
        } else {
            self.add_code(0, value as u64)
        }
    }

    /// Add a tag.
    pub fn add_tag(&mut self, value: usize) -> Result<()> {
        self.account_item();
        self.add_code(6, value as u64)?;
        self.nested.push(Nested::Single);
        Ok(())
    }

    /// Add a byte-string.
    pub fn add_bytestring(&mut self, data: &[u8]) -> Result<()> {
        self.account_item();
        self.add_code(2, data.len() as u64)?;
        self.writer.write_all(data)?;
        Ok(())
    }

    pub fn add_utf8(&mut self, text: &str) -> Result<()> {
        self.account_item();
        self.add_code(3, text.len() as u64)?;
        self.writer.write_all(text.as_bytes())?;
        Ok(())
    }

    pub fn add_array(&mut self, len: usize) -> Result<()> {
        self.account_item();
        self.add_code(4, len as u64)?;
        if len > 0 {
            self.nested.push(Nested::Array(len));
        }
        Ok(())
    }

    /// Add a map to the CBOR.  'len' should be the number of elements in
    /// the map.  The caller must then encoder 2 elements for each of these
    /// elements in order to close out the map.
    pub fn add_map(&mut self, len: usize) -> Result<()> {
        let nlen = len as u64;
        if nlen > std::u64::MAX / 2 {
            panic!("Attempt to encode a map larger than u64::MAX / 2");
        }
        self.account_item();
        self.add_code(5, nlen)?;
        if len > 0 {
            self.nested.push(Nested::Map(len * 2));
        }
        Ok(())
    }

    pub fn add_null(&mut self) -> Result<()> {
        self.account_item();
        self.add_code(7, 22)
    }

    /// Write a single CBOR tag/value.
    fn add_code(&mut self, code: u8, value: u64) -> Result<()> {
        if value < 24 {
            self.writer.write_u8(code << 5 | value as u8)?;
        } else if value <= 0xff {
            self.writer.write_u8(code << 5 | 24)?;
            self.writer.write_u8(value as u8)?;
        } else if value <= 0xffff {
            self.writer.write_u8(code << 5 | 25)?;
            self.writer.write_u16::<BigEndian>(value as u16)?;
        } else if value <= 0xffffffff {
            self.writer.write_u8(code << 5 | 26)?;
            self.writer.write_u32::<BigEndian>(value as u32)?;
        } else {
            self.writer.write_u8(code << 5 | 27)?;
            self.writer.write_u64::<BigEndian>(value)?;
        }

        Ok(())
    }

    /// Check that the CBOR encoded is complete.  This can't be called
    /// during `drop` because drop will be called on error exit.  This
    /// should only be called after the item should be fully written.
    pub fn assert_complete(&self) {
        if !self.nested.is_empty() {
            panic!("Incomplete CBOR written: {:?} expected", self.nested);
        }
    }
}
