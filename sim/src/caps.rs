// Copyright (c) 2017-2019 Linaro LTD
// Copyright (c) 2019 JUUL Labs
// Copyright (c) 2019-2021 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

// Query the bootloader's capabilities.

#[repr(u32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[allow(unused)]
pub enum Caps {
    RSA2048              = (1 << 0),
    EcdsaP224            = (1 << 1),
    EcdsaP256            = (1 << 2),
    SwapUsingScratch     = (1 << 3),
    OverwriteUpgrade     = (1 << 4),
    EncRsa               = (1 << 5),
    EncKw                = (1 << 6),
    ValidatePrimarySlot  = (1 << 7),
    RSA3072              = (1 << 8),
    Ed25519              = (1 << 9),
    EncEc256             = (1 << 10),
    SwapUsingMove        = (1 << 11),
    DowngradePrevention  = (1 << 12),
    EncX25519            = (1 << 13),
    Bootstrap            = (1 << 14),
    Aes256               = (1 << 15),
}

impl Caps {
    pub fn present(self) -> bool {
        let caps = unsafe { bootutil_get_caps() };
        (caps as u32) & (self as u32) != 0
    }

    /// Query for the number of images that have been configured into this
    /// MCUboot build.
    pub fn get_num_images() -> usize {
        (unsafe { bootutil_get_num_images() }) as usize
    }
}

extern "C" {
    fn bootutil_get_caps() -> Caps;
    fn bootutil_get_num_images() -> u32;
}
