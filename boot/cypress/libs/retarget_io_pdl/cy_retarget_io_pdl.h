/***************************************************************************//**
* \file cy_retarget_io.h
*
* \brief
* Provides APIs for transmitting messages to or from the board via standard
* printf/scanf functions. Messages are transmitted over a UART connection which
* is generally connected to a host machine. Transmission is done at 115200 baud
* using the tx and rx pins provided by the user of this library. The UART
* instance is made available via cy_retarget_io_uart_obj in case any changes
* to the default configuration are desired.
* NOTE: If the application is built using newlib-nano, by default, floating
* point format strings (%f) are not supported. To enable this support you must
* add '-u _printf_float' to the linker command line.
*
********************************************************************************
* \copyright
* Copyright 2018-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#ifndef CY_RETARGET_IO_PDL_H
#define CY_RETARGET_IO_PDL_H

#pragma once

#include <stdio.h>
#include "cy_result.h"
#include "cy_pdl.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** UART baud rate */
#define CY_RETARGET_IO_BAUDRATE             (115200)

/** Defining this macro enables conversion of line feed (LF) into carriage
 * return followed by line feed (CR & LF) on the output direction (STDOUT). You
 * can define this macro through the DEFINES variable in the application
 * Makefile.
 */
#define CY_RETARGET_IO_CONVERT_LF_TO_CRLF

cy_rslt_t cy_retarget_io_pdl_init(uint32_t baudrate);

void cy_retarget_io_wait_tx_complete(CySCB_Type *base, uint32_t tries_count);

void cy_retarget_io_pdl_deinit(void);

#if defined(__cplusplus)
}
#endif

#endif /* CY_RETARGET_IO_PDL_H */
