//! SUIT encoder

use cboria::Encoder;
use failure::format_err;
use ring::{digest, rand, signature};
use std::{
    io::Write,
    result,
};

type Result<T> = result::Result<T, failure::Error>;

/// A builder for a suit manifest.  Create one of these, set the
/// appropriate data, and then generate the signed result with `generate`.
pub struct SuitGenerator {
    // The COSE Signature for the payload.
    payload: Option<Vec<u8>>,

    // The algorithm used to sign the payload.
    signer: Option<Box<dyn Signer>>,

    // The key ID used.
    key_id: Option<Vec<u8>>,

    // The sequence number.
    seq: u32,
}

impl SuitGenerator {
    /// Create a new generator.
    pub fn new() -> SuitGenerator {
        SuitGenerator {
            payload: None,
            signer: None,
            key_id: None,
            seq: 0,
        }
    }

    /// Add a primary payload.
    pub fn add_payload(&mut self, data: &[u8]) -> Result<()> {
        if self.payload.is_some() {
            panic!("Attempt to add multiple payloads");
        }

        self.payload = Some(data.to_owned());
        Ok(())
    }

    /// Set the sequence number for this manifest.
    pub fn set_sequence(&mut self, seq: u32) -> Result<()> {
        self.seq = seq;
        Ok(())
    }

    /// Set the key ID for this key.
    pub fn set_key_id(&mut self, key_id: Vec<u8>) -> Result<()> {
        self.key_id = Some(key_id);
        Ok(())
    }

    /// Set the Signer that will be used to sign messages.
    pub fn set_signer(&mut self, signer: Box<dyn Signer>) -> Result<()> {
        self.signer = Some(signer);
        Ok(())
    }

    /// Take the data we have, and generate a manifest.
    pub fn generate(mut self) -> Result<Vec<u8>> {
        if self.payload.is_none() {
            panic!("Manifest needs payload");
        }

        let mut buf: Vec<u8> = Vec::new();
        let mut encoder = Encoder::new(&mut buf);
        let manifest = self.gen_manifest()?;

        encoder.add_map(2)?;
        encoder.add_unsigned(1)?;
        self.sign_manifest(&manifest, &mut encoder)?;
        encoder.add_unsigned(2)?;
        encoder.add_bytestring(&manifest)?;
        encoder.assert_complete();
        Ok(buf)
    }

    /// Generate the manifest part of the data.
    fn gen_manifest(&self) -> Result<Vec<u8>> {
        let mut buf: Vec<u8> = Vec::new();
        let mut encoder = Encoder::new(&mut buf);

        let payload = self.payload.as_ref().unwrap();

        encoder.add_map(3)?;
        // 1:1  manifest version.
        encoder.add_unsigned(1)?;
        encoder.add_unsigned(1)?;

        // 2:seq  sequence number
        encoder.add_unsigned(2)?;
        encoder.add_unsigned(self.seq as usize)?;

        // 5:Payload.
        encoder.add_unsigned(3)?;
        encoder.add_array(1)?;
        encoder.add_map(3)?;

        // Component designator.
        encoder.add_unsigned(1)?;
        encoder.add_array(1)?;
        encoder.add_bytestring(b"0")?;

        // 2:size
        encoder.add_unsigned(2)?;
        encoder.add_unsigned(payload.len())?;

        // 3:digest
        encoder.add_unsigned(3)?;
        gen_digest(&mut encoder, payload)?;

        encoder.assert_complete();
        Ok(buf)
    }

    fn sign_manifest<W: Write>(&mut self, manifest: &[u8], encoder: &mut Encoder<W>) -> Result<()> {
        let body_prot = {
            let mut buf: Vec<u8> = Vec::new();
            let mut encoder = Encoder::new(&mut buf);

            encoder.add_map(1)?;
            encoder.add_unsigned(3)?;
            encoder.add_unsigned(0)?;
            encoder.assert_complete();
            buf
        };

        let sig_type = self.signer.as_ref().map(|signer| signer.sig_type()).unwrap_or(0);
        let sig_prot = {
            let mut buf: Vec<u8> = Vec::new();
            let mut encoder = Encoder::new(&mut buf);

            encoder.add_map(1)?;
            encoder.add_unsigned(1)?;
            encoder.add_signed(sig_type)?;
            encoder.assert_complete();
            buf
        };

        // Build the block to actually sign.
        let sig_block = {
            let mut buf: Vec<u8> = Vec::new();
            let mut encoder = Encoder::new(&mut buf);

            encoder.add_array(5)?;
            encoder.add_utf8("Signature")?;
            encoder.add_bytestring(&body_prot)?;
            encoder.add_bytestring(&sig_prot)?;
            encoder.add_bytestring(b"")?;
            encoder.add_bytestring(&manifest)?;
            encoder.assert_complete();
            buf
        };

        let key_id = self.key_id.as_ref().ok_or_else(|| format_err!("Did not set key id"))?;

        encoder.add_tag(98)?;
        encoder.add_array(4)?;
        encoder.add_bytestring(&body_prot)?;
        encoder.add_map(0)?;
        encoder.add_null()?;
        encoder.add_array(1)?;
        encoder.add_array(3)?;
        encoder.add_bytestring(&sig_prot)?;
        encoder.add_map(1)?;
        encoder.add_unsigned(4)?;
        encoder.add_bytestring(key_id)?;
        match self.signer.as_mut() {
            None => encoder.add_null()?,
            Some(signer) => {
                let sig = signer.gen_signature(&sig_block)?;
                encoder.add_bytestring(&sig)?;
            }
        }

        Ok(())
    }
}

/// Generate a COSE Digest containing a SHA-256 signature of the given
/// payload.  Since there isn't a COSE Digest structure, this is based off
/// of the somewhat ambiguous description in the manifest draft.
fn gen_digest<W: Write>(encoder: &mut Encoder<W>, data: &[u8]) -> Result<()> {
    encoder.add_array(4)?;

    let id = {
        let mut buf: Vec<u8> = Vec::new();
        let mut encoder = Encoder::new(&mut buf);

        encoder.add_map(1)?;
        encoder.add_unsigned(1)?;
        encoder.add_unsigned(41)?;
        encoder.assert_complete();
        buf
    };
    encoder.add_bytestring(&id)?;
    encoder.add_map(0)?;
    encoder.add_null()?;

    let hash = {
        let mut buf: Vec<u8> = Vec::new();
        let mut encoder = Encoder::new(&mut buf);

        encoder.add_array(3)?;
        encoder.add_utf8("Digest")?;
        encoder.add_bytestring(&id)?;
        encoder.add_bytestring(data)?;
        encoder.assert_complete();
        digest::digest(&digest::SHA256, &buf)
    };
    encoder.add_bytestring(hash.as_ref())?;

    Ok(())
}

/// A signer is something that is able to sign a message (for COSE).
pub trait Signer {
    /// Return the COSE numeric code for this signature type.
    fn sig_type(&self) -> isize;

    /// Compute a signature for a block of data.
    fn gen_signature(&mut self, data: &[u8]) -> Result<Vec<u8>>;
}

pub struct RSA2048Signer {
    key: signature::RsaKeyPair,
}

impl RSA2048Signer {
    pub fn from_bytes(key_bytes: &[u8]) -> Result<RSA2048Signer> {
        // Note that the error from pem is not Send+Sync.  To make this
        // error work, encapsulate the error as a string.
        let key_bytes = pem::parse(key_bytes.as_ref())
            .map_err(|e| format_err!("pem error: {:?}", e))?;
        if key_bytes.tag != "RSA PRIVATE KEY" {
            return Err(format_err!("pem not of expected RSA PRIVATE KEY ({:?})", key_bytes.tag));
        }
        let key_bytes = untrusted::Input::from(&key_bytes.contents);
        let key = signature::RsaKeyPair::from_der(key_bytes)?;
        Ok(RSA2048Signer {
            key: key,
        })
    }
}


impl Signer for RSA2048Signer {
    fn sig_type(&self) -> isize {
        -37
    }

    fn gen_signature(&mut self, data: &[u8]) -> Result<Vec<u8>> {
        let mut signature = vec![0; self.key.public_modulus_len()];
        assert_eq!(signature.len(), 256);
        let rng = rand::SystemRandom::new();
        self.key.sign(&signature::RSA_PSS_SHA256, &rng, &data, &mut signature)?;
        Ok(signature)
    }
}
