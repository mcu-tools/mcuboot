- Hardened RSA-PSS signature verification against fault injection.
  With the PSA crypto backend, the new Zephyr option
  `CONFIG_BOOT_RSA_PSA_DOUBLE_VERIFY` (`MCUBOOT_RSA_PSA_DOUBLE_VERIFY`)
  runs the verification twice and requires both to succeed. It is on
  by default with the MEDIUM and HIGH fault injection hardening
  profiles, and roughly doubles the RSA verification time.
