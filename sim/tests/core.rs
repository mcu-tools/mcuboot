//! Core tests
//!
//! Run the existing testsuite as a Rust unit test.

use bootsim::{
    DepTest, DepType, UpgradeInfo,
    ImagesBuilder,
    Images,
    NO_DEPS,
    testlog,
};
use std::{
    env,
    sync::atomic::{AtomicUsize, Ordering},
};

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
sim_test!(norevert_newimage, make_no_upgrade_image(&NO_DEPS), run_norevert_newimage());
sim_test!(basic_revert, make_image(&NO_DEPS, true), run_basic_revert());
sim_test!(revert_with_fails, make_image(&NO_DEPS, false), run_revert_with_fails());
sim_test!(perm_with_fails, make_image(&NO_DEPS, true), run_perm_with_fails());
sim_test!(perm_with_random_fails, make_image(&NO_DEPS, true), run_perm_with_random_fails(5));
sim_test!(norevert, make_image(&NO_DEPS, true), run_norevert());
sim_test!(status_write_fails_complete, make_image(&NO_DEPS, true), run_with_status_fails_complete());
sim_test!(status_write_fails_with_reset, make_image(&NO_DEPS, true), run_with_status_fails_with_reset());

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
    }
});

/// These are the variants of dependencies we will test.
pub static TEST_DEPS: &[DepTest] = &[
    // First is a sanity test, no dependencies should upgrade.
    DepTest {
        depends: [DepType::Nothing, DepType::Nothing],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Upgraded],
    },

    DepTest {
        depends: [DepType::Correct, DepType::Correct],
        upgrades: [UpgradeInfo::Upgraded, UpgradeInfo::Upgraded],
    },

    DepTest {
        depends: [DepType::Newer, DepType::Newer],
        upgrades: [UpgradeInfo::Held, UpgradeInfo::Held],
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
