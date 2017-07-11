//! TLV Support
//!
//! mcuboot images are followed immediately by a list of TLV items that contain integrity
//! information about the image.  Their generation is made a little complicated because the size of
//! the TLV block is in the image header, which is included in the hash.  Since some signatures can
//! vary in size, we just make them the largest size possible.
//!
//! Because of this header, we have to make two passes.  The first pass will compute the size of
//! the TLV, and the second pass will build the data for the TLV.

use crypto::digest::Digest;
use crypto::sha2::Sha256;

#[derive(EnumFlags, Copy, Clone, Debug)]
#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum Flags {
    PIC = 0x000001,
    SHA256 = 0x000002,
    PKCS15_RSA2048_SHA256 = 0x000004,
    ECDSA224_SHA256 = 0x000008,
    NON_BOOTABLE = 0x000010,
    ECDSA256_SHA256 = 0x000020,
    PKCS1_PSS_RSA2048_SHA256 = 0x000040,
}

#[repr(u8)]
#[derive(Copy, Clone, PartialEq, Eq)]
#[allow(dead_code)] // TODO: For now
pub enum TlvKinds {
    SHA256 = 1,
    RSA2048 = 2,
    ECDSA224 = 3,
    ECDSA256 = 4,
}

pub struct TlvGen {
    flags: Flags,
    kinds: Vec<TlvKinds>,
    size: u16,
    hasher: Sha256,
}

impl TlvGen {
    /// Construct a new tlv generator that will only contain a hash of the data.
    pub fn new_hash_only() -> TlvGen {
        TlvGen {
            flags: Flags::SHA256,
            kinds: vec![TlvKinds::SHA256],
            size: 4 + 32,
            hasher: Sha256::new(),
        }
    }

    /// Retrieve the header flags for this configuration.  This can be called at any time.
    pub fn get_flags(&self) -> u32 {
        self.flags as u32
    }

    /// Retrieve the size that the TLV will occupy.  This can be called at any time.
    pub fn get_size(&self) -> u16 {
        self.size
    }

    /// Add bytes to the covered hash.
    pub fn add_bytes(&mut self, bytes: &[u8]) {
        self.hasher.input(bytes);
    }

    /// Compute the TLV given the specified block of data.
    pub fn make_tlv(mut self) -> Vec<u8> {
        let mut result: Vec<u8> = vec![];

        if self.kinds.contains(&TlvKinds::SHA256) {
            let hash_size = self.hasher.output_bytes();
            assert!(hash_size == 32); // Assumption made.

            let mut hash = vec![0; hash_size];

            self.hasher.result(&mut hash);

            result.push(TlvKinds::SHA256 as u8);
            result.push(0);
            result.push(32);
            result.push(0);
            result.append(&mut hash);
        }

        result
    }
}
