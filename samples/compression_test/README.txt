Independent LZMA test
---------------------

This tool finds and extracts compressed stream, decompresses it and verifies if
decompressed one is identical as before compression.

Building and running:

	change directory to the top of the repos:

		cd $ZEPHYR_BASE
		cd ..

	build tool:

		g++ bootloader/mcuboot/samples/compression_test/independent_cmp.c -o indcmp

	build example application:

		west build -b nrf54l15dk/nrf54l15/cpuapp -p
			-s zephyr/samples/hello_world/ --
			-DSB_CONFIG_BOOTLOADER_MCUBOOT=y
			-DSB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY=y
			-DSB_CONFIG_MCUBOOT_COMPRESSED_IMAGE_SUPPORT=y


	compare application image with the one carried by signed binary:

		./indcmp build/hello_world/zephyr/zephyr.signed.bin
			build/hello_world/zephyr/zephyr.bin

	note: order of arguments matter. Compressed goes first.
