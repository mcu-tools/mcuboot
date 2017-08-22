#ifndef ECDSA_H
#define ECDSA_H

#include <inttypes.h>
#include <string.h>

#define MBEDTLS_OID_EC_ALG_UNRESTRICTED    {0}
#define MBEDTLS_OID_EC_GRP_SECP224R1       {0}

#define MBEDTLS_ASN1_CONSTRUCTED           0
#define MBEDTLS_ASN1_SEQUENCE              1

typedef struct {
    void *p;
    int len;
} mbedtls_asn1_buf;

typedef struct {
    int grp;
    int Q;
} mbedtls_ecdsa_context;

int mbedtls_ecdsa_init(mbedtls_ecdsa_context *);
int mbedtls_asn1_get_tag(uint8_t **, uint8_t *, size_t *, int);
int mbedtls_asn1_get_alg(uint8_t **, uint8_t *, mbedtls_asn1_buf *, mbedtls_asn1_buf *);
int mbedtls_ecp_group_load_secp224r1(int *);
int mbedtls_asn1_get_bitstring_null(uint8_t **, uint8_t *, size_t *);
int mbedtls_ecp_point_read_binary(int *, int *, uint8_t *, long);
int mbedtls_ecp_check_pubkey(int *, int *);
int mbedtls_ecdsa_read_signature(mbedtls_ecdsa_context *, uint8_t *, uint32_t, uint8_t *, int);
void mbedtls_ecdsa_free(mbedtls_ecdsa_context *);

#endif
