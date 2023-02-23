// Copyright (c) 2017-2021 Linaro LTD
// Copyright (c) 2019 JUUL Labs
// Copyright (c) 2019-2023 Arm Limited
//
// SPDX-License-Identifier: Apache-2.0

// Query the bootloader's capabilities.

#[repr(u32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[allow(unused)]
pub enum Caps {
    RSA2048              = (1 << 0),
    EcdsaP256            = (1 << 1),
    SwapUsingScratch     = (1 << 2),
    OverwriteUpgrade     = (1 << 3),
    EncRsa               = (1 << 4),
    EncKw                = (1 << 5),
    ValidatePrimarySlot  = (1 << 6),
    RSA3072              = (1 << 7),
    Ed25519              = (1 << 8),
    EncEc256             = (1 << 9),
    SwapUsingMove        = (1 << 10),
    DowngradePrevention  = (1 << 11),
    EncX25519            = (1 << 12),
    Bootstrap            = (1 << 13),
    Aes256               = (1 << 14),
    RamLoad              = (1 << 15),
    DirectXip            = (1 << 16),
}

impl Caps {
    pub fn present(self) -> bool {
        let caps = unsafe { bootutil_get_caps() };
        (caps as u32) & (self as u32) != 0
    }

    /// Does this build have ECDSA of some type enabled for signatures.
    pub fn has_ecdsa() -> bool {
        Caps::EcdsaP256.present()
    }

    /// Query for the number of images that have been configured into this
    /// MCUboot build.
    pub fn get_num_images() -> usize {
        (unsafe { bootutil_get_num_images() }) as usize
    }

    /// Query if this configuration performs some kind of upgrade by writing to flash.
    pub fn modifies_flash() -> bool {
        // All other configurations perform upgrades by writing to flash.
        !(Self::RamLoad.present() || Self::DirectXip.present())
    }
}

extern "C" {
    fn bootutil_get_caps() -> Caps;
    fn bootutil_get_num_images() -> u32;
}
