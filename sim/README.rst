MCUboot Simulator
#################

This is a small simulator designed to exercise the mcuboot upgrade
code, specifically testing untimely reset scenarios to make sure the
code is robust.

Prerequisites
=============

The simulator is written in Rust_, and you will need to install it to
build it.  The installation_ page describes this process.  The
simulator can be built with the stable release of Rust.

.. _Rust: https://www.rust-lang.org/

.. _installation: https://www.rust-lang.org/en-US/install.html

Building
========

Once Rust is installed, build cargo by::

  $ cargo build --release

this should download and compile the necessary dependencies, compile
the relevant modules from mcuboot, and build the simulator.  The
resulting executable will be placed in ``./target/release/bootsim``
and can be run directly::

  $ ./target/release/bootsim run --device k64f

Calling with ``--help`` will give a more thorough usage.

Debugging
=========

If the simulator indicates a failure, you can turn on additional
logging by setting ``RUST_LOG=warn`` or ``RUST_LOG=error`` in the
environment::

  $ RUST_LOG=warn ./target/release/bootsim run ...
