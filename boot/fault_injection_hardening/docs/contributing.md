# Contributing to the Fault Injection Hardening Library

Thank you for your interest in contributing! We welcome contributions including:
- Bug reports and fixes
- Feature enhancements
- Documentation improvements
- Platform ports and optimizations
- Testing and validation

## Getting Started

1. **Check existing issues** — Before starting work, check if your issue/feature is already being discussed
2. **Discuss major changes** — For significant changes, open an issue first to get feedback
3. **Fork and branch** — Create a feature branch for your work

## Development Setup

### Prerequisites

- C99 compatible compiler (GCC, Clang, IAR, MSVC)
- CMake 3.12+ (for standalone builds)
- Git

### Building

```bash
mkdir build
cd build
cmake ..
make
```

## Coding Standards

### Style Guidelines

- **Indentation**: Use the existing style (typically 4 spaces or tabs)
- **Comments**: Add comments for non-obvious logic, especially in fault-hardened code
- **Naming**: Use clear, descriptive names for variables and functions
- **Line length**: Keep lines under 120 characters where practical
- **SPDX Headers**: Include SPDX license identifier in all source files

Example file header:
```c
/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Your Organization
 *
 * Description of what this file does.
 */
```

### Compiler Support

Code should compile cleanly with:
- GCC (versions 7+)
- Clang (versions 5+)
- IAR Embedded Workbench
- MSVC (with appropriate platform support)

Avoid compiler-specific extensions unless necessary, and guard them appropriately.

## Testing

### Before Submitting

Test your changes with:

1. **Multiple Compilers**: At minimum GCC and Clang
2. **All Profiles**: Test with all FIH profiles (HIGH, MEDIUM, LOW, OFF)
3. **Existing Tests**: Ensure no regressions in existing tests

### Adding Tests

- Unit tests go in a `tests/` subdirectory (following the CMakeLists.txt hook)
- Include test documentation explaining:
  - What is being tested
  - Why the test is needed
  - How to run the test

## Submitting Changes

### Pull Request Process

1. **Create a descriptive PR**:
   - Clear title describing the change
   - Detailed description of what and why
   - Link to related issues

2. **Commit messages** should be clear:
   ```
   component: Brief description

   Longer explanation if needed. Reference issues with #123.
   ```

3. **Documentation**:
   - Update README or docs if adding features
   - Add copyright headers to new files
   - Include license and attribution as appropriate

4. **Code review**:
   - Be responsive to feedback
   - Discuss disagreements respectfully
   - Make requested changes promptly

### Expectations

All contributions must:
- [ ] Pass compilation checks
- [ ] Include appropriate comments/documentation
- [ ] Not break existing functionality
- [ ] Follow the project's code style
- [ ] Include SPDX license headers
- [ ] Be under Apache License 2.0

## Reporting Issues

### Bug Reports

Include:
- Clear title and description
- Steps to reproduce
- Expected vs. actual behavior
- Compiler and platform information
- Relevant code or configuration

### Feature Requests

Include:
- Use case and motivation
- Proposed design (if applicable)
- Potential impact on existing code
- Suggested implementation (optional)

## License

By contributing to this project, you agree that your contributions will be licensed under the Apache License 2.0.

## Code of Conduct

Participants are expected to uphold the [Contributor Covenant Code of Conduct](../CODE_OF_CONDUCT.md).

## Questions?

- Open an issue for public questions
- For security concerns, see [SECURITY.md](SECURITY.md)
- Contact the MCUboot community for general questions

## Recognition

Contributors are recognized in:
- Commit history
- Release notes for substantial contributions
- Project documentation (if applicable)

Thank you for making FIH better!
