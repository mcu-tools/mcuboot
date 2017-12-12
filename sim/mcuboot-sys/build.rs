// Build mcuboot as a library, based on the requested features.

extern crate gcc;

use std::env;
use std::fs;
use std::io;
use std::path::Path;

fn main() {
    // Feature flags.
    let sig_rsa = env::var("CARGO_FEATURE_SIG_RSA").is_ok();
    let sig_ecdsa = env::var("CARGO_FEATURE_SIG_ECDSA").is_ok();
    let overwrite_only = env::var("CARGO_FEATURE_OVERWRITE_ONLY").is_ok();
    let validate_slot0 = env::var("CARGO_FEATURE_VALIDATE_SLOT0").is_ok();

    let mut conf = gcc::Build::new();
    conf.define("__BOOTSIM__", None);
    conf.define("MCUBOOT_USE_FLASH_AREA_GET_SECTORS", None);

    if validate_slot0 {
        conf.define("MCUBOOT_VALIDATE_SLOT0", None);
    }

    // Currently, mbed TLS cannot build with both RSA and ECDSA.
    if sig_rsa && sig_ecdsa {
        panic!("mcuboot does not support RSA and ECDSA at the same time");
    }

    if sig_rsa {
        conf.define("MCUBOOT_SIGN_RSA", None);
        conf.define("MCUBOOT_USE_MBED_TLS", None);

        conf.define("MCUBOOT_USE_MBED_TLS", None);
        conf.define("MBEDTLS_CONFIG_FILE", Some("<config-boot.h>"));
        conf.include("mbedtls/include");
        conf.file("mbedtls/library/sha256.c");
        conf.file("../../boot/zephyr/keys.c");

        conf.file("mbedtls/library/rsa.c");
        conf.file("mbedtls/library/bignum.c");
        conf.file("mbedtls/library/asn1parse.c");
    } else if sig_ecdsa {
        conf.define("MCUBOOT_SIGN_EC256", None);
        conf.define("MCUBOOT_USE_TINYCRYPT", None);

        conf.define("MBEDTLS_CONFIG_FILE", Some("<config-boot.h>"));
        conf.include("mbedtls/include");
        conf.include("../../ext/tinycrypt/lib/include");

        conf.file("../../boot/zephyr/keys.c");

        conf.file("../../ext/tinycrypt/lib/source/utils.c");
        conf.file("../../ext/tinycrypt/lib/source/sha256.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_dsa.c");
        conf.file("../../ext/tinycrypt/lib/source/ecc_platform_specific.c");

        conf.file("mbedtls/library/bignum.c");
        conf.file("mbedtls/library/asn1parse.c");
    } else {
        // Neither signature type, only verify sha256.
        conf.define("MCUBOOT_USE_MBED_TLS", None);
        conf.define("MBEDTLS_CONFIG_FILE", Some("<config-boot.h>"));
        conf.include("mbedtls/include");
        conf.file("mbedtls/library/sha256.c");
    }

    if overwrite_only {
        conf.define("MCUBOOT_OVERWRITE_ONLY", None);
        conf.define("MCUBOOT_OVERWRITE_ONLY_FAST", None);
    }

    conf.file("../../boot/bootutil/src/image_validate.c");
    if sig_rsa {
        conf.file("../../boot/bootutil/src/image_rsa.c");
    } else if sig_ecdsa {
        conf.file("../../boot/bootutil/src/image_ec256.c");
    }
    conf.file("../../boot/bootutil/src/loader.c");
    conf.file("../../boot/bootutil/src/caps.c");
    conf.file("../../boot/bootutil/src/bootutil_misc.c");
    conf.file("csupport/run.c");
    conf.include("../../boot/bootutil/include");
    conf.include("../../boot/zephyr/include");
    conf.include("csupport");
    conf.debug(true);
    conf.flag("-Wall");
    conf.flag("-Werror");

    // FIXME: travis-ci still uses gcc 4.8.4 which defaults to std=gnu90.
    // It has incomplete std=c11 and std=c99 support but std=c99 was checked
    // to build correctly so leaving it here to updated in the future...
    conf.flag("-std=c99");

    conf.compile("libbootutil.a");

    walk_dir("../../boot").unwrap();
    walk_dir("../../ext/tinycrypt/lib/source").unwrap();
    walk_dir("csupport").unwrap();
    walk_dir("mbedtls/include").unwrap();
    walk_dir("mbedtls/library").unwrap();
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
