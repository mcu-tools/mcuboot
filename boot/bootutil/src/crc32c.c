/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Cypress Semiconductors
 *
 * Original license:
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "crc32c.h"

#define NIBBLE_POS                          (4u)
#define NIBBLE_MSK                          (0xFu)
#define CRC_TABLE_SIZE                      (16u)           /* A number of uint32_t elements in the CRC32 table */
#define CRC_INIT                            (0xFFFFFFFFu)


/*******************************************************************************
* Function Name: crc32c_checksum
****************************************************************************//**
*
* This function computes a CRC-32C for the provided number of bytes contained
* in the provided buffer.
*
* \param address    The pointer to a buffer containing the data to compute
*                   the checksum for.
* \param length     The number of bytes in the buffer to compute the checksum
*                   for.
*
* \return CRC-32C for the provided data.
*
*******************************************************************************/
uint32_t crc32c_checksum(const uint8_t *address, uint32_t length)
{
    /* Contains generated values to calculate CRC-32C by 4 bits per iteration*/
    static const uint32_t crcTable[CRC_TABLE_SIZE] =
    {
        0x00000000u, 0x105ec76fu, 0x20bd8edeu, 0x30e349b1u,
        0x417b1dbcu, 0x5125dad3u, 0x61c69362u, 0x7198540du,
        0x82f63b78u, 0x92a8fc17u, 0xa24bb5a6u, 0xb21572c9u,
        0xc38d26c4u, 0xd3d3e1abu, 0xe330a81au, 0xf36e6f75u,
    };

    uint32_t crc = CRC_INIT;
    if (length != 0u)
    {
        do
        {
            crc = crc ^ *address;
            crc = (crc >> NIBBLE_POS) ^ crcTable[crc & NIBBLE_MSK];
            crc = (crc >> NIBBLE_POS) ^ crcTable[crc & NIBBLE_MSK];
            --length;
            ++address;
        } while (length != 0u);
    }
    return (~crc);
}
