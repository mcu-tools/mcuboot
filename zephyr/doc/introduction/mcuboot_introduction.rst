.. _mcuboot_introduction:


Introduction
############

MCUboot-on-Zephyr is the MCUboot secure bootloader for 32-bit microcontrollers
ported into the Zephyr Project ecosystem. This port "project" includes:

- a reusable Zephyr-specific boot application
- integration of the common bootloader and flash layout infrastructure


Licensing
*********

MCUboot is permissively licensed using the `Apache 2.0 license`_
(as found in the ``LICENSE`` file in the
project's `GitHub repo`_).  There are some
imported or reused components of the MCUboot-on-Zephyr project
that use other licensing as described in :ref:`Zephyr_Licensing`.

.. _Apache 2.0 license:
   https://github.com/mcu-tools/mcuboot/blob/main/LICENSE

.. _mcuboot_GitHub repo: https://github.com/mcu-tools/mcuboot


Distinguishing Features
***********************

MCUboot-on-Zephyr offers a significant set of features including:

**First (Immutable) and Second (Mutable) Bootloader Configurations**

**Multiple Crypto Algorithms**

**Highly configurable / Modular for flexibility**
   Allows an integration to incorporate *only* the capabilities it needs as it
   needs them, and to specify their quantity and size.

**Integration into Single- and Multi-threaded Executable**

**Optional Recovery Application**
   Allows developers and testers in a trusted environment to update a prototype
   during early development and integration.


.. include:: ../../README.rst
   :start-after: start_include_here


Fundamental Terms and Concepts
******************************

See :ref:`glossary`
