# Security Policy

## Reporting Security Vulnerabilities

The security of the Fault Injection Hardening Library is important to us. If you discover a security vulnerability in this project, please report it responsibly.

### How to Report

**Do not open a public issue** for security vulnerabilities.

Instead, please email your report to the MCUboot security team at:
- **mcuboot-security@arm.com** (primary)
- Or report through the upstream [MCUboot project](https://github.com/mcu-tools/mcuboot/security)

Include the following information:
- Description of the vulnerability
- Steps to reproduce the issue
- Potential impact
- Any suggested fixes (if you have them)

### What to Expect

- We will acknowledge receipt of your report within 48 hours
- We will investigate and determine the severity
- We will work on a fix and coordinate a responsible disclosure
- We will credit you in the security advisory (unless you prefer anonymity)

## Security Considerations

When using the FIH library, keep in mind:

1. **These are mitigations, not guarantees** — FIH hardening significantly increases the difficulty of successful fault injection attacks, but does not guarantee immunity from all attacks.

2. **Compiler-dependent** — Effectiveness can vary by compiler, optimization level, and platform. Test on your target hardware with your toolchain.

3. **Combine with hardware defenses** — Use FIH alongside hardware-level protections such as:
   - Tamper detection sensors
   - Glitch filters
   - Power supply monitoring
   - Physical shielding (where applicable)

4. **Profile selection matters** — Choose the appropriate FIH profile based on your security requirements:
   - `HIGH`: Maximum protection, highest overhead
   - `MEDIUM`: Balanced protection and performance
   - `LOW`: Minimal overhead, basic protection
   - `OFF`: Disabled (for development/testing only)

5. **Entropy source quality** — When using `FIH_PROFILE_HIGH`, the quality of random delays depends on your RNG implementation.

6. **Testing** — Validate effectiveness through fault injection testing tools and hardware testing on your target platform.

## Responsible Disclosure Timeline

We follow a responsible disclosure process:

1. **Private Report** → Immediate acknowledgment (48 hours)
2. **Investigation** → 5-10 business days
3. **Fix Development** → 2-4 weeks (depending on severity)
4. **Coordinated Release** → Public advisory and patch release simultaneously
5. **Credit** → Security advisory acknowledges reporter (with permission)

## Past Security Issues

For information about previously discovered and fixed security issues, see the [MCUboot Security Advisories](https://github.com/mcu-tools/mcuboot/security/advisories).

## Security Resources

- [OWASP: Fault Injection](https://owasp.org/www-community/attacks/Fault_injection)
- [ARM: Fault Injection Attacks](https://developer.arm.com/)
- [MCUboot Documentation](https://mcuboot.readthedocs.io/)
