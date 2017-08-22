#ifndef ASN1_H
#define ASN1_H

#include <stddef.h>

#include <mbedtls/bignum.h>

#define MBEDTLS_ASN1_CONSTRUCTED 0x20
#define MBEDTLS_ASN1_SEQUENCE    0x10

int mbedtls_asn1_get_tag(unsigned char **, const unsigned char *, size_t *, int);
int mbedtls_asn1_get_mpi(unsigned char **, const unsigned char *, mbedtls_mpi *);

#endif
