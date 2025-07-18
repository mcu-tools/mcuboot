/**
 * \file external_memory.c
 * \version 1.0
 *
 * \brief
 *  This is the source file of external flash driver adoption layer between
 *PSoC6 and standard MCUBoot code.
 *
 ********************************************************************************
 * \copyright
 *
 * (c) 2025, Cypress Semiconductor Corporation
 * or a subsidiary of Cypress Semiconductor Corporation. All rights
 * reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software"), is owned by Cypress Semiconductor
 * Corporation or one of its subsidiaries ("Cypress") and is protected by
 * and subject to worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-
 * exclusive, non-transferable license to copy, modify, and compile the
 * Software source code solely for use in connection with Cypress?s
 * integrated circuit products. Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO
 * WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING,
 * BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE. Cypress reserves the right to make
 * changes to the Software without notice. Cypress does not assume any
 * liability arising out of the application or use of the Software or any
 * product or circuit described in the Software. Cypress does not
 * authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *
 ******************************************************************************/
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysflash/sysflash.h>

#include "cy_device_headers.h"
#include "cy_flash.h"
#include "cy_syspm.h"
#include "flash_map_backend/flash_map_backend.h"
#include "flash_map_backend_platform.h"
#include "flash_qspi.h"

#define SMIF_OFFSET(addr) ((uint32_t)(addr) - SMIF_MEM_START_PLATFORM)

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
#define CY_GET_XIP_REMAP_ADDR(addr) ((addr) - CY_XIP_BASE + CY_XIP_CBUS_BASE)
#define ICACHE_INVALIDATE()                               \
    ({                                                    \
        /* Invalidate the cache */                        \
        ICACHE0->CMD = ICACHE0->CMD | ICACHE_CMD_INV_Msk; \
                                                          \
        /*Wait for invalidation complete */               \
        while (ICACHE0->CMD & ICACHE_CMD_INV_Msk);        \
    })

#endif

static uint32_t get_base_address(uint8_t fa_device_id)
{
    uint32_t res = 0U;

    if ((fa_device_id & FLASH_DEVICE_EXTERNAL_FLAG) ==
        FLASH_DEVICE_EXTERNAL_FLAG) {
        res = SMIF_MEM_START_PLATFORM;
    } else {
        res = 0U;
    }

    return res;
}

static uint32_t get_min_erase_size(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return qspi_get_erase_size();
}

static uint32_t get_align_size(uint8_t fa_device_id)
{
    (void)fa_device_id;

#if !defined(MCUBOOT_SWAP_USING_STATUS)
    return sizeof(uint32_t);
#else
    return qspi_get_erase_size();
#endif
}

static uint8_t get_erase_val(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return EXTERNAL_MEMORY_ERASE_VALUE_PLATFORM;
}

static int read(uint8_t fa_device_id, uintptr_t addr, void *data, uint32_t len)
{
    (void)fa_device_id;

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
    uintptr_t src = CY_GET_XIP_REMAP_ADDR(addr);
    size_t mem_sz = qspi_get_mem_size();

    if ((src >= CY_XIP_CBUS_BASE) && (src <= (CY_XIP_CBUS_BASE + mem_sz))) {
        if ((src + len) <= (CY_XIP_CBUS_BASE + mem_sz)) {
            memcpy(data, (const void *)src, len);

            return 0;
        }
    }
    return -1;
#else

    int rc = -1;
    cy_stc_smif_mem_config_t *cfg;
    cy_en_smif_status_t st;
    uint32_t offset;

    cfg = qspi_get_memory_config(0u);

    offset = (uint32_t)addr - SMIF_MEM_START_PLATFORM;

    st = Cy_SMIF_MemRead(qspi_get_device(), cfg, offset, data, len,
                         qspi_get_context());
    if (st == CY_SMIF_SUCCESS) {
        rc = 0;
    }
    return rc;
#endif
}

CY_RAMFUNC_BEGIN
static cy_en_smif_status_t smif_write(uint32_t offset, const void *data, size_t len)
{
    SMIF_Type *device = qspi_get_device();
    cy_stc_smif_context_t *ctx = qspi_get_context();
    cy_stc_smif_mem_config_t *cfg = qspi_get_memory_config(0u);

    return Cy_SMIF_MemWrite(device, cfg, offset, data, len, ctx);
}
CY_RAMFUNC_END

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
CY_RAMFUNC_BEGIN
static cy_en_smif_status_t smif_encrypt(void *data, uint32_t len, uintptr_t addr)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    SMIF_Type *device = qspi_get_device();
    cy_stc_smif_context_t *ctx = qspi_get_context();

    Cy_SMIF_SetMode(device, CY_SMIF_NORMAL);
    status = Cy_SMIF_Encrypt(device, CY_GET_XIP_REMAP_ADDR(addr), data, len, ctx);
    Cy_SMIF_SetMode(device, CY_SMIF_MEMORY);

    return status;
}
CY_RAMFUNC_END

CY_RAMFUNC_BEGIN
static cy_en_smif_status_t
smif_write_encrypt_block(const void **data, uint32_t *len, uintptr_t *addr)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uintptr_t write_address = *addr;
    uintptr_t prev_align = write_address & CY_SMIF_CRYPTO_ADDR_MASK;
    uintptr_t next_align = prev_align + CY_SMIF_AES128_BYTES;
    size_t align_offset = write_address - prev_align;
    uint8_t tmp[CY_SMIF_AES128_BYTES] = {0U};
    size_t bytes_to_cpy = *len;

    if ((*len) > CY_SMIF_AES128_BYTES) {
        bytes_to_cpy = CY_SMIF_AES128_BYTES - ((*len) % CY_SMIF_AES128_BYTES);
    }

    (void)memcpy((void *)&tmp[align_offset], *data, bytes_to_cpy);

    status = smif_encrypt(tmp, CY_SMIF_AES128_BYTES, prev_align);

    if (status == CY_SMIF_SUCCESS) {
        status = smif_write(SMIF_OFFSET(write_address), &tmp[align_offset], bytes_to_cpy);
    }

    if (status == CY_SMIF_SUCCESS) {
        *len -= bytes_to_cpy;
        *addr = next_align;
        *data = &((uint8_t *)(*data))[bytes_to_cpy];
    }

    return status;
}
CY_RAMFUNC_END
#endif

CY_RAMFUNC_BEGIN
static int write(uint8_t fa_device_id, uintptr_t addr, const void *data,
                 uint32_t len)
{
    (void)fa_device_id;

    int rc = -1;
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    const void *p_data = data;

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
    while ((len > 0U) && (status == CY_SMIF_SUCCESS)) {
        status = smif_write_encrypt_block(&p_data, &len, &addr);
    }
#else
    status = smif_write(SMIF_OFFSET(addr), p_data, len);
#endif

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
    ICACHE_INVALIDATE();
#endif

    rc = (status == CY_SMIF_SUCCESS) ? 0 : -1;

    return rc;
}
CY_RAMFUNC_END

CY_RAMFUNC_BEGIN
static int erase(uint8_t fa_device_id, uintptr_t addr, uint32_t size)
{
    (void)fa_device_id;

    int rc = -1;

    if (size > 0u) {
        SMIF_Type *device = qspi_get_device();
        cy_stc_smif_context_t *ctx = qspi_get_context();
        cy_stc_smif_mem_config_t *cfg = qspi_get_memory_config(0u);
        uint32_t eraseSize = qspi_get_erase_size();
        uint32_t offset = ((uint32_t)addr - SMIF_MEM_START_PLATFORM) & ~((uint32_t)(eraseSize - 1u));

        while ((size > 0u) && (CY_SMIF_SUCCESS == Cy_SMIF_MemEraseSector(device, cfg, offset, eraseSize, ctx))) {
            size -= (size >= eraseSize) ? eraseSize : size;
            offset += eraseSize;
        }

        if (size == 0) {
            rc = 0;
        }
    }

#if defined(MCUBOOT_ENC_IMAGES_SMIF)
    ICACHE_INVALIDATE();
#endif

    return rc;
}
CY_RAMFUNC_END

static int open(uint8_t fa_device_id)
{
    (void)fa_device_id;

    return 0;
}

static void close(uint8_t fa_device_id)
{
    (void)fa_device_id;
}

const struct flash_area_interface external_mem_interface = {
    .open = &open,
    .close = &close,
    .read = &read,
    .write = &write,
    .erase = &erase,
    .get_erase_val = &get_erase_val,
    .get_erase_size = &get_min_erase_size,
    .get_align_size = &get_align_size,
    .get_base_address = &get_base_address};
