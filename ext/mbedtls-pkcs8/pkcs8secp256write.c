#include "common.h"

#if defined(MBEDTLS_PK_WRITE_C)

#include "mbedtls/pk.h"
#include "mbedtls/asn1write.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "pk_internal.h"

#endif
#include <string.h>

#if defined(MBEDTLS_ECP_C)
#include "mbedtls/bignum.h"
#include "mbedtls/ecp.h"
#include "mbedtls/platform_util.h"
#endif
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#include "pk_internal.h"
#endif
#if defined(MBEDTLS_RSA_C) || defined(MBEDTLS_PK_HAVE_ECC_KEYS)
#include "pkwrite.h"
#endif
#if defined(MBEDTLS_PEM_WRITE_C)
#include "mbedtls/pem.h"
#endif

#if defined(MBEDTLS_PK_USE_PSA_EC_DATA)
#define PK_MAX_EC_PUBLIC_KEY_SIZE       PSA_EXPORT_PUBLIC_KEY_MAX_SIZE
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_PSA_MAX_EC_KEY_PAIR_LENGTH
#elif defined(MBEDTLS_USE_PSA_CRYPTO)
#define PK_MAX_EC_PUBLIC_KEY_SIZE       PSA_EXPORT_PUBLIC_KEY_MAX_SIZE
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_PSA_MAX_EC_KEY_PAIR_LENGTH
#else
#define PK_MAX_EC_PUBLIC_KEY_SIZE       MBEDTLS_ECP_MAX_PT_LEN
#define PK_MAX_EC_KEY_PAIR_SIZE         MBEDTLS_ECP_MAX_BYTES
#endif

static mbedtls_pk_type_t pk_get_type_ext(const mbedtls_pk_context *pk)
{
    mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(pk);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (pk_type == MBEDTLS_PK_OPAQUE) {
        psa_key_attributes_t opaque_attrs = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_type_t opaque_key_type;

        if (psa_get_key_attributes(pk->priv_id, &opaque_attrs) != PSA_SUCCESS) {
            return MBEDTLS_PK_NONE;
        }
        opaque_key_type = psa_get_key_type(&opaque_attrs);
        psa_reset_key_attributes(&opaque_attrs);

        if (PSA_KEY_TYPE_IS_ECC(opaque_key_type)) {
            return MBEDTLS_PK_ECKEY;
        } else if (PSA_KEY_TYPE_IS_RSA(opaque_key_type)) {
            return MBEDTLS_PK_RSA;
        } else {
            return MBEDTLS_PK_NONE;
        }
    } else
#endif
    return pk_type;
}

static int pk_write_ec_private(unsigned char **p, unsigned char *start,
                               const mbedtls_pk_context *pk)
{
    size_t byte_length;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char tmp[PK_MAX_EC_KEY_PAIR_SIZE];

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_status_t status;
    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        status = psa_export_key(pk->priv_id, tmp, sizeof(tmp), &byte_length);
        if (status != PSA_SUCCESS) {
            ret = PSA_PK_ECDSA_TO_MBEDTLS_ERR(status);
            return ret;
        }
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    {
        mbedtls_ecp_keypair *ec = mbedtls_pk_ec_rw(*pk);
        byte_length = (ec->grp.pbits + 7) / 8;

        ret = mbedtls_ecp_write_key_ext(ec, &byte_length, tmp, sizeof(tmp));
        if (ret != 0) {
            goto exit;
        }
    }
    ret = mbedtls_asn1_write_octet_string(p, start, tmp, byte_length);
exit:
    mbedtls_platform_zeroize(tmp, sizeof(tmp));
    return ret;
}

static int pk_write_ec_pubkey(unsigned char **p, unsigned char *start,
                              const mbedtls_pk_context *pk)
{
    size_t len = 0;
    unsigned char buf[PK_MAX_EC_PUBLIC_KEY_SIZE];
    mbedtls_ecp_keypair *ec = mbedtls_pk_ec(*pk);
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_OPAQUE) {
        if (psa_export_public_key(pk->priv_id, buf, sizeof(buf), &len) != PSA_SUCCESS) {
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        }
        *p -= len;
        memcpy(*p, buf, len);
        return (int) len;
    } else
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    {
        if ((ret = mbedtls_ecp_point_write_binary(&ec->grp, &ec->Q,
                                                  MBEDTLS_ECP_PF_UNCOMPRESSED,
                                                  &len, buf, sizeof(buf))) != 0) {
            return ret;
        }
    }

    if (*p < start || (size_t) (*p - start) < len) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *p -= len;
    memcpy(*p, buf, len);

    return (int) len;
}


#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
/*=====Encodes an EC private key into PKCS#8 DER format.=====*/
static int pk_write_ecpkcs8_der(unsigned char **p, unsigned char *buf,
                           const mbedtls_pk_context *pk)
{


    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;
    size_t pubkey_raw_len = 0;
    size_t pubkey_bitstring_len = 0;
    size_t pubkey_block_len = 0;
    size_t ec_privkey_len = 0;
    size_t alg_id_len = 0;

    /* ========publicKey========= */
    MBEDTLS_ASN1_CHK_ADD(pubkey_raw_len, pk_write_ec_pubkey(p, buf, pk));

    if (*p - buf < 1)
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;

    (*p)--;
    **p = 0;
    pubkey_raw_len += 1;

    /*bit string publicKey*/
    MBEDTLS_ASN1_CHK_ADD(pubkey_bitstring_len, mbedtls_asn1_write_len(p, buf, pubkey_raw_len));
    MBEDTLS_ASN1_CHK_ADD(pubkey_bitstring_len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_BIT_STRING));

    pubkey_block_len = pubkey_raw_len + pubkey_bitstring_len;

    /*bit string publicKey in a specific tag 1 */
    MBEDTLS_ASN1_CHK_ADD(pubkey_block_len, mbedtls_asn1_write_len(p, buf, pubkey_block_len));
    MBEDTLS_ASN1_CHK_ADD(pubkey_block_len, mbedtls_asn1_write_tag(p, buf,
        MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 1));

    len += pubkey_block_len;

    /* ======privateKey====== */
    MBEDTLS_ASN1_CHK_ADD(ec_privkey_len, pk_write_ec_private(p, buf, pk));

    /* version privateKey */
    MBEDTLS_ASN1_CHK_ADD(ec_privkey_len, mbedtls_asn1_write_int(p, buf, 1));

    ec_privkey_len += pubkey_block_len;

    /* sequence privateKey */
    MBEDTLS_ASN1_CHK_ADD(ec_privkey_len, mbedtls_asn1_write_len(p, buf, ec_privkey_len));
    MBEDTLS_ASN1_CHK_ADD(ec_privkey_len, mbedtls_asn1_write_tag(p, buf,
        MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    /* octet string privateKey */
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, buf, ec_privkey_len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, buf, MBEDTLS_ASN1_OCTET_STRING));


    len += ec_privkey_len - pubkey_block_len;

    /*======AlgotithmIdentifier=====*/
	MBEDTLS_ASN1_CHK_ADD(alg_id_len, mbedtls_asn1_write_oid(p, buf,
						 MBEDTLS_OID_EC_GRP_SECP256R1,
						 MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP256R1)));

	/*id-ecPublicKey*/
	const char oid_ec_public_key[] = MBEDTLS_OID_EC_ALG_UNRESTRICTED;
	MBEDTLS_ASN1_CHK_ADD(alg_id_len, mbedtls_asn1_write_oid(p, buf,
						 oid_ec_public_key, strlen(oid_ec_public_key)));

	/*oid sequence*/
    MBEDTLS_ASN1_CHK_ADD(alg_id_len, mbedtls_asn1_write_len(p, buf, alg_id_len));
    MBEDTLS_ASN1_CHK_ADD(alg_id_len, mbedtls_asn1_write_tag(p, buf,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
    /* version */
    size_t version_len = 0;
    MBEDTLS_ASN1_CHK_ADD(version_len, mbedtls_asn1_write_int(p, buf, 0));

    /*SEQUENCE*/
    size_t total_len = version_len + alg_id_len + len;

    MBEDTLS_ASN1_CHK_ADD(total_len, mbedtls_asn1_write_len(p, buf, total_len));
    MBEDTLS_ASN1_CHK_ADD(total_len, mbedtls_asn1_write_tag(p, buf,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    len += total_len - len;


    return (int) len;
}
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */


int mbedtls_pk_write_keypkcs8_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size)
{
    unsigned char *c;

    if (size == 0) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    c = buf + size;

#if defined(MBEDTLS_RSA_C)
    if (pk_get_type_ext(key) == MBEDTLS_PK_RSA) {
        return pk_write_rsa_der(&c, buf, key);
    } else
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_PK_HAVE_ECC_KEYS)
    if (pk_get_type_ext(key) == MBEDTLS_PK_ECKEY) {
#if defined(MBEDTLS_PK_HAVE_RFC8410_CURVES)
        if (mbedtls_pk_is_rfc8410(key)) {
            return pk_write_ec_rfc8410_der(&c, buf, key);
        }
#endif /* MBEDTLS_PK_HAVE_RFC8410_CURVES */
        return pk_write_ecpkcs8_der(&c, buf, key);
    } else
#endif /* MBEDTLS_PK_HAVE_ECC_KEYS */
    return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
}


