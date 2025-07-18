/****************************************************************************//**
* \file rollback_counter.h
* \version 0.0.1
*
* \brief
*  This is the source code implementing rollback protection feature.
*
*********************************************************************************
* \copyright
* Copyright 2019-2025 Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Result codes for RBC operations
 */
typedef enum
{
    CY_RBC_SUCCESS = 0,      /**< Operation completed successfully */
    CY_RBC_INVALID = -1,  /**< Invalid operation or error occurred */
} cy_rbc_result_t;

/**
 * @brief Reads the rollback counter for a specific counter number
 * @param counter The counter number to read
 * @return The counter value
 */
uint32_t cy_rbc_read_rollback_counter(uint8_t counter);

/**
 * @brief Updates the rollback counter for a specific counter number
 * @param counter The counter number to update
 * @param value The new value for the counter
 * @return The result of the operation
 */
cy_rbc_result_t cy_rbc_update_rollback_counter(uint8_t counter, uint32_t value);

/**
 * @brief Recovers the rollback counter to its initial state
 * @return The result of the operation
 */
cy_rbc_result_t cy_rbc_recover_rollback_counter(void);

/**
 * @brief Calculates the checksum of a data block
 * @param data Pointer to the data block
 * @param dataSize Size of the data block in bytes
 * @param checkSum Pointer to store the calculated checksum
 * @return The result of the operation
 */
extern cy_rbc_result_t cy_rbc_calc_check_sum(const uint32_t *data, uint32_t dataSize, uint8_t *checkSum);

/**
 * @brief Programs the data at a specific offset
 * @param src Pointer to the source data
 * @param offset Offset to program the data at
 * @return The result of the operation
 */
extern cy_rbc_result_t cy_rbc_program(const uint8_t* src, uintptr_t offset);

/**
 * @brief Erases the data at a specific offset
 * @param offset Offset to erase the data at
 * @return The result of the operation
 */
extern cy_rbc_result_t cy_rbc_erase(uintptr_t offset);

/**
 * @brief Checks if the data at a specific offset is erased
 * @param data Pointer to the data block
 * @return True if the data is erased, false otherwise
 */
extern bool cy_rbc_is_erased(const uint8_t *data);