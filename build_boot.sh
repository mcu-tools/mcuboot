#!/bin/sh

# Assume first argument is the board name (as defined in Zephyr)
BOARD=$1

if [ -z "$BOARD" ]; then
	echo "Please specify the board name (as in Zephyr) as first argument."
	exit 1;
fi

# Check if there is a valid Zephyr environment available
if [ -z "$ZEPHYR_BASE" ]; then
	echo "ZEPHYR_BASE not provided by the environment."
	exit 1;
fi

make BOARD=$BOARD
