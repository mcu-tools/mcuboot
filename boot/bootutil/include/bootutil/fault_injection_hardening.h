/*
 * Copyright (c) 2020-2021, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef FAULT_INJECTION_HARDENING_H
#define FAULT_INJECTION_HARDENING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Fault injection mitigation library.
 *
 * Has support for different measures, which can either be enabled/disabled
 * separately or by defining one of the MCUBOOT_FIH_PROFILEs.
 *
 * NOTE: It is not guaranteed that these constructs against fault injection
 *       attacks can be preserved in all compilers.
 *
 * FIH_ENABLE_DOUBLE_VARS makes critical variables into a tuple (x, x ^ msk).
 * Then the correctness of x can be checked by XORing the two tuple values
 * together. This also means that comparisons between fih_ints can be verified
 * by doing x == y && x.msk == y_msk.
 *
 * FIH_ENABLE_GLOBAL_FAIL makes all while(1) failure loops redirect to a global
 * failure loop. This loop has mitigations against loop escapes / unlooping.
 * This also means that any unlooping won't immediately continue executing the
 * function that was executing before the failure.
 *
 * FIH_ENABLE_CFI (Control Flow Integrity) creates a global counter that is
 * incremented before every FIH_CALL of vulnerable functions. On the function
 * return the counter is decremented, and after the return it is verified that
 * the counter has the same value as before this process. This can be used to
 * verify that the function has actually been called. This protection is
 * intended to discover that important functions are called in an expected
 * sequence and none of them is missed due to an instruction skip which could
 * be a result of glitching attack. It does not provide protection against ROP
 * or JOP attacks.
 *
 * FIH_ENABLE_DELAY causes random delays. This makes it hard to cause faults
 * precisely. It requires an RNG. An mbedtls integration is provided in
 * fault_injection_hardening_delay_mbedtls.h, but any RNG that has an entropy
 * source can be used by implementing the fih_delay_random_uchar function.
 *
 * The basic call pattern is:
 *
 * fih_int fih_rc = FIH_FAILURE;
 * FIH_CALL(vulnerable_function, fih_rc, arg1, arg2);
 * if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
 *     error_handling();
 * }
 *
 * If a fault injection is detected, call FIH_PANIC to trap the execution.
 *
 * Note that any function called by FIH_CALL must only return using FIH_RET,
 * as otherwise the CFI counter will not be decremented and the CFI check will
 * fail causing a panic.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mcuboot_config/mcuboot_config.h"
#undef FIH_ENABLE_GLOBAL_FAIL
#undef FIH_ENABLE_CFI
#undef FIH_ENABLE_DOUBLE_VARS
#undef FIH_ENABLE_DELAY

#ifdef CY_COVERITY_2012_CHECK
#define CY_COVERITY_PRAGMA_STR(a) #a
#define FIH_MISRA_DEVIATE_BLOCK_START(MISRA, COUNT, MESSAGE) \
_Pragma(CY_COVERITY_PRAGMA_STR(coverity compliance block (deviate:COUNT MISRA MESSAGE)))

#define FIH_MISRA_BLOCK_END(MISRA) \
_Pragma(CY_COVERITY_PRAGMA_STR(coverity compliance end_block MISRA))
#else
#define FIH_MISRA_DEVIATE_BLOCK_START(MISRA, COUNT, MESSAGE)
#define FIH_MISRA_BLOCK_END(MISRA)
#endif /* CY_COVERITY_2012_CHECK */

FIH_MISRA_DEVIATE_BLOCK_START('MISRA C-2012 Rule 10.1', 10, 'Signed integer bitwise operations')
FIH_MISRA_DEVIATE_BLOCK_START('MISRA C-2012 Rule 10.4', 10, 'Signed integer bitwise operations')

#ifdef MCUBOOT_FIH_PROFILE_ON
#if defined(MCUBOOT_FIH_PROFILE_LOW)
#define FIH_ENABLE_GLOBAL_FAIL
#define FIH_ENABLE_CFI

#elif defined(MCUBOOT_FIH_PROFILE_MEDIUM)
#define FIH_ENABLE_DOUBLE_VARS
#define FIH_ENABLE_GLOBAL_FAIL
#define FIH_ENABLE_CFI

#elif defined(MCUBOOT_FIH_PROFILE_HIGH)
#define FIH_ENABLE_DELAY         /* Requires an entropy source */
#define FIH_ENABLE_DOUBLE_VARS
#define FIH_ENABLE_GLOBAL_FAIL
#define FIH_ENABLE_CFI

#else
#error "Invalid FIH Profile configuration"
#endif /* MCUBOOT_FIH_PROFILE */

/* Where possible, glue the FIH_TRUE from two components. */
#define FIH_TRUE_1              ((int32_t)0xC00AUL)
#define FIH_TRUE_2              ((int32_t)0x0350UL)
#define FIH_TRUE                ((int32_t)0xC35AUL) /* i.e., FIH_TRUE_1 | FIH_TRUE_2 */
#define FIH_FALSE               ((int32_t)0xA5C3UL)

#define FIH_POSITIVE_VALUE      ((int32_t) 0x5555AAAAUL)
#define FIH_NEGATIVE_VALUE      ((int32_t) 0xAAAA5555UL)

#ifdef FIH_ENABLE_DOUBLE_VARS
/*
 * A volatile mask is used to prevent compiler optimization - the mask is XORed
 * with the variable to create the backup and the integrity can be checked with
 * another xor. The mask value doesn't _really_ matter that much, as long as
 * it has reasonably high Hamming weight.
 */
#define FIH_MASK_VALUE          0xA5C35A3C
#define FIH_UINT_MASK_VALUE     0xA5C35A3CU

#define FIH_INT_VAL_MASK(val) ((int32_t)((val) ^ (int32_t)FIH_MASK_VALUE))
#define FIH_UINT_VAL_MASK(val) ((val) ^ FIH_UINT_MASK_VALUE)

/*
 * All ints are replaced with two int - the normal one and a backup which is
 * XORed with the mask.
 */
typedef struct {
    volatile int32_t val;
    volatile int32_t msk;
} fih_int;

#define FIH_INT_INIT(x)                 ((fih_int){(x), FIH_INT_VAL_MASK(x)})

/* FIH_INT_INIT_GLOBAL is created to declare global or static global veriables to 
  avoid the compile time Error[Pe028]: expression must have a constant value
  on IAR compiler */
#define FIH_INT_INIT_GLOBAL(x)          {(x), FIH_INT_VAL_MASK(x)}

typedef struct {
    volatile uint32_t val;
    volatile uint32_t msk;
} fih_uint;

#define FIH_UINT_INIT(x)                ((fih_uint){(x), FIH_UINT_VAL_MASK(x)})

/* FIH_UINT_INIT_GLOBAL is created to declare global or static global veriables to 
  avoid the compile time Error[Pe028]: expression must have a constant value
  on IAR compiler */
#define FIH_UINT_INIT_GLOBAL(x)         {(x), FIH_UINT_VAL_MASK(x)}

#else /* FIH_ENABLE_DOUBLE_VARS */
/*
 * GCC issues a warning: "type qualifiers ignored on function return type"
 * when return type is volatile. This is NOT an issue, and as a workaround
 * separate types for return values introduced
 */
typedef struct {
    volatile int32_t val;
} fih_int;

typedef struct {
    volatile uint32_t val;
} fih_uint;

#define FIH_INT_INIT(x)                 ((fih_int){(x)})

/* FIH_UINT_INIT_GLOBAL and FIH_INT_INIT_GLOBAL are created to declare global
  or static global veriables to avoid the compile time Error[Pe028]: expression
  must have a constant value on IAR compiler */
#define FIH_INT_INIT_GLOBAL(x)          {(x)}
#define FIH_UINT_INIT(x)                ((fih_uint){(x)})
#define FIH_UINT_INIT_GLOBAL(x)         {(x)}
#endif /* FIH_ENABLE_DOUBLE_VARS */

#define FIH_SUCCESS                     (FIH_INT_INIT(FIH_POSITIVE_VALUE))
#define FIH_FAILURE                     (FIH_INT_INIT(FIH_NEGATIVE_VALUE))
#define FIH_UINT_ZERO                   (FIH_UINT_INIT(0U))
#define FIH_INT_ZERO                    (FIH_INT_INIT(0L))
#define FIH_UINT_MAX                    (FIH_UINT_INIT(0xFFFFFFFFU))


#ifdef FIH_ENABLE_GLOBAL_FAIL
/**
 * Global failure handler - more resistant to unlooping. "noinline" and "used"
 * are used to prevent optimization.
 * NOTE: this failure handler shall be used as FIH specific error handling to
 * capture FI attacks.
 * Error handling in SPM and SP should be enhanced respectively.
 */
__attribute__((noinline))
__attribute__((noreturn))
void fih_panic_loop(void);
#define FIH_PANIC fih_panic_loop()
#else /* FIH_ENABLE_GLOBAL_FAIL */
#define FIH_PANIC  \
        do { \
            FIH_LABEL("FAILURE_LOOP"); \
            while (true) {} \
        } while (false)
#endif  /* FIH_ENABLE_GLOBAL_FAIL */

/*
 * NOTE: for functions to be inlined outside their compilation unit they have to
 * have the body in the header file. This is required as function calls are easy
 * to skip.
 */

#ifdef FIH_ENABLE_DELAY
/**
 * @brief Set up the RNG for use with random delays. Called once at startup.
 */
void fih_delay_init(void);

bool fih_delay(void);

#else /* FIH_ENABLE_DELAY */
/* NOOP */
#define fih_delay_init()

/* NOOP */
#define fih_delay() (true)
#endif /* FIH_ENABLE_DELAY */

#ifdef FIH_ENABLE_DOUBLE_VARS
/**
 * Validate fih_int for tampering.
 *
 * @param x  fih_int value to be validated.
 */
__attribute__((always_inline)) static inline
bool fih_int_validate(fih_int x)
{
    int32_t x_msk = x.msk;

    if (x.val != FIH_INT_VAL_MASK(x_msk)) {
        FIH_PANIC;
    }

    return true;
}

/**
 * Validate fih_uint for tampering.
 *
 * @param x  fih_uint value to be validated.
 */
__attribute__((always_inline)) static inline
bool fih_uint_validate(fih_uint x)
{
    uint32_t x_msk = x.msk;

    if (x.val != FIH_UINT_VAL_MASK(x_msk)) {
        FIH_PANIC;
    }

    return true;
}

/**
 * Convert a fih_int to an int. Validate for tampering.
 *
 * @param x  fih_int value to be converted.
 *
 * @return   Value converted to int.
 */
__attribute__((always_inline)) static inline
int32_t fih_int_decode(fih_int x)
{
    (void)fih_int_validate(x);
    return x.val;
}

/**
 * Convert a fih_uint to an unsigned int. Validate for tampering.
 *
 * @param x  fih_uint value to be converted.
 *
 * @return   Value converted to unsigned int.
 */
__attribute__((always_inline)) static inline
uint32_t fih_uint_decode(fih_uint x)
{
    (void)fih_uint_validate(x);
    return x.val;
}

/**
 * Convert an int to a fih_int, can be used to encode specific error codes.
 *
 * @param x  Integer value to be converted.
 *
 * @return   Value converted to fih_int.
 */
__attribute__((always_inline)) static inline
fih_int fih_int_encode(int32_t x)
{
    fih_int ret = FIH_INT_INIT(x);
    return ret;
}

/**
 * Convert an unsigned int to a fih_uint, can be used to encode specific error
 * codes.
 *
 * @param x  Unsigned integer value to be converted.
 *
 * @return   Value converted to fih_uint.
 */
__attribute__((always_inline)) static inline
fih_uint fih_uint_encode(uint32_t x)
{
    fih_uint ret = FIH_UINT_INIT(x);
    return ret;
}

/**
 * Standard equality for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x == y, false otherwise.
 */
#define fih_eq(x, y) \
        ( fih_int_validate(x) && \
          fih_int_validate(y) && \
          ((x).val == (y).val) && \
          fih_delay() && \
          ((x).msk == (y).msk) && \
          fih_delay() && \
          ((x).val == FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard equality for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x == y, false otherwise.
 */
#define fih_uint_eq(x, y) \
        ( fih_uint_validate(x) && \
          fih_uint_validate(y) && \
          ((x).val == (y).val) && \
          fih_delay() && \
          ((x).msk == (y).msk) && \
          fih_delay() && \
          ((x).val == FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard non-equality for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x != y, false otherwise.
 */
#define fih_not_eq(x, y) \
        ( fih_int_validate(x) && \
          fih_int_validate(y) && \
          ((x).val != (y).val) && \
          fih_delay() && \
          ((x).msk != (y).msk) && \
          fih_delay() && \
          ((x).val != FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard non-equality for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x != y, false otherwise.
 */
#define fih_uint_not_eq(x, y) \
        ( fih_uint_validate(x) && \
          fih_uint_validate(y) && \
          ((x).val != (y).val) && \
          fih_delay() && \
          ((x).msk != (y).msk) && \
          fih_delay() && \
          ((x).val != FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard greater than comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x > y, false otherwise.
 */
#define fih_gt(x, y) \
        ( fih_int_validate(x)  && \
          fih_int_validate(y) && \
          ((x).val > (y).val) && \
          fih_delay() && \
          (FIH_INT_VAL_MASK((x).msk) > FIH_INT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val > FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard greater than comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x > y, false otherwise.
 */
#define fih_uint_gt(x, y) \
        ( fih_uint_validate(x)  && \
          fih_uint_validate(y) && \
          ((x).val > (y).val) && \
          fih_delay() && \
          (FIH_UINT_VAL_MASK((x).msk) > FIH_UINT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val > FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard greater than or equal comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x >= y, false otherwise.
 */
#define fih_ge(x, y) \
        ( fih_int_validate(x)  && \
          fih_int_validate(y) && \
          ((x).val >= (y).val) && \
          fih_delay() && \
          (FIH_INT_VAL_MASK((x).msk) >= FIH_INT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val >= FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard greater than or equal comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x >= y, false otherwise.
 */
#define fih_uint_ge(x, y) \
        ( fih_uint_validate(x)  && \
          fih_uint_validate(y) && \
          ((x).val >= (y).val) && \
          fih_delay() && \
          (FIH_UINT_VAL_MASK((x).msk) >= FIH_UINT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val >= FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard less than comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x < y, false otherwise.
 */
#define fih_lt(x, y) \
        ( fih_int_validate(x)  && \
          fih_int_validate(y) && \
          ((x).val < (y).val) && \
          fih_delay() && \
          (FIH_INT_VAL_MASK((x).msk) < FIH_INT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val < FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard less than comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x < y, false otherwise.
 */
#define fih_uint_lt(x, y) \
        ( fih_uint_validate(x)  && \
          fih_uint_validate(y) && \
          ((x).val < (y).val) && \
          fih_delay() && \
          (FIH_UINT_VAL_MASK((x).msk) < FIH_UINT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val < FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard less than or equal comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x <= y, false otherwise.
 */
#define fih_le(x, y) \
        ( fih_int_validate(x)  && \
          fih_int_validate(y) && \
          ((x).val <= (y).val) && \
          fih_delay() && \
          (FIH_INT_VAL_MASK((x).msk) <= FIH_INT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val <= FIH_INT_VAL_MASK((y).msk)) \
        )

/**
 * Standard less than or equal comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x <= y, false otherwise.
 */
#define fih_uint_le(x, y) \
        ( fih_uint_validate(x)  && \
          fih_uint_validate(y) && \
          ((x).val <= (y).val) && \
          fih_delay() && \
          (FIH_UINT_VAL_MASK((x).msk) <= FIH_UINT_VAL_MASK((y).msk)) && \
          fih_delay() && \
          ((x).val <= FIH_UINT_VAL_MASK((y).msk)) \
        )

/**
 * Standard logical OR for fih_uint values.
 *
 * @param x  1st fih_uint value to be ORed.
 * @param y  2nd fih_uint value to be ORed.
 *
 * @return   ORed value
 */
__attribute__((always_inline)) static inline
fih_uint fih_uint_or(fih_uint x, fih_uint y)
{
    /* Use local variable to avoid persistent side effect MISRA violation
     * in operations with volatile variables. */
    uint32_t y_val = y.val;
    uint32_t y_msk = y.msk;
    volatile fih_uint rc = {0};

    rc.val = x.val | y_val;

    rc.msk = FIH_UINT_VAL_MASK(FIH_UINT_VAL_MASK(x.msk) | FIH_UINT_VAL_MASK(y_msk));

    return rc;
}

/**
 * Standard logical OR for fih_int values.
 *
 * @param x  1st fih_int value to be ORed.
 * @param y  2nd fih_int value to be ORed.
 *
 * @return   ORed value
 */
__attribute__((always_inline)) static inline
fih_int fih_or(fih_int x, fih_int y)
{
    /* Use local variable to avoid persistent side effect MISRA violation
     * in operations with volatile variables. */
    int32_t y_val = y.val;
    int32_t y_msk = y.msk;
    volatile fih_int rc = {0};

    rc.val = x.val | y_val;

    rc.msk = FIH_INT_VAL_MASK(FIH_INT_VAL_MASK(x.msk) | FIH_INT_VAL_MASK(y_msk));

    return rc;
}

/**
 * Standard logical AND for fih_uint values.
 *
 * @param x  1st fih_uint value to be ORed.
 * @param y  2nd fih_uint value to be ORed.
 *
 * @return   ANDed value
 */
__attribute__((always_inline)) static inline
fih_uint fih_uint_and(fih_uint x, fih_uint y)
{
    /* Use local variable to avoid persistent side effect MISRA violation
     * in operations with volatile variables. */
    uint32_t y_val = y.val;
    uint32_t y_msk = y.msk;
    volatile fih_uint rc = {0};

    rc.val = x.val & y_val;

    rc.msk = FIH_UINT_VAL_MASK(FIH_UINT_VAL_MASK(x.msk) & FIH_UINT_VAL_MASK(y_msk));

    return rc;
}

#else /* FIH_ENABLE_DOUBLE_VARS */

/* NOOP */
#define fih_int_validate(x)  (true)
#define fih_uint_validate(x) (true)

/* NOOP */
#define fih_int_decode(x)       ((x).val)
#define fih_uint_decode(x)      ((x).val)

/* NOOP */
#define fih_int_encode(x)       (FIH_INT_INIT(x))
#define fih_uint_encode(x)      (FIH_UINT_INIT(x))

/**
 * Standard equality for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x == y, false otherwise.
 */
#define fih_eq(x, y) \
        ( ((x).val == (y).val) && \
          fih_delay() && \
          !((x).val != (y).val) \
        )

/**
 * Standard equality for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x == y, false otherwise.
 */
#define fih_uint_eq(x, y) \
        ( ((x).val == (y).val) && \
          fih_delay() && \
          !((x).val != (y).val) \
        )

/**
 * Standard non-equality for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return    true if x != y, false otherwise.
 */
#define fih_not_eq(x, y) \
        ( ((x).val != (y).val) && \
          fih_delay() && \
          !((x).val == (y).val) \
        )

/**
 * Standard non-equality for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return    true if x != y, false otherwise.
 */
#define fih_uint_not_eq(x, y) \
        ( ((x).val != (y).val) && \
          fih_delay() && \
          !((x).val == (y).val) \
        )
/**
 * Standard greater than comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x > y, false otherwise.
 */
#define fih_gt(x, y) \
        ( ((x).val > (y).val) && \
          fih_delay() && \
          !((x).val <= (y).val) \
        )
/**
 * Standard greater than comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x > y, false otherwise.
 */
#define fih_uint_gt(x, y) \
        ( ((x).val > (y).val) && \
          fih_delay() && \
          !((x).val <= (y).val) \
        )
/**
 * Standard greater than or equal comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x >= y, false otherwise.
 */
#define fih_ge(x, y) \
        ( ((x).val >= (y).val) && \
          fih_delay() && \
          !((x).val < (y).val) \
        )

/**
 * Standard greater than or equal comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x >= y, false otherwise.
 */
#define fih_uint_ge(x, y) \
        ( ((x).val >= (y).val) && \
          fih_delay() && \
          !((x).val < (y).val) \
        )
/**
 * Standard less than comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x < y, false otherwise.
 */
#define fih_lt(x, y) \
        ( ((x).val < (y).val) && \
          fih_delay() && \
          !((x).val >= (y).val) \
        )

/**
 * Standard less than comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x < y, false otherwise.
 */
#define fih_uint_lt(x, y) \
        ( ((x).val < (y).val) && \
          fih_delay() && \
          !((x).val >= (y).val) \
        )
/**
 * Standard less than or equal comparison for fih_int values.
 *
 * @param x  1st fih_int value to be compared.
 * @param y  2nd fih_int value to be compared.
 *
 * @return   true if x <= y, false otherwise.
 */
#define fih_le(x, y) \
        ( ((x).val <= (y).val) && \
          fih_delay() && \
          !((x).val > (y).val) \
        )

/**
 * Standard less than or equal comparison for fih_uint values.
 *
 * @param x  1st fih_uint value to be compared.
 * @param y  2nd fih_uint value to be compared.
 *
 * @return   true if x <= y, false otherwise.
 */
#define fih_uint_le(x, y) \
        ( ((x).val <= (y).val) && \
          fih_delay() && \
          !((x).val > (y).val) \
        )

/**
 * Standard logical OR for fih_uint values.
 *
 * @param x  1st fih_uint value to be ORed.
 * @param y  2nd fih_uint value to be ORed.
 *
 * @return   ORed value
 */

__attribute__((always_inline)) static inline
fih_uint fih_uint_or(fih_uint x, fih_uint y)
{
    fih_uint rc = {x.val | y.val};

    (void)fih_delay();

    if (rc.val != (x.val | y.val)) {
        FIH_PANIC;
    }

    return rc;
}

/**
 * Standard logical OR for fih_int values.
 *
 * @param x  1st fih_int value to be ORed.
 * @param y  2nd fih_int value to be ORed.
 *
 * @return   ORed value
 */
__attribute__((always_inline)) static inline
fih_int fih_or(fih_int x, fih_int y)
{
    fih_int rc = {x.val | y.val};

    (void)fih_delay();

    if (rc.val != (x.val | y.val)) {
        FIH_PANIC;
    }

    return rc;
}

/**
 * Standard logical AND for fih_uint values.
 *
 * @param x  1st fih_uint value to be ORed.
 * @param y  2nd fih_uint value to be ORed.
 *
 * @return   ANDed value
 */
__attribute__((always_inline)) static inline
fih_uint fih_uint_and(fih_uint x, fih_uint y)
{
    fih_uint rc = {x.val & y.val};

    (void)fih_delay();

    if (rc.val != (x.val & y.val)) {
        FIH_PANIC;
    }

    return rc;
}

#endif /* FIH_ENABLE_DOUBLE_VARS */

/**
 * C has a common return pattern where 0 is a correct value and all others are
 * errors.  This function converts 0 to FIH_SUCCESS, and any other number to a
 * value that is not FIH_SUCCESS.
 *
 * @param x  Return code to be checked.
 *
 * @return   FIH_SUCCESS if x == 0, FIH_FAILURE otherwise.
 */
__attribute__((always_inline)) static inline
fih_int fih_int_encode_zero_equality(int32_t x)
{
    if (x != 0) {
        return FIH_FAILURE;
    } else {
        return FIH_SUCCESS;
    }
}

#ifdef FIH_ENABLE_CFI
/* Global Control Flow Integrity counter */
extern fih_uint fih_cfi_ctr;

/**
 * Increment the CFI counter by input counter and return the value before the
 * increment.
 * NOTE: this function shall not be called directly.
 *
 * @param x  Increment value.
 *
 * @return   Previous value of the CFI counter.
 */
fih_uint fih_cfi_get_and_increment(uint8_t cnt);

/**
 * Validate that the saved precall value is the same as the value of the global
 * counter. For this to be the case, a fih_ret must have been called between
 * these functions being executed. If the values aren't the same then panic.
 * NOTE: this function shall not be called directly.
 *
 * @param saved  Saved value.
 */
void fih_cfi_validate(fih_uint saved);

/**
 * Decrement the global CFI counter by one, so that it has the same value as
 * before the cfi_precall.
 * NOTE: this function shall not be called directly.
 */
void fih_cfi_decrement(void);

/*
 * Macro wrappers for functions - Even when the functions have zero body this
 * saves a few bytes on noop functions as it doesn't generate the call/ret
 *
 * CFI precall function saves the CFI counter and then increments it - the
 * postcall then checks if the counter is equal to the saved value. In order for
 * this to be the case a FIH_RET must have been performed inside the called
 * function in order to decrement the counter, so the function must have been
 * called.
 */
#define FIH_CFI_PRECALL_BLOCK \
        fih_uint fih_cfi_precall_saved_value = fih_cfi_get_and_increment(1u)

#define FIH_CFI_POSTCALL_BLOCK \
        fih_cfi_validate(fih_cfi_precall_saved_value)

#define FIH_CFI_PRERET \
        fih_cfi_decrement()

/*
 * Marcos to support protect the control flow integrity inside a function.
 *
 * The FIH_CFI_PRECALL_BLOCK/FIH_CFI_POSTCALL_BLOCK pair mainly protect function
 * calls from fault injection. Fault injection may attack a function to skip its
 * critical steps which are not function calls. It is difficult for the caller
 * to dectect the injection as long as the function successfully returns.
 *
 * The following macros can be called in a function to track the critical steps,
 * especially those which are not function calls.
 */

/*
 * FIH_CFI_STEP_INIT() saves the CFI counter and increase the CFI counter by the
 * number of the critical steps. It should be called before execution starts.
 */
#define FIH_CFI_STEP_INIT(x) \
        fih_uint fih_cfi_step_saved_value = fih_cfi_get_and_increment(x)

/*
 * FIH_CFI_STEP_DECREMENT() decrease the CFI counter by one. It can be called
 * after each critical step execution completes.
 */
#define FIH_CFI_STEP_DECREMENT() \
        fih_cfi_decrement()

/*
 * FIH_CFI_STEP_ERR_RESET() resets the CFI counter to the previous value saved
 * by FIH_CFI_STEP_INIT(). It shall be called only when a functionality error
 * occurs and forces the function to exit. It can enable the caller to capture
 * the functionality error other than being trapped in fault injection error
 * handling.
 */
#define FIH_CFI_STEP_ERR_RESET() \
        do { \
            fih_cfi_ctr = fih_cfi_step_saved_value; \
            (void)fih_int_validate(fih_cfi_ctr); \
        } while(0)

#else /* FIH_ENABLE_CFI */
#define FIH_CFI_PRECALL_BLOCK
#define FIH_CFI_POSTCALL_BLOCK
#define FIH_CFI_PRERET

#define FIH_CFI_STEP_INIT(x)
#define FIH_CFI_STEP_DECREMENT()
#define FIH_CFI_STEP_ERR_RESET()
#endif /* FIH_ENABLE_CFI */

/*
 * Label for interacting with FIH testing tool. Can be parsed from the elf file
 * after compilation. Does not require debug symbols.
 */
#define FIH_LABEL(str) __asm volatile ("FIH_LABEL_" str "_0_%=:" ::)
#define FIH_LABEL_CRITICAL_POINT() FIH_LABEL("FIH_CRITICAL_POINT")

/*
 * Main FIH calling macro. return variable is second argument. Does some setup
 * before and validation afterwards. Inserts labels for use with testing script.
 *
 * First perform the precall step - this gets the current value of the CFI
 * counter and saves it to a local variable, and then increments the counter.
 *
 * Then set the return variable to FIH_FAILURE as a base case.
 *
 * Then perform the function call. As part of the function FIH_RET must be
 * called which will decrement the counter.
 *
 * The postcall step gets the value of the counter and compares it to the
 * previously saved value. If this is equal then the function call and all child
 * function calls were performed.
 */
#define FIH_CALL(f, ret, ...) \
    do { \
        FIH_LABEL("FIH_CALL_START_" # f); \
        FIH_CFI_PRECALL_BLOCK; \
        (ret) = FIH_FAILURE; \
        (void)fih_delay(); \
        (ret) = (f)(__VA_ARGS__); \
        FIH_CFI_POSTCALL_BLOCK; \
        (void)fih_int_validate(ret); \
        FIH_LABEL("FIH_CALL_END"); \
    } while (false)

/*
 * Similar to FIH_CALL, but return value is ignored, like (void)f(...)
 */
#define FIH_VOID(f, ...) \
    do { \
        FIH_CFI_PRECALL_BLOCK; \
        (void)fih_delay(); \
        (void)(f)(__VA_ARGS__); \
        FIH_CFI_POSTCALL_BLOCK; \
        FIH_LABEL("FIH_CALL_END"); \
    } while (false)

/*
 * Similar to FIH_CALL, but ret is fih_uint instead of fih_int.
 * NOTE: intended use is bit masks, so initialized by 0 instead of FIH_FAILURE!
 */
#define FIH_UCALL(f, ret, ...) \
    do { \
        FIH_LABEL("FIH_CALL_START_" # f); \
        FIH_CFI_PRECALL_BLOCK; \
        (ret) = FIH_UINT_ZERO; \
        (void)fih_delay(); \
        (ret) = (f)(__VA_ARGS__); \
        FIH_CFI_POSTCALL_BLOCK; \
        (void)fih_uint_validate(ret); \
        FIH_LABEL("FIH_CALL_END"); \
    } while (false)

/*
 * FIH return changes the state of the internal state machine. If you do a
 * FIH_CALL then you need to do a FIH_RET else the state machine will detect
 * tampering and panic.
 */
#define FIH_RET(ret) \
    do { \
        FIH_CFI_PRERET; \
        return ret; \
    } while (false)

#else /* MCUBOOT_FIH_PROFILE_ON */
typedef int32_t fih_int;
typedef uint32_t fih_uint;

typedef fih_int fih_int;
typedef fih_uint fih_uint;


/* FIH_UINT_INIT_GLOBAL and FIH_INT_INIT_GLOBAL are created to declare global 
  or static global veriables to avoid the compile time Error[Pe028]: expression
  must have a constant value on IAR compiler */
#define FIH_INT_INIT(x)         (x)
#define FIH_INT_INIT_GLOBAL(x)  (x)
#define FIH_UINT_INIT(x)        (x)
#define FIH_UINT_INIT_GLOBAL(x) (x)

#define FIH_SUCCESS             (0)
#define FIH_FAILURE            (-1)
#define FIH_UINT_ZERO           (0UL)
#define FIH_INT_ZERO            ((signed)0)
#define FIH_UINT_MAX            (0xFFFFFFFFUL)

#define FIH_TRUE                (1)
#define FIH_FALSE               (0)

#define fih_int_validate(x)
#define fih_uint_validate(x)

#define fih_int_decode(x)       (x)
#define fih_uint_decode(x)      (x)

#define fih_int_encode(x)       (x)
#define fih_uint_encode(x)      (x)

#define fih_int_encode_zero_equality(x) ((x) == 0 ? 0 : 1)

#define fih_eq(x, y)            ((x) == (y))
#define fih_uint_eq(x, y)       ((x) == (y))

#define fih_not_eq(x, y)        ((x) != (y))
#define fih_uint_not_eq(x, y)   ((x) != (y))

#define fih_gt(x, y)            ((x) > (y))
#define fih_uint_gt(x, y)       ((x) > (y))

#define fih_ge(x, y)            ((x) >= (y))
#define fih_uint_ge(x, y)       ((x) >= (y))

#define fih_lt(x, y)            ((x) < (y))
#define fih_uint_lt(x, y)       ((x) < (y))

#define fih_le(x, y)            ((x) <= (y))
#define fih_uint_le(x, y)       ((x) <= (y))

#define fih_or(x, y)            ((x) | (y))
#define fih_uint_or(x, y)       ((x) | (y))
#define fih_uint_and(x, y)      ((x) & (y))

#define fih_delay_init()        (0)
#define fih_delay()

#define FIH_CALL(f, ret, ...) \
    do { \
        (ret) = (f)(__VA_ARGS__); \
    } while (false)

#define FIH_VOID(f, ...) \
    do { \
        (void)(f)(__VA_ARGS__); \
    } while (false)

#define FIH_UCALL(f, ret, ...) \
    do { \
        (ret) = (f)(__VA_ARGS__); \
    } while (false)

#define FIH_RET(ret) \
    do { \
        return ret; \
    } while (false)

#define FIH_PANIC do { \
        while (true) {} \
    } while (false)

#define FIH_CFI_STEP_INIT(x)
#define FIH_CFI_STEP_DECREMENT()
#define FIH_CFI_STEP_ERR_RESET()

#define FIH_LABEL_CRITICAL_POINT()

#endif /* MCUBOOT_FIH_PROFILE_ON */

#ifdef __cplusplus
}
#endif /* __cplusplus */

FIH_MISRA_BLOCK_END('MISRA C-2012 Rule 10.1')
FIH_MISRA_BLOCK_END('MISRA C-2012 Rule 10.4')

#endif /* FAULT_INJECTION_HARDENING_H */
