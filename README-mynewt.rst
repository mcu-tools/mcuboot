Running mynewt apps with mcuboot
################################

Due to small differences between Mynewt's bundled bootloader and **mcuboot**,
when building an app that will be run with **mcuboot** as the bootloader and
which at the same time requires to use **newtmgr** to manage images, **mcuboot**
must be added as a new dependency for this app.

First you need to add the repo to your ``project.yml``:

.. code-block:: yaml

    project.repositories:
        - mcuboot

    repository.mcuboot:
        type: github
        vers: 0-dev
        user: runtimeco
        repo: mcuboot

Then update your app's ``pkg.yml`` adding the extra dependency:

.. code-block:: yaml

    pkg.deps:
        - "@mcuboot/boot/bootutil"

Also remove any dependency on ``boot/bootutil`` (mynewt's bundled bootloader)
which might exist.

To configure **mcuboot** check all the options available in
``boot/mynewt/mcuboot_config/syscfg.yml``.

Also, **mcuboot** uses a different image header struct as well as slightly
different TLV structure, so images created by **newt** have to be generated
in this new format. That is done by passing the extra parameter **-2** as in:

``newt create-image <target> <version> <pubkey> -2``
