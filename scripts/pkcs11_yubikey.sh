#!/bin/sh -eux

# This was tested with a Yubikey 5C on Ubuntu 22.04 with:
# * apt install opensc-pkcs11 opensc gnutls-bin libengine-pkcs11-openssl p11-kit yubikey-manager
# * yubico-piv-tool built from git. v2.2.0 is broken:
#   https://groups.google.com/g/linux.debian.bugs.dist/c/sykOoivgJUE

# The temporary file is a stand-in for a firmware image
TEMP_FILE=$(mktemp)
truncate -s37000 "${TEMP_FILE}"

# This will wipe out the Yubikey's PIV data, but it prompts the user first
ykman piv reset

# set nozero serial number (disabled here)
# yubico-piv-tool -a set-chuid

# enable retired key slots
echo -n C10114C20100FE00 | \
	yubico-piv-tool --key=010203040506070801020304050607080102030405060708 \
			-a write-object --id 0x5FC10C -i -

# generate a private key
yubico-piv-tool -a generate -s 9a -A ECCP256 -o pub.pem

# or if you want to require user physical presence to sign:
# yubico-piv-tool -a generate -s 9a -A ECCP256 -o pub.pem --touch-policy=always

# generate a dummy cert (it's not used, but it keeps opensc happy)
yubico-piv-tool -a verify -a selfsign-certificate -s 9a -o cert.pem -S "/CN=selfsigned/" -i pub.pem -P 123456
yubico-piv-tool -a import-certificate -i cert.pem -s 9a

# test signing using known-good utilities, should say "ok"
p11tool --login --test-sign 'pkcs11:serial=00000000;id=%01;pin-value=123456'

# sign our firmware image using the Yubikey
./imgtool.py sign \
	--key 'pkcs11:serial=00000000;id=%01;pin-value=123456' \
	--align 4 --version 0.0.0 --header-size 0x200 \
	--slot-size 0x74000 --pad-header \
	${TEMP_FILE} ${TEMP_FILE}.signed

# verify against the pubkey that was saved earlier
./imgtool.py verify --key pub.pem ${TEMP_FILE}.signed
