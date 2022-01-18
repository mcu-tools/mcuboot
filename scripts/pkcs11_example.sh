#!/bin/sh -eux

# The temporary file is a stand-in for a firmware image
TEMP_FILE=$(mktemp)
truncate -s37000 "${TEMP_FILE}"

# You can create a private (signing) key on a Nitrokey using the following code:
# pkcs11-tool --login --keypairgen --key-type EC:secp256r1 --label $KEY_LABEL

# You can list all private keys on the Nitrokey usign the following code
# p11tool --list-keys --login

PKCS11_PIN=648219 ./imgtool.py \
 sign \
 --key 'pkcs11:model=PKCS%2315%20emulated;manufacturer=www.CardContact.de;serial=DENK0103525;token=SmartCard-HSM%20%28UserPIN%29;id=%1B%B4%85%90%F3%15%58%D3%CA%99%1C%7D%5E%2A%2A%6D%B0%BE%8E%D2;object=imgtool.py%20test%20key%202022-01-18T12%3A26%3A01.913886;type=private' \
 --header-size 0x200 \
 --align 8 \
 --version 0.1 \
 --slot-size 0x37000 \
 "${TEMP_FILE}" \
 "${TEMP_FILE}".signed
