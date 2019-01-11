// Query the bootloader's capabilities.

#[repr(u32)]
#[derive(Copy, Clone, Eq, PartialEq)]
#[allow(unused)]
pub enum Caps {
    RSA2048          = (1 << 0),
    EcdsaP224        = (1 << 1),
    EcdsaP256        = (1 << 2),
    SwapUpgrade      = (1 << 3),
    OverwriteUpgrade = (1 << 4),
    EncRsa           = (1 << 5),
    EncKw            = (1 << 6),
}

impl Caps {
    pub fn present(self) -> bool {
        let caps = unsafe { bootutil_get_caps() };
        (caps as u32) & (self as u32) != 0
    }
}

extern "C" {
    fn bootutil_get_caps() -> Caps;
}
