.. _mcuboot_external_config:

MCUBoot External Config
#######################

Overview
********

A simple sample illustrating how to configure from a different module
an MCUBoot image, using local keys and certificates.

NOTE: This references ``${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr``
for most configuration files for the verified set of boards.  Other
board configuration files will need to be provided locally.

Building and Running
********************

This application can be built and executed on QEMU as follows:

.. code-block:: console

  west build -b nrf52840dk_nrf52840 \
    bootloader/mcuboot/samples/modules/mcuboot/mcuboot_external_config


Sample Output
=============

None.
