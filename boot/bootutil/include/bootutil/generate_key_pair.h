/*
 * Copyright (c) 2026 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __GENERATE_KEY_PAIR_H__
#define __GENERATE_KEY_PAIR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate public and private key for encryption in (PKCS8 and PEM format).
 *
 * @return On failure, print error message.
 * 
 */
void generate_enc_key_pair(void);

#ifdef __cplusplus
}
#endif

#endif /* __GENERATE_KEY_PAIR_H__ */