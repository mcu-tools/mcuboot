# Key Generation for Firmware Encryption with Mcuboot

## Context

Mcuboot supports firmware encryption using the Elliptic Curve Integrated Encryption Scheme (ECIES). This mechanism requires a key pair based on the SECP256R1 elliptic curve.

- The private key is securely generated and stored within the microcontroller.
- The public key is used to encrypt firmware updates before transmission.

## Current Implementation

In the default Mcuboot implementation, the private key used for firmware decryption is compiled directly into the bootloader binary.

- The key pair is generated externally (e.g, on a production machine).
- The public is used to encrypt the firmware image.
- The private key is embedded in the bootloader and used to decrypt and verify the firmware during boot.


## Implemented Functionality

During the first boot, the microcontroller automatically generates a key pair. The process includes:

1. Secure random number generation using STM32 hardware TRNG (True Random Generator).
2. ECC key pair creation (SECP256R1) using the mbedTLS library.
3. Conversion of the private key to DER format following the PKCS#8 standard.
4. Conversion of the public key to PEM format for export.

The private key remains stored in a secure area of the microcontroller and is never exposed. The public key can be retrived during the update.

## Process Steps

1. Verify the integrity of the current firmware.
2. Generate a secure random number.
3. Create ECC key pair (SECP256R1).
4. Convert the private key to DER format (PKCS#8).
5. Convert the public key to PEM format.

## PKCS#8 standard

PrivateKeyInfo ::= SEQUENCE {
   version Version,
   privateKeyAlgorithm PrivateKeyAlgorithmIdentifier,
   privateKey PrivateKey,
   attributes [0] IMPLICIT Attributes OPTIONAL }

## SEC1 standard

ECPrivateKey ::= SEQUENCE {
    version        INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
    privateKey     OCTET STRING,
    parameters [0] ECParameters {{ NamedCurve }} OPTIONAL,
    publicKey  [1] BIT STRING OPTIONAL
}
