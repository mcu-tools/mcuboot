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
               /* reserved (1 << 1) */
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
    RamLoad              = (1 << 16),
    DirectXip            = (1 << 17),
    HwRollbackProtection = (1 << 18),
    EcdsaP384            = (1 << 19),
    SwapUsingOffset      = (1 << 20),
    EncXipEc256          = (1 << 21),
}

impl Caps {
    pub fn present(self) -> bool {
        let caps = unsafe { bootutil_get_caps() };
        (caps as u32) & (self as u32) != 0
    }

    pub fn has(self) -> bool {
        match self {
            Caps::RSA2048              => cfg!(feature = "rsa2048"),
            Caps::EcdsaP256            => cfg!(feature = "ecdsa-p256"),
            Caps::SwapUsingScratch     => cfg!(feature = "swap-using-scratch"),
            Caps::OverwriteUpgrade     => cfg!(feature = "overwrite-upgrade"),
            Caps::EncRsa               => cfg!(feature = "enc-rsa"),
            Caps::EncKw                => cfg!(feature = "enc-kw"),
            Caps::ValidatePrimarySlot  => cfg!(feature = "validate-primary-slot"),
            Caps::RSA3072              => cfg!(feature = "rsa3072"),
            Caps::Ed25519              => cfg!(feature = "ed25519"),
            Caps::EncEc256             => cfg!(feature = "enc-ec256"),
            Caps::SwapUsingMove        => cfg!(feature = "swap-using-move"),
            Caps::DowngradePrevention  => cfg!(feature = "downgrade-prevention"),
            Caps::EncX25519            => cfg!(feature = "enc-x25519"),
            Caps::Bootstrap            => cfg!(feature = "bootstrap"),
            Caps::Aes256               => cfg!(feature = "aes256"),
            Caps::RamLoad              => cfg!(feature = "ram-load"),
            Caps::DirectXip            => cfg!(feature = "direct-xip"),
            Caps::HwRollbackProtection => cfg!(feature = "hw-rollback-protection"),
            Caps::EcdsaP384            => cfg!(feature = "ecdsa-p384"),
            Caps::SwapUsingOffset      => cfg!(feature = "swap-using-offset"),
            Caps::EncXipEc256          => cfg!(feature = "enc-xip-ec256"),
        }
    }

    /// Does this build have ECDSA of some type enabled for signatures.
    pub fn has_ecdsa() -> bool {
        Caps::EcdsaP256.present() || Caps::EcdsaP384.present()
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
