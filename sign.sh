#! /bin/sh

# This script can be used as an example of how to sign images.

source $(dirname $0)/target.sh

# RSA signatures can be made with the signing script in the scripts
# directory.
if true; then
	./scripts/zep2newt.py \
	    --bin ../zephyr/samples/shell/outdir/$BOARD/zephyr.bin \
	    --key root.pem \
	    --sig RSA \
	    --out shell.signed.bin \
	    --vtoff 0x200 \
	    --word-size 8 \
	    --image-version 3 \
	    --bit --pad 0x20000

	./scripts/zep2newt.py \
	    --bin ../zephyr/samples/hello_world/outdir/$BOARD/zephyr.bin \
	    --key root.pem \
	    --sig RSA \
	    --vtoff 0x200 \
	    --word-size 8 \
	    --image-version 2 \
	    --out hello.signed.bin
fi

# Currently, ECDSA signatures need to be made with the imgtool.  See
# 'imgtool' for instructions on building the tool.
if false; then
	imgtool sign \
		--key root_ec.pem \
		--header-size 0x200 \
		--version 3.0 \
		--align 8 \
		--pad 0x20000 \
		../zephyr/samples/shell/outdir/$BOARD/zephyr.bin \
		shell.signed.bin

	imgtool sign \
		--key root_ec.pem \
		--header-size 0x200 \
		--version 3.0 \
		../zephyr/samples/hello_world/outdir/$BOARD/zephyr.bin \
		hello.signed.bin
fi
