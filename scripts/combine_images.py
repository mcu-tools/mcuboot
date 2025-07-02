#! /usr/bin/env python3
#
# Copyright (C) 2025 Analog Devices, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import pathlib
import subprocess
import sys

import yaml
import shutil


# Function definitions


def main():
    global img_tool
    global output_dir
    global config_path

    parser = argparse.ArgumentParser(description="Create application package", allow_abbrev=False)
    parser.add_argument('--config', help="The path to config yaml file", required=True)
    parser.add_argument('--imgtool', help="The path to ImgTool", required=True)
    parser.add_argument('--output', help="Output directory", required=True)

    args = parser.parse_args()

    if not os.path.isfile(args.config):
        print(f"Error: The config file '{args.config}' does not exist")
        return

    config_path = os.path.dirname(os.path.abspath(args.config))
    print(f"config_path: {config_path}")

    with open(args.config) as config_file:
        config = yaml.safe_load(config_file)

    img_tool = args.imgtool
    if img_tool.endswith('.py'):
        if not os.path.exists(img_tool): # Is python file exist?
            print(f"Error: The '{img_tool}' not found")
            return
    else:
        if not shutil.which(img_tool): # Is binary file exist?
            print(f"Error: The '{img_tool}' not found in the path")
            return


    output_dir = args.output
    os.makedirs(output_dir, exist_ok=True)

    parse_app_pack(config, None)

    print(f"\nCreated {package_name}")


def verify_file_exists(filename):
    filepath = pathlib.Path(filename)

    if not filepath.exists():
        print("ERROR: File " + filename + " not found")
        sys.exit(1)


def parse_app_pack(app_pack, name):
    header = None
    image = []
    imgtype = 0
    global package_name

    for key in app_pack:
        if key.endswith("_pack"):
            imagename, imagetype = parse_app_pack(app_pack[key], name)
            image += [imagename]
        if key.lower() == "header":
            header = app_pack[key]
        if key.lower() == "image":
            image_path = os.path.join(config_path, app_pack[key])
            verify_file_exists(image_path)
            image += [image_path]
        if key.lower() == "outputfile":
            name = app_pack[key]

    # Exit if header or image are not specified
    if (header is None) or (image is None):
        return (None, 0)

    operation = 0

    cmd = []
    if img_tool.endswith('.py'):
        cmd += [sys.executable]
    
    cmd += [img_tool]
    cmd += ["sign"]

    for key in header:
        if key == "align":
            cmd += ["--align", str(header[key])]
        elif key == "aes-gcm-key":
            operation += 2
            gcm_key_path = os.path.join(config_path, header[key])
            verify_file_exists(gcm_key_path)
            cmd += ["--aes-gcm-key", gcm_key_path]
        elif key == "aes-kw-key":
            operation += 2
            kw_key_path = os.path.join(config_path, header[key])
            verify_file_exists(kw_key_path)
            cmd += ["--aes-kw-key", kw_key_path]
        elif key == "header-size":
            cmd += ["--header-size", hex(header[key])]
        elif key == "load-addr":
            cmd += ["--load-addr", hex(header[key])]
        elif key == "pad-header":
            if header[key] is True:
                cmd += ["--pad-header"]
        elif key == "private_signing_key":
            operation += 1
            private_key_path = os.path.join(config_path, header[key])
            verify_file_exists(private_key_path)
            cmd += ["--key", private_key_path]
        elif key == "public-key-format":
            cmd += ["--public-key-format", header[key]]
        elif key == "slot-size":
            cmd += ["--slot-size", hex(header[key])]
        elif key == "version":
            cmd += ["--version", header[key]]
        else:
            print("Unknown argument: " + key)

    # If there are multiple input files they must be combined for signing or encryption
    if len(image) > 1:
        combined_images = os.path.join(output_dir, name + ".bin")

        with open(combined_images, 'wb') as outfile:
            for fname in image:
                with open(fname, 'rb') as infile:
                    outfile.write(infile.read())
                    infile.close()
            outfile.close()

        image_input = combined_images
    else:
        image_input = image[0]

    if operation > 1:
        image_output = os.path.join(output_dir, name + "_signed_encrypted.bin")
    else:
        image_output = os.path.join(output_dir, name + "_signed.bin")

    cmd += [image_input]
    cmd += [image_output]

    print(f'Calling imgtool to generate file {image_output}')
    package_name = image_output
    subprocess.run(cmd)

    return (image_output, imgtype)


if __name__ == "__main__":
    main()
