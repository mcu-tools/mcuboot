# Contributing to the MCUboot project

When contributing to MCUboot, please first discuss the changes you
wish to make via issue, email or Discord.

## Pull Request Process

1. Please ensure that the simulator tests pass.  If you are modifying
   existing functionality, it may be sufficient to just ensure that
   these tests pass.  If you are adding new functionality, you must
   also extend the simulator to test this new functionality.
1. Ensure your changes follow the coding and style conventions for the
   files you are modifying (see below).
1. If you wish (or are required to), be sure that copyright statements
   are up to date.  Note that we follow the general guidance used in
   the Linux kernel, and adding a copyright statement is generally
   only acceptable for "significant" changes to a file.
1. Be sure that all of your commits are signed off.  Contributions
   must be released under the Apache 2.0 license, following the
   [Developer Certificate of
   Origin](https://developercertificate.org/).
1. All contributions should follow the project code of conduct found
   in the root of the project directory.
1. Pull requests will trigger various builds and tests.  These must
   all pass.
1. Non-trivial changes must have a release-note stub (see below).

## Coding style and guidelines

### Mynewt coding

A majority of the code in this project is written in C, and intended
to run on embedded devices.  This code began as a part of the mynewt
RTOS, and still follows most of the [coding
conventions](https://github.com/apache/mynewt-core/blob/master/CODING_STANDARDS.md)
of that project.  The primary exception is the copyright header, which
is not appropriate as this is not an Apache project.

For files that came from MyNewt, we keep the existing Apache header,
and add an additional header for MCUboot.  In general, the following
is sufficient.

```c
/*
 * SPDX-License-Identifier: Apache-2.0
 */
```

this can be followed by individual contributor copyrights, if that is
necessary.

### General guidelines

Although MCUboot needs to be small and compact, please try to hold
back on the use of ifdefs in the code.  It is better to factor out
common code, and have different features in separate files, protected
by a single ifdef than to have a large function with ifdefs sprinkled
throughout it.

## Simulator coding

The simulator `sim` is an important part of what makes mcuboot reliable and
robust.  As such, it is important to make sure that changes made to the code are
being tested by the simulator.

The simulator puts the boot code through an intense stress test, testing
thousands of possibly failures (including numerous power failure conditions).
It would take very long, and be very difficult to test these cases on target,
and most devices would wear out after just a few runs.

As such, development should primarily be done using the simulator.  Testing on
actual targets should be done _after_ the code is working in the simulator, as a
sanity test that it really works.

Because of this, changes that change the behavior of the bootloader need to also
be demonstrated to be tested by the simulator.  For many changes, this means
changing the simulator itself to test the new configuration. For example, a
change that makes a new device work needs to have that device's flash
configuration added to the simulator to demonstrate that it doesn't cause
problems with corner cases and bad powerdowns.

### Running the simulator

CI will run the simulator through a multitude of configurations.  See
`.github/workflows/sim.yaml` for a full list.  Individual configurations can
easily be tested from the command line with something like:

```bash
$ cargo test \
    --features "sig-ecdsa-mbedtls swap-move" \
    -- --nocapture --test-threads 1
```

Additional arguments will be patterns to match the tests cases that can be run.

To run one of these tests in the debugger, you can do something like:

```bash
$ cargo test --features "swap-move" --no-run
```

This will print out the executables generated, and you will typically want the
executable that starts with 'core-' followed by a hash.

Running this executable, with the `--nocapture --test-threads 1` arguments, and
possibly test filters can be done with the debugger to allow the debugger to be
used on the code.

## Release notes

To facility creating release notes at release time, all non-trivial
changes must include a release note snippet with the pull request.
This can be either a separate commit, or as part of a single commit
(generally, if there are multiple commits to the pull request, the
release note snippet should be a separate commit, and a pull request
that is a single commit can include the release note snippet in that
commit).

The release notes should be placed in the `docs/release-notes.d`
directory. Please see the readme file in that directory for specifics.
