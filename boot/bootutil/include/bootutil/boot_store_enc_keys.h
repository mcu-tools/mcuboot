/*
 * Copyright (c) 2026 WIKA Alexander Wiegand SE & Co. KG
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _BOOT_STORE_ENC_KEYS_H_
#define _BOOT_STORE_ENC_KEYS_H_

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Store the private key into the flash partition
 * 
 * @param key       Pointer to the buffer where the key is stored 
 * @param key_len   Size of the key
 * 
 * @return          0 on success; nonzero on failure.
 */
int store_priv_enc_key(const unsigned char *key, size_t key_len);

/**
*  @brief Check the private key encryption storage is empty.
*
*  @retval true     The key storage is empty.
*  @retval false    The key storage is not empty or the flash area could not be accessed. 
*
**/
bool is_key_storage_erased();

#endif /*_BOOT_STORE_ENC_KEYS_H_*/