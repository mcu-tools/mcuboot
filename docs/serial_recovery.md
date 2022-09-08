<!--
  - SPDX-License-Identifier: Apache-2.0

  - Copyright (c) 2022 Nordic Semiconductor ASA

  - Original license:

  - Licensed to the Apache Software Foundation (ASF) under one
  - or more contributor license agreements.  See the NOTICE file
  - distributed with this work for additional information
  - regarding copyright ownership.  The ASF licenses this file
  - to you under the Apache License, Version 2.0 (the
  - "License"); you may not use this file except in compliance
  - with the License.  You may obtain a copy of the License at

  -  http://www.apache.org/licenses/LICENSE-2.0

  - Unless required by applicable law or agreed to in writing,
  - software distributed under the License is distributed on an
  - "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  - KIND, either express or implied.  See the License for the
  - specific language governing permissions and limitations
  - under the License.
-->

# Serial recovery

MCUboot implements a Simple Management Protocol (SMP) server.
SMP is a basic transfer encoding for use with the MCUmgr management protocol. 
See the Zephyr [Device Management](https://docs.zephyrproject.org/latest/services/device_mgmt/index.html#device-mgmt) documentation for more information about MCUmgr and SMP.

MCUboot supports the following subset of the MCUmgr commands:
* echo (OS group)
* reset (OS group)
* image list (IMG group)
* image upload (IMG group)

It can also support system-specific MCUmgr commands depending on the given mcuboot-port
if the ``MCUBOOT_PERUSER_MGMT_GROUP_ENABLED`` option is enabled.

The serial recovery feature can use any serial interface provided by the platform.

## Image uploading

Uploading an image is targeted to the primary slot by default.

An image can be loaded to other slots only when the ``MCUBOOT_SERIAL_DIRECT_IMAGE_UPLOAD`` option is enabled for the platform.

MCUboot supports progressive erasing of a slot to which an image is uploaded to if the ``MCUBOOT_ERASE_PROGRESSIVELY`` option is enabled.
As a result, a device can receive images smoothly, and can erase required part of a flash automatically.

## Configuration of serial recovery

How to enable and configure the serial recovery feature depends on the given mcuboot-port implementation.
Refer to the respective documentation and source code for more details.

## Entering serial recovery mode

Entering the serial recovery mode is usually possible right after a device reset, for instance as a reaction on a GPIO pin state.
Refer to the given mcuboot-port details to get information on how to enter the serial recovery mode.

## Serial recovery mode usage

### MCU Manager CLI installation

The MCUmgr command line tool can be used as an SMP client for evaluation purposes.
The tool is available under the [MCU Manager](https://github.com/apache/mynewt-mcumgr-cli)
Github page and is described in the Zephyr 
[MCU Manager CLI](https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html#mcumgr-cli) documentation.

Use the following command to install the MCU Manager CLI tool:
``` console
go install github.com/apache/mynewt-mcumgr-cli/mcumgr@latest
```
Enter serial recovery mode in the device and use an SMP client application for communication with the MCUboot's SMP server.

### Connection configuration

Use the following command to set the connection configuration,
for linux:
``` console
mcumgr conn add serial_1 type="serial" connstring="dev=/dev/ttyACM0,baud=115200"
```
for windows:
``` console
mcumgr conn add serial_1 type="serial" connstring="COM1,baud=115200"
```

### Image management

The connection configuration must be established to perform the following image-related commands:

* Upload the image:
  ``` console
  mcumgr image upload <path-to-signed-image-bin-file> -c serial_1
  ```

  Once done successfully, the following notification will be displayed:
  ``` console
  20.25 KiB / 20.25 KiB [=================================] 100.00% 3.08 KiB/s 6s
  Done
  ```

* List all images:
  ``` console
  mcumgr image list -c serial_1
  ```
  The terminal will show the response from the module:
  ``` console
  Images:
   image=0 slot=0
      version: 0.0.0.0
      bootable: false
      flags: 
      hash: Unavailable
  Split status: N/A (0)
  ```

### Device reset

Reset your device with the following command:
``` console
mcumgr reset -c serial_1
```
The terminal should respond:
``` console
Done
```
