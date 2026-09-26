# Submitting Patches to the Fault Injection Hardening Library

This document provides detailed guidelines for submitting patches and pull requests.

## Before You Start

1. **Check the Issue Tracker** — Look for related issues or existing PRs
2. **Discuss Major Changes** — Open an issue first to get feedback on significant changes
3. **Understand the Scope** — Familiarize yourself with the FIH library architecture

## Patch Submission Process

### Step 1: Fork and Create a Branch

```bash
git clone https://github.com/your-username/mcuboot.git
cd mcuboot
git checkout -b feature/my-feature
```

Branch naming conventions:
- `fix/issue-description` — Bug fixes
- `feature/feature-name` — New features
- `docs/documentation-update` — Documentation only
- `refactor/component-name` — Code refactoring

### Step 2: Make Your Changes

#### Code Changes

- Keep changes focused and atomic
- One logical change per commit
- Write descriptive commit messages (see below)
- Test frequently during development

#### Commit Messages

Good commit messages:
```
component: Brief one-line summary (50 chars max)

Longer explanation of the change, wrapped at 72 characters.
Explain the problem being solved and why this approach was chosen.

- Use bullet points for multiple related changes
- Reference issues: Fixes #123
- Reference related commits if relevant

Signed-off-by: Your Name <your.email@example.com>
```

### Step 3: Test Your Changes

#### Compilation Testing

```bash
# Test with different compilers
gcc -Wall -Wextra -std=c99 ...
clang -Wall -Wextra -std=c99 ...

# Test all FIH profiles
# Compile with: -DFIH_PROFILE_HIGH, -DFIH_PROFILE_MEDIUM, etc.
```

#### Functional Testing

1. **Unit Tests**: Run existing tests
   ```bash
   cd build && make test
   ```

2. **Integration Testing**: Verify with MCUboot
   - Build MCUboot with your changes
   - Run MCUboot test suite
   - Test on target hardware if possible

3. **Profile Testing**: Test with all FIH profiles
   - HIGH (all protections)
   - MEDIUM (no delay)
   - LOW (basic)
   - OFF (disabled)

### Step 4: Format and Style Checks

Before committing:

```bash
# Check for style issues
# (project uses existing C89/C99 conventions)

# Ensure SPDX headers are present
grep -n "SPDX-License-Identifier" your_file.c

# Check line lengths (aim for <120 chars)
awk 'length > 120 {print NR": "length": "$0}' your_file.c
```

### Step 5: Create a Pull Request

#### PR Title
- Clear, descriptive (50 characters or less)
- Format: `component: Brief description`
- Examples:
  - `fih: Fix FIH_EQ comparison for high profile`
  - `docs: Add delay RNG integration guide`
  - `build: Update CMakeLists.txt for standalone compilation`

#### PR Description

```markdown
## Description
Brief explanation of what this PR does.

## Motivation
Why this change is needed.

## Testing
- [ ] Compiled with GCC
- [ ] Compiled with Clang
- [ ] Tested with FIH_PROFILE_HIGH
- [ ] Tested with FIH_PROFILE_OFF
- [ ] Existing tests pass
- [ ] Manual testing completed

## Checklist
- [ ] SPDX license header added/updated
- [ ] Comments added for complex logic
- [ ] No breaking changes to public API
- [ ] Documentation updated if needed
- [ ] Commit messages are clear

## Related Issues
Fixes #123
Related to #456
```

## Special Considerations

### Performance-Critical Changes

If your change affects performance:
- Include benchmark results (if applicable)
- Explain trade-offs between protection and performance
- Note any overhead introduced
- Consider impact of different FIH profiles

### Security-Related Changes

If your change affects security:
- See [SECURITY.md](docs/SECURITY.md)
- Report privately if it's a vulnerability discovery
- Include security review notes in PR

### Platform-Specific Changes

If supporting a new platform:
- Clearly document platform assumptions
- Add platform guards where needed
- Include testing notes for that platform
- Don't break existing platforms

### API Changes

If you modify the public API:
- Maintain backward compatibility if possible
- Document breaking changes clearly
- Update all affected code samples
- Consider deprecation periods for removals

## Code Review Process

### What to Expect

1. **Initial Review** (1-3 days)
   - Automated checks run
   - Maintainers review for scope and approach

2. **Technical Review** (3-7 days)
   - Code quality and standards review
   - Testing validation
   - Security considerations

3. **Discussion** (as needed)
   - Questions about design decisions
   - Requests for changes or clarifications
   - Suggestions for improvements

4. **Approval and Merge**
   - Once all feedback is addressed
   - Tests pass successfully
   - PR is approved by maintainers

### Responding to Feedback

- **Be respectful** — Code review is about the code, not the person
- **Ask for clarification** — If feedback is unclear, ask questions
- **Provide rationale** — Explain your design decisions
- **Be willing to change** — Sometimes reviewers have good points
- **Respond promptly** — Timely responses keep the review moving

## Common Patch Types

### Bug Fixes
- Must reference the issue being fixed
- Include a minimal test case if possible
- Document the root cause in the commit message

### Feature Additions
- Discuss design in an issue first (if major)
- Include documentation
- Add tests for new functionality
- Update examples if applicable

### Documentation Improvements
- Clear before/after comparison
- Explain any terminology changes
- Check for consistency with other docs

### Performance Optimizations
- Include performance measurements
- Explain the optimization approach
- Note any platform dependencies
- Don't sacrifice readability without good reason

## Signing Off

All commits should include a "Signed-off-by" line:

```bash
git commit -s -m "component: Your commit message"
```

This confirms you have the right to submit the code and agree to the Apache 2.0 license.

## Troubleshooting

### PR is not building
- Check the CI logs for specific errors
- Run local tests matching the CI environment
- Ensure all dependencies are available

### Feedback is overwhelming
- Break the PR into smaller changes
- Ask reviewers to prioritize feedback
- Focus on addressing critical issues first

### Disagreement with feedback
- Discuss respectfully
- Provide technical rationale
- Ask for clarification if needed
- Escalate to maintainers if necessary

## Questions?

- Check the [Contributing Guide](contributing.md)
- Review existing PRs and issues for examples
- Ask in a new issue for clarification

Thank you for contributing!
