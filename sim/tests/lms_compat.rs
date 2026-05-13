// Copyright (c) 2026 Linaro LTD
//
// SPDX-License-Identifier: Apache-2.0

//! LMS RFC 8554 wire-format compatibility between imgtool's pyhsslms
//! wrapper and the `lms-signature` crate the simulator uses.
//!
//! Each test generates a fresh keypair at run time on whichever side is
//! signing — no committed key material. pyhsslms is reached via Python
//! subprocess, importing imgtool.keys.lms with `scripts/` on PYTHONPATH.

use std::path::PathBuf;
use std::process::Command;

use hybrid_array::Array;
use lms_signature::{
    lms::{LmsSha256M32H5, Signature, SigningKey, VerifyingKey},
    ots::LmsOtsSha256N32W8,
};
use signature::{RandomizedSignerMut, Verifier};

type Mode = LmsSha256M32H5<LmsOtsSha256N32W8>;

fn scripts_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("scripts")
}

fn run_python(script: &str, args: &[&str]) -> std::process::Output {
    let mut cmd = Command::new("python3");
    cmd.env("PYTHONPATH", scripts_dir());
    cmd.arg("-c").arg(script);
    for a in args {
        cmd.arg(a);
    }
    cmd.output()
        .expect("failed to spawn python3 — install python3 and pyhsslms")
}

fn hex_encode(bytes: &[u8]) -> String {
    let mut s = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        s.push_str(&format!("{:02x}", b));
    }
    s
}

fn hex_decode(s: &str) -> Vec<u8> {
    let s = s.trim();
    assert!(s.len() % 2 == 0, "odd-length hex");
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).expect("invalid hex"))
        .collect()
}

const PYTHON_SIGN: &str = r#"
import binascii, sys
from imgtool.keys.lms import LMS

msg = binascii.unhexlify(sys.argv[1])
key = LMS.generate('lms-sha256-h5-w8')
pub = key.get_public_bytes()
sig = key.sign_digest(msg)
sys.stdout.write(binascii.hexlify(pub).decode() + '\n')
sys.stdout.write(binascii.hexlify(sig).decode() + '\n')
"#;

const PYTHON_VERIFY: &str = r#"
import binascii, sys
import pyhsslms

pub_bytes = binascii.unhexlify(sys.argv[1])
sig = binascii.unhexlify(sys.argv[2])
msg = binascii.unhexlify(sys.argv[3])
pub = pyhsslms.LmsPublicKey.deserialize(pub_bytes)
ok = pub.verify(msg, sig)
sys.exit(0 if ok else 2)
"#;

#[test]
fn python_signs_rust_verifies() {
    let msg = b"mcuboot LMS compat: python signs, rust verifies";
    let out = run_python(PYTHON_SIGN, &[&hex_encode(msg)]);
    assert!(
        out.status.success(),
        "python signer failed:\nstdout: {}\nstderr: {}",
        String::from_utf8_lossy(&out.stdout),
        String::from_utf8_lossy(&out.stderr),
    );

    let stdout = String::from_utf8(out.stdout).expect("non-utf8 stdout from python");
    let mut lines = stdout.lines();
    let pub_bytes = hex_decode(lines.next().expect("missing pubkey line"));
    let sig_bytes = hex_decode(lines.next().expect("missing signature line"));

    let pk = VerifyingKey::<Mode>::try_from(&pub_bytes[..])
        .expect("pyhsslms public key did not parse via lms-signature");
    let sig = Signature::<Mode>::try_from(&sig_bytes[..])
        .expect("pyhsslms signature did not parse via lms-signature");

    pk.verify(msg, &sig)
        .expect("lms-signature rejected pyhsslms-produced signature");
}

#[test]
fn rust_signs_python_verifies() {
    let mut rng = rand_core::UnwrapErr(getrandom::SysRng);
    let mut sk = SigningKey::<Mode>::new(&mut rng);

    let msg = b"mcuboot LMS compat: rust signs, python verifies";
    let sig = sk
        .try_sign_with_rng(&mut rng, msg)
        .expect("lms-signature signing failed");

    let pub_arr: Array<u8, _> = sk.public().into();
    let pub_bytes = pub_arr.as_slice().to_vec();
    let sig_bytes: Vec<u8> = sig.into();

    let out = run_python(
        PYTHON_VERIFY,
        &[
            &hex_encode(&pub_bytes),
            &hex_encode(&sig_bytes),
            &hex_encode(msg),
        ],
    );
    assert!(
        out.status.success(),
        "pyhsslms rejected lms-signature output:\nstdout: {}\nstderr: {}",
        String::from_utf8_lossy(&out.stdout),
        String::from_utf8_lossy(&out.stderr),
    );
}
