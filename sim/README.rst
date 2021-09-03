MCUboot Simulator
#################

This is a small simulator designed to exercise the MCUboot upgrade
code, specifically testing untimely reset scenarios to make sure the
code is robust.

Prerequisites
=============

The simulator is written in Rust_, and you will need to install it to
build it.  The installation_ page describes this process.  The
simulator can be built with the stable release of Rust.

.. _Rust: https://www.rust-lang.org/

.. _installation: https://www.rust-lang.org/en-US/install.html

Dependent code
--------------

The simulator depends on some external modules.  These are stored as
submodules within git.  To fetch these dependencies the first time::

  $ git submodule update --init --recursive

will clone and check out these trees in the appropriate place.

Testing
=======

The tests are written as unit tests in Rust, and can be built and run
automatically::

  $ cargo test

this should download and compile the necessary dependencies, compile
the relevant modules from MCUboot, build the simulator, and run the
tests.

There are several different features you can test. For example,
testing RSA signatures can be done with::

  $ cargo test --features sig-rsa

For a complete list of features, see Cargo.toml.

Debugging
=========

If the simulator indicates a failure, you can turn on additional
logging by setting ``RUST_LOG=warn`` or ``RUST_LOG=error`` in the
environment::

  $ RUST_LOG=warn ./target/release/bootsim run ...

It is also possible to run specific tests, for example::

  $ cargo test -- basic_revert

which will run only the `basic_revert` test.
