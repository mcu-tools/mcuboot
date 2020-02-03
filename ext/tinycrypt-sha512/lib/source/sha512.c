/* sha512.c - TinyCrypt SHA-512 crypto hash algorithm implementation */

/*
 *  Copyright (C) 2020 by Intel Corporation, All Rights Reserved.
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

#include <tinycrypt/sha512.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/utils.h>

static void compress(uint64_t *iv, const uint8_t *data);

int tc_sha512_init(TCSha512State_t s)
{
	/* input sanity check: */
	if (s == (TCSha512State_t) 0) {
		return TC_CRYPTO_FAIL;
	}

	/*
	 * Setting the initial state values.
	 * These values correspond to the first 64 bits of the fractional parts
	 * of the square roots of the first 8 primes: 2, 3, 5, 7, 11, 13, 17
	 * and 19.
	 */
	_set((uint8_t *) s, 0x00, sizeof(*s));
	s->iv[0] = 0x6a09e667f3bcc908;
	s->iv[1] = 0xbb67ae8584caa73b;
	s->iv[2] = 0x3c6ef372fe94f82b;
	s->iv[3] = 0xa54ff53a5f1d36f1;
	s->iv[4] = 0x510e527fade682d1;
	s->iv[5] = 0x9b05688c2b3e6c1f;
	s->iv[6] = 0x1f83d9abfb41bd6b;
	s->iv[7] = 0x5be0cd19137e2179;

	return TC_CRYPTO_SUCCESS;
}

int tc_sha512_update(TCSha512State_t s, const uint8_t *data, size_t datalen)
{
	/* input sanity check: */
	if (s == (TCSha512State_t) 0 || data == (void *) 0) {
		return TC_CRYPTO_FAIL;
	} else if (datalen == 0) {
		return TC_CRYPTO_SUCCESS;
	}

	while (datalen-- > 0) {
		s->leftover[s->leftover_offset++] = *(data++);
		if (s->leftover_offset >= TC_SHA512_BLOCK_SIZE) {
			compress(s->iv, s->leftover);
			s->leftover_offset = 0;
			s->bits_hashed += (TC_SHA512_BLOCK_SIZE << 3);
		}
	}

	return TC_CRYPTO_SUCCESS;
}

int tc_sha512_final(uint8_t *digest, TCSha512State_t s)
{
	unsigned int i;

	/* input sanity check: */
	if (digest == (uint8_t *) 0 || s == (TCSha512State_t) 0) {
		return TC_CRYPTO_FAIL;
	}

	s->bits_hashed += (s->leftover_offset << 3);

	s->leftover[s->leftover_offset++] = 0x80; /* always room for one byte */
	if (s->leftover_offset > (sizeof(s->leftover) - 16)) {
		/* there is not room for all the padding in this block */
		_set(s->leftover + s->leftover_offset, 0x00,
		     sizeof(s->leftover) - s->leftover_offset);
		compress(s->iv, s->leftover);
		s->leftover_offset = 0;
	}

	/*
	 * add the padding and the length in big-Endian format
	 *
	 * NOTE: SHA-512 uses 128 bits for the length of the message, but the
	 * current implementation is only using 64 bits for size, leaving the
	 * 64 "upper" bits zeroed.
	 */
	_set(s->leftover + s->leftover_offset, 0x00,
	     sizeof(s->leftover) - 8 - s->leftover_offset);
	s->leftover[sizeof(s->leftover) - 1]  = (uint8_t)(s->bits_hashed);
	s->leftover[sizeof(s->leftover) - 2]  = (uint8_t)(s->bits_hashed >> 8);
	s->leftover[sizeof(s->leftover) - 3]  = (uint8_t)(s->bits_hashed >> 16);
	s->leftover[sizeof(s->leftover) - 4]  = (uint8_t)(s->bits_hashed >> 24);
	s->leftover[sizeof(s->leftover) - 5]  = (uint8_t)(s->bits_hashed >> 32);
	s->leftover[sizeof(s->leftover) - 6]  = (uint8_t)(s->bits_hashed >> 40);
	s->leftover[sizeof(s->leftover) - 7]  = (uint8_t)(s->bits_hashed >> 48);
	s->leftover[sizeof(s->leftover) - 8]  = (uint8_t)(s->bits_hashed >> 56);

	/* hash the padding and length */
	compress(s->iv, s->leftover);

	/* copy the iv out to digest */
	for (i = 0; i < TC_SHA512_STATE_BLOCKS; ++i) {
		uint64_t t = *((uint64_t *) &s->iv[i]);
		*digest++ = (uint8_t)(t >> 56);
		*digest++ = (uint8_t)(t >> 48);
		*digest++ = (uint8_t)(t >> 40);
		*digest++ = (uint8_t)(t >> 32);
		*digest++ = (uint8_t)(t >> 24);
		*digest++ = (uint8_t)(t >> 16);
		*digest++ = (uint8_t)(t >> 8);
		*digest++ = (uint8_t)(t);
	}

	/* destroy the current state */
	_set(s, 0, sizeof(*s));

	return TC_CRYPTO_SUCCESS;
}

/*
 * Initializing SHA-512 Hash constant words K.
 * These values correspond to the first 64 bits of the fractional parts of the
 * cube roots of the first 80 primes between 2 and 409.
 */
static const uint64_t k512[80] = {
	0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc, 0x3956c25bf348b538,
	0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242, 0x12835b0145706fbe,
	0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
	0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
	0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5, 0x983e5152ee66dfab,
	0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
	0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed,
	0x53380d139d95b3df, 0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
	0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
	0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8, 0x19a4c116b8d2d0c8, 0x1e376c085141ab53,
	0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373,
	0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
	0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b, 0xca273eceea26619c,
	0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba, 0x0a637dc5a2c898a6,
	0x113f9804bef90dae, 0x1b710b35131c471b, 0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
	0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

static inline uint64_t ROTR(uint64_t a, uint64_t n)
{
	return (((a) >> n) | ((a) << (64 - n)));
}

#define Sigma0(a)(ROTR((a), 28) ^ ROTR((a), 34) ^ ROTR((a), 39))
#define Sigma1(a)(ROTR((a), 14) ^ ROTR((a), 18) ^ ROTR((a), 41))
#define sigma0(a)(ROTR((a), 1) ^ ROTR((a), 8) ^ ((a) >> 7))
#define sigma1(a)(ROTR((a), 19) ^ ROTR((a), 61) ^ ((a) >> 6))

#define Ch(a, b, c)(((a) & (b)) ^ ((~(a)) & (c)))
#define Maj(a, b, c)(((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))

static inline uint64_t BigEndian(const uint8_t **c)
{
	uint64_t n = 0;

	n  = (uint64_t)(*((*c)++)) << 56;
	n |= (uint64_t)(*((*c)++)) << 48;
	n |= (uint64_t)(*((*c)++)) << 40;
	n |= (uint64_t)(*((*c)++)) << 32;
	n |= (uint64_t)(*((*c)++)) << 24;
	n |= (uint64_t)(*((*c)++)) << 16;
	n |= (uint64_t)(*((*c)++)) << 8;
	n |= (uint64_t)(*((*c)++));
	return n;
}

static void compress(uint64_t *iv, const uint8_t *data)
{
	uint64_t a, b, c, d, e, f, g, h;
	uint64_t s0, s1;
	uint64_t t1, t2;
	uint64_t work_space[16];
	uint64_t n;
	unsigned int i;

	a = iv[0]; b = iv[1]; c = iv[2]; d = iv[3];
	e = iv[4]; f = iv[5]; g = iv[6]; h = iv[7];

	for (i = 0; i < 16; ++i) {
		n = BigEndian(&data);
		t1 = work_space[i] = n;
		t1 += h + Sigma1(e) + Ch(e, f, g) + k512[i];
		t2 = Sigma0(a) + Maj(a, b, c);
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	for ( ; i < 80; ++i) {
		s0 = work_space[(i+1)&0x0f];
		s0 = sigma0(s0);
		s1 = work_space[(i+14)&0x0f];
		s1 = sigma1(s1);

		t1 = work_space[i&0xf] += s0 + s1 + work_space[(i+9)&0xf];
		t1 += h + Sigma1(e) + Ch(e, f, g) + k512[i];
		t2 = Sigma0(a) + Maj(a, b, c);
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	iv[0] += a; iv[1] += b; iv[2] += c; iv[3] += d;
	iv[4] += e; iv[5] += f; iv[6] += g; iv[7] += h;
}
