# BOOT_IMG_MAGIC =                0x12344321
BOOT_IMG_MAGIC = [ 0xf395c277, 0x7fefd260, 0x0f505235, 0x8079b62c ]
IMAGE_MAGIC =			0x96f3b83c
IMAGE_MAGIC_NONE =		0xffffffff

#
# Image header flags.
#
IMAGE_F_PIC =			0x00000001
IMAGE_F_SHA256 =		0x00000002  # Image contains hash TLV
IMAGE_F_PKCS15_RSA2048_SHA256 = 0x00000004  # PKCS15 w/RSA and SHA
IMAGE_F_ECDSA224_SHA256 =	0x00000008  # ECD
IMAGE_HEADER_SIZE =		32

# Digest sizes
SHA256_DIGEST_SIZE =		32

# Signature sizes
RSA_SIZE =                      256
ECDSA_SIZE =                    68

# 
# Image trailer TLV types.
# 
IMAGE_TLV_SHA256 =		1   # SHA256 of image hdr and body
IMAGE_TLV_RSA2048 =		2   # RSA2048 of hash output
IMAGE_TLV_ECDSA224 =		3   # ECDSA of hash output


OFFSET_VECTOR_TABLE = 0x20
OFFSET_IH_MAGIC = 0
OFFSET_IH_TLV_SIZE = 4
OFFSET_IH_KEY_ID = 6
OFFSET_IH_PAD1 = 7
OFFSET_IH_HDR_SIZE = 8
OFFSET_IH_PAD2 = 10
OFFSET_IH_IMG_SIZE = 12
OFFSET_IH_FLAGS = 16
OFFSET_IH_VER_MAJOR = 20
OFFSET_IH_VER_MINOR = 21
OFFSET_IH_REVISION = 22
OFFSET_IH_BUILD_NUM = 24
OFFSET_IH_PAD3 = 28
#struct image_version { uint8_t iv_major; uint8_t iv_minor; uint16_t iv_revision;
#    uint32_t iv_build_num;
#};
#
#/** Image header.  All fields are in little endian byte order. */
#struct image_header {
#    uint32_t ih_magic;
#    uint16_t ih_tlv_size; /* Trailing TLVs */
#    uint8_t  ih_key_id;
#    uint8_t  _pad1;
#    uint16_t ih_hdr_size;
#    uint16_t _pad2;
#    uint32_t ih_img_size; /* Does not include header. */
#    uint32_t ih_flags;
#    struct image_version ih_ver;
#    uint32_t _pad3;
#};
#
#/** Image trailer TLV format. All fields in little endian. */
#struct image_tlv {
#    uint8_t  it_type;
#    uint8_t  _pad;
#    uint16_t it_len;
#};
