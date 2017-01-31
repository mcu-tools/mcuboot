Image Manipulation Tool
#######################

Introduction
============

The *mcuboot* bootloader began as the bootloader for the Mynewt_
project.  This project's *newt* tool, in addition to configuration and
compilation, also contains code to manipulate image headers and apply
signatures.

.. _Mynewt: https://mynewt.apache.org/

In order to use *mcuboot* with other OSes (starting with Zephyr), we
need a tool that doesn't depend on being run inside of a *Mynewt*
project.  This ``imgtool`` command is a small program that uses the
*newt* tool's library as a small image manipulation tool

Building It
===========

The *newt* tool is written in Go_, and as such, this imgtool is also.
In order to build from source, you will need the Go toolchain
installed.  On most distributions, this can be accomplished by
installing a ``go`` or ``golang`` package.  The tool is driven by a
program ``go``, and::

    $ go version

.. _Go: https://golang.org/

should print out the version installed.

Go prefers a fairly specific source tree layout, and it is easiest to
work with if you keep this layout.  To do this, you will need a
directory to hold your go source.  Once you've created this, the
environment variable ``GOPATH`` needs to be set to point to this
directory.  Once this is done, you can get this code, and its
necessary dependencies by running::

    $ go get github.com/runtimeco/mcuboot/imgtool

and the package can be built with::

    $ go install github.com/runtimeco/mcuboot/imgtool

This will place the build tool in ``$GOPATH/bin/imgtool``.  Feel free
to either place this directory in your path, or move the image to
somewhere in your path.

Usage
=====

Run ``imgtool`` either with no arguments, or with ``help`` to get more
help.  The tool is capable of generating the signing keys needed for
the bootloader, extracting the public key to embed in the image, and
signing images.

Generating keys
---------------

Keys are generated with the ``keygen`` subcommand.  The ``-k`` option
allows you to specify a different file for the resulting key.  The key
is stored in *PEM* format.

Extracting the public key
-------------------------

In order to use a key other than the key distributed in the Zephyr
directory, it needs to be extracted from the private key.  The
``getpub`` subcommand does this.  The ``-k`` option can be used to
specify a keyfile.  The program will then output the public key to
stdout.  This should be written to a ``.cc`` file and included in the
build.  For Zephyr builds, this is in the file ``boot/zephyr/keys.c``
in the main tree.

Signing images
--------------

In order for the bootloader to be able to upgrade an image, it must
have a header prefixed to it, and a signature placed at the end.  The
``sign`` subcommand can perform these operations.

In order to use Zephyr images, you must set CONFIG_TEXT_SECTION_OFFSET
in the main Zephyr program you want to build.  The exact value depends
on the particular target.  The FRDM-K64F, 0x200 should be used.

The ``sign`` command needs to be told this header offset as well, with
the ``--header-size 0x200`` argument.

The ``sign`` command is also able to mark images for upgrade.  This is
done with the ``--pad``, and ``--align`` arguments.  The pad should be
set to the size of the image slot (in bytes), and the alignment needs
to be set to the write alignment of the flash device.  For FRDM-K64F,
the alignment is 8.
