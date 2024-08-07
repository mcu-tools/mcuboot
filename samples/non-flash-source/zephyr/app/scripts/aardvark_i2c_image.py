# SPDX-License-Identifier: Apache-2.0

import argparse
import signal
import sys
import time

from aardvark_py import *

done = False
contents = None


def parse_args():
    global args
    parser = argparse.ArgumentParser(description='Aardvark I2C "Storage Device"')
    parser.add_argument("-p", "--port", default=0, help="Aardvark port number")
    parser.add_argument("device_addr", type=str, help="I2C device address")
    parser.add_argument("file", type=str, help="File to provide")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    args = parser.parse_args()


def signal_handler(sig, frame):
    global done
    global contents

    if sig == signal.SIGINT:
        done = True
    elif sig == signal.SIGQUIT:
        print("Reloading file")
        with open(args.file, "rb") as f:
            contents = f.read()


def watch(handle):
    print("Watching I2C data (Use ^\\ to reload file) ...")

    # Loop until aa_async_poll times out
    while True:
        result = aa_async_poll(handle, 100)
        # Read the I2C message.
        if (result & AA_ASYNC_I2C_READ) == AA_ASYNC_I2C_READ:
            # Get data written by master
            (num_bytes, addr, data_in) = aa_i2c_slave_read(handle, 6)

            if num_bytes < 0:
                print("slave_read error: %s" % aa_status_string(num_bytes))
                return

            if num_bytes == 0:
                continue

            if data_in[0] == 0x1:
                if args.verbose:
                    print(
                        f"Got 0x1, asking size of file. Responding with {len(contents)}"
                    )
                ret = aa_i2c_slave_set_response(
                    handle,
                    array(
                        "B",
                        [
                            (len(contents) >> 24) & 0xFF,
                            (len(contents) >> 16) & 0xFF,
                            (len(contents) >> 8) & 0xFF,
                            len(contents) & 0xFF,
                        ],
                    ),
                )
            elif data_in[0] == 0x2:
                addr = (
                    data_in[1] << 24 | data_in[2] << 16 | data_in[3] << 8 | data_in[4]
                )
                size = data_in[5]

                if args.verbose:
                    print(f"Got 0x2, asking for data, at {addr} with size {size}.")

                if addr < 0 or addr + size > len(contents):
                    print("Requested data is out of bounds, responding with 0x0")
                    ret = aa_i2c_slave_set_response(handle, array("B", [0x0]))
                else:
                    ret = aa_i2c_slave_set_response(
                        handle, array("B", contents[addr : addr + size])
                    )
            else:
                print("Got unknown data, responding with 0x0")
                ret = aa_i2c_slave_set_response(handle, array("B", [0x0]))

        elif (result & AA_ASYNC_I2C_WRITE) == AA_ASYNC_I2C_WRITE:
            # Get number of bytes written to master
            num_bytes = aa_i2c_slave_write_stats(handle)

            if num_bytes < 0:
                print("slave_write_stats error: %s" % aa_status_string(num_bytes))
                return

            # Print status information to the screen
            if args.verbose:
                print(f"Number of bytes written to master: {num_bytes:04}\n")

        elif result == AA_ASYNC_NO_DATA:
            if done:
                break
        else:
            print("error: non-I2C asynchronous message is pending", result)
            return


def main():
    global contents

    parse_args()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGQUIT, signal_handler)

    port = args.port
    addr = int(args.device_addr, 0)

    # Open the device
    handle = aa_open(port)
    if handle <= 0:
        print("Unable to open Aardvark device on port %d" % port)
        print("Error code = %d" % handle)
        sys.exit()

    # Ensure that the I2C subsystem is enabled
    aa_configure(handle, AA_CONFIG_SPI_I2C)

    # Disable the I2C bus pullup resistors (2.2k resistors).
    # This command is only effective on v2.0 hardware or greater.
    # The pullup resistors on the v1.02 hardware are enabled by default.
    aa_i2c_pullup(handle, AA_I2C_PULLUP_NONE)

    # Power the EEPROM using the Aardvark adapter's power supply.
    # This command is only effective on v2.0 hardware or greater.
    # The power pins on the v1.02 hardware are not enabled by default.
    aa_target_power(handle, AA_TARGET_POWER_BOTH)

    # Set default response
    aa_i2c_slave_set_response(handle, array("B", [0]))

    # Enabled the device
    aa_i2c_slave_enable(handle, addr, 64, 6)

    # Read the file
    with open(args.file, "rb") as f:
        contents = f.read()

    # Watch the I2C port
    watch(handle)

    # Disable and close the device
    aa_i2c_slave_disable(handle)
    aa_close(handle)


if __name__ == "__main__":
    main()
