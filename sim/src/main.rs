extern crate env_logger;

extern crate bootsim;

fn main() {
    env_logger::init().unwrap();

    bootsim::main();
}
