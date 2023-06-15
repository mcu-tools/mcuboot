
#ifndef H_ENCRYPTED_PRIV_
#define H_ENCRYPTED_PRIV_

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_ENC_IMAGES)

#include <stddef.h>
#include <inttypes.h>
#include <string.h>

#if defined(MCUBOOT_ENCRYPT_EC256)
#include "bootutil/crypto/ecdh_p256.h"
#elif defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/ecdh_x25519.h"
#endif

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)
#include "bootutil/crypto/sha256.h"
#include "bootutil/crypto/hmac_sha256.h"
#include "mbedtls/oid.h"
#include "mbedtls/asn1.h"

#endif /* MCUBOOT_ENCRYPT_EC256 || MCUBOOT_ENCRYPT_X25519 */


#include "bootutil/image.h"
#include "bootutil/enc_key.h"
#include "bootutil/sign_key.h"
#include "bootutil/crypto/common.h"

#include "bootutil_priv.h"

#define EXPECTED_ENC_LEN        BOOT_ENC_TLV_SIZE

#if defined(MCUBOOT_ENCRYPT_RSA)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_RSA2048
#elif defined(MCUBOOT_ENCRYPT_KW)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_KW
#elif defined(MCUBOOT_ENCRYPT_EC256)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_EC256
#    define SHARED_KEY_LEN      NUM_ECC_BYTES
#    define PRIV_KEY_LEN        NUM_ECC_BYTES
#    define EC_PUBK_INDEX       (0)
#    define EC_TAG_INDEX        (65)
#    define EC_CIPHERKEY_INDEX  (65 + 32)
_Static_assert(EC_CIPHERKEY_INDEX + BOOT_ENC_KEY_SIZE == EXPECTED_ENC_LEN,
        "Please fix ECIES-P256 component indexes");
#include "bootutil/crypto/ecdh_p256.h"
#elif defined(MCUBOOT_ENCRYPT_X25519)
#    define EXPECTED_ENC_TLV    IMAGE_TLV_ENC_X25519
#    define SHARED_KEY_LEN      (32)
#    define PRIV_KEY_LEN        (32)
#    define EC_PUBK_INDEX       (0)
#    define EC_TAG_INDEX        (32)
#    define EC_CIPHERKEY_INDEX  (32 + 32)
_Static_assert(EC_CIPHERKEY_INDEX + BOOT_ENC_KEY_SIZE == EXPECTED_ENC_LEN,
        "Please fix ECIES-X25519 component indexes");
#include "bootutil/crypto/ecdh_x25519.h"
#endif

#if defined(MCUBOOT_ENCRYPT_EC256) || defined(MCUBOOT_ENCRYPT_X25519)

int expand_secret(uint8_t *derived_key, uint8_t *shared);

int HMAC_key(const uint8_t *buf, uint8_t *derived_key);

int decrypt_ciphered_key(const uint8_t *buf, uint8_t *counter,
     uint8_t *derived_key, uint8_t *enckey);

#endif /* MCUBOOT_ENCRYPT_EC256 || MCUBOOT_ENCRYPT_X25519 */

#endif /* MCUBOOT_ENC_IMAGES */

#endif /* H_ENCRYPTED_PRIV_ */