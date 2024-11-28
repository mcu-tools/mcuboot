/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "bootutil/fault_injection_hardening.h"

#ifdef FIH_ENABLE_DELAY
#include "boot_rng.h"

#define FIH_DELAY_MSK 0x3FUL
#endif /* FIH_ENABLE_DELAY */

#ifdef FIH_ENABLE_CFI
fih_uint fih_cfi_ctr = FIH_UINT_INIT_GLOBAL(0u);

fih_uint fih_cfi_get_and_increment(uint8_t cnt)
{
    fih_uint saved_ctr = fih_cfi_ctr;

    if (fih_uint_decode(fih_cfi_ctr) > UINT32_MAX - cnt) {
        /* Overflow */
        FIH_PANIC;
    }

    fih_cfi_ctr = fih_uint_encode(fih_uint_decode(fih_cfi_ctr) + cnt);

    (void)fih_uint_validate(fih_cfi_ctr);
    (void)fih_uint_validate(saved_ctr);

    return saved_ctr;
}

void fih_cfi_validate(fih_uint saved)
{
    if (!fih_uint_eq(saved, fih_cfi_ctr))
    {
        FIH_PANIC;
    }
}

void fih_cfi_decrement(void)
{
    if (fih_uint_decode(fih_cfi_ctr) < 1u) {
        FIH_PANIC;
    }

    fih_cfi_ctr = fih_uint_encode(fih_uint_decode(fih_cfi_ctr) - 1u);

    (void)fih_uint_validate(fih_cfi_ctr);
}
#endif /* FIH_ENABLE_CFI */

#ifdef FIH_ENABLE_GLOBAL_FAIL
/* Global failure loop for bootloader code. Uses attribute used to prevent
 * compiler removing due to non-standard calling procedure. Multiple loop jumps
 * used to make unlooping difficult.
 */
__attribute__((noinline))
__attribute__((noreturn))
__attribute__((weak))
void fih_panic_loop(void)
{
    FIH_LABEL("FAILURE_LOOP");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    __asm volatile ("b fih_panic_loop");
    while (true) {} /* Satisfy noreturn */
}
#endif /* FIH_ENABLE_GLOBAL_FAIL */

#ifdef FIH_ENABLE_DELAY

/*******************************************************************************
 * Function Name: fih_delay_init
 *******************************************************************************
 * \brief Initialize assets which are required for random delay generation.
 *
 ******************************************************************************/
void fih_delay_init(void)
{
    if(!boot_rng_init())
    {
        FIH_PANIC;
    }
}


/*******************************************************************************
 * Function Name: fih_delay_random
 *******************************************************************************
 * \brief Generate 8-bit random delay number masked with FIH_DELAY_MSK.
 *
 * \return  8-bit random delay number masked with FIH_DELAY_MSK.
 *
 ******************************************************************************/
static uint8_t fih_delay_random(void)
{
    return (uint8_t)(boot_rng_random_generate() & FIH_DELAY_MSK);
}


/*******************************************************************************
 * Function Name: fih_delay
 *******************************************************************************
 * \brief FIH delay execution.
 *
 * \return  status of execution. The return value is required by calling macros
 *          like fih_uint_eq(). It always returns true or hang in FIH_PANIC.
 *
 ******************************************************************************/
bool fih_delay(void)
{
    volatile uint8_t counter = 0U;
    uint8_t delay = fih_delay_random();

    for (uint8_t i = 0; i < delay; i++)
    {
        counter++;
    }

    if (counter != delay)
    {
        FIH_PANIC;
    }

    return true;
}

#endif /* FIH_ENABLE_DELAY */
