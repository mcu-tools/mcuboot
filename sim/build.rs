// Build the library.

extern crate gcc;

fn main() {
    let mut conf = gcc::Config::new();

    conf.file("../boot/bootutil/src/loader.c");
    conf.file("../boot/bootutil/src/bootutil_misc.c");
    conf.file("csupport/run.c");
    conf.include("../boot/bootutil/include");
    conf.include("../zephyr/include");
    conf.debug(true);
    conf.compile("libbootutil.a");
}
