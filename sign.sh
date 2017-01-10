#! /bin/sh

source $(dirname $0)/target.sh

./scripts/zep2newt.py \
    --bin ../zephyr/samples/shell/outdir/$BOARD/zephyr.bin \
    --key root.pem \
    --sig RSA \
    --out shell.signed.bin \
    --vtoff 0x200 \
    --word-size 8 \
    --bit --pad 0x20000

./scripts/zep2newt.py \
    --bin ../zephyr/samples/hello_world/outdir/$BOARD/zephyr.bin \
    --key root.pem \
    --sig RSA \
    --vtoff 0x200 \
    --word-size 8 \
    --out hello.signed.bin
