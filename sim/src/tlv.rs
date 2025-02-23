// Copyright (c) 2017-2021 Linaro LTD
// Copyright (c) 2017-2020 JUUL Labs
// Copyright (c) 2021-2023 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! TLV Support
//!
//! mcuboot images are followed immediately by a list of TLV items that contain integrity
//! information about the image.  Their generation is made a little complicated because the size of
//! the TLV block is in the image header, which is included in the hash.  Since some signatures can
//! vary in size, we just make them the largest size possible.
//!
//! Because of this header, we have to make two passes.  The first pass will compute the size of
//! the TLV, and the second pass will build the data for the TLV.

use byteorder::{
    LittleEndian, WriteBytesExt,
};
use cipher::FromBlockCipher;
use crate::caps::Caps;
use crate::image::ImageVersion;
use log::info;
use ring::{digest, rand, agreement, hkdf, hmac};
use ring::rand::SecureRandom;
use ring::signature::{
    RsaKeyPair,
    RSA_PSS_SHA256,
    EcdsaKeyPair,
    ECDSA_P256_SHA256_ASN1_SIGNING,
    Ed25519KeyPair,
    ECDSA_P384_SHA384_ASN1_SIGNING,
};
use aes::{
    Aes128,
    Aes128Ctr,
    Aes256,
    Aes256Ctr,
    NewBlockCipher
};
use cipher::{
    generic_array::GenericArray,
    StreamCipher,
};
use mcuboot_sys::c;
use typenum::{U16, U32};

#[repr(u16)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[allow(dead_code)] // TODO: For now
pub enum TlvKinds {
    KEYHASH = 0x01,
    SHA256 = 0x10,
    SHA384 = 0x11,
    RSA2048 = 0x20,
    ECDSASIG = 0x22,
    RSA3072 = 0x23,
    ED25519 = 0x24,
    ENCRSA2048 = 0x30,
    ENCKW = 0x31,
    ENCEC256 = 0x32,
    ENCX25519 = 0x33,
    DEPENDENCY = 0x40,
    SECCNT = 0x50,
}

#[allow(dead_code, non_camel_case_types)]
pub enum TlvFlags {
    PIC = 0x01,
    NON_BOOTABLE = 0x02,
    ENCRYPTED_AES128 = 0x04,
    ENCRYPTED_AES256 = 0x08,
    RAM_LOAD = 0x20,
}

/// A generator for manifests.  The format of the manifest can be either a
/// traditional "TLV" or a SUIT-style manifest.
pub trait ManifestGen {
    /// Retrieve the header magic value for this manifest type.
    fn get_magic(&self) -> u32;

    /// Retrieve the flags value for this particular manifest type.
    fn get_flags(&self) -> u32;

    /// Retrieve the number of bytes of this manifest that is "protected".
    /// This field is stored in the outside image header instead of the
    /// manifest header.
    fn protect_size(&self) -> u16;

    /// Add a dependency on another image.
    fn add_dependency(&mut self, id: u8, version: &ImageVersion);

    /// Add a sequence of bytes to the payload that the manifest is
    /// protecting.
    fn add_bytes(&mut self, bytes: &[u8]);

    /// Set an internal flag indicating that the next `make_tlv` should
    /// corrupt the signature.
    fn corrupt_sig(&mut self);

    /// Estimate the size of the TLV.  This can be called before the payload is added (but after
    /// other information is added).  Some of the signature algorithms can generate variable sized
    /// data, and therefore, this can slightly overestimate the size.
    fn estimate_size(&self) -> usize;

    /// Construct the manifest for this payload.
    fn make_tlv(self: Box<Self>) -> Vec<u8>;

    /// Generate a new encryption random key
    fn generate_enc_key(&mut self);

    /// Return the current encryption key
    fn get_enc_key(&self) -> Vec<u8>;

    /// Set the security counter to the specified value.
    fn set_security_counter(&mut self, security_cnt: Option<u32>);

    /// Sets the ignore_ram_load_flag so that can be validated when it is missing,
    /// it will not load successfully.
    fn set_ignore_ram_load_flag(&mut self);
}

#[derive(Debug, Default)]
pub struct TlvGen {
    flags: u32,
    kinds: Vec<TlvKinds>,
    payload: Vec<u8>,
    dependencies: Vec<Dependency>,
    enc_key: Vec<u8>,
    /// Should this signature be corrupted.
    gen_corrupted: bool,
    security_cnt: Option<u32>,
    /// Ignore RAM_LOAD flag
    ignore_ram_load_flag: bool,
}

#[derive(Debug)]
struct Dependency {
    id: u8,
    version: ImageVersion,
}

impl TlvGen {
    /// Construct a new tlv generator that will only contain a hash of the data.
    #[allow(dead_code)]
    pub fn new_hash_only() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa_pss() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa3072_pss() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA3072],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa() -> TlvGen {
        let hash_kind = if cfg!(feature = "sig-p384") {
            TlvKinds::SHA384
        } else {
            TlvKinds::SHA256
        };
        TlvGen {
            kinds: vec![hash_kind, TlvKinds::ECDSASIG],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ed25519() -> TlvGen {
        TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::ED25519],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_enc_rsa(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCRSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_sig_enc_rsa(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048, TlvKinds::ENCRSA2048],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_enc_kw(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCKW],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_rsa_kw(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::RSA2048, TlvKinds::ENCKW],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa_kw(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ECDSASIG, TlvKinds::ENCKW],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecies_p256(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCEC256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecdsa_ecies_p256(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ECDSASIG, TlvKinds::ENCEC256],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ecies_x25519(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ENCX25519],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_ed25519_ecies_x25519(aes_key_size: u32) -> TlvGen {
        let flag = if aes_key_size == 256 {
            TlvFlags::ENCRYPTED_AES256 as u32
        } else {
            TlvFlags::ENCRYPTED_AES128 as u32
        };
        TlvGen {
            flags: flag,
            kinds: vec![TlvKinds::SHA256, TlvKinds::ED25519, TlvKinds::ENCX25519],
            ..Default::default()
        }
    }

    #[allow(dead_code)]
    pub fn new_sec_cnt() -> TlvGen {
       TlvGen {
            kinds: vec![TlvKinds::SHA256, TlvKinds::SECCNT],
            ..Default::default()
        }
    }

}

impl ManifestGen for TlvGen {
    fn get_magic(&self) -> u32 {
        0x96f3b83d
    }

    /// Retrieve the header flags for this configuration.  This can be called at any time.
    fn get_flags(&self) -> u32 {
        // For the RamLoad case, add in the flag for this feature.
        if Caps::RamLoad.present() && !self.ignore_ram_load_flag {
            self.flags | (TlvFlags::RAM_LOAD as u32)
        } else {
            self.flags
        }
    }

    /// Add bytes to the covered hash.
    fn add_bytes(&mut self, bytes: &[u8]) {
        self.payload.extend_from_slice(bytes);
    }

    fn protect_size(&self) -> u16 {
        let mut size = 0;
        if !self.dependencies.is_empty() || (Caps::HwRollbackProtection.present() && self.security_cnt.is_some()) {
            // include the TLV area header.
            size += 4;
            // add space for each dependency.
            size +=  (self.dependencies.len() as u16) * (4 + std::mem::size_of::<Dependency>() as u16);
            if Caps::HwRollbackProtection.present() && self.security_cnt.is_some() {
                size += 4 + 4;
            }
        }
        size
    }

    fn add_dependency(&mut self, id: u8, version: &ImageVersion) {
        self.dependencies.push(Dependency {
            id,
            version: version.clone(),
        });
    }

    fn corrupt_sig(&mut self) {
        self.gen_corrupted = true;
    }

    fn estimate_size(&self) -> usize {
        // Begin the estimate with the 4 byte header.
        let mut estimate = 4;
        // A very poor estimate.

        // Estimate the size of the image hash.
        if self.kinds.contains(&TlvKinds::SHA256) {
            estimate += 4 + 32;
        } else if self.kinds.contains(&TlvKinds::SHA384) {
            estimate += 4 + 48;
        }

        // Add an estimate in for each of the signature algorithms.
        if self.kinds.contains(&TlvKinds::RSA2048) {
            estimate += 4 + 32; // keyhash
            estimate += 4 + 256; // RSA2048
        }
        if self.kinds.contains(&TlvKinds::RSA3072) {
            estimate += 4 + 32; // keyhash
            estimate += 4 + 384; // RSA3072
        }
        if self.kinds.contains(&TlvKinds::ED25519) {
            estimate += 4 + 32; // keyhash
            estimate += 4 + 64; // ED25519 signature.
        }
        if self.kinds.contains(&TlvKinds::ECDSASIG) {
            // ECDSA signatures are encoded as ASN.1 with the x and y values
            // stored as signed integers. As such, the size can vary by 2 bytes,
            // if for example the 256-bit value has the high bit, it takes an
            // extra 0 byte to avoid it being seen as a negative number.
            if self.kinds.contains(&TlvKinds::SHA384) {
                estimate += 4 + 48;  // SHA384
                estimate += 4 + 104; // ECDSA384 (varies)
            } else {
                estimate += 4 + 32;  // SHA256
                estimate += 4 + 72;  // ECDSA256 (varies)
            }
        }

        // Estimate encryption.
        let flag = TlvFlags::ENCRYPTED_AES256 as u32;
        let aes256 = (self.get_flags() & flag) == flag;

        if self.kinds.contains(&TlvKinds::ENCRSA2048) {
            estimate += 4 + 256;
        }
        if self.kinds.contains(&TlvKinds::ENCKW) {
            estimate += 4 + if aes256 { 40 } else { 24 };
        }
        if self.kinds.contains(&TlvKinds::ENCEC256) {
            estimate += 4 + if aes256 { 129 } else { 113 };
        }
        if self.kinds.contains(&TlvKinds::ENCX25519) {
            estimate += 4 + if aes256 { 96 } else { 80 };
        }

        // Gather the size of the protected TLV area.
        estimate += self.protect_size() as usize;

        estimate
    }

    /// Compute the TLV given the specified block of data.
    fn make_tlv(self: Box<Self>) -> Vec<u8> {
        let size_estimate = self.estimate_size();

        let mut protected_tlv: Vec<u8> = vec![];

        if self.protect_size() > 0 {
            protected_tlv.push(0x08);
            protected_tlv.push(0x69);
            let size = self.protect_size();
            protected_tlv.write_u16::<LittleEndian>(size).unwrap();
            for dep in &self.dependencies {
                protected_tlv.write_u16::<LittleEndian>(TlvKinds::DEPENDENCY as u16).unwrap();
                protected_tlv.write_u16::<LittleEndian>(12).unwrap();

                // The dependency.
                protected_tlv.push(dep.id);
                protected_tlv.push(0);
                protected_tlv.write_u16::<LittleEndian>(0).unwrap();
                protected_tlv.push(dep.version.major);
                protected_tlv.push(dep.version.minor);
                protected_tlv.write_u16::<LittleEndian>(dep.version.revision).unwrap();
                protected_tlv.write_u32::<LittleEndian>(dep.version.build_num).unwrap();
            }

            // Security counter has to be at the protected TLV area also
            if Caps::HwRollbackProtection.present() && self.security_cnt.is_some() {
                protected_tlv.write_u16::<LittleEndian>(TlvKinds::SECCNT as u16).unwrap();
                protected_tlv.write_u16::<LittleEndian>(std::mem::size_of::<u32>() as u16).unwrap();
                protected_tlv.write_u32::<LittleEndian>(self.security_cnt.unwrap() as u32).unwrap();
            }

            assert_eq!(size, protected_tlv.len() as u16, "protected TLV length incorrect");
        }

        // Ring does the signature itself, which means that it must be
        // given a full, contiguous payload.  Although this does help from
        // a correct usage perspective, it is fairly stupid from an
        // efficiency view.  If this is shown to be a performance issue
        // with the tests, the protected data could be appended to the
        // payload, and then removed after the signature is done.  For now,
        // just make a signed payload.
        let mut sig_payload = self.payload.clone();
        sig_payload.extend_from_slice(&protected_tlv);

        let mut result: Vec<u8> = vec![];

        // add back signed payload
        result.extend_from_slice(&protected_tlv);

        // add non-protected payload
        let npro_pos = result.len();
        result.push(0x07);
        result.push(0x69);
        // Placeholder for the size.
        result.write_u16::<LittleEndian>(0).unwrap();

        if self.kinds.iter().any(|v| v == &TlvKinds::SHA256 || v == &TlvKinds::SHA384) {
            // If a signature is not requested, corrupt the hash we are
            // generating.  But, if there is a signature, output the
            // correct hash.  We want the hash test to pass so that the
            // signature verification can be validated.
            let mut corrupt_hash = self.gen_corrupted;
            for k in &[TlvKinds::RSA2048, TlvKinds::RSA3072,
                TlvKinds::ED25519, TlvKinds::ECDSASIG]
            {
                if self.kinds.contains(k) {
                    corrupt_hash = false;
                    break;
                }
            }

            if corrupt_hash {
                sig_payload[0] ^= 1;
            }
            let (hash,hash_size,tlv_kind) =  if self.kinds.contains(&TlvKinds::SHA256)
            {
                let hash = digest::digest(&digest::SHA256, &sig_payload);
                (hash,32,TlvKinds::SHA256)
            }
            else {
                let hash = digest::digest(&digest::SHA384, &sig_payload);
                (hash,48,TlvKinds::SHA384)
            };
            let hash = hash.as_ref();

            assert!(hash.len() == hash_size);
            result.write_u16::<LittleEndian>(tlv_kind as u16).unwrap();
            result.write_u16::<LittleEndian>(hash_size as u16).unwrap();
            result.extend_from_slice(hash);

            // Undo the corruption.
            if corrupt_hash {
                sig_payload[0] ^= 1;
            }

        }

        if self.gen_corrupted {
            // Corrupt what is signed by modifying the input to the
            // signature code.
            sig_payload[0] ^= 1;
        }

        if self.kinds.contains(&TlvKinds::RSA2048) ||
            self.kinds.contains(&TlvKinds::RSA3072) {

            let is_rsa2048 = self.kinds.contains(&TlvKinds::RSA2048);

            // Output the hash of the public key.
            let hash = if is_rsa2048 {
                digest::digest(&digest::SHA256, RSA_PUB_KEY)
            } else {
                digest::digest(&digest::SHA256, RSA3072_PUB_KEY)
            };
            let hash = hash.as_ref();

            assert!(hash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(hash);

            // For now assume PSS.
            let key_bytes = if is_rsa2048 {
                pem::parse(include_bytes!("../../root-rsa-2048.pem").as_ref()).unwrap()
            } else {
                pem::parse(include_bytes!("../../root-rsa-3072.pem").as_ref()).unwrap()
            };
            assert_eq!(key_bytes.tag, "RSA PRIVATE KEY");
            let key_pair = RsaKeyPair::from_der(&key_bytes.contents).unwrap();
            let rng = rand::SystemRandom::new();
            let mut signature = vec![0; key_pair.public_modulus_len()];
            if is_rsa2048 {
                assert_eq!(signature.len(), 256);
            } else {
                assert_eq!(signature.len(), 384);
            }
            key_pair.sign(&RSA_PSS_SHA256, &rng, &sig_payload, &mut signature).unwrap();

            if is_rsa2048 {
                result.write_u16::<LittleEndian>(TlvKinds::RSA2048 as u16).unwrap();
            } else {
                result.write_u16::<LittleEndian>(TlvKinds::RSA3072 as u16).unwrap();
            }
            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(&signature);
        }

        if self.kinds.contains(&TlvKinds::ECDSASIG) {
            let keyhash;
            let key_bytes;
            let sign_algo;
            let key_pair;
            let keyhash_size;
            let max_sig_size;

            if self.kinds.contains(&TlvKinds::SHA384) {
                keyhash = digest::digest(&digest::SHA384, ECDSAP384_PUB_KEY);
                keyhash_size = 48;
                key_bytes = pem::parse(include_bytes!("../../root-ec-p384-pkcs8.pem").as_ref()).unwrap();
                sign_algo = &ECDSA_P384_SHA384_ASN1_SIGNING;
                key_pair = EcdsaKeyPair::from_pkcs8(sign_algo, &key_bytes.contents).unwrap();
                max_sig_size = 104; // 96 bytes for "raw" r and s + at most 8 bytes for ASN.1 encoding
            } else {
                keyhash = digest::digest(&digest::SHA256, ECDSA256_PUB_KEY);
                keyhash_size = 32;
                key_bytes = pem::parse(include_bytes!("../../root-ec-p256-pkcs8.pem").as_ref()).unwrap();
                sign_algo = &ECDSA_P256_SHA256_ASN1_SIGNING;
                key_pair = EcdsaKeyPair::from_pkcs8(sign_algo, &key_bytes.contents).unwrap();
                max_sig_size = 72; // 64 bytes for "raw" r and s + at most 8 bytes for ASN.1 encoding
            }

            // ECDSA signatures are encoded as ASN.1 with the r and s values stored as signed
            // integers. As such, the size can vary depending on the value of the high bits of r and
            // s. The maximum size is obtained when the high bit of both r and s is '1'. To generate
            // the largest possible image, the size of the TLV area was estimated assuming the
            // signature has the maximal size. To obtain a TLV area size exactly equal to the
            // estimated size, the signing process is repeated multiple times until a signature
            // having the largest possible size is obtained. This is taking advantage of the fact
            // ECDSA signing uses a randomly-generated nonce and is therefore non-deterministic.
            // Theoretically, four tries should be needed on average and the probability of not
            // having obtained a signature of the desired size after 100 tries is lower than 10e-12.
            let rng = rand::SystemRandom::new();
            let max_tries = 100;
            let mut signature;
            let mut tries = 0;

            loop {
                signature = key_pair.sign(&rng, &sig_payload).unwrap();

                if signature.as_ref().len() == max_sig_size {
                    break;
                }

                tries += 1;

                if tries >= max_tries {
                    panic!("Failed to generate signature of correct size");
                }
            }

            // Write public key
            let keyhash_slice = keyhash.as_ref();
            assert!(keyhash_slice.len() == keyhash_size);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(keyhash_size as u16).unwrap();
            result.extend_from_slice(keyhash_slice);

            // Write signature
            result.write_u16::<LittleEndian>(TlvKinds::ECDSASIG as u16).unwrap();
            let signature = signature.as_ref().to_vec();
            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(&signature);
        }

        if self.kinds.contains(&TlvKinds::ED25519) {
            let keyhash = digest::digest(&digest::SHA256, ED25519_PUB_KEY);
            let keyhash = keyhash.as_ref();

            assert!(keyhash.len() == 32);
            result.write_u16::<LittleEndian>(TlvKinds::KEYHASH as u16).unwrap();
            result.write_u16::<LittleEndian>(32).unwrap();
            result.extend_from_slice(keyhash);

            let hash = digest::digest(&digest::SHA256, &sig_payload);
            let hash = hash.as_ref();
            assert!(hash.len() == 32);

            let key_bytes = pem::parse(include_bytes!("../../root-ed25519.pem").as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "PRIVATE KEY");

            let key_pair = Ed25519KeyPair::from_seed_and_public_key(
                &key_bytes.contents[16..48], &ED25519_PUB_KEY[12..44]).unwrap();
            let signature = key_pair.sign(&hash);

            result.write_u16::<LittleEndian>(TlvKinds::ED25519 as u16).unwrap();

            let signature = signature.as_ref().to_vec();
            result.write_u16::<LittleEndian>(signature.len() as u16).unwrap();
            result.extend_from_slice(signature.as_ref());
        }

        if self.kinds.contains(&TlvKinds::ENCRSA2048) {
            let key_bytes = pem::parse(include_bytes!("../../enc-rsa2048-pub.pem")
                                       .as_ref()).unwrap();
            assert_eq!(key_bytes.tag, "PUBLIC KEY");

            let cipherkey = self.get_enc_key();
            let cipherkey = cipherkey.as_slice();
            let encbuf = match c::rsa_oaep_encrypt(&key_bytes.contents, cipherkey) {
                Ok(v) => v,
                Err(_) => panic!("Failed to encrypt secret key"),
            };

            assert!(encbuf.len() == 256);
            result.write_u16::<LittleEndian>(TlvKinds::ENCRSA2048 as u16).unwrap();
            result.write_u16::<LittleEndian>(256).unwrap();
            result.extend_from_slice(&encbuf);
        }

        if self.kinds.contains(&TlvKinds::ENCKW) {
            let flag = TlvFlags::ENCRYPTED_AES256 as u32;
            let aes256 = (self.get_flags() & flag) == flag;
            let key_bytes = if aes256 {
                base64::decode(
                    include_str!("../../enc-aes256kw.b64").trim()).unwrap()
            } else {
                base64::decode(
                    include_str!("../../enc-aes128kw.b64").trim()).unwrap()
            };
            let cipherkey = self.get_enc_key();
            let cipherkey = cipherkey.as_slice();
            let keylen = if aes256 { 32 } else { 16 };
            let encbuf = match c::kw_encrypt(&key_bytes, cipherkey, keylen) {
                Ok(v) => v,
                Err(_) => panic!("Failed to encrypt secret key"),
            };

            let size = if aes256 { 40 } else { 24 };
            assert!(encbuf.len() == size);
            result.write_u16::<LittleEndian>(TlvKinds::ENCKW as u16).unwrap();
            result.write_u16::<LittleEndian>(size as u16).unwrap();
            result.extend_from_slice(&encbuf);
        }

        if self.kinds.contains(&TlvKinds::ENCEC256) || self.kinds.contains(&TlvKinds::ENCX25519) {
            let key_bytes = if self.kinds.contains(&TlvKinds::ENCEC256) {
                pem::parse(include_bytes!("../../enc-ec256-pub.pem").as_ref()).unwrap()
            } else {
                pem::parse(include_bytes!("../../enc-x25519-pub.pem").as_ref()).unwrap()
            };
            assert_eq!(key_bytes.tag, "PUBLIC KEY");
            let rng = rand::SystemRandom::new();
            let alg = if self.kinds.contains(&TlvKinds::ENCEC256) {
                &agreement::ECDH_P256
            } else {
                &agreement::X25519
            };
            let pk = match agreement::EphemeralPrivateKey::generate(alg, &rng) {
                Ok(v) => v,
                Err(_) => panic!("Failed to generate ephemeral keypair"),
            };

            let pubk = match pk.compute_public_key() {
                Ok(pubk) => pubk,
                Err(_) => panic!("Failed computing ephemeral public key"),
            };

            let peer_pubk = if self.kinds.contains(&TlvKinds::ENCEC256) {
                agreement::UnparsedPublicKey::new(&agreement::ECDH_P256, &key_bytes.contents[26..])
            } else {
                agreement::UnparsedPublicKey::new(&agreement::X25519, &key_bytes.contents[12..])
            };

            #[derive(Debug, PartialEq)]
            struct OkmLen<T: core::fmt::Debug + PartialEq>(T);

            impl hkdf::KeyType for OkmLen<usize> {
                fn len(&self) -> usize {
                    self.0
                }
            }

            let flag = TlvFlags::ENCRYPTED_AES256 as u32;
            let aes256 = (self.get_flags() & flag) == flag;

            let derived_key = match agreement::agree_ephemeral(
                pk, &peer_pubk, ring::error::Unspecified, |shared| {
                    let salt = hkdf::Salt::new(hkdf::HKDF_SHA256, &[]);
                    let prk = salt.extract(&shared);
                    let okm_len = if aes256 { 64 } else { 48 };
                    let okm = match prk.expand(&[b"MCUBoot_ECIES_v1"], OkmLen(okm_len)) {
                        Ok(okm) => okm,
                        Err(_) => panic!("Failed building HKDF OKM"),
                    };
                    let mut buf = if aes256 { vec![0u8; 64] } else { vec![0u8; 48] };
                    match okm.fill(&mut buf) {
                        Ok(_) => Ok(buf),
                        Err(_) => panic!("Failed generating HKDF output"),
                    }
                },
            ) {
                Ok(v) => v,
                Err(_) => panic!("Failed building HKDF"),
            };

            let nonce = GenericArray::from_slice(&[0; 16]);
            let mut cipherkey = self.get_enc_key();
            if aes256 {
                let key: &GenericArray<u8, U32> = GenericArray::from_slice(&derived_key[..32]);
                let block = Aes256::new(&key);
                let mut cipher = Aes256Ctr::from_block_cipher(block, &nonce);
                cipher.apply_keystream(&mut cipherkey);
            } else {
                let key: &GenericArray<u8, U16> = GenericArray::from_slice(&derived_key[..16]);
                let block = Aes128::new(&key);
                let mut cipher = Aes128Ctr::from_block_cipher(block, &nonce);
                cipher.apply_keystream(&mut cipherkey);
            }

            let size = if aes256 { 32 } else { 16 };
            let key = hmac::Key::new(hmac::HMAC_SHA256, &derived_key[size..]);
            let tag = hmac::sign(&key, &cipherkey);

            let mut buf = vec![];
            buf.append(&mut pubk.as_ref().to_vec());
            buf.append(&mut tag.as_ref().to_vec());
            buf.append(&mut cipherkey);

            if self.kinds.contains(&TlvKinds::ENCEC256) {
                let size = if aes256 { 129 } else { 113 };
                assert!(buf.len() == size);
                result.write_u16::<LittleEndian>(TlvKinds::ENCEC256 as u16).unwrap();
                result.write_u16::<LittleEndian>(size as u16).unwrap();
            } else {
                let size = if aes256 { 96 } else { 80 };
                assert!(buf.len() == size);
                result.write_u16::<LittleEndian>(TlvKinds::ENCX25519 as u16).unwrap();
                result.write_u16::<LittleEndian>(size as u16).unwrap();
            }
            result.extend_from_slice(&buf);
        }

        // Patch the size back into the TLV header.
        let size = (result.len() - npro_pos) as u16;
        let mut size_buf = &mut result[npro_pos + 2 .. npro_pos + 4];
        size_buf.write_u16::<LittleEndian>(size).unwrap();

        if size_estimate != result.len() {
            panic!("Incorrect size estimate: {} (actual {})", size_estimate, result.len());
        }

        result
    }

    fn generate_enc_key(&mut self) {
        let rng = rand::SystemRandom::new();
        let flag = TlvFlags::ENCRYPTED_AES256 as u32;
        let aes256 = (self.get_flags() & flag) == flag;
        let mut buf = if aes256 {
            vec![0u8; 32]
        } else {
            vec![0u8; 16]
        };
        if rng.fill(&mut buf).is_err() {
            panic!("Error generating encrypted key");
        }
        info!("New encryption key: {:02x?}", buf);
        self.enc_key = buf;
    }

    fn get_enc_key(&self) -> Vec<u8> {
        if self.enc_key.len() != 32 && self.enc_key.len() != 16 {
            panic!("No random key was generated");
        }
        self.enc_key.clone()
    }

    fn set_security_counter(&mut self, security_cnt: Option<u32>) {
        self.security_cnt = security_cnt;
    }

    fn set_ignore_ram_load_flag(&mut self) {
        self.ignore_ram_load_flag = true;
    }
}

include!("rsa_pub_key-rs.txt");
include!("rsa3072_pub_key-rs.txt");
include!("ecdsa_pub_key-rs.txt");
include!("ed25519_pub_key-rs.txt");
