//! TLV Support
//!
//! mcuboot images are followed immediately by a list of TLV items that contain integrity
//! information about the image.  Their generation is made a little complicated because the size of
//! the TLV block is in the image header, which is included in the hash.  Since some signatures can
//! vary in size, we just make them the largest size possible.
//!
//! Because of this header, we have to make two passes.  The first pass will compute the size of
//! the TLV, and the second pass will build the data for the TLV.

use std::sync::Arc;
use pem;
use ring::{digest, rand, signature};
use untrusted;

#[repr(u8)]
#[derive(Copy, Clone, PartialEq, Eq)]
#[allow(dead_code)] // TODO: For now
pub enum TlvKinds {
    KEYHASH = 0x01,
    SHA256 = 0x10,
    RSA2048 = 0x20,
    ECDSA224 = 0x21,
    ECDSA256 = 0x22,
}

pub struct TlvGen {
    flags: u32,
    kinds: Vec<TlvKinds>,
    size: u16,
    payload: Vec<u8>,
}

impl TlvGen {
    /// Construct a new tlv generator that will only contain a hash of the data.
    #[allow(dead_code)]
    pub fn new_hash_only() -> TlvGen {
        TlvGen {
            flags: 0,
            kinds: vec![TlvKinds::SHA256],
            size: 4 + 32,
            payload: vec![],
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa_pss() -> TlvGen {
        TlvGen {
            flags: 0,
            kinds: vec![TlvKinds::SHA256, TlvKinds::KEYHASH, TlvKinds::RSA2048],
            size: 4 + 32 + 4 + 256,
            payload: vec![],
        }
    }

    /// Retrieve the header flags for this configuration.  This can be called at any time.
    pub fn get_flags(&self) -> u32 {
        self.flags
    }

    /// Retrieve the size that the TLV will occupy.  This can be called at any time.
    pub fn get_size(&self) -> u16 {
        self.size
    }

    /// Add bytes to the covered hash.
    pub fn add_bytes(&mut self, bytes: &[u8]) {
        self.payload.extend_from_slice(bytes);
    }

    /// Compute the TLV given the specified block of data.
    pub fn make_tlv(self) -> Vec<u8> {
        let mut result: Vec<u8> = vec![];

        if self.kinds.contains(&TlvKinds::SHA256) {
            let hash = digest::digest(&digest::SHA256, &self.payload);
            let hash = hash.as_ref();

            assert!(hash.len() == 32);
            result.push(TlvKinds::SHA256 as u8);
            result.push(0);
            result.push(32);
            result.push(0);
            result.extend_from_slice(hash);
        }

        if self.kinds.contains(&TlvKinds::RSA2048) {
            // Output the hash of the public key.
            let hash = digest::digest(&digest::SHA256, RSA_PUB_KEY);
            let hash = hash.as_ref();

            assert!(hash.len() == 32);
            result.push(TlvKinds::KEYHASH as u8);
            result.push(0);
            result.push(32);
            result.push(0);
            result.extend_from_slice(hash);

            // For now assume PSS.
            let key_bytes = pem::parse(include_bytes!("../../root-rsa-2048.pem").as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "RSA PRIVATE KEY");
            let key_bytes = untrusted::Input::from(&key_bytes.contents);
            let key = signature::RSAKeyPair::from_der(key_bytes).unwrap();
            let mut signer = signature::RSASigningState::new(Arc::new(key)).unwrap();
            let rng = rand::SystemRandom::new();
            let mut signature = vec![0; signer.key_pair().public_modulus_len()];
            assert_eq!(signature.len(), 256);
            signer.sign(&signature::RSA_PSS_SHA256, &rng, &self.payload, &mut signature).unwrap();

            result.push(TlvKinds::RSA2048 as u8);
            result.push(0);
            result.push((signature.len() & 0xFF) as u8);
            result.push(((signature.len() >> 8) & 0xFF) as u8);
            result.extend_from_slice(&signature);
        }

        result
    }
}

include!("rsa_pub_key-rs.txt");
