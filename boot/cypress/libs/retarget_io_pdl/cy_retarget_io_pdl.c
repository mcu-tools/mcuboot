/***************************************************************************//**
* \file cy_retarget_io.c
*
* \brief
* Provides APIs for retargeting stdio to UART hardware contained on the Cypress
* kits.
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

#include "cy_retarget_io_pdl.h"

#include "cycfg_peripherals.h"

#include "cy_sysint.h"
#include "cy_scb_uart.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Tracks the previous character sent to output stream */
#ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
static char cy_retarget_io_stdout_prev_char = '\0';
#endif /* CY_RETARGET_IO_CONVERT_LF_TO_CRLF */

static cy_stc_scb_uart_context_t CYBSP_UART_context;

static uint8_t cy_retarget_io_getchar(void);
static void cy_retarget_io_putchar(char c);

#if defined(__ARMCC_VERSION) /* ARM-MDK */

int fputc(int ch, FILE *f);
int fgetc(FILE *f);

#elif defined (__ICCARM__) /* IAR */

size_t __write(int handle, const unsigned char * buffer, size_t size);
size_t __read(int handle, unsigned char * buffer, size_t size);

#else /* (__GNUC__)  GCC */

int _write(int fd, const char *ptr, int len);
int _read(int fd, char *ptr, int len);

#endif /* defined(__ARMCC_VERSION) */


#if defined(__ARMCC_VERSION) /* ARM-MDK */
    /***************************************************************************
    * Function Name: fputc
    ***************************************************************************/
    __attribute__((weak)) int fputc(int ch, FILE *f)
    {
        (void)f;
    #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
        if ((char)ch == '\n' && cy_retarget_io_stdout_prev_char != '\r')
        {
            cy_retarget_io_putchar('\r');
        }

        cy_retarget_io_stdout_prev_char = (char)ch;
    #endif /* CY_RETARGET_IO_CONVERT_LF_TO_CRLF */
        cy_retarget_io_putchar(ch);
        return (ch);
    }
#elif defined (__ICCARM__) /* IAR */
    #include <yfuns.h>

    /***************************************************************************
    * Function Name: __write
    ***************************************************************************/
    __weak size_t __write(int handle, const unsigned char * buffer, size_t size)
    {
        size_t nChars = 0;
        /* This template only writes to "standard out", for all other file
        * handles it returns failure. */
        if (handle != _LLIO_STDOUT)
        {
            return (_LLIO_ERROR);
        }
        if (buffer != NULL)
        {
            for (/* Empty */; nChars < size; ++nChars)
            {
            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
                if (*buffer == '\n' && cy_retarget_io_stdout_prev_char != '\r')
                {
                    cy_retarget_io_putchar('\r');
                }

                cy_retarget_io_stdout_prev_char = *buffer;
            #endif /* CY_RETARGET_IO_CONVERT_LF_TO_CRLF */
                cy_retarget_io_putchar(*buffer);
                ++buffer;
            }
        }
        return (nChars);
    }
#else /* (__GNUC__)  GCC */
    /***************************************************************************
    * Function Name: _write
    ***************************************************************************/
    __attribute__((weak)) int _write(int fd, const char *ptr, int len)
    {
        int nChars = 0;
        (void)fd;
        if (ptr != NULL)
        {
            for (/* Empty */; nChars < len; ++nChars)
            {
            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
                if (*ptr == '\n' && cy_retarget_io_stdout_prev_char != '\r')
                {
                    cy_retarget_io_putchar('\r');
                }

                cy_retarget_io_stdout_prev_char = *ptr;
            #endif /* CY_RETARGET_IO_CONVERT_LF_TO_CRLF */
                cy_retarget_io_putchar(*ptr);
                ++ptr;
            }
        }
        return (nChars);
    }
#endif /* defined(__ARMCC_VERSION) */


#if defined(__ARMCC_VERSION) /* ARM-MDK */
    /***************************************************************************
    * Function Name: fgetc
    ***************************************************************************/
    __attribute__((weak)) int fgetc(FILE *f)
    {
        (void)f;
        return (cy_retarget_io_getchar());
    }
#elif defined (__ICCARM__) /* IAR */
    __weak size_t __read(int handle, unsigned char * buffer, size_t size)
    {
        /* This template only reads from "standard in", for all other file
        handles it returns failure. */
        if ((handle != _LLIO_STDIN) || (buffer == NULL))
        {
            return (_LLIO_ERROR);
        }
        else
        {
            *buffer = cy_retarget_io_getchar();
            return (1);
        }
    }
#else /* (__GNUC__)  GCC */
    __attribute__((weak)) int _read(int fd, char *ptr, int len)
    {
        int nChars = 0;
        (void)fd;
        if (ptr != NULL)
        {
            while (nChars < len)
            {
                *ptr = (char)cy_retarget_io_getchar();
                ++nChars;
                if((*ptr == '\n') || (*ptr == '\r'))
                {
                    break;
                }
                ++ptr;
            }
        }
        return (nChars);
    }
#endif /* defined(__ARMCC_VERSION) */

static uint8_t cy_retarget_io_getchar(void)
{
    uint32_t read_value = Cy_SCB_UART_Get(CYBSP_UART_HW);
    while (read_value == CY_SCB_UART_RX_NO_DATA)
    {
        read_value = Cy_SCB_UART_Get(CYBSP_UART_HW);
    }

    return (uint8_t)read_value;
}

static void cy_retarget_io_putchar(char c)
{
    uint32_t count = 0U;
    while (count == 0U)
    {
        count = Cy_SCB_UART_Put(CYBSP_UART_HW, (uint8_t)c);
    }
}

static cy_rslt_t cy_retarget_io_pdl_setbaud(CySCB_Type *base, uint32_t baudrate)
{
    cy_rslt_t result = CY_RSLT_TYPE_ERROR;

    uint8_t oversample_value = 8U;
    uint8_t frac_bits = 0U;
    uint32_t divider;

    Cy_SCB_UART_Disable(base, NULL);

    result = (cy_rslt_t) Cy_SysClk_PeriphDisableDivider(CY_SYSCLK_DIV_16_BIT, 0);

    divider = ((Cy_SysClk_ClkPeriGetFrequency() * (1UL << frac_bits)) + ((baudrate * oversample_value) / 2U)) / (baudrate * oversample_value) - 1U;

    if (result == CY_RSLT_SUCCESS)
    {
        result = (cy_rslt_t) Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 0U, divider);
    }
    
    if (result == CY_RSLT_SUCCESS)
    {
        result = Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 0U);
    }

    Cy_SCB_UART_Enable(base);

    return result;
}

cy_rslt_t cy_retarget_io_pdl_init(uint32_t baudrate)
{
    cy_rslt_t result = CY_RSLT_TYPE_ERROR;

    result = (cy_rslt_t)Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &CYBSP_UART_context);

    if (result == CY_RSLT_SUCCESS)
    {
        result = cy_retarget_io_pdl_setbaud(CYBSP_UART_HW, baudrate);
    }

    if (result == CY_RSLT_SUCCESS)
    {
        Cy_SCB_UART_Enable(CYBSP_UART_HW);
    }

    return result;
}

/**
 * @brief Wait while UART completes transfer. Try for tries_count times -
 *        once each 10 millisecons.
 */
void cy_retarget_io_wait_tx_complete(CySCB_Type *base, uint32_t tries_count)
{
    while(tries_count > 0U)
    {
        if (!Cy_SCB_UART_IsTxComplete(base)) {
            Cy_SysLib_DelayCycles(10U * cy_delayFreqKhz);
            tries_count -= 1U;
        } else {
            return;
        }
    }
}

void cy_retarget_io_pdl_deinit(void)
{
    Cy_SCB_UART_DeInit(CYBSP_UART_HW);
}

#if defined(__cplusplus)
}
#endif
