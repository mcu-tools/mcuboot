#!/bin/bash -x

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2025 Arm Limited
#

FIH_IMAGE_VERSION=0.0.4

FIH_IMAGE_NAME=mcuboot-fih-test

FIH_IMAGE=$FIH_IMAGE_NAME:$FIH_IMAGE_VERSION

CONTAINER_REGISTRY=ghcr.io/mcu-tools

TFM_TAG=ef7e8f34b48100c9ab0d169980e84d37d6b3c8b9
