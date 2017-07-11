// Build the library.

extern crate gcc;

use std::fs;
use std::io;
use std::path::Path;

fn main() {
    let mut conf = gcc::Config::new();

    conf.file("../boot/bootutil/src/loader.c");
    conf.file("../boot/bootutil/src/caps.c");
    conf.file("../boot/bootutil/src/bootutil_misc.c");
    conf.file("../boot/bootutil/src/image_validate.c");
    conf.file("csupport/run.c");
    conf.include("../boot/bootutil/include");
    conf.include("../boot/zephyr/include");
    conf.debug(true);
    conf.flag("-Wall");
    conf.define("__BOOTSIM__", None);
    // conf.define("MCUBOOT_OVERWRITE_ONLY", None);
    conf.define("MCUBOOT_USE_FLASH_AREA_GET_SECTORS", None);

    // Bring in some pieces of mbedtls we need.
    conf.define("MCUBOOT_USE_MBED_TLS", None);
    conf.define("MBEDTLS_CONFIG_FILE", Some("<config-boot.h>"));
    conf.include("mbedtls/include");
    conf.file("mbedtls/library/sha256.c");

    // Test that signature/hashes are present.
    conf.define("MCUBOOT_VALIDATE_SLOT0", None);

    conf.compile("libbootutil.a");
    walk_dir("../boot").unwrap();
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
