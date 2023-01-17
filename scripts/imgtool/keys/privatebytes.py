# SPDX-License-Identifier: Apache-2.0

from cryptography.hazmat.primitives import serialization


class PrivateBytesMixin():
    def _get_private_bytes(self, minimal, format, exclass):
        if format is None:
            format = self._DEFAULT_FORMAT
        if format not in self._VALID_FORMATS:
            raise exclass("{} does not support {}".format(
                self.shortname(), format))
        return format, self.key.private_bytes(
                encoding=serialization.Encoding.DER,
                format=self._VALID_FORMATS[format],
                encryption_algorithm=serialization.NoEncryption())
