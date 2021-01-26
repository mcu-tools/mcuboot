#! /bin/bash
#
# SPDX-License-Identifier: Apache-2.0

source $(dirname $0)/../target.sh

# Start the jlink gdb server
JLinkGDBServer -if swd -device $SOC -speed auto
