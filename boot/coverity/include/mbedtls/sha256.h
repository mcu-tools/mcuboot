#ifndef SHA256_H
#define SHA256_H

#include <inttypes.h>
#include <string.h>

typedef struct {

} mbedtls_sha256_context;

int mbedtls_sha256_init(mbedtls_sha256_context *);
int mbedtls_sha256_starts(mbedtls_sha256_context *, int);
int mbedtls_sha256_update(mbedtls_sha256_context *, const uint8_t *, size_t);
void mbedtls_sha256_finish( mbedtls_sha256_context *, unsigned char *);

#endif
