#ifndef RSA_H
#define RSA_H

#include <stdint.h>
#include <stddef.h>

#include <mbedtls/bignum.h>

typedef struct
{
    int ver;
    size_t len;

    mbedtls_mpi N;
    mbedtls_mpi E;

    mbedtls_mpi D;
    mbedtls_mpi P;
    mbedtls_mpi Q;
    mbedtls_mpi DP;
    mbedtls_mpi DQ;
    mbedtls_mpi QP;

    mbedtls_mpi RN;
    mbedtls_mpi RP;
    mbedtls_mpi RQ;

    mbedtls_mpi Vi;
    mbedtls_mpi Vf;

    int padding;
    int hash_id;

} mbedtls_rsa_context;

void mbedtls_rsa_free(mbedtls_rsa_context *);
int mbedtls_rsa_public(mbedtls_rsa_context *, const unsigned char *, unsigned char *);
void mbedtls_rsa_init(mbedtls_rsa_context *, int, int);
int mbedtls_rsa_check_pubkey(const mbedtls_rsa_context *);

#endif
