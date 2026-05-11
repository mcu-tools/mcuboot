#!/bin/bash -x

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2025 Arm Limited
#

FIH_IMAGE_VERSION=0.0.5

FIH_IMAGE_NAME=mcuboot-fih-test

FIH_IMAGE=$FIH_IMAGE_NAME:$FIH_IMAGE_VERSION

CONTAINER_REGISTRY=ghcr.io/mcu-tools

TFM_TAG=5d906d29d7b6d1a7f7134960d228f9e75f6a8a07
