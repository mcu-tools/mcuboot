use env_logger;

use bootsim;

fn main() {
    env_logger::init().unwrap();

    bootsim::main();
}
