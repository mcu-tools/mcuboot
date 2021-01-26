#!/bin/bash
#
# SPDX-License-Identifier: Apache-2.0

source $(dirname $0)/../target.sh

JLinkExe -speed auto -si SWD -device $SOC
