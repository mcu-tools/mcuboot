from .general import KeyClass

class AESKW(KeyClass):
    def __init__(self, key):
        if len(key) not in (16, 32):
            raise ValueError("Invalid AES key length for AES-KW: must be 16 or 32 bytes.")
        self.key = key

    def get_key(self):
        return self.key

    def key_size(self):
        return len(self.key)
