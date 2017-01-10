#! /bin/bash

source $(dirname $0)/../target.sh

lscript=/tmp/flash$$.jlink

cat >$lscript <<EOF
h
r
loadfile outdir/$BOARD/zephyr.bin $BASE_BOOT
loadfile hello.signed.bin $BASE_SLOT0
loadfile shell.signed.bin $BASE_SLOT1
q
EOF

JLinkExe -device $SOC -si SWD -speed auto \
	-CommanderScript $lscript
rm $lscript
