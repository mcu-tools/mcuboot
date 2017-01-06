#! /bin/bash

source $(dirname 0)/target.sh
source ../../zephyr/zephyr-env.sh

make BOARD=$BOARD "$@"
