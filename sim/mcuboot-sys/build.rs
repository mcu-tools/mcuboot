// Build mcuboot as a library, based on the requested features.

extern crate cc;
extern crate cmake;

use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

fn main() {
    // Feature flags.
    let psa_crypto_api = env::var("CARGO_FEATURE_PSA_CRYPTO_API").is_ok();
    let sig_rsa = env::var("CARGO_FEATURE_SIG_RSA").is_ok();
    let sig_rsa3072 = env::var("CARGO_FEATURE_SIG_RSA3072").is_ok();
    let sig_ecdsa = env::var("CARGO_FEATURE_SIG_ECDSA").is_ok();
    let sig_ecdsa_mbedtls = env::var("CARGO_FEATURE_SIG_ECDSA_MBEDTLS").is_ok();
    let sig_ecdsa_psa = env::var("CARGO_FEATURE_SIG_ECDSA_PSA").is_ok();
    let sig_p384 = env::var("CARGO_FEATURE_SIG_P384").is_ok();
    let sig_ed25519 = env::var("CARGO_FEATURE_SIG_ED25519").is_ok();
    let sig_second_key = env::var("CARGO_FEATURE_SIG_SECOND_KEY").is_ok();
    let overwrite_only = env::var("CARGO_FEATURE_OVERWRITE_ONLY").is_ok();
    let swap_move = env::var("CARGO_FEATURE_SWAP_MOVE").is_ok();
    let swap_offset = env::var("CARGO_FEATURE_SWAP_OFFSET").is_ok();
    let validate_primary_slot =
                  env::var("CARGO_FEATURE_VALIDATE_PRIMARY_SLOT").is_ok();
    let enc_rsa = env::var("CARGO_FEATURE_ENC_RSA").is_ok();
    let enc_aes256_rsa = env::var("CARGO_FEATURE_ENC_AES256_RSA").is_ok();
    let enc_kw = env::var("CARGO_FEATURE_ENC_KW").is_ok();
    let enc_aes256_kw = env::var("CARGO_FEATURE_ENC_AES256_KW").is_ok();
    let enc_ec256 = env::var("CARGO_FEATURE_ENC_EC256").is_ok();
    let enc_ec256_mbedtls = env::var("CARGO_FEATURE_ENC_EC256_MBEDTLS").is_ok();
    let enc_aes256_ec256 = env::var("CARGO_FEATURE_ENC_AES256_EC256").is_ok();
    let enc_x25519 = env::var("CARGO_FEATURE_ENC_X25519").is_ok();
    let enc_aes256_x25519 = env::var("CARGO_FEATURE_ENC_AES256_X25519").is_ok();
    let bootstrap = env::var("CARGO_FEATURE_BOOTSTRAP").is_ok();
    let multiimage = env::var("CARGO_FEATURE_MULTIIMAGE").is_ok();
    let downgrade_prevention = env::var("CARGO_FEATURE_DOWNGRADE_PREVENTION").is_ok();
    let ram_load = env::var("CARGO_FEATURE_RAM_LOAD").is_ok();
    let direct_xip = env::var("CARGO_FEATURE_DIRECT_XIP").is_ok();
    let max_align_16 = env::var("CARGO_FEATURE_MAX_ALIGN_16").is_ok();
    let max_align_32 = env::var("CARGO_FEATURE_MAX_ALIGN_32").is_ok();
    let hw_rollback_protection = env::var("CARGO_FEATURE_HW_ROLLBACK_PROTECTION").is_ok();
    let check_load_addr = env::var("CARGO_FEATURE_CHECK_LOAD_ADDR").is_ok();
    let custom_crypto = env::var("CARGO_FEATURE_CUSTOM_CRYPTO").is_ok();
    let custom_enc_crypto = env::var("CARGO_FEATURE_CUSTOM_ENC_CRYPTO").is_ok();
    let mbedtls_v4 = env::var("CARGO_FEATURE_MBEDTLS_V4").is_ok();
    let logical_sectors_4k = env::var("CARGO_FEATURE_LOGICAL_SECTORS_4K").is_ok();

    let mut conf = CachedBuild::new();
    conf.conf.define("__BOOTSIM__", None);
    conf.conf.define("MCUBOOT_HAVE_LOGGING", None);
    conf.conf.define("MCUBOOT_USE_FLASH_AREA_GET_SECTORS", None);
    conf.conf.define("MCUBOOT_HAVE_ASSERT_H", None);
    conf.conf.define("MCUBOOT_MAX_IMG_SECTORS", Some("128"));

    if max_align_32 {
        conf.conf.define("MCUBOOT_BOOT_MAX_ALIGN", Some("32"));
    } else if max_align_16 {
        conf.conf.define("MCUBOOT_BOOT_MAX_ALIGN", Some("16"));
    } else {
        conf.conf.define("MCUBOOT_BOOT_MAX_ALIGN", Some("8"));
    }

    if logical_sectors_4k {
        conf.conf.define("MCUBOOT_LOGICAL_SECTOR_SIZE", Some("4096"));
    }

    conf.conf.define("MCUBOOT_IMAGE_NUMBER", Some(if multiimage { "2" } else { "1" }));

    if sig_second_key {
        if !sig_ed25519 {
            panic!("sig-second-key currently only supports sig-ed25519 in the simulator");
        }
        conf.conf.define("MCUBOOT_SIGN_KEY_2", None);
    }

    if downgrade_prevention && !overwrite_only {
        panic!("Downgrade prevention requires overwrite only");
    }

    if bootstrap {
        conf.conf.define("MCUBOOT_BOOTSTRAP", None);
        conf.conf.define("MCUBOOT_OVERWRITE_ONLY_FAST", None);
    }

    if check_load_addr {
        conf.conf.define("MCUBOOT_CHECK_HEADER_LOAD_ADDRESS", None);
    }

    if validate_primary_slot {
        conf.conf.define("MCUBOOT_VALIDATE_PRIMARY_SLOT", None);
    }

    if downgrade_prevention {
        conf.conf.define("MCUBOOT_DOWNGRADE_PREVENTION", None);
    }

    if ram_load {
        conf.conf.define("MCUBOOT_RAM_LOAD", None);
    }

    if direct_xip {
        conf.conf.define("MCUBOOT_DIRECT_XIP", None);
    }

    if hw_rollback_protection {
        conf.conf.define("MCUBOOT_HW_ROLLBACK_PROT", None);
        conf.file("csupport/security_cnt.c");
    }

    // Currently no more than one sig type can be used simultaneously.
    if vec![sig_rsa, sig_rsa3072, sig_ecdsa, sig_ed25519].iter()
        .fold(0, |sum, &v| sum + v as i32) > 1 {
        panic!("mcuboot does not support more than one sig type at the same time");
    }

    // custom-crypto selects MCUBOOT_USE_CUSTOM_CRYPTO and therefore
    // conflicts with features that also set a backend macro.
    if custom_crypto && (sig_rsa || sig_rsa3072 || sig_ecdsa ||
                         sig_ecdsa_mbedtls || sig_ecdsa_psa || sig_ed25519) {
        panic!("custom-crypto cannot be combined with sig-* features");
    }

    // custom-crypto encryption currently supports only ECIES-P256 via the
    // mbedTLS-backed stubs (enc-ec256-mbedtls / enc-aes256-ec256 / custom-enc-crypto).
    if custom_crypto && (enc_rsa || enc_aes256_rsa || enc_kw || enc_aes256_kw ||
                         enc_ec256 || enc_x25519 || enc_aes256_x25519) {
        panic!("custom-crypto encryption only supports enc-ec256-mbedtls, enc-aes256-ec256, and custom-enc-crypto");
    }

    if mbedtls_v4 && !sig_ecdsa_psa {
        panic!("mbedtls-v4 is only supported in combination with sig-ecdsa-psa");
    }

    if psa_crypto_api && !mbedtls_v4 {
        if sig_ecdsa || enc_ec256 || enc_x25519 ||
                enc_aes256_ec256 || sig_ecdsa_mbedtls || enc_aes256_x25519 ||
                enc_kw  || enc_aes256_kw {
            conf.file("csupport/psa_crypto_init_stub.c");
        } else {
            conf.conf.define("MCUBOOT_USE_PSA_CRYPTO", None);
            conf.file("../../ext/mbedtls-3.6.0/library/aes.c");
            conf.file("../../ext/mbedtls-3.6.0/library/aesni.c");
            conf.file("../../ext/mbedtls-3.6.0/library/aria.c");
            conf.file("../../ext/mbedtls-3.6.0/library/asn1write.c");
            conf.file("../../ext/mbedtls-3.6.0/library/base64.c");
            conf.file("../../ext/mbedtls-3.6.0/library/camellia.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ccm.c");
            conf.file("../../ext/mbedtls-3.6.0/library/chacha20.c");
            conf.file("../../ext/mbedtls-3.6.0/library/chachapoly.c");
            conf.file("../../ext/mbedtls-3.6.0/library/cipher.c");
            conf.file("../../ext/mbedtls-3.6.0/library/cipher_wrap.c");
            conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ctr_drbg.c");
            conf.file("../../ext/mbedtls-3.6.0/library/des.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ecdsa.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ecp.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ecp_curves.c");
            conf.file("../../ext/mbedtls-3.6.0/library/entropy.c");
            conf.file("../../ext/mbedtls-3.6.0/library/entropy_poll.c");
            conf.file("../../ext/mbedtls-3.6.0/library/gcm.c");
            conf.file("../../ext/mbedtls-3.6.0/library/md5.c");
            conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
            conf.file("../../ext/mbedtls-3.6.0/library/oid.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pem.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pk.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pkcs5.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pkcs12.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pkparse.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pk_wrap.c");
            conf.file("../../ext/mbedtls-3.6.0/library/pkwrite.c");
            conf.file("../../ext/mbedtls-3.6.0/library/poly1305.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_cipher.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_client.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_ecp.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_hash.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_mac.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_rsa.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_slot_management.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_crypto_storage.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_its_file.c");
            conf.file("../../ext/mbedtls-3.6.0/library/psa_util.c");
            conf.file("../../ext/mbedtls-3.6.0/library/ripemd160.c");
            conf.file("../../ext/mbedtls-3.6.0/library/rsa_alt_helpers.c");
            conf.file("../../ext/mbedtls-3.6.0/library/sha1.c");
            conf.file("../../ext/mbedtls-3.6.0/library/sha512.c");
            conf.file("../../ext/mbedtls-3.6.0/tests/src/random.c");
            conf.conf.include("../../ext/mbedtls-3.6.0/library");
        }

        conf.conf.include("../../ext/mbedtls-3.6.0/tests/include/");
        conf.file("../../ext/mbedtls-3.6.0/tests/src/fake_external_rng_for_test.c");
    }

    if sig_rsa || sig_rsa3072 {
        conf.conf.define("MCUBOOT_SIGN_RSA", None);
        // The Kconfig style defines must be added here as well because
        // they are used internally by "config-rsa.h"
        if sig_rsa {
            conf.conf.define("MCUBOOT_SIGN_RSA_LEN", "2048");
            conf.conf.define("CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN", "2048");
        } else {
            conf.conf.define("MCUBOOT_SIGN_RSA_LEN", "3072");
            conf.conf.define("CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN", "3072");
        }
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("csupport/keys.c");

        conf.file("../../ext/mbedtls-3.6.0/library/rsa.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/md.c");

    } else if sig_ecdsa {
        conf.conf.define("MCUBOOT_SIGN_EC256", None);
        conf.conf.define("MCUBOOT_USE_TINYCRYPT", None);

        if !enc_kw {
            conf.conf.include("../../ext/mbedtls-3.6.0/include");
        }
        conf.conf.include("../../ext/tinycrypt/lib/include");

        conf.file("csupport/keys.c");

        conf.file("../../ext/tinycrypt/lib/source/utils.c");
        conf.file("../../ext/tinycrypt/lib/source/sha256.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_dsa.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_platform_specific.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
    } else if sig_ecdsa_mbedtls {
        conf.conf.define("MCUBOOT_SIGN_EC256", None);
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("csupport/keys.c");

        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecdsa.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp_curves.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
    } else if sig_ecdsa_psa && mbedtls_v4 {
        add_mbedtls_v4_psa_ecdsa(&mut conf, sig_p384, enc_ec256 || enc_aes256_ec256);
    } else if sig_ecdsa_psa {
        conf.conf.include("../../ext/mbedtls-3.6.0/include");

        if sig_p384 {
            conf.conf.define("MCUBOOT_SIGN_EC384", None);
            conf.file("../../ext/mbedtls-3.6.0/library/sha512.c");
        } else {
            conf.conf.define("MCUBOOT_SIGN_EC256", None);
            conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        }

        conf.file("csupport/keys.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp_curves.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
    } else if sig_ed25519 {
        conf.conf.define("MCUBOOT_SIGN_ED25519", None);
        conf.conf.define("MCUBOOT_USE_TINYCRYPT", None);

        conf.conf.include("../../ext/tinycrypt/lib/include");
        conf.conf.include("../../ext/tinycrypt-sha512/lib/include");
        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/tinycrypt/lib/source/sha256.c");
        conf.file("../../ext/tinycrypt-sha512/lib/source/sha512.c");
        conf.file("../../ext/tinycrypt/lib/source/utils.c");
        conf.file("csupport/keys.c");
        conf.file("../../ext/fiat/src/curve25519.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
    } else if custom_crypto {
        // MCUBOOT_USE_CUSTOM_CRYPTO with mbedTLS stubs from csupport/custom_crypto/.
        // mcuboot_config.h includes mcuboot_custom_crypto.h when MCUBOOT_USE_CUSTOM_CRYPTO
        // is defined, ensuring types are available before bootutil/crypto headers.
        conf.conf.define("MCUBOOT_USE_CUSTOM_CRYPTO", None);
        conf.conf.define("MCUBOOT_SIGN_EC256", None);
        conf.conf.include("csupport/custom_crypto");
        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.conf.include("../../ext/mbedtls-3.6.0/library");
        conf.file("csupport/keys.c");
        conf.file("../../ext/mbedtls-3.6.0/library/aes.c");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecdsa.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp_curves.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecdh.c");
        conf.file("../../ext/mbedtls-3.6.0/library/md.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");

        // Encryption support: ECIES-P256 via sim_custom_encrypted.c which
        // provides all boot_enc_* symbols using the custom crypto stubs.
        // custom-enc-crypto is a convenience alias for this path.
        if enc_ec256_mbedtls || enc_aes256_ec256 || custom_enc_crypto {
            if enc_aes256_ec256 {
                conf.conf.define("MCUBOOT_AES_256", None);
            }
            conf.conf.define("MCUBOOT_ENCRYPT_EC256", None);
            conf.conf.define("MCUBOOT_ENC_IMAGES", None);
            conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);
            conf.conf.include("../../boot/bootutil/src");
            conf.file("csupport/custom_crypto/sim_custom_encrypted.c");
        }
    } else if !enc_ec256 && !enc_x25519 {
        // No signature type, only sha256 validation. The default
        // configuration file bundled with mbedTLS is sufficient.
        // When using ECIES-P256 rely on Tinycrypt.
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);
        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
    }

    if overwrite_only {
        conf.conf.define("MCUBOOT_OVERWRITE_ONLY", None);
    }

    if swap_offset {
        conf.conf.define("MCUBOOT_SWAP_USING_OFFSET", None);
    } else if swap_move {
        conf.conf.define("MCUBOOT_SWAP_USING_MOVE", None);
    } else if !overwrite_only && !direct_xip && !ram_load {
        conf.conf.define("CONFIG_BOOT_SWAP_USING_SCRATCH", None);
        conf.conf.define("MCUBOOT_SWAP_USING_SCRATCH", None);
    }

    if enc_rsa || enc_aes256_rsa {
        if enc_aes256_rsa {
                conf.conf.define("MCUBOOT_AES_256", None);
        }
        conf.conf.define("MCUBOOT_ENCRYPT_RSA", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("csupport/keys.c");

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.conf.include("../../ext/mbedtls-3.6.0/library");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");

        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/rsa.c");
        conf.file("../../ext/mbedtls-3.6.0/library/rsa_alt_helpers.c");
        conf.file("../../ext/mbedtls-3.6.0/library/md.c");
        conf.file("../../ext/mbedtls-3.6.0/library/aes.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
    }

    if enc_kw || enc_aes256_kw {
        if enc_aes256_kw {
            conf.conf.define("MCUBOOT_AES_256", None);
        }
        conf.conf.define("MCUBOOT_ENCRYPT_KW", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("csupport/keys.c");

        if sig_rsa || sig_rsa3072 {
            conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        }

        /* Simulator uses Mbed-TLS to wrap keys */
        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.conf.include("../../ext/mbedtls-3.6.0/library");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/cipher.c");
        conf.file("../../ext/mbedtls-3.6.0/library/cipher_wrap.c");
        conf.file("../../ext/mbedtls-3.6.0/library/aes.c");

        if sig_ecdsa {
            conf.conf.define("MCUBOOT_USE_TINYCRYPT", None);

            conf.conf.include("../../ext/tinycrypt/lib/include");

            conf.file("../../ext/tinycrypt/lib/source/utils.c");
            conf.file("../../ext/tinycrypt/lib/source/sha256.c");
            conf.file("../../ext/tinycrypt/lib/source/aes_encrypt.c");
            conf.file("../../ext/tinycrypt/lib/source/aes_decrypt.c");
            conf.file("../../ext/tinycrypt/lib/source/ctr_mode.c");
        }

        if sig_ed25519 {
            panic!("ed25519 does not support image encryption with KW yet");
        }
    }

    if (enc_ec256 || enc_aes256_ec256) && mbedtls_v4 {
        // Genuine PSA encryption. encrypted.c holds the high-level
        // boot_enc_* interface (unconditional); encrypted_psa.c
        // supplies the crypto primitives (boot_decrypt_key,
        // bootutil_aes_ctr_*) under MCUBOOT_USE_PSA_CRYPTO.
        // ECDH + HKDF + AES-CTR + HMAC all come from the
        // tfpsacrypto library built by add_mbedtls_v4_psa_ecdsa().
        // MCUBOOT_USE_PSA_CRYPTO is already defined there; we must
        // not define MCUBOOT_USE_TINYCRYPT — ecdsa.h errors on both
        // backends being set.
        //
        // enc_aes256_ec256 shares the ECIES-P256 machinery and
        // differs only in BOOT_ENC_KEY_SIZE (32 vs. 16 bytes), which
        // is gated by MCUBOOT_AES_256. No additional PSA_WANT_*
        // entries are needed — PSA_KEY_TYPE_AES covers all AES key
        // sizes.
        //
        // CONFIG_BOOT_ECDSA_PSA is a Zephyr Kconfig symbol that
        // encrypted.c uses to skip its duplicated legacy ASN.1 +
        // ECDH code (a block that would otherwise try to compile
        // against MBEDTLS_OID_* macros no longer public in 4.x).
        // Setting it here turns the file into the thin boot_enc_*
        // wrapper we want, delegating to encrypted_psa.c.
        if enc_aes256_ec256 {
            conf.conf.define("MCUBOOT_AES_256", None);
        }
        conf.conf.define("MCUBOOT_ENCRYPT_EC256", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);
        conf.conf.define("CONFIG_BOOT_ECDSA_PSA", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("../../boot/bootutil/src/encrypted_psa.c");
        // keys.c is already added by add_mbedtls_v4_psa_ecdsa().
    } else if enc_ec256 {
        conf.conf.define("MCUBOOT_ENCRYPT_EC256", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_USE_TINYCRYPT", None);
        conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("csupport/keys.c");

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.conf.include("../../ext/tinycrypt/lib/include");

        /* FIXME: fail with other signature schemes ? */

        conf.file("../../ext/tinycrypt/lib/source/utils.c");
        conf.file("../../ext/tinycrypt/lib/source/sha256.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_dsa.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_platform_specific.c");

        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");

        conf.file("../../ext/tinycrypt/lib/source/aes_encrypt.c");
        conf.file("../../ext/tinycrypt/lib/source/aes_decrypt.c");
        conf.file("../../ext/tinycrypt/lib/source/ctr_mode.c");
        conf.file("../../ext/tinycrypt/lib/source/hmac.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_dh.c");
    } else if (enc_ec256_mbedtls || enc_aes256_ec256) && !custom_crypto {
        if enc_aes256_ec256 {
            conf.conf.define("MCUBOOT_AES_256", None);
        }
        conf.conf.define("MCUBOOT_ENCRYPT_EC256", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);
        conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);

        conf.conf.include("../../ext/mbedtls-3.6.0/include");

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum.c");
        conf.file("../../ext/mbedtls-3.6.0/library/bignum_core.c");
        conf.file("../../ext/mbedtls-3.6.0/library/constant_time.c");
        conf.file("../../ext/mbedtls-3.6.0/library/nist_kw.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecdh.c");
        conf.file("../../ext/mbedtls-3.6.0/library/md.c");
        conf.file("../../ext/mbedtls-3.6.0/library/aes.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp.c");
        conf.file("../../ext/mbedtls-3.6.0/library/ecp_curves.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("csupport/keys.c");
    }

    if enc_x25519 {
        conf.conf.define("MCUBOOT_ENCRYPT_X25519", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_USE_TINYCRYPT", None);
        conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("csupport/keys.c");

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.conf.include("../../ext/tinycrypt/lib/include");
        conf.conf.include("../../ext/tinycrypt-sha512/lib/include");

        conf.file("../../ext/fiat/src/curve25519.c");

        conf.file("../../ext/tinycrypt/lib/source/utils.c");
        conf.file("../../ext/tinycrypt/lib/source/sha256.c");

        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");

        conf.file("../../ext/tinycrypt/lib/source/aes_encrypt.c");
        conf.file("../../ext/tinycrypt/lib/source/aes_decrypt.c");
        conf.file("../../ext/tinycrypt/lib/source/ctr_mode.c");
        conf.file("../../ext/tinycrypt/lib/source/hmac.c");
    }

    else if enc_aes256_x25519 {
        conf.conf.define("MCUBOOT_AES_256", None);
        conf.conf.define("MCUBOOT_ENCRYPT_X25519", None);
        conf.conf.define("MCUBOOT_ENC_IMAGES", None);
        conf.conf.define("MCUBOOT_USE_MBED_TLS", None);
        conf.conf.define("MCUBOOT_SWAP_SAVE_ENCTLV", None);

        conf.file("../../boot/bootutil/src/encrypted.c");
        conf.file("csupport/keys.c");

        conf.conf.include("../../ext/mbedtls-3.6.0/include");
        conf.file("../../ext/fiat/src/curve25519.c");
        conf.file("../../ext/mbedtls-3.6.0/library/asn1parse.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform.c");
        conf.file("../../ext/mbedtls-3.6.0/library/platform_util.c");
        conf.file("../../ext/mbedtls-3.6.0/library/aes.c");
        conf.file("../../ext/mbedtls-3.6.0/library/sha256.c");
        conf.file("../../ext/mbedtls-3.6.0/library/md.c");
        conf.file("../../ext/mbedtls-3.6.0/library/sha512.c");
    }

    if sig_rsa && enc_kw {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-rsa-kw.h>"));
    } else if sig_rsa || sig_rsa3072 || enc_rsa || enc_aes256_rsa {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-rsa.h>"));
    } else if (sig_ecdsa_mbedtls || enc_ec256_mbedtls || enc_aes256_ec256 || custom_crypto) && !mbedtls_v4 {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-ec.h>"));
    } else if (sig_ecdsa || enc_ec256) && !enc_kw && !mbedtls_v4 {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-asn1.h>"));
    } else if sig_ed25519 || enc_x25519 {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-asn1.h>"));
    } else if enc_kw || enc_aes256_kw {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-kw.h>"));
    } else if enc_aes256_x25519 {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-ed25519.h>"));
    } else if sig_ecdsa_psa && mbedtls_v4 {
        // 4.x uses TF_PSA_CRYPTO_CONFIG_FILE (set by
        // add_mbedtls_v4_psa_ecdsa()) and has no MBEDTLS_CONFIG_FILE.
    } else if sig_ecdsa_psa {
        conf.conf.define("MBEDTLS_CONFIG_FILE", Some("<config-ec-psa.h>"));
    }

    conf.file("../../boot/bootutil/src/image_validate.c");
    conf.file("../../boot/bootutil/src/bootutil_find_key.c");
    conf.file("../../boot/bootutil/src/bootutil_img_hash.c");
    conf.file("../../boot/bootutil/src/bootutil_img_security_cnt.c");
    if sig_rsa || sig_rsa3072 {
        conf.file("../../boot/bootutil/src/image_rsa.c");
    } else if sig_ecdsa || sig_ecdsa_mbedtls || sig_ecdsa_psa || custom_crypto {
        conf.file("../../boot/bootutil/src/image_ecdsa.c");
    } else if sig_ed25519 {
        conf.file("../../boot/bootutil/src/image_ed25519.c");
    }

    conf.file("../../boot/bootutil/src/loader.c");
    if ram_load {
        conf.file("../../boot/bootutil/src/ram_load.c");
    }
    conf.file("../../boot/bootutil/src/swap_misc.c");
    conf.file("../../boot/bootutil/src/swap_scratch.c");
    conf.file("../../boot/bootutil/src/swap_move.c");
    conf.file("../../boot/bootutil/src/swap_offset.c");
    conf.file("../../boot/bootutil/src/caps.c");
    conf.file("../../boot/bootutil/src/bootutil_misc.c");
    conf.file("../../boot/bootutil/src/bootutil_area.c");
    conf.file("../../boot/bootutil/src/bootutil_loader.c");
    conf.file("../../boot/bootutil/src/bootutil_public.c");
    conf.file("../../boot/bootutil/src/tlv.c");
    conf.file("../../boot/bootutil/src/fault_injection_hardening.c");
    conf.file("csupport/run.c");
    conf.conf.include("../../boot/bootutil/include");
    conf.conf.include("csupport");
    conf.conf.debug(true);
    conf.conf.flag("-Wall");
    conf.conf.flag("-Werror");

    // FIXME: travis-ci still uses gcc 4.8.4 which defaults to std=gnu90.
    // It has incomplete std=c11 and std=c99 support but std=c99 was checked
    // to build correctly so leaving it here to updated in the future...
    conf.conf.flag("-std=c99");

    conf.conf.compile("libbootutil.a");

    walk_dir("../../boot").unwrap();
    walk_dir("../../ext/tinycrypt/lib/source").unwrap();
    walk_dir("../../ext/mbedtls-asn1").unwrap();
    walk_dir("csupport").unwrap();
    walk_dir("../../ext/mbedtls-3.6.0/include").unwrap();
    walk_dir("../../ext/mbedtls-3.6.0/library").unwrap();
    if mbedtls_v4 {
        walk_dir("../../ext/mbedtls-4.1.0/include").unwrap();
        walk_dir("../../ext/mbedtls-4.1.0/tf-psa-crypto").unwrap();
    }
}

/// Configure the build to route sig-ecdsa-psa through Mbed TLS 4.1.0
/// (TF-PSA-Crypto 1.1.0) instead of the default 3.6.0.
///
/// The approach mirrors Zephyr's Mbed TLS 4.1 integration: drive the
/// upstream CMake build to produce `libtfpsacrypto.a`, then link our
/// simulator's libbootutil.a against it. Compared to hand-picking
/// sources, this lets upstream own the (still-evolving) 4.x file
/// layout, generator plumbing, and config-adjustment logic; our only
/// inputs are the config header and a handful of CMake `-D` knobs.
///
/// Requires `cmake` in PATH and `python3` with `jinja2` + `jsonschema`
/// (see `scripts/requirements.txt`). The 4.x CMake invokes its own
/// Python generators for `psa_crypto_driver_wrappers*` and
/// `tf_psa_crypto_config_check_*.h` when GEN_FILES is ON (the default
/// on non-Windows hosts).
fn add_mbedtls_v4_psa_ecdsa(conf: &mut CachedBuild, sig_p384: bool, enc_ec256: bool) {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")
        .expect("CARGO_MANIFEST_DIR not set"));
    let tf_src = manifest_dir
        .join("../../ext/mbedtls-4.1.0/tf-psa-crypto")
        .canonicalize()
        .expect("tf-psa-crypto source dir not found — did submodules initialize?");
    let config_file = manifest_dir
        .join("csupport/config-ec-psa-v4.h")
        .canonicalize()
        .expect("config-ec-psa-v4.h not found");

    // Build the static TF-PSA-Crypto library via upstream CMake.
    // - Only the `tfpsacrypto` target: skips programs, tests, shared lib.
    // - `TF_PSA_CRYPTO_CONFIG_FILE` points at our header; upstream compiles
    //   every object with `-DTF_PSA_CRYPTO_CONFIG_FILE=\"<abs path>\"`.
    // - `MCUBOOT_SIGN_EC256`/`EC384` is forwarded so the `#if defined`
    //   gates inside config-ec-psa-v4.h are evaluated the same way for
    //   the library build as for our boot-code build; otherwise the
    //   library would lack P-384 / SHA-384 support.
    // - `TF_PSA_CRYPTO_FATAL_WARNINGS=OFF` so upstream's -Wall/-Wextra
    //   don't fail our build on warning differences between toolchains.
    let mut cmake_conf = cmake::Config::new(&tf_src);
    cmake_conf
        .define("TF_PSA_CRYPTO_CONFIG_FILE", config_file.to_str().unwrap())
        .define("ENABLE_PROGRAMS", "OFF")
        .define("ENABLE_TESTING", "OFF")
        .define("USE_STATIC_TF_PSA_CRYPTO_LIBRARY", "ON")
        .define("USE_SHARED_TF_PSA_CRYPTO_LIBRARY", "OFF")
        .define("TF_PSA_CRYPTO_FATAL_WARNINGS", "OFF")
        .define("DISABLE_PACKAGE_CONFIG_AND_INSTALL", "ON")
        .build_target("tfpsacrypto");
    if sig_p384 {
        cmake_conf.cflag("-DMCUBOOT_SIGN_EC384");
    } else {
        cmake_conf.cflag("-DMCUBOOT_SIGN_EC256");
    }
    if enc_ec256 {
        cmake_conf.cflag("-DMCUBOOT_ENCRYPT_EC256");
    }
    let dst = cmake_conf.build();

    // `cmake` crate returns the install-prefix path even with build_target;
    // the static library is produced in the build tree under core/.
    let lib_dir = dst.join("build").join("core");
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static=tfpsacrypto");

    // Tell Cargo to rebuild if the config file or any .c/.h under the
    // source tree changes; walk_dir() below covers the source tree, and
    // this covers the config header explicitly.
    println!("cargo:rerun-if-changed={}", config_file.display());

    // Compile-time glue for the C code in libbootutil.a: define the
    // MCUboot PSA macros, point the public headers at our config, and
    // add include paths so `<psa/crypto.h>` and `<mbedtls/...>` resolve.
    conf.conf.define("MCUBOOT_USE_PSA_CRYPTO", None);
    conf.conf.define(
        "TF_PSA_CRYPTO_CONFIG_FILE",
        Some(format!("\"{}\"", config_file.display()).as_str()),
    );
    if sig_p384 {
        conf.conf.define("MCUBOOT_SIGN_EC384", None);
    } else {
        conf.conf.define("MCUBOOT_SIGN_EC256", None);
    }
    conf.conf.include("../../ext/mbedtls-4.1.0/include");
    conf.conf.include("../../ext/mbedtls-4.1.0/tf-psa-crypto/include");
    conf.conf.include("../../ext/mbedtls-4.1.0/tf-psa-crypto/drivers/builtin/include");

    // Public key data.
    conf.file("csupport/keys.c");

    // MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG (set in config-ec-psa-v4.h) makes
    // the library reference `mbedtls_psa_external_get_random`. Upstream
    // supplies this only under ENABLE_TESTING, and that shim drags in
    // the whole test-framework header tree. Our self-contained stub
    // uses libc rand() — sufficient for verification-only tests.
    conf.file("csupport/psa_rng_stub_v4.c");
}

// Output the names of all files within a directory so that Cargo knows when to rebuild.
fn walk_dir<P: AsRef<Path>>(path: P) -> io::Result<()> {
    for ent in fs::read_dir(path.as_ref())? {
        let ent = ent?;
        let p = ent.path();
        if p.is_dir() {
            walk_dir(p)?;
        } else {
            // Note that non-utf8 names will fail.
            let name = p.to_str().unwrap();
            if name.ends_with(".c") || name.ends_with(".h") {
                println!("cargo:rerun-if-changed={}", name);
            }
        }
    }

    Ok(())
}

/// Wrap the cc::Build type so that we can make sure that files are only added a single time.
/// Other methods can be passed through as needed.
struct CachedBuild {
    conf: cc::Build,
    seen: BTreeSet<PathBuf>,
}

impl CachedBuild {
    fn new() -> CachedBuild {
        CachedBuild {
            conf: cc::Build::new(),
            seen: BTreeSet::new(),
        }
    }

    /// Works like `file` in the Build, but doesn't add a file if the same path has already been
    /// given.
    fn file<P: AsRef<Path>>(&mut self, p: P) -> &mut CachedBuild {
        let p = p.as_ref();
        if !self.seen.contains(p) {
            self.conf.file(p);
            self.seen.insert(p.to_owned());
        }
        self
    }
}
