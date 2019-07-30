//! Core tests
//!
//! Run the existing testsuite as a Rust unit test.

use bootsim::{ImagesBuilder, testlog};

macro_rules! sim_test {
    ($name:ident, $maker:ident($($margs:expr),*), $test:ident($($targs:expr),*)) => {
        #[test]
        fn $name() {
            testlog::setup();

            ImagesBuilder::each_device(|r| {
                let image = r.$maker($($margs),*);
                assert!(!image.$test($($targs),*));
            });
        }
    };
}

sim_test!(bad_secondary_slot, make_bad_secondary_slot_image(), run_signfail_upgrade());
sim_test!(norevert_newimage, make_no_upgrade_image(), run_norevert_newimage());
sim_test!(basic_revert, make_image(true), run_basic_revert());
sim_test!(revert_with_fails, make_image(false), run_revert_with_fails());
sim_test!(perm_with_fails, make_image(true), run_perm_with_fails());
sim_test!(perm_with_random_fails, make_image(true), run_perm_with_random_fails(5));
sim_test!(norevert, make_image(true), run_norevert());
sim_test!(status_write_fails_complete, make_image(true), run_with_status_fails_complete());
sim_test!(status_write_fails_with_reset, make_image(true), run_with_status_fails_with_reset());
