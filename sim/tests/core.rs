// Copyright (c) 2017-2021 Linaro LTD
// Copyright (c) 2017-2019 JUUL Labs
// Copyright (c) 2021-2023 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

//! Core tests
//!
//! Run the existing testsuite as a Rust unit test.

use bootsim::{
    DepTest, DepType, UpgradeInfo,
    ImagesBuilder,
    Images,
    NO_DEPS,
    REV_DEPS,
    testlog,
    ImageManipulation
};
use std::{
    env,
    sync::atomic::{AtomicUsize, Ordering},
};
use mcuboot_sys::c;

/// A single test, after setting up logging and such.  Within the $body,
/// $arg will be bound to each device.
macro_rules! test_shell {
    ($name:ident, $arg: ident, $body:expr) => {
        #[test]
        fn $name() {
            testlog::setup();
            ImagesBuilder::each_device(|$arg| {
                $body;
            });
        }
    }
}

/// A typical test calls a particular constructor, and runs a given test on
/// that constructor.
macro_rules! sim_test {
    ($name:ident, $maker:ident($($margs:expr),*), $test:ident($($targs:expr),*)) => {
        test_shell!($name, r, {
            let image = r.$maker($($margs),*);
            dump_image(&image, stringify!($name));
            assert!(!image.$test($($targs),*));
        });
    };
}

sim_test!(bad_secondary_slot, make_bad_secondary_slot_image(), run_signfail_upgrade());
sim_test!(secondary_trailer_leftover, make_erased_secondary_image(), run_secondary_leftover_trailer());
sim_test!(bootstrap, make_bootstrap_image(), run_bootstrap());
sim_test!(oversized_bootstrap, make_oversized_bootstrap_image(), run_oversized_bootstrap());
sim_test!(norevert_newimage, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_norevert_newimage());
sim_test!(basic_revert, make_image(&NO_DEPS, true), run_basic_revert());
sim_test!(revert_with_fails, make_image(&NO_DEPS, false), run_revert_with_fails());
sim_test!(perm_with_fails, make_image(&NO_DEPS, true), run_perm_with_fails());
sim_test!(perm_with_random_fails, make_image(&NO_DEPS, true), run_perm_with_random_fails(5));
sim_test!(norevert, make_image(&NO_DEPS, true), run_norevert());
sim_test!(oversized_secondary_slot, make_oversized_secondary_slot_image(), run_oversizefail_upgrade());

sim_test!(status_write_fails_complete, make_image(&NO_DEPS, true), run_with_status_fails_complete());
sim_test!(status_write_fails_with_reset, make_image(&NO_DEPS, true), run_with_status_fails_with_reset());
sim_test!(downgrade_prevention, make_image(&REV_DEPS, true), run_nodowngrade());

sim_test!(direct_xip_first, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_direct_xip());
sim_test!(ram_load_first, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_ram_load());
sim_test!(ram_load_split, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_split_ram_load());
sim_test!(hw_prot_failed_security_cnt_check, make_image_with_security_counter(Some(0)), run_hw_rollback_prot());
sim_test!(hw_prot_missing_security_cnt, make_image_with_security_counter(None), run_hw_rollback_prot());
sim_test!(ram_load_out_of_bounds, make_no_upgrade_image(&NO_DEPS, ImageManipulation::WrongOffset), run_ram_load_boot_with_result(false));
sim_test!(ram_load_missing_header_flag, make_no_upgrade_image(&NO_DEPS, ImageManipulation::IgnoreRamLoadFlag), run_ram_load_boot_with_result(false));
sim_test!(ram_load_failed_validation, make_no_upgrade_image(&NO_DEPS, ImageManipulation::BadSignature), run_ram_load_boot_with_result(false));
sim_test!(ram_load_corrupt_higher_version_image, make_no_upgrade_image(&NO_DEPS, ImageManipulation::CorruptHigherVersionImage), run_ram_load_boot_with_result(true));

#[cfg(feature = "multiimage")]
sim_test!(ram_load_overlapping_images_same_base, make_no_upgrade_image(&NO_DEPS, ImageManipulation::OverlapImages(true)), run_ram_load_boot_with_result(false));
#[cfg(feature = "multiimage")]
sim_test!(ram_load_overlapping_images_offset, make_no_upgrade_image(&NO_DEPS, ImageManipulation::OverlapImages(false)), run_ram_load_boot_with_result(false));

// Test various combinations of incorrect dependencies.
test_shell!(dependency_combos, r, {
    // Only test setups with two images.
    if r.num_images() != 2 {
        return;
    }

    for dep in TEST_DEPS {
        let image = r.clone().make_image(&dep, true);
        dump_image(&image, "dependency_combos");
        assert!(!image.run_check_deps(&dep));
        c::reset_security_counters();
    }
});

/// These are the variants of dependencies we will test.
pub static TEST_DEPS: &[DepTest] = &[
    // A sanity test, no dependencies should upgrade.
    DepTest {
        depends: [DepType::Nothing, DepType::Nothing],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Upgraded],
        downgrade: false,
    },

    // If all of the dependencies are met, we should also upgrade.
    DepTest {
        depends: [DepType::Correct, DepType::Correct],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Upgraded],
        downgrade: false,
    },

    // If none of the dependencies are met, the images should be held.
    DepTest {
        depends: [DepType::Newer, DepType::Newer],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
        downgrade: false,
    },

    // If the first image is not met, we should hold back on the
    // dependencies (it is not well defined what the correct behavior is
    // here, it could also be correct to upgrade only the second image).
    DepTest {
        depends: [DepType::Newer, DepType::Correct],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
        downgrade: false,
    },

    // Test the variant in the other direction.
    DepTest {
        depends: [DepType::Correct, DepType::Newer],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
        downgrade: false,
    },

    // Test where only the first image is upgraded, and there are no
    // dependencies.
    DepTest {
        depends: [DepType::Nothing, DepType::NoUpgrade],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Held],
        downgrade: false,
    },

    // Test one image with a valid dependency on the first image.
    DepTest {
        depends: [DepType::OldCorrect, DepType::NoUpgrade],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Held],
        downgrade: false,
    },

    // Test one image with an invalid dependency on the first image.
    DepTest {
        depends: [DepType::Newer, DepType::NoUpgrade],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
        downgrade: false,
    },

    // Test where only the second image is upgraded, and there are no
    // dependencies.
    DepTest {
        depends: [DepType::NoUpgrade, DepType::Nothing],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Upgraded],
        downgrade: false,
    },

    // Test one image with a valid dependency on the second image.
    DepTest {
        depends: [DepType::NoUpgrade, DepType::OldCorrect],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Upgraded],
        downgrade: false,
    },

    // Test one image with an invalid dependency on the second image.
    DepTest {
        depends: [DepType::NoUpgrade, DepType::Newer],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
        downgrade: false,
    },
];

/// Counter for the image number.
static IMAGE_NUMBER: AtomicUsize = AtomicUsize::new(0);

/// Dump an image if that makes sense.  The name is the name of the test
/// being run.  If the MCUBOT_DEBUG_DUMP environment variable contains, in
/// one of its comma separate strings a substring of this name, then this
/// image will be dumped.  As a special case, we will dump everything if
/// this environment variable is set to all.
fn dump_image(image: &Images, name: &str) {
    if let Ok(request) = env::var("MCUBOOT_DEBUG_DUMP") {
        if request.split(',').any(|req| {
            req == "all" || name.contains(req)
        }) {
            let count = IMAGE_NUMBER.fetch_add(1, Ordering::SeqCst);
            let full_name = format!("{}-{:04}", name, count);
            log::info!("Dump {:?}", full_name);
            image.debug_dump(&full_name);
        }
    }
}
