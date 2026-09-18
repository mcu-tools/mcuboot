# Fault Injection Hardening Library

A portable C library providing mitigations against fault injection attacks on cryptographic operations and other security-critical code.

## Overview

This library provides a set of compiler-independent mechanisms to harden code against fault injection attacks, which attempt to cause computational errors through physical manipulation (such as electromagnetic pulses or precision timing attacks). It offers configurable levels of protection:

- **High Profile** (`FIH_PROFILE_HIGH`): Enables delay randomization, double-variable encoding, global failure loops, and control flow integrity checking
- **Medium Profile** (`FIH_PROFILE_MEDIUM`): Enables double-variable encoding, global failure loops, and control flow integrity checking (no delay randomization)
- **Low Profile** (`FIH_PROFILE_LOW`): Enables global failure loops and control flow integrity checking
- **Off** (`FIH_PROFILE_OFF`): All protections disabled (default if not configured)

## Features

### FIH_ENABLE_DOUBLE_VARS
Represents critical variables as tuples `(value, value ^ mask)` to detect bit flips. Provides both basic storage and comparison operations that validate integrity.

### FIH_ENABLE_GLOBAL_FAIL
Redirects all failure paths to a global, hardened loop resistant to unlooping attacks. More difficult for an attacker to escape than simple inline `while(1)` loops.

### FIH_ENABLE_CFI (Control Flow Integrity)
Maintains a global counter incremented before and decremented after sensitive function calls. Post-call validation ensures the function was executed and not skipped via glitch attacks.

### FIH_ENABLE_DELAY
Introduces random delays using an external entropy source (RNG) to make precise fault injection timing much harder.

## Usage

Include the main header:

```c
#include "fault_injection_hardening.h"
```

The library expects `fih_config.h` to be available on the include path, which should define one of `FIH_PROFILE_*` macros. For MCUboot, this is provided by [boot/bootutil/include/fih_config.h](../bootutil/include/fih_config.h).

For non-MCUboot projects, provide your own `fih_config.h`:

```c
#ifndef __FIH_CONFIG_H__
#define __FIH_CONFIG_H__

/* Define your desired FIH profile */
#define FIH_PROFILE_HIGH

#endif
```

### Example: Protecting a Function

```c
#include "fault_injection_hardening.h"

fih_ret validate_signature(const uint8_t *sig, const uint8_t *key) {
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    /* Perform validation */
    int result = crypto_verify(sig, key);

    /* Encode result as FIH value */
    FIH_SET(fih_rc, fih_ret_encode_zero_equality(result));

    FIH_RET(fih_rc);
}

int main(void) {
    FIH_DECLARE(result, FIH_FAILURE);

    /* Call with FIH protection and validation */
    FIH_CALL(validate_signature, result, sig_data, key_data);

    /* Check result with fault-injection-resistant comparison */
    if (FIH_NOT_EQ(result, FIH_SUCCESS)) {
        FIH_PANIC;  /* Hardened panic loop */
    }

    return 0;
}
```

## Integration

### With Delay Randomization

If using `FIH_PROFILE_HIGH`, the library requires an entropy source for random delays. Implement `fih_delay_random_uchar()` to provide random bytes:

```c
unsigned char fih_delay_random_uchar(void) {
    return get_random_byte_from_rng();
}
```

### Return Values

Use `fih_ret_encode_zero_equality()` to convert standard C return patterns:

```c
fih_ret my_function(void) {
    int result = do_work();  /* 0 = success, non-zero = error */
    FIH_RET(fih_ret_encode_zero_equality(result));
}
```

## Compatibility

- **C Standard**: C99 and later
- **Compilers**: GCC, Clang, IAR Embedded Workbench, MSVC (with appropriate builtin support)
- **Architecture**: Architecture-independent (portable C)

## Notes

- These mitigations are **not guaranteed to be secure** for all compilers, but they increase resilience against fault injection significantly.
- The effectiveness depends on compiler behavior and optimization levels.
- Always pair with hardware-level protections where available.
- Testing and fault injection simulation tools are recommended to verify effectiveness.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

## References

The original development of this library was part of the MCUboot secure bootloader project. For more information on MCUboot, visit [mcuboot.com](http://mcuboot.com/).
