// Copyright (c) 2017 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

// Printable hexdump.

pub trait HexDump {
    // Output the data value in hex.
    fn dump(&self);
}

struct Dumper {
    hex: String,
    ascii: String,
    count: usize,
    total_count: usize,
}

impl Dumper {
    fn new() -> Dumper {
        Dumper {
            hex: String::with_capacity(49),
            ascii: String::with_capacity(16),
            count: 0,
            total_count: 0,
        }
    }

    fn add_byte(&mut self, ch: u8) {
        if self.count == 16 {
            self.ship();
        }
        if self.count == 8 {
            self.hex.push(' ');
        }
        self.hex.push_str(&format!(" {:02x}", ch)[..]);
        self.ascii.push(if ch >= ' ' as u8 && ch <= '~' as u8 {
            ch as char
        } else {
            '.'
        });
        self.count += 1;
    }

    fn ship(&mut self) {
        if self.count == 0 {
            return;
        }

        println!("{:06x} {:-49} |{}|", self.total_count, self.hex, self.ascii);

        self.hex.clear();
        self.ascii.clear();
        self.total_count += 16;
        self.count = 0;
    }
}

impl<'a> HexDump for &'a [u8] {
    fn dump(&self) {
        let mut dump = Dumper::new();
        for ch in self.iter() {
            dump.add_byte(*ch);
        }
        dump.ship();
    }
}

impl HexDump for Vec<u8> {
    fn dump(&self) {
        (&self[..]).dump()
    }
}

#[test]
fn samples() {
    "Hello".as_bytes().dump();
    "This is a much longer string".as_bytes().dump();
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f".as_bytes().dump();
}
