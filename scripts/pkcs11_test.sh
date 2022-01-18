#!/bin/sh -eux
PKCS11_PIN='648219' \
python -m imgtool.keys.imgtool_keys_pkcs11_test "$@" --failfast
