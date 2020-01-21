// Copyright (c) 2017-2019 Linaro LTD
// Copyright (c) 2017-2019 JUUL Labs
//
// SPDX-License-Identifier: Apache-2.0

use env_logger;

use bootsim;

fn main() {
    env_logger::init();

    bootsim::main();
}
