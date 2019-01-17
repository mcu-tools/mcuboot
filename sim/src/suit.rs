//! Manifest generation for the suit manifest format.

use crate::tlv::ManifestGen;
use suit::{SuitGenerator, RSA2048Signer};

pub struct SuitManifestGenerator {
    gen: SuitGenerator,
    payload: Vec<u8>,
}

impl SuitManifestGenerator {
    pub fn new() -> SuitManifestGenerator {
        SuitManifestGenerator {
            gen: SuitGenerator::new(),
            payload: Vec::new(),
        }
    }

    pub fn set_sequence(&mut self, seq: u32) {
        self.gen.set_sequence(seq).unwrap();
    }
}

impl ManifestGen for SuitManifestGenerator {
    fn get_magic(&self) -> u32 {
        0x96f3b83e
    }

    fn get_flags(&self) -> u32 {
        0
    }

    fn add_bytes(&mut self, bytes: &[u8]) {
        // Just append them to the bytes that we have.  We could eliminate
        // the extra copy by changing the API, but it probably doesn't hurt
        // much.
        self.payload.extend_from_slice(bytes);
    }

    fn make_tlv(mut self: Box<Self>) -> Vec<u8> {
        let signer = RSA2048Signer::from_bytes(include_bytes!("../../root-rsa-2048.pem").as_ref())
            .unwrap();
        self.gen.set_signer(Box::new(signer)).unwrap();
        self.gen.set_key_id(b"root-rsa-2048.pem".to_owned().to_vec()).unwrap();
        self.gen.add_payload(&self.payload).unwrap();
        self.gen.generate().unwrap()
    }
}
