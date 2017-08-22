#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>
#include <stddef.h>

#define MBEDTLS_MPI_MAX_SIZE 1024

typedef struct
{
    int s;
    size_t n;
    uint32_t *p;
} mbedtls_mpi;

size_t mbedtls_mpi_size(const mbedtls_mpi *);

#endif
