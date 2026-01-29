"""
AES-KW key management
"""

# SPDX-License-Identifier: Apache-2.0


class AESKWKey:
    def __init__(self, kek):
        self.kek = kek
