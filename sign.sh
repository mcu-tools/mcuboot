#!/usr/bin/env bash
set -euo pipefail
KEY="my-root-2048.pem"
HDR=0x200
ALIGN=4
SLOT=0x40000   # 256 KiB slots (change if your real device differs)

in="${1:?usage: ./sign.sh <input.bin> [version]}"
ver="${2:-1.0.0}"
base="$(basename "${in%.*}")"
out="${base}_v${ver}.signed.bin"

imgtool sign \
  --key "$KEY" \
  --align "$ALIGN" \
  --header-size "$HDR" \
  --pad-header \
  --slot-size "$SLOT" \
  --version "$ver" \
  "$in" "$out"

imgtool verify --key "$KEY" "$out"
hexdump -C -n 16 "$out" | head -n1   # should begin: 3d b8 f3 96
stat -c 'bytes=%s -> %n' "$out"
echo "OK: $out"
