// SPDX-License-Identifier: BSD-2-Clause AND MIT
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommyhash.h"

/******************************************************************************/
/* hash */

#include <string.h> /* for memcpy */

/**
 * Swap endianness.
 * They are needed only if BigEndian.
 */
#if defined(__GNUC__)
#define tommy_imp_hash_swap32(x) __builtin_bswap32(x)
#define tommy_imp_hash_swap64(x) __builtin_bswap64(x)
#else
tommy_inline tommy_uint32_t tommy_imp_hash_swap32(tommy_uint32_t v)
{
	return ((v & 0xFF000000) >> 24) |
	       ((v & 0x00FF0000) >> 8) |
	       ((v & 0x0000FF00) << 8) |
	       ((v & 0x000000FF) << 24);
}

tommy_inline tommy_uint64_t tommy_imp_hash_swap64(tommy_uint64_t v)
{
	return ((v & 0xFF00000000000000ULL) >> 56) |
	       ((v & 0x00FF000000000000ULL) >> 40) |
	       ((v & 0x0000FF0000000000ULL) >> 24) |
	       ((v & 0x000000FF00000000ULL) >> 8) |
	       ((v & 0x00000000FF000000ULL) << 8) |
	       ((v & 0x0000000000FF0000ULL) << 24) |
	       ((v & 0x000000000000FF00ULL) << 40) |
	       ((v & 0x00000000000000FFULL) << 56);
}
#endif

tommy_inline tommy_uint32_t tommy_imp_hash_le_uint32_read(const void* ptr)
{
	tommy_uint32_t v;
	memcpy(&v, ptr, sizeof(v));
#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) || \
	(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	v = tommy_imp_hash_swap32(v);
#endif
	return v;
}

tommy_inline tommy_uint64_t tommy_imp_hash_le_uint64_read(const void* ptr)
{
	tommy_uint64_t v;
	memcpy(&v, ptr, sizeof(v));
#if defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) || \
	(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	v = tommy_imp_hash_swap64(v);
#endif
	return v;
}

/******************************************************************************/
/* a5hash */

/**
 * a5hash32 by Aleksey Vaneev, from https://github.com/avaneev/a5hash.
 * Adapted from version 5.25 with little-endian word reads.
 * Copyright (c) 2025 Aleksey Vaneev. Released under the MIT license, see LICENSES/MIT.
 */

tommy_inline void tommy_imp_a5hash_mul(tommy_uint32_t a, tommy_uint32_t b, tommy_uint32_t* lo, tommy_uint32_t* hi)
{
	/* both operands are 32-bit; the full product supplies two state words */
	tommy_uint64_t v = (tommy_uint64_t)a * b;
	*lo = (tommy_uint32_t)v;
	*hi = v >> 32;
}

TOMMY_API tommy_uint32_t tommy_hash_u32(tommy_uint32_t init_val, const void* void_key, tommy_size_t key_len)
{
	const unsigned char* key = tommy_cast(const unsigned char*, void_key);
	tommy_uint32_t val01 = 0x55555555;
	tommy_uint32_t val10 = 0xaaaaaaaa;
	tommy_uint32_t h[4] = { 0x243f6a88, 0x85a308d3, 0xfb0bd3ea, 0x0f58fd47 };
	tommy_uint32_t a, b;
	tommy_size_t len = key_len;

	h[0] ^= key_len;
	h[1] ^= key_len;
#if TOMMY_SIZE_BIT == 64
	tommy_imp_a5hash_mul((key_len >> 32) ^ 0x452821e6, (key_len >> 32) ^ 0x38d01377, &h[2], &h[3]);
#endif
	tommy_imp_a5hash_mul(h[1] ^ (init_val & val10), h[0] ^ (init_val & val01), &h[0], &h[1]);

	if (len <= 16) {
		if (len >= 4) {
			const unsigned char* tail = key + len - 4;
			a = tommy_imp_hash_le_uint32_read(key);
			b = tommy_imp_hash_le_uint32_read(tail);
			if (len >= 9) {
				tommy_size_t offset = (len / 8) * 4;
				tommy_uint32_t c = tommy_imp_hash_le_uint32_read(key + offset);
				tommy_uint32_t d = tommy_imp_hash_le_uint32_read(tail - offset);
				tommy_imp_a5hash_mul(c + h[2], d + h[3], &h[2], &h[3]);
			}
		} else {
			a = 0;
			b = 0;
			if (len != 0) {
				a = key[0];
				if (len >= 2)
					a |= key[1] << 8;
				if (len == 3)
					a |= key[2] << 16;
			}
		}
	} else {
		val01 ^= h[0];
		val10 ^= h[1];
		do {
			tommy_uint32_t s1 = h[0];
			tommy_uint32_t s4 = h[3];
			tommy_imp_a5hash_mul(tommy_imp_hash_le_uint32_read(key) + h[0], tommy_imp_hash_le_uint32_read(key + 4) + h[1], &h[0], &h[1]);
			tommy_imp_a5hash_mul(tommy_imp_hash_le_uint32_read(key + 8) + h[2], tommy_imp_hash_le_uint32_read(key + 12) + h[3], &h[2], &h[3]);
			h[0] += val01;
			h[1] += s4;
			h[2] += s1;
			h[3] += val10;
			key += 16;
			len -= 16;
		} while (len > 16);

		/* the last words may overlap the previous block, but stay within the input */
		const unsigned char* tail = key + len;
		a = tommy_imp_hash_le_uint32_read(tail - 8);
		b = tommy_imp_hash_le_uint32_read(tail - 4);
		if (len >= 9) {
			tommy_uint32_t c = tommy_imp_hash_le_uint32_read(tail - 16);
			tommy_uint32_t d = tommy_imp_hash_le_uint32_read(tail - 12);
			tommy_imp_a5hash_mul(c + h[2], d + h[3], &h[2], &h[3]);
		}
	}

	h[0] ^= h[2];
	h[1] ^= h[3];
	tommy_imp_a5hash_mul(a + h[0], b + h[1], &h[0], &h[1]);
	tommy_imp_a5hash_mul(val01 ^ h[0], h[1], &a, &b);
	return a ^ b;
}

/******************************************************************************/
/* rapidhashnano */

/**
 * rapidhashnano (64-bit) by Nicolas De Carli, from https://github.com/Nicoshev/rapidhash.
 * Adapted from rapidhash V3 with little-endian word reads.
 * Copyright (C) 2025 Nicolas De Carli. Released under the MIT license.
 */

tommy_inline tommy_uint64_t tommy_imp_rapid_mix(tommy_uint64_t a, tommy_uint64_t b)
{
	tommy_mum(&a, &b);
	return a ^ b;
}

TOMMY_API tommy_uint64_t tommy_hash_u64(tommy_uint64_t init_val, const void* void_key, tommy_size_t key_len)
{
	const unsigned char* key = tommy_cast(const unsigned char*, void_key);
	static const tommy_uint64_t secret[8] = {
		0x2d358dccaa6c78a5ULL,
		0x8bb84b93962eacc9ULL,
		0x4b33a62ed433d4a3ULL,
		0x4d5a2da51de1aa47ULL,
		0xa0761d6478bd642fULL,
		0xe7037ed1a0b428dbULL,
		0x90ed1765281c388cULL,
		0xaaaaaaaaaaaaaaaaULL
	};
	tommy_uint64_t seed = init_val;
	tommy_uint64_t a = 0;
	tommy_uint64_t b = 0;
	tommy_size_t len = key_len;

	seed ^= tommy_imp_rapid_mix(seed ^ secret[2], secret[1]);

	if (len <= 16) {
		if (len >= 4) {
			seed ^= len;
			if (len >= 8) {
				const unsigned char* tail = key + len - 8;
				a = tommy_imp_hash_le_uint64_read(key);
				b = tommy_imp_hash_le_uint64_read(tail);
			} else {
				const unsigned char* tail = key + len - 4;
				a = tommy_imp_hash_le_uint32_read(key);
				b = tommy_imp_hash_le_uint32_read(tail);
			}
		} else if (len != 0) {
			a = ((tommy_uint64_t)key[0] << 45) | key[len - 1];
			b = key[len >> 1];
		}
	} else {
		if (len > 48) {
			tommy_uint64_t see1 = seed;
			tommy_uint64_t see2 = seed;
			do {
				seed = tommy_imp_rapid_mix(tommy_imp_hash_le_uint64_read(key) ^ secret[0], tommy_imp_hash_le_uint64_read(key + 8) ^ seed);
				see1 = tommy_imp_rapid_mix(tommy_imp_hash_le_uint64_read(key + 16) ^ secret[1], tommy_imp_hash_le_uint64_read(key + 24) ^ see1);
				see2 = tommy_imp_rapid_mix(tommy_imp_hash_le_uint64_read(key + 32) ^ secret[2], tommy_imp_hash_le_uint64_read(key + 40) ^ see2);
				key += 48;
				len -= 48;
			} while (len > 48);
			seed ^= see1;
			seed ^= see2;
		}
		if (len > 16) {
			seed = tommy_imp_rapid_mix(tommy_imp_hash_le_uint64_read(key) ^ secret[2], tommy_imp_hash_le_uint64_read(key + 8) ^ seed);
			if (len > 32)
				seed = tommy_imp_rapid_mix(tommy_imp_hash_le_uint64_read(key + 16) ^ secret[2], tommy_imp_hash_le_uint64_read(key + 24) ^ seed);
		}
		a = tommy_imp_hash_le_uint64_read(key + len - 16) ^ len;
		b = tommy_imp_hash_le_uint64_read(key + len - 8);
	}

	a ^= secret[1];
	b ^= seed;
	tommy_mum(&a, &b);
	return tommy_imp_rapid_mix(a ^ secret[7], b ^ secret[1] ^ len);
}

