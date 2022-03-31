"""
Copyright (c) 2021 Cypress Semiconductor Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import os
import struct
from cryptography.hazmat.primitives.ciphers import (
    Cipher, algorithms, modes
)

NONCE_SIZE = 12

class EncryptorMXS40Sv2:
    def __init__(self, key, nonce, initial_counter=0):
        # with open(key_path, 'rb') as f:
        #     self.key = f.read()
        self.key = key
        self.nonce = nonce
        self.counter = 0
        self.initial_counter = initial_counter
        cipher = Cipher(algorithms.AES(key), modes.ECB())
        self.encryptor = cipher.encryptor()

    def _load(self, image_path):
        with open(image_path, 'rb') as f:
            image = f.read()
        return image

    def _save(self, data, output_path):
        with open(output_path, 'wb') as f:
            f.write(data)

    def update(self, data):
        """
        Encrypts a byte array using a customized AES-CTR mode
        where a counter is incremented by 16 per block.
        A nonce format is (128 bit):
            bits 0...31 - counter + initial values
            bits 32...127 - random nonce
        """
        chunk_size = 16
        counter = self.counter
        ciphertext = bytes()
        for i in range(0, len(image), chunk_size):
            indata = struct.pack('<I', initial_counter + counter) + nonce
            counter += chunk_size
            cipher_block = self.encryptor.update(indata)
            chunk = image[i:i + chunk_size]
            ciphertext += bytes(a ^ b for a, b in zip(chunk, cipher_block))
        self.counter = counter
        return ciphertext

    def encrypt(self, image, initial_counter=0):
        """
        Encrypts a byte array using a customized AES-CTR mode
        where a counter is incremented by 16 per block.
        A nonce format is (128 bit):
            bits 0...31 - counter + initial values
            bits 32...127 - random nonce
        """
        chunk_size = 16
        counter = 0
        # self.initial_counter = initial_counter
        # counter = 0 if initial_counter is None else int(initial_counter, 0)
        # counter = initial_counter
        ciphertext = bytes()
        for i in range(0, len(image), chunk_size):
            indata = struct.pack('<I', initial_counter + counter) + self.nonce[:12]
            counter += chunk_size
            cipher_block = self.encryptor.update(indata)
            chunk = image[i:i + chunk_size]
            ciphertext += bytes(a ^ b for a, b in zip(chunk, cipher_block))
        self.encryptor.finalize()
        # return ciphertext, nonce
        return ciphertext

    def encrypt_image(self, input_path, initial_counter=None, output_path=None, nonce_path=None):
        """
        Encrypts an image each time using new random nonce.
        Saves the nonce and the encrypted image to specified locations.
        If the output locations are not given the output files are saved
        in the same location as the input image with predefined names.
        """
        image = self._load(input_path)

        nonce = os.urandom(NONCE_SIZE)
        init = 0 if initial_counter is None else int(initial_counter, 0)
        ciphertext, nonce = self._encrypt(image, nonce, init)

        if output_path is None:
            output_path = '{0}_{2}{1}'.format(*os.path.splitext(input_path) + ('encrypted',))

        if nonce_path is None:
            nonce_path = '{0}_{2}{1}'.format(*os.path.splitext(input_path) + ('nonce',))

        self._save(ciphertext, output_path)
        self._save(nonce, nonce_path)

        return output_path, nonce_path
