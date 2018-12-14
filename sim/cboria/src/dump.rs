//! Simplistic cbor dump utility

use byteorder::{
    BigEndian,
    ReadBytesExt,
};
use crate::pdump::HexDump;
use ring::digest;
use std::{
    collections::HashSet,
    io::{BufReader, Read},
    mem,
};

use crate::Result;

/// Query if a given bytestring should be decoded as additional CBOR.
pub trait SubDecode {
    /// Determine if the given data should be decoded as additional CBOR.
    fn should_decode(&self, data: &[u8], context: &Context) -> bool;
}

impl<'h> SubDecode for &'h HashSet<String> {
    fn should_decode(&self, data: &[u8], _context: &Context) -> bool {
        let hash = digest::digest(&digest::SHA256, data);
        let hash = &hex::encode(hash.as_ref())[..8];
        println!("           key: {}", hash);
        self.contains(hash)
    }
}

pub struct CborDump<R, S> {
    reader: R,
    nests: S,
    context: Context,
}

impl<R: Read, S: SubDecode + Clone> CborDump<R, S> {
    pub fn new(r: R, nests: S) -> CborDump<R, S> {
        CborDump {
            reader: r,
            nests: nests,
            context: Context::new(),
        }
    }

    pub fn dump(&mut self) -> Result<()> {
        self.walk()
    }

    /// Walk a CBOR structure from the given reader.
    pub fn walk(&mut self) -> Result<()> {
        let (code, count) = self.get_code()?;
        // println!("code: {}, count: 0x{:x}", code, count);
        match code {
            0 => {
                println!("{}{:02x} {:<10}{} Integer",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
            }
            1 => {
                println!("{}{:02x} {:<10}{} Neg Integer",
                         self.context.left(),
                         code,
                         -1 - (count as isize),
                         self.context.pad(2 + 1 + 10));
            }
            2 => {
                println!("{}{:02x} {:<10x}{} Byte string",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
                let mut buf = vec![0u8; count];
                self.reader.read_exact(&mut buf)?;
                // Sometimes byte strings are nested CBOR elements, but this
                // can't be determined without knowing the schema.  Print a
                // digest of the payload, which can be used as an argument to
                // walk that.
                if self.nests.should_decode(&buf, &self.context) {
                    // Create a nested walker.
                    let mut nested = CborDump {
                        reader: BufReader::new(&buf[..]),
                        nests: self.nests.clone(),
                        context: mem::replace(&mut self.context, Context::new()),
                    };
                    nested.context.push(ContextItem::Nested);
                    nested.walk()?;
                    nested.context.pop();
                    mem::replace(&mut self.context, nested.context);
                } else {
                    buf.dump();
                }
            }
            3 => {
                println!("{}{:02x} {:<10x}{} UTF-8",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
                let mut buf = vec![0u8; count];
                self.reader.read_exact(&mut buf)?;
                println!("{:?}", String::from_utf8(buf)?);
            }
            4 => {
                println!("{}{:02x} {:<10x}{} Array",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
                self.context.push(ContextItem::Array(0));
                for _ in 0 .. count {
                    self.walk()?;
                }
                self.context.pop();
            }
            5 => {
                println!("{}{:02x} {:<10x}{} Map",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
                self.context.push(ContextItem::Map(0));
                for _ in 0 .. count {
                    self.walk()?;
                    self.walk()?;
                }
                self.context.pop();
            }
            6 => {
                println!("{}{:02x} {:<10x}{} Tag",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10));
                self.walk()?;
            }
            7 => {
                let kind = match count {
                    20 => "false",
                    21 => "true",
                    22 => "null",
                    23 => "undefined",
                    _ => panic!("TODO: Handle special {}", count),
                };
                println!("{}{:02x} {:<10x}{} {}",
                         self.context.left(),
                         code,
                         count,
                         self.context.pad(2 + 1 + 10), kind);
            }
            code => panic!("TODO: Implement code {}", code),
        }
        self.context.succ();
        // println!("Context: {:?}", self.context);

        Ok(())
    }

    /// Read a CBOR code value, and the associated integer.
    fn get_code(&mut self) -> Result<(u8, usize)> {
        let b = self.reader.read_u8()?;
        let code = b >> 5;
        let count = match b & 0x1f {
            count @ 0 ..= 23 => count as usize,
            24 => self.reader.read_u8()? as usize,
            25 => self.reader.read_u16::<BigEndian>()? as usize,
            26 => self.reader.read_u32::<BigEndian>()? as usize,
            27 => self.reader.read_u64::<BigEndian>()? as usize,
            31 => panic!("TODO: Implement indefinite"),
            count => panic!("Invalid length value: 0x{:x}", count),
        };
        Ok((code, count))
    }
}

/// The context of the decoder determines both indentation, as well as how
/// many elements within another node we are (as well as the type of nested
/// node).
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ContextItem {
    // We are within a tag.
    Tag,
    // We are within an array, having seen the given number of elements.
    Array(usize),
    // We are within a map, having seen the given number of elements.  This
    // is incremented for both keys and values.
    Map(usize),
    // We are visiting a nested CBOR block.
    Nested,
}

/// Tracks the context where we are.
#[derive(Debug, PartialEq, Eq)]
pub struct Context(pub Vec<ContextItem>);

impl Context {
    pub fn new() -> Context {
        Context(vec![])
    }

    /// Return spaces for the left of an indent.
    fn left(&self) -> String {
        let mut result = String::with_capacity(1 + 3 * self.0.len());
        result.push(' ');
        for _ in 0 .. self.0.len() {
            result.push_str("   ");
        }
        result
    }

    /// Return a padding string, the arguments being the number of bytes to
    /// print.
    fn pad(&self, printed: usize) -> String {
        let count = if self.0.len() + printed < 55 { 55 - (self.0.len() + printed) } else { 1 };
        let mut result = String::with_capacity(count);
        for _ in 0 .. count {
            result.push(' ');
        }
        result
    }

    /// Add another nest level to the current indentation.
    fn push(&mut self, context: ContextItem) {
        self.0.push(context);
    }

    /// Remove a level, panics if we weren't nested.
    fn pop(&mut self) {
        match self.0.pop() {
            None => panic!("Invalid nesting"),
            Some(_) => (),
        }
    }

    /// Advance the current context to advance the nodes we are visiting.
    fn succ(&mut self) {
        match self.0.pop() {
            None => (),
            Some(ref mut node) => {
                let new_node = match node {
                    ContextItem::Array(n) => ContextItem::Array(*n+1),
                    ContextItem::Map(n) => ContextItem::Map(*n+1),
                    node => node.clone(),
                };
                self.0.push(new_node);
            }
        }
    }

    /// Encode the context in a simple textual form for string/regex-based
    /// matching.
    pub fn to_text(&self) -> String {
        use std::fmt::Write;

        let mut result = String::new();

        for v in &self.0 {
            match v {
                ContextItem::Tag => result.push('t'),
                ContextItem::Nested => result.push('n'),
                ContextItem::Array(n) => write!(&mut result, "a{}", n).unwrap(),
                ContextItem::Map(n) => write!(&mut result, "m{}", n).unwrap(),
            }
        }

        result
    }
}
