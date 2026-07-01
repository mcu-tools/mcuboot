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

sim_test!(bad_secondary_slot, make_bad_secondary_slot_image(ImageManipulation::BadSignature), run_signfail_upgrade());
sim_test!(secondary_trailer_leftover, make_erased_secondary_image(), run_secondary_leftover_trailer());
sim_test!(bootstrap, make_bootstrap_image(), run_bootstrap());
sim_test!(oversized_bootstrap, make_oversized_bootstrap_image(), run_oversized_bootstrap());
sim_test!(norevert_newimage, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_norevert_newimage());
sim_test!(basic_revert, make_image(&NO_DEPS, true), run_basic_revert());
sim_test!(revert_with_fails, make_image(&NO_DEPS, false), run_revert_with_fails());
sim_test!(perm_with_fails, make_image(&NO_DEPS, true), run_perm_with_fails());
sim_test!(perm_with_random_fails, make_image(&NO_DEPS, true), run_perm_with_random_fails(5));
sim_test!(norevert, make_image(&NO_DEPS, true), run_norevert());
sim_test!(oversized_secondary_slot, make_oversized_secondary_slot_image(), run_fail_upgrade_primary_intact());
#[cfg(feature = "check-load-addr")]
sim_test!(wrong_load_addr, make_bad_secondary_slot_image(ImageManipulation::WrongOffset), run_fail_upgrade_primary_intact());

sim_test!(status_write_fails_complete, make_image(&NO_DEPS, true), run_with_status_fails_complete());
sim_test!(status_write_fails_with_reset, make_image(&NO_DEPS, true), run_with_status_fails_with_reset());
sim_test!(downgrade_prevention, make_image(&REV_DEPS, true), run_nodowngrade());

sim_test!(direct_xip_first, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_direct_xip());
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_first, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_ram_load());
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_split, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_split_ram_load());
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_from_flash, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_ram_load_from_flash());
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_out_of_bounds, make_no_upgrade_image(&NO_DEPS, ImageManipulation::WrongOffset), run_ram_load_boot_with_result(false));
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_missing_header_flag, make_no_upgrade_image(&NO_DEPS, ImageManipulation::IgnoreRamLoadFlag), run_ram_load_boot_with_result(false));
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_failed_validation, make_no_upgrade_image(&NO_DEPS, ImageManipulation::BadSignature), run_ram_load_boot_with_result(false));
#[cfg(not(feature = "check-load-addr"))]
sim_test!(ram_load_corrupt_higher_version_image, make_no_upgrade_image(&NO_DEPS, ImageManipulation::CorruptHigherVersionImage), run_ram_load_boot_with_result(true));

sim_test!(hw_prot_missing_security_cnt, make_image_with_security_counter(None), run_hw_rollback_prot());
sim_test!(hw_prot_failed_security_cnt_check, make_image_with_security_counter(Some(0)), run_hw_rollback_prot());

#[cfg(feature = "sig-ed25519")]
mod multi_key {
    //! Multi-signing-key matrix.
    //!
    //! These tests only run when the simulator was built with `sig-ed25519` —
    //! `sig-second-key` alone is meaningless without a supported signature
    //! type, and build.rs panics if the pairing is missing. The whole module
    //! is dropped on incompatible feature combinations rather than silently
    //! turning into a no-op.

    use super::*;
    use bootsim::tlv::SigningKey;

    // With a single-key build, the image signed with the primary key must
    // boot and upgrade normally. With a two-key build (sig-second-key on)
    // this exercises the iteration-finds-first-match path in
    // bootutil_find_key().
    test_shell!(signed_primary_key_boots, r, {
        let image = r.make_no_upgrade_image_with_key(
            &NO_DEPS,
            ImageManipulation::None,
            SigningKey::Primary,
        );
        dump_image(&image, "signed_primary_key_boots");
        assert!(!image.run_norevert_newimage());
    });

    // With `sig-second-key`, the bootloader embeds a second verification
    // key; an image signed with that key must boot. Without the feature,
    // this test is dropped (see cfg-gated module).
    #[cfg(feature = "sig-second-key")]
    test_shell!(signed_secondary_key_boots, r, {
        let image = r.make_no_upgrade_image_with_key(
            &NO_DEPS,
            ImageManipulation::None,
            SigningKey::Secondary,
        );
        dump_image(&image, "signed_secondary_key_boots");
        assert!(!image.run_norevert_newimage());
    });

    // A third ("unknown") key must always be rejected, regardless of
    // whether the bootloader was built single- or multi-key.
    test_shell!(signed_unknown_key_rejected, r, {
        let image = r.make_secondary_slot_image_with_key(SigningKey::Unknown);
        dump_image(&image, "signed_unknown_key_rejected");
        assert!(!image.run_signfail_upgrade());
    });

    // Regression guard: a single-key build must reject images signed with
    // the secondary key (since it doesn't embed it). Only valid when
    // sig-second-key is OFF.
    #[cfg(not(feature = "sig-second-key"))]
    test_shell!(single_key_build_rejects_secondary_key_image, r, {
        let image = r.make_secondary_slot_image_with_key(SigningKey::Secondary);
        dump_image(&image, "single_key_build_rejects_secondary_key_image");
        assert!(!image.run_signfail_upgrade());
    });
}

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

// XIP encryption tests (ECIES-P256).
// When enc-xip-ec256 is enabled, make_tlv() selects TlvGen::new_xip_ecies_p256()
// and ImageData::find() returns encrypted content for both slots. The existing
// run_* helpers work unchanged because swap is a raw copy with XIP encryption.

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_basic_boot, make_no_upgrade_image(&NO_DEPS, ImageManipulation::None), run_norevert_newimage());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_upgrade_revert, make_image(&NO_DEPS, true), run_basic_revert());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_permanent_upgrade, make_image(&NO_DEPS, true), run_norevert());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_swap_not_decrypted, make_image(&NO_DEPS, true), run_xip_swap_not_decrypted());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_overwrite_not_decrypted, make_image(&NO_DEPS, true), run_xip_overwrite_not_decrypted());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_revert_with_fails, make_image(&NO_DEPS, false), run_revert_with_fails());

#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_perm_with_fails, make_image(&NO_DEPS, true), run_perm_with_fails());

/// Validate that an imgtool-generated XIP-encrypted image is accepted by the
/// C-side boot_image_check_hook (ECIES unwrap + SHA-256 hash verification).
///
/// Uses the existing sim_test infrastructure to get a valid flash/areadesc,
/// then overwrites the primary slot with an imgtool-generated image.
#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_imgtool_validation,
          make_no_upgrade_image(&NO_DEPS, ImageManipulation::None),
          run_imgtool_xip_validation());

/// Verify that Rust-side AES-CTR encryption (edgeprotecttools format:
/// counter_LE32 || nonce[0:12]) round-trips correctly through C-side
/// boot_decrypt_xip with a non-zero slot base address.
#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_ctr_format_matches_edgeprotecttools,
          make_no_upgrade_image(&NO_DEPS, ImageManipulation::None),
          run_xip_ctr_format_test());

/// Verify that the SHA-256 hash stored in the image TLV is computed over
/// ciphertext (header + encrypted payload), NOT over plaintext. This
/// explicitly confirms the encrypt-then-sign model.
#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_hash_covers_ciphertext,
          make_no_upgrade_image(&NO_DEPS, ImageManipulation::None),
          run_xip_ciphertext_hash_test());

// --- Negative tests: a corrupted XIP image must fail validation ---------
//
// Each builds a valid encrypted image in the primary slot and a *tampered*
// encrypted image in the secondary slot, marks the upgrade pending, and runs
// the bootloader. boot_image_check_hook must reject the secondary image, so
// no swap occurs and the primary slot is left intact and bootable.

// Corrupt one ciphertext byte of the upgrade image. The SHA-256 stored in the
// TLV (computed over the pristine ciphertext) no longer matches flash, so
// Step 2 (hash verification) of boot_image_check_hook rejects the upgrade.
#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_corrupt_payload_rejected,
          make_bad_secondary_slot_image(ImageManipulation::CorruptXipPayload),
          run_fail_upgrade_primary_intact());

// Tamper the ECIES key-wrap envelope (HMAC tag) of the upgrade image. The
// hash and signature still validate, so the image is rejected specifically at
// Step 4 (xip_enc_ecies_unwrap), exercising the key-unwrap failure path.
#[cfg(feature = "enc-xip-ec256")]
sim_test!(xip_enc_corrupt_ecies_rejected,
          make_bad_secondary_slot_image(ImageManipulation::CorruptXipEcies),
          run_fail_upgrade_primary_intact());

// Directly exercise the per-image IV uniqueness guard in xip_enc_ecies_unwrap.
// A 113-byte standard ECIES envelope carries no per-image IV, so the unwrap
// derives an all-zero nonce that the guard must reject. A 177-byte extended
// envelope (HKDF-derived non-zero nonce) is the positive control -- it also
// proves the test's envelope construction matches the C key derivation, so the
// standard-envelope rejection can only be the IV guard, not a tag mismatch.
#[cfg(feature = "enc-xip-ec256")]
#[test]
fn xip_enc_zero_iv_unwrap_rejected() {
    testlog::setup();
    let enc_key = [0x11u8; 16];

    let ext = bootsim::build_test_ecies_envelope(&enc_key, true);
    let (rc_ext, _key_ext, iv_ext) = c::xip_ecies_unwrap(&ext);
    assert_eq!(rc_ext, 0, "valid extended ECIES envelope must unwrap");
    assert!(iv_ext[..12].iter().any(|&b| b != 0),
            "extended envelope must yield a non-zero nonce");

    let std = bootsim::build_test_ecies_envelope(&enc_key, false);
    let (rc_std, _key_std, _iv_std) = c::xip_ecies_unwrap(&std);
    assert_ne!(rc_std, 0,
               "standard ECIES envelope (zero nonce) must be rejected by the zero-IV guard");
}

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
