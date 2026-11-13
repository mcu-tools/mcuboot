- The PSA ECDSA verifier now calls `psa_crypto_init()` before importing
  the public key. Previously it relied on the platform having
  initialized PSA, and failed every ECDSA image with
  `PSA_ERROR_BAD_STATE` when it hadn't.
