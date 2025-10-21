# Copyright (c) 2020-2025 Arm Limited
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

FROM ubuntu:plucky

# Get dependencies for retrieving and building TF-M with MCUBoot, and QEMU.
RUN apt-get update && \
    DEBIAN_FRONTEND="noninteractive" \
    apt-get install -y \
    cmake \
    curl \
    gcc-arm-none-eabi \
    gdb-multiarch \
    git \
    libncurses6 \
    python3 \
    python3-pip \
    python3-venv \
    qemu-system-arm \
    file &&\
    apt-get clean all

# Add tfm work directory
WORKDIR /root/work

# Get rid of spurious git ownership errors
RUN git config --global --add safe.directory '*'

# Create Python virtual environment since PEP 668 protection is present
RUN python3 -m venv .venv

# Activate Python environment and install Python packages
RUN . .venv/bin/activate && python3 -m pip install \
    imgtool>=1.9.0 \
    Jinja2>=2.10.3 \
    PyYAML \
    pyasn1

# run the command
CMD ["bash"]
