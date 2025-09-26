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

import pytest

# List of tests expected to fail for some reason
XFAILED_TESTS = {
    # Unsupported key_type and format combinations
    "tests/test_keys.py::TestGetPriv::test_getpriv[openssl-ed25519]",
    "tests/test_keys.py::TestGetPriv::test_getpriv[openssl-x25519]",
    "tests/test_keys.py::TestGetPriv::test_getpriv[pkcs8-rsa-2048]",
    "tests/test_keys.py::TestGetPriv::test_getpriv[pkcs8-rsa-3072]",
    "tests/test_keys.py::TestGetPriv::test_getpriv[pkcs8-ed25519]",
    "tests/test_keys.py::TestGetPriv::test_getpriv_with_password[openssl-ed25519]",
    "tests/test_keys.py::TestGetPriv::test_getpriv_with_password[openssl-x25519]",
    "tests/test_keys.py::TestGetPriv::test_getpriv_with_password[pkcs8-rsa-2048]",
    "tests/test_keys.py::TestGetPriv::test_getpriv_with_password[pkcs8-rsa-3072]",
    "tests/test_keys.py::TestGetPriv::test_getpriv_with_password[pkcs8-ed25519]",
    # 'Ed25519' object has no attribute 'get_public_pem'
    "tests/test_keys.py::TestGetPub::test_getpub[pem-ed25519]",
    "tests/test_keys.py::TestGetPub::test_getpub_with_encoding[pem-ed25519]",
    "tests/test_keys.py::TestGetPub::test_getpub_no_outfile[pem-ed25519]",
    "tests/test_keys.py::TestLoading::test_load_key_public[pem-ed25519]",
    "tests/test_keys.py::TestLoading::test_load_key_public_with_password[pem-ed25519]",
}


def pytest_runtest_setup(item):
    if item.nodeid in XFAILED_TESTS:
        pytest.xfail()
