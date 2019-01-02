//! Core tests
//!
//! Run the existing testsuite as a Rust unit test.

use bootsim::{Run, testlog};

macro_rules! sim_test {
    ($name:ident, $maker:ident, $test:ident) => {
        #[test]
        fn $name() {
            testlog::setup();

            Run::each_device(|r| {
                let image = r.$maker();
                assert!(!image.$test());
            });
        }
    };
}

sim_test!(bad_slot1, make_bad_slot1_image, run_signfail_upgrade);
sim_test!(norevert_newimage, make_no_upgrade_image, run_norevert_newimage);
sim_test!(basic_revert, make_image, run_basic_revert);
sim_test!(revert_with_fails, make_image, run_revert_with_fails);
sim_test!(perm_with_fails, make_image, run_perm_with_fails);
sim_test!(perm_with_random_fails, make_image, run_perm_with_random_fails_5);
sim_test!(norevert, make_image, run_norevert);
sim_test!(status_write_fails_complete, make_image, run_with_status_fails_complete);
sim_test!(status_write_fails_with_reset, make_image, run_with_status_fails_with_reset);
