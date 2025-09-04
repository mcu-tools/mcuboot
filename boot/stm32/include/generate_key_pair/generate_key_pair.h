#ifndef __GENERATE_KEY_PAIR_H__
#define __GENERATE_KEY_PAIR_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "stm32wlxx_hal.h"

extern RNG_HandleTypeDef hrng;
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);
int gen_p256_keypair(mbedtls_pk_context *pk);
void dump_p256(const mbedtls_pk_context *pk);


#ifdef __cplusplus
}
#endif

#endif /* __GENERATE_KEY_PAIR_H__ */
