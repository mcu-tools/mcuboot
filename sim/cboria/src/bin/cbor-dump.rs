//! Dump a CBor encoded file in a readable fashion.

use cboria::CborDump;
use std::{
    collections::HashSet,
    env,
    fs::File,
    io::BufReader,
    result,
};

type Result<T> = result::Result<T, failure::Error>;

fn main() -> Result<()> {
    for arg in env::args().skip(1) {
        println!("decoding: {:?}", arg);
        let mut nests = HashSet::new();
        // nests.insert("4591c357".to_string());
        nests.insert("79134af4".to_string());
        nests.insert("bea4d923".to_string());
        nests.insert("6dd9cd89".to_string());
        nests.insert("95473d0e".to_string());
        let buf = BufReader::new(File::open(arg)?);
        let mut dec = CborDump::new(buf, &nests);
        dec.walk()?;
    }
    Ok(())
}
