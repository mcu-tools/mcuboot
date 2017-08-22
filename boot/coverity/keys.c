#include <bootutil/sign_key.h>

unsigned char pubkey[] = {
  0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xa9, 0xf7, 0x9d,
  0xc8, 0xf5, 0x67, 0x98, 0x74, 0x0c, 0xfd, 0xe1, 0x78, 0x3e, 0xea, 0x86,
  0xfe, 0xe1, 0xa9, 0xe1, 0x3c, 0x0d, 0xb9, 0xd2, 0x5e, 0x1d, 0xf1, 0xaf,
  0xda, 0x98, 0x9e, 0xa0, 0xf2, 0xd0, 0xb1, 0x95, 0xe2, 0x93, 0x0d, 0x66,
  0x39, 0x6f, 0x86, 0x9c, 0x20, 0xa1, 0x1c, 0x4a, 0x0e, 0x6d, 0xc0, 0xe3,
  0xca, 0x44, 0xc7, 0x7e, 0x5d, 0xbc, 0x67, 0x5b, 0xf7, 0xa6, 0x2c, 0x65,
  0x74, 0x8f, 0x09, 0x2a, 0xa9, 0x80, 0x4c, 0xef, 0x7e, 0x47, 0xbc, 0xf7,
  0xd6, 0x2a, 0x9a, 0x2e, 0x95, 0x59, 0x92, 0x45, 0xe3, 0x83, 0x16, 0x33,
  0x0e, 0x89, 0xc6, 0x57, 0x54, 0x1d, 0x71, 0x25, 0x27, 0x64, 0x47, 0xee,
  0x82, 0xb7, 0xd7, 0x1b, 0x18, 0x6a, 0x70, 0x4d, 0x50, 0x88, 0xea, 0x6b,
  0xc3, 0x09, 0x24, 0xa7, 0x37, 0x8c, 0x22, 0xe9, 0xd2, 0xd4, 0xdd, 0x17,
  0xed, 0xb4, 0x7d, 0x11, 0x67, 0x0d, 0x78, 0x97, 0x89, 0x19, 0xe8, 0x35,
  0xe5, 0x40, 0xb4, 0x61, 0x3d, 0x91, 0x6c, 0x0f, 0x25, 0x7b, 0xa4, 0x7f,
  0xd6, 0xec, 0x4e, 0x07, 0x83, 0xf8, 0x6e, 0x12, 0x77, 0xdf, 0x3b, 0xb8,
  0x61, 0xd5, 0x07, 0xfb, 0xda, 0x2f, 0x0f, 0xf9, 0x85, 0xfe, 0x76, 0x78,
  0x70, 0x31, 0xa3, 0xfa, 0xa5, 0x25, 0xe0, 0x38, 0x33, 0x4f, 0x54, 0x64,
  0x7e, 0x7c, 0xf4, 0xea, 0xfe, 0x1b, 0xb3, 0x41, 0xb3, 0x80, 0x0c, 0x7b,
  0xcf, 0x11, 0x8c, 0xb5, 0x3c, 0x9a, 0xc9, 0xbe, 0xf3, 0xb3, 0x5b, 0x4f,
  0x15, 0xf9, 0xb0, 0xb6, 0x4d, 0xc6, 0x50, 0x4b, 0x88, 0x71, 0x65, 0x1e,
  0xcf, 0x1c, 0xf6, 0xba, 0xa9, 0x7f, 0x79, 0xee, 0x3b, 0x04, 0x23, 0xe7,
  0x18, 0xa5, 0xe0, 0xdd, 0x53, 0xea, 0x96, 0x24, 0x46, 0xe5, 0x51, 0x0d,
  0x3c, 0xa1, 0x34, 0x36, 0xe0, 0x40, 0x55, 0x68, 0x65, 0x3b, 0x10, 0xe1,
  0x45, 0x02, 0x03, 0x01, 0x00, 0x01
};
static unsigned int pubkey_len = 270;
const struct bootutil_key bootutil_keys[] = {
    [0] = {
        .key = pubkey,
        .len = &pubkey_len,
    }
};
const int bootutil_key_cnt = sizeof(bootutil_keys) / sizeof(bootutil_keys[0]);
