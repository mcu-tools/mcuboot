/* aes_decrypt.c - TinyCrypt implementation of AES decryption procedure */

/*
 *  Copyright (C) 2017 by Intel Corporation, All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *    - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *    - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    - Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <tinycrypt/aes.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/utils.h>

static const uint8_t inv_sbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
	0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
	0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
	0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
	0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
	0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
	0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
	0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
	0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
	0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
	0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
	0x55, 0x21, 0x0c, 0x7d
};

#ifdef TC_USE_AES_TTABLES

static const uint32_t rt0[256] = {
	0x50a7f451, 0x5365417e, 0xc3a4171a, 0x965e273a, 0xcb6bab3b, 0xf1459d1f,
	0xab58faac, 0x9303e34b, 0x55fa3020, 0xf66d76ad, 0x9176cc88, 0x254c02f5,
	0xfcd7e54f, 0xd7cb2ac5, 0x80443526, 0x8fa362b5, 0x495ab1de, 0x671bba25,
	0x980eea45, 0xe1c0fe5d, 0x02752fc3, 0x12f04c81, 0xa397468d, 0xc6f9d36b,
	0xe75f8f03, 0x959c9215, 0xeb7a6dbf, 0xda595295, 0x2d83bed4, 0xd3217458,
	0x2969e049, 0x44c8c98e, 0x6a89c275, 0x78798ef4, 0x6b3e5899, 0xdd71b927,
	0xb64fe1be, 0x17ad88f0, 0x66ac20c9, 0xb43ace7d, 0x184adf63, 0x82311ae5,
	0x60335197, 0x457f5362, 0xe07764b1, 0x84ae6bbb, 0x1ca081fe, 0x942b08f9,
	0x58684870, 0x19fd458f, 0x876cde94, 0xb7f87b52, 0x23d373ab, 0xe2024b72,
	0x578f1fe3, 0x2aab5566, 0x0728ebb2, 0x03c2b52f, 0x9a7bc586, 0xa50837d3,
	0xf2872830, 0xb2a5bf23, 0xba6a0302, 0x5c8216ed, 0x2b1ccf8a, 0x92b479a7,
	0xf0f207f3, 0xa1e2694e, 0xcdf4da65, 0xd5be0506, 0x1f6234d1, 0x8afea6c4,
	0x9d532e34, 0xa055f3a2, 0x32e18a05, 0x75ebf6a4, 0x39ec830b, 0xaaef6040,
	0x069f715e, 0x51106ebd, 0xf98a213e, 0x3d06dd96, 0xae053edd, 0x46bde64d,
	0xb58d5491, 0x055dc471, 0x6fd40604, 0xff155060, 0x24fb9819, 0x97e9bdd6,
	0xcc434089, 0x779ed967, 0xbd42e8b0, 0x888b8907, 0x385b19e7, 0xdbeec879,
	0x470a7ca1, 0xe90f427c, 0xc91e84f8, 0x00000000, 0x83868009, 0x48ed2b32,
	0xac70111e, 0x4e725a6c, 0xfbff0efd, 0x5638850f, 0x1ed5ae3d, 0x27392d36,
	0x64d90f0a, 0x21a65c68, 0xd1545b9b, 0x3a2e3624, 0xb1670a0c, 0x0fe75793,
	0xd296eeb4, 0x9e919b1b, 0x4fc5c080, 0xa220dc61, 0x694b775a, 0x161a121c,
	0x0aba93e2, 0xe52aa0c0, 0x43e0223c, 0x1d171b12, 0x0b0d090e, 0xadc78bf2,
	0xb9a8b62d, 0xc8a91e14, 0x8519f157, 0x4c0775af, 0xbbdd99ee, 0xfd607fa3,
	0x9f2601f7, 0xbcf5725c, 0xc53b6644, 0x347efb5b, 0x7629438b, 0xdcc623cb,
	0x68fcedb6, 0x63f1e4b8, 0xcadc31d7, 0x10856342, 0x40229713, 0x2011c684,
	0x7d244a85, 0xf83dbbd2, 0x1132f9ae, 0x6da129c7, 0x4b2f9e1d, 0xf330b2dc,
	0xec52860d, 0xd0e3c177, 0x6c16b32b, 0x99b970a9, 0xfa489411, 0x2264e947,
	0xc48cfca8, 0x1a3ff0a0, 0xd82c7d56, 0xef903322, 0xc74e4987, 0xc1d138d9,
	0xfea2ca8c, 0x360bd498, 0xcf81f5a6, 0x28de7aa5, 0x268eb7da, 0xa4bfad3f,
	0xe49d3a2c, 0x0d927850, 0x9bcc5f6a, 0x62467e54, 0xc2138df6, 0xe8b8d890,
	0x5ef7392e, 0xf5afc382, 0xbe805d9f, 0x7c93d069, 0xa92dd56f, 0xb31225cf,
	0x3b99acc8, 0xa77d1810, 0x6e639ce8, 0x7bbb3bdb, 0x097826cd, 0xf418596e,
	0x01b79aec, 0xa89a4f83, 0x656e95e6, 0x7ee6ffaa, 0x08cfbc21, 0xe6e815ef,
	0xd99be7ba, 0xce366f4a, 0xd4099fea, 0xd67cb029, 0xafb2a431, 0x31233f2a,
	0x3094a5c6, 0xc066a235, 0x37bc4e74, 0xa6ca82fc, 0xb0d090e0, 0x15d8a733,
	0x4a9804f1, 0xf7daec41, 0x0e50cd7f, 0x2ff69117, 0x8dd64d76, 0x4db0ef43,
	0x544daacc, 0xdf0496e4, 0xe3b5d19e, 0x1b886a4c, 0xb81f2cc1, 0x7f516546,
	0x04ea5e9d, 0x5d358c01, 0x737487fa, 0x2e410bfb, 0x5a1d67b3, 0x52d2db92,
	0x335610e9, 0x1347d66d, 0x8c61d79a, 0x7a0ca137, 0x8e14f859, 0x893c13eb,
	0xee27a9ce, 0x35c961b7, 0xede51ce1, 0x3cb1477a, 0x59dfd29c, 0x3f73f255,
	0x79ce1418, 0xbf37c773, 0xeacdf753, 0x5baafd5f, 0x146f3ddf, 0x86db4478,
	0x81f3afca, 0x3ec468b9, 0x2c342438, 0x5f40a3c2, 0x72c31d16, 0x0c25e2bc,
	0x8b493c28, 0x41950dff, 0x7101a839, 0xdeb30c08, 0x9ce4b4d8, 0x90c15664,
	0x6184cb7b, 0x70b632d5, 0x745c6c48, 0x4257b8d0,
};

#define GET_UINT32_LE(n,b,i)                               \
{                                                          \
    (n) = ((uint32_t) (b)[(i)    ]      )                  \
        | ((uint32_t) (b)[(i) + 1] <<  8)                  \
        | ((uint32_t) (b)[(i) + 2] << 16)                  \
        | ((uint32_t) (b)[(i) + 3] << 24);                 \
}

#define PUT_UINT32_LE(n,b,i)                               \
{                                                          \
    (b)[(i)    ] = (unsigned char) (((n)      ) & 0xff);   \
    (b)[(i) + 1] = (unsigned char) (((n) >>  8) & 0xff);   \
    (b)[(i) + 2] = (unsigned char) (((n) >> 16) & 0xff);   \
    (b)[(i) + 3] = (unsigned char) (((n) >> 24) & 0xff);   \
}

#define ROTL8(x)  ((uint32_t)((x) <<  8) + (uint32_t)((x) >> 24))
#define ROTL16(x) ((uint32_t)((x) << 16) + (uint32_t)((x) >> 16))
#define ROTL24(x) ((uint32_t)((x) << 24) + (uint32_t)((x) >>  8))

#define AES_RT0(idx) rt0[idx]
#define AES_RT1(idx) ROTL8(rt0[idx])
#define AES_RT2(idx) ROTL16(rt0[idx])
#define AES_RT3(idx) ROTL24(rt0[idx])

#define AES_RROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)                 \
    do                                                      \
    {                                                       \
        (X0) = *RK++ ^ AES_RT0(((Y0)      ) & 0xFF) ^       \
                       AES_RT1(((Y3) >>  8) & 0xFF) ^       \
                       AES_RT2(((Y2) >> 16) & 0xFF) ^       \
                       AES_RT3(((Y1) >> 24) & 0xFF);        \
                                                            \
        (X1) = *RK++ ^ AES_RT0(((Y1)      ) & 0xFF) ^       \
                       AES_RT1(((Y0) >>  8) & 0xFF) ^       \
                       AES_RT2(((Y3) >> 16) & 0xFF) ^       \
                       AES_RT3(((Y2) >> 24) & 0xFF);        \
                                                            \
        (X2) = *RK++ ^ AES_RT0(((Y2)      ) & 0xFF) ^       \
                       AES_RT1(((Y1) >>  8) & 0xFF) ^       \
                       AES_RT2(((Y0) >> 16) & 0xFF) ^       \
                       AES_RT3(((Y3) >> 24) & 0xFF);        \
                                                            \
        (X3) = *RK++ ^ AES_RT0(((Y3)      ) & 0xFF) ^       \
                       AES_RT1(((Y2) >>  8) & 0xFF) ^       \
                       AES_RT2(((Y1) >> 16) & 0xFF) ^       \
                       AES_RT3(((Y0) >> 24) & 0xFF);        \
    } while( 0 )

#endif /* TC_USE_AES_TTABLES */

extern const uint8_t sbox[256];

int tc_aes128_set_decrypt_key(TCAesKeySched_t s, const uint8_t *k)
{
#ifndef TC_USE_AES_TTABLES

	return tc_aes128_set_encrypt_key(s, k);

#else

	int i, j;
	uint32_t buf[Nb * (Nr + 1)];
	uint32_t *RK = (uint32_t *)s->words;

	tc_aes128_set_encrypt_key(s, k);

	(void)_copy((uint8_t *)buf, sizeof(buf),
                (uint8_t *)s->words, sizeof(s->words));
	uint32_t *SK = &buf[Nr * 4];
	RK = (uint32_t *)s->words;

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

	for (i = Nr - 1, SK -= 8; i > 0; i--, SK -= 8) {
		for (j = 0; j < 4; j++, SK++) {
			*RK++ = AES_RT0(sbox[(*SK      ) & 0xFF]) ^
			        AES_RT1(sbox[(*SK >>  8) & 0xFF]) ^
			        AES_RT2(sbox[(*SK >> 16) & 0xFF]) ^
			        AES_RT3(sbox[(*SK >> 24) & 0xFF]);
		}
	}

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

#endif

	return TC_CRYPTO_SUCCESS;
}

#ifndef TC_USE_AES_TTABLES

#define mult8(a)(_double_byte(_double_byte(_double_byte(a))))
#define mult9(a)(mult8(a)^(a))
#define multb(a)(mult8(a)^_double_byte(a)^(a))
#define multd(a)(mult8(a)^_double_byte(_double_byte(a))^(a))
#define multe(a)(mult8(a)^_double_byte(_double_byte(a))^_double_byte(a))

static inline void mult_row_column(uint8_t *out, const uint8_t *in)
{
	out[0] = multe(in[0]) ^ multb(in[1]) ^ multd(in[2]) ^ mult9(in[3]);
	out[1] = mult9(in[0]) ^ multe(in[1]) ^ multb(in[2]) ^ multd(in[3]);
	out[2] = multd(in[0]) ^ mult9(in[1]) ^ multe(in[2]) ^ multb(in[3]);
	out[3] = multb(in[0]) ^ multd(in[1]) ^ mult9(in[2]) ^ multe(in[3]);
}

static inline void inv_mix_columns(uint8_t *s)
{
	uint8_t t[Nb*Nk];

	mult_row_column(t, s);
	mult_row_column(&t[Nb], s+Nb);
	mult_row_column(&t[2*Nb], s+(2*Nb));
	mult_row_column(&t[3*Nb], s+(3*Nb));
	(void)_copy(s, sizeof(t), t, sizeof(t));
}

static inline void add_round_key(uint8_t *s, const unsigned int *k)
{
	s[0] ^= (uint8_t)(k[0] >> 24); s[1] ^= (uint8_t)(k[0] >> 16);
	s[2] ^= (uint8_t)(k[0] >> 8); s[3] ^= (uint8_t)(k[0]);
	s[4] ^= (uint8_t)(k[1] >> 24); s[5] ^= (uint8_t)(k[1] >> 16);
	s[6] ^= (uint8_t)(k[1] >> 8); s[7] ^= (uint8_t)(k[1]);
	s[8] ^= (uint8_t)(k[2] >> 24); s[9] ^= (uint8_t)(k[2] >> 16);
	s[10] ^= (uint8_t)(k[2] >> 8); s[11] ^= (uint8_t)(k[2]);
	s[12] ^= (uint8_t)(k[3] >> 24); s[13] ^= (uint8_t)(k[3] >> 16);
	s[14] ^= (uint8_t)(k[3] >> 8); s[15] ^= (uint8_t)(k[3]);
}

static inline void inv_sub_bytes(uint8_t *s)
{
	unsigned int i;

	for (i = 0; i < (Nb*Nk); ++i) {
		s[i] = inv_sbox[s[i]];
	}
}

/*
 * This inv_shift_rows also implements the matrix flip required for
 * inv_mix_columns, but performs it here to reduce the number of memory
 * operations.
 */
static inline void inv_shift_rows(uint8_t *s)
{
	uint8_t t[Nb*Nk];

	t[0]  = s[0]; t[1] = s[13]; t[2] = s[10]; t[3] = s[7];
	t[4]  = s[4]; t[5] = s[1]; t[6] = s[14]; t[7] = s[11];
	t[8]  = s[8]; t[9] = s[5]; t[10] = s[2]; t[11] = s[15];
	t[12] = s[12]; t[13] = s[9]; t[14] = s[6]; t[15] = s[3];
	(void)_copy(s, sizeof(t), t, sizeof(t));
}

#endif

int tc_aes_decrypt(uint8_t *out, const uint8_t *in, const TCAesKeySched_t s)
{
	uint8_t state[Nk*Nb];
	unsigned int i;

	if (out == (uint8_t *) 0) {
		return TC_CRYPTO_FAIL;
	} else if (in == (const uint8_t *) 0) {
		return TC_CRYPTO_FAIL;
	} else if (s == (TCAesKeySched_t) 0) {
		return TC_CRYPTO_FAIL;
	}

	(void)_copy(state, sizeof(state), in, sizeof(state));

#ifndef TC_USE_AES_TTABLES

	add_round_key(state, s->words + Nb*Nr);

	for (i = Nr - 1; i > 0; --i) {
		inv_shift_rows(state);
		inv_sub_bytes(state);
		add_round_key(state, s->words + Nb*i);
		inv_mix_columns(state);
	}

	inv_shift_rows(state);
	inv_sub_bytes(state);
	add_round_key(state, s->words);

#else

	#define N_WORDS 4
	const unsigned int U32_SZ = 4;
	unsigned int j;
	uint32_t x[N_WORDS], y[N_WORDS];
	uint32_t *RK = (uint32_t *)s->words;

	for (i = 0; i < N_WORDS; i++) {
		GET_UINT32_LE(x[i], state, i * U32_SZ);
		x[i] ^= *RK++;
	}

	for(i = (Nr >> 1) - 1; i > 0; i--) {
		AES_RROUND(y[0], y[1], y[2], y[3], x[0], x[1], x[2], x[3]);
		AES_RROUND(x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]);
	}

	AES_RROUND(y[0], y[1], y[2], y[3], x[0], x[1], x[2], x[3]);

#define DEC(x) ((x) = (x) > 0 ? (x) - 1 : 3)

	for (i = 0, j = 0; i < N_WORDS; i++) {
		x[i]  = *RK++;
		x[i] ^= (uint32_t)inv_sbox[ y[j]        & 0xff];       DEC(j);
		x[i] ^= (uint32_t)inv_sbox[(y[j] >>  8) & 0xff] <<  8; DEC(j);
		x[i] ^= (uint32_t)inv_sbox[(y[j] >> 16) & 0xff] << 16; DEC(j);
		x[i] ^= (uint32_t)inv_sbox[(y[j] >> 24) & 0xff] << 24; /* no roll */
	}

	for (i = 0; i < N_WORDS; i++) {
		PUT_UINT32_LE(x[i], state, i * U32_SZ);
	}

#endif

	(void)_copy(out, sizeof(state), state, sizeof(state));

	/*zeroing out the state buffer */
	_set(state, TC_ZERO_BYTE, sizeof(state));


	return TC_CRYPTO_SUCCESS;
}
