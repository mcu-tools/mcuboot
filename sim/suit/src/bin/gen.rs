//! Generate a sample cbor.

use cboria::{
    Context,
    CborDump,
    SubDecode,
    pdump::HexDump,
};
use std::{
    result,
};
use suit::{SuitGenerator, RSA2048Signer};

type Result<T> = result::Result<T, failure::Error>;

fn main() -> Result<()> {
    let mut gen = SuitGenerator::new();
    gen.add_payload(b"Hello world")?;
    gen.set_sequence(0x1234)?;
    let signer = RSA2048Signer::from_bytes(include_bytes!("../../../../root-rsa-2048.pem").as_ref())?;
    gen.set_signer(Box::new(signer))?;
    let buf = gen.generate()?;
    buf.dump();
    println!("\nDump of generated CBOR:");
    CborDump::new(&buf[..], Deep()).dump()?;
    Ok(())
}

/// A simple subtype that attempts to decode nested CBOR.
#[derive(Clone)]
struct Deep();

impl SubDecode for Deep {
    fn should_decode(&self, _data: &[u8], context: &Context) -> bool {
        let textual = context.to_text();
        // println!("Context: {:?}", textual);
        textual == "m1a0" ||
            textual == "m1a3a0a0" ||
            textual == "m4" ||
            textual == "m4nm5a0m5a0" ||
            false
    }
}
