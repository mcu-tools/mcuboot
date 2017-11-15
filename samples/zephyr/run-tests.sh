#!/bin/bash

# Test runner
#
# Copyright (c) 2017 Open Source Foundries Limited

#
# This script can be used to execute the Zephyr test plan detailed in
# docs/testplan-zephyr.md.
#

function ok_yn () {
    while true ; do
        read -p "Test result OK (y/n)? " -n 1 choice
        echo
        case "$choice" in
            y|Y )
                return
                ;;
            n|N )
                echo "Test failed; exiting"
                exit 1
                ;;
            * )
                echo Please enter y or n
                ;;
        esac
    done
}

set -e

echo '--------------------------------------------------------'
echo '------------------------ GOOD RSA ----------------------'
make test-good-rsa
pyocd-flashtool -ce
echo "Flashing bootloader"
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello2 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ GOOD ECDSA --------------------'
make test-good-ecdsa
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello2 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ OVERWRITE ---------------------'
make test-overwrite
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello2 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello2 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ BAD RSA -----------------------'
make test-bad-rsa-upgrade
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello1 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ BAD ECDSA ---------------------'
make test-bad-ecdsa-upgrade
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello1 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ NO BOOTCHECK ------------------'
make test-no-bootcheck
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello1 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ WRONG RSA ---------------------'
make test-wrong-rsa
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello1 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '--------------------------------------------------------'
echo '------------------------ WRONG ECDSA -------------------'
make test-wrong-ecdsa
pyocd-flashtool -ce
make flash_boot
echo "Expected result: unable to find bootable image"
ok_yn
echo "Flashing hello 1"
make flash_hello1
echo "Expected result: hello1 runs"
ok_yn
echo "Flashing hello 2"
make flash_hello2
echo "Expected result: hello1 runs"
ok_yn
echo "Resetting"
pyocd-tool reset
echo "Expected result: hello1 runs"
ok_yn

echo '========================================================'
echo '                    ALL TESTS PASSED'
echo '========================================================'
