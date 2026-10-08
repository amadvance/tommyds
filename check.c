// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/**
 * Tommy check program.
 *
 * Simply run it without any options. If it terminates printing "OK" all the
 * checks are succesful.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#if defined(__linux)
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#if defined(__MACH__)
#include <mach/mach_time.h>
#endif

#include "tommyds/tommy.h"

#define TOMMY_SIZE 1000000

#define PAYLOAD 16 /**< Size of payload data for objects */

struct object {
	int value;
	tommy_node node;
	char payload[PAYLOAD];
};

unsigned compare_counter;

int compare(const void* void_a, const void* void_b)
{
	const struct object* a = void_a;
	const struct object* b = void_b;

	++compare_counter;

	if (a->value < b->value)
		return -1;
	if (a->value > b->value)
		return 1;
	return 0;
}

struct object_vector {
	int value;
	char payload[PAYLOAD];
};

int compare_vector(const void* void_a, const void* void_b)
{
	const struct object_vector* a = void_a;
	const struct object_vector* b = void_b;

	++compare_counter;

	if (a->value < b->value)
		return -1;
	if (a->value > b->value)
		return 1;
	return 0;
}

struct object_hash {
	int value;
	tommy_node node;
	char payload[PAYLOAD];
};

struct object_tree {
	int value;
	tommy_tree_node node;
	char payload[PAYLOAD];
};

int compare_tree_reverse(const void* void_a, const void* void_b)
{
	const struct object_tree* a = void_a;
	const struct object_tree* b = void_b;

	if (a->value > b->value)
		return -1;
	if (a->value < b->value)
		return 1;
	return 0;
}

int compare_tree_group(const void* void_a, const void* void_b)
{
	const struct object_tree* a = void_a;
	const struct object_tree* b = void_b;

	if (a->value / 2 < b->value / 2)
		return -1;
	if (a->value / 2 > b->value / 2)
		return 1;
	return 0;
}

int compare_tree_int(const void* void_a, const void* void_b)
{
	const int* a = void_a;
	const struct object_tree* b = void_b;

	if (*a < b->value)
		return -1;
	if (*a > b->value)
		return 1;
	return 0;
}

struct object_trie {
	int value;
	tommy_trie_node node;
	char payload[PAYLOAD];
};

struct object_trie_inplace {
	int value;
	tommy_trie_inplace_node node;
	char payload[PAYLOAD];
};

/******************************************************************************/
/* time */

#if defined(_WIN32)
static LARGE_INTEGER win_frequency;
#endif

static void nano_init(void)
{
#if defined(_WIN32)
	if (!QueryPerformanceFrequency(&win_frequency)) {
		win_frequency.QuadPart = 0;
	}
#endif
}

static tommy_uint64_t nano(void)
{
#if defined(_WIN32)
	LARGE_INTEGER t;

	if (!QueryPerformanceCounter(&t))
		return 0;

	tommy_uint64_t ret = (t.QuadPart / win_frequency.QuadPart) * 1000000000;

	ret += (t.QuadPart % win_frequency.QuadPart) * 1000000000 / win_frequency.QuadPart;
	return ret;
#elif defined(__MACH__)
	mach_timebase_info_data_t info;
	tommy_uint64_t t = mach_absolute_time();

	kern_return_t r = mach_timebase_info(&info);
	if (r != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_uint64_t ret = (t / info.denom) * info.numer;

	ret += (t % info.denom) * info.numer / info.denom;
	return ret;
#elif defined(__linux)
	struct timespec ts;

	int r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	return ts.tv_sec * (tommy_uint64_t)1000000000 + ts.tv_nsec;
#else
	struct timeval tv;

	int r = gettimeofday(&tv, 0);
	if (r != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	return tv.tv_sec * (tommy_uint64_t)1000000000 + tv.tv_usec * 1000;
#endif
}

/******************************************************************************/
/* random */

/**
 * Pseudo random number generator.
 * Note that using (rand() % max) in Visual C results in totally bogus values,
 * with *strong* cache effects when accessing elements in a not really random order.
 * This happen because Visual C uses a simple linear congruential generator with only 32 bits.
 */
tommy_uint64_t SEED = 0;

unsigned rnd(unsigned max)
{
	/* linear congruential generator from MMIX by Donald Knuth, http://en.wikipedia.org/wiki/Linear_congruential_generator */
#ifdef _MSC_VER
	tommy_uint64_t divider = 0xFFFFFFFFFFFFFFFF / max;
#else
	tommy_uint64_t divider = 0xFFFFFFFFFFFFFFFFULL / max;
#endif

	while (1) {
#ifdef _MSC_VER
		SEED = SEED * 6364136223846793005 + 1442695040888963407;
#else
		SEED = SEED * 6364136223846793005LL + 1442695040888963407LL;
#endif

		unsigned r = (unsigned)(SEED / divider);

		/* it may happen as the divider is approximated down */
		if (r < max)
			return r;
	}
}

/******************************************************************************/
/* helper */

unsigned isqrt(unsigned n)
{
	unsigned root = 0;
	unsigned remain = n;
	unsigned place = 0x40000000;

	while (place > remain)
		place /= 4;

	while (place) {
		if (remain >= root + place) {
			remain -= root + place;
			root += 2 * place;
		}

		root /= 2;
		place /= 4;
	}

	return root;
}

/**
 * Cache clearing buffer.
 */
static unsigned char the_cache[16 * 1024 * 1024];
static const char* the_str;
static tommy_uint64_t the_start;

void cache_clear(void)
{
	/* read & write */
	for (unsigned i = 0; i < sizeof(the_cache); i += 32)
		the_cache[i] += 1;

#ifdef WIN32
	Sleep(0);
#endif
}

void start(const char* str)
{
	cache_clear();
	compare_counter = 0;
	the_str = str;
	the_start = nano();
}

void stop()
{
	tommy_uint64_t the_stop = nano();
	printf("%25s %8u [ms], %8u [compare]\n", the_str, (unsigned)((the_stop - the_start) / 1000000), compare_counter);
}

#define START(s) start(s)
#define STOP() stop()

/******************************************************************************/
/* test */

static unsigned the_count;

static void count_callback(void* data)
{
	(void)data;
	++the_count;
}

static void count_arg_callback(void* arg, void* data)
{
	unsigned* count = arg;
	(void)data;
	++*count;
}

static void array_foreach_check_callback(void* data)
{
	if (data != (void*)(tommy_uintptr_t)(the_count + 1)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	++the_count;
}

static void array_foreach_check_arg_callback(void* arg, void* data)
{
	tommy_uintptr_t* expected = arg;
	if (data != (void*)(*expected + 1)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	++*expected;
}

static void arrayof_foreach_check_callback(void* data)
{
	unsigned* val = data;
	if (*val != the_count + 1) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	*val = *val * 2;
	++the_count;
}

static void arrayof_foreach_check_arg_callback(void* arg, void* data)
{
	unsigned* expected = arg;
	unsigned* val = data;
	if (*val != (*expected + 1) * 2) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	++*expected;
}

static int search_callback(const void* arg, const void* obj)
{
	return arg != obj;
}

static int search_hash_value_callback(const void* arg, const void* obj)
{
	const struct object_hash* object = obj;
	++compare_counter;
	return *(const int*)arg != object->value;
}

struct hash32_test {
	char* data;
	tommy_uint32_t len;
	tommy_uint32_t hash;
} HASH32[] = {
	{ "", 0, 0x3ba63d24 },
	{ "a", 1, 0xc27589e2 },
	{ "abc", 3, 0x4f0589dd },
	{ "message digest", 14, 0x7bd2e191 },
	{ "abcdefghijklmnopqrstuvwxyz", 26, 0xa1eebba8 },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62, 0xc84d4c51 },
	{ "The quick brown fox jumps over the lazy dog", 43, 0x294e8a7b },
	{ "\x00", 1, 0x3e1fbed8 },
	{ "\x16\x27", 2, 0x00233fc6 },
	{ "\xe2\x56\xb4", 3, 0xd3f61ec1 },
	{ "\xc9\x4d\x9c\xda", 4, 0xfa10da80 },
	{ "\x79\xf1\x29\x69\x5d", 5, 0xcd5e7468 },
	{ "\x00\x7e\xdf\x1e\x31\x1c", 6, 0x8ba76f9b },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 7, 0xbf2129d0 },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 8, 0xc3d26370 },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 9, 0xa6840538 },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 10, 0x36115feb },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\x00\xb7\xae", 11, 0x48a3b1fb },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 12, 0x23412f54 },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 13, 0x04332eac },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 14, 0xd3025b16 },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 15, 0x4ce44257 },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 16, 0x47d86f33 },
	{ 0, 0, 0 }
};

struct strhash32_test {
	char* data;
	tommy_uint32_t hash;
} STRHASH32[] = {
	{ "", 0x3ba63d24 },
	{ "a", 0xc27589e2 },
	{ "abc", 0x4f0589dd },
	{ "message digest", 0x7bd2e191 },
	{ "abcdefghijklmnopqrstuvwxyz", 0xa1eebba8 },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0xc84d4c51 },
	{ "The quick brown fox jumps over the lazy dog", 0x294e8a7b },
	{ "\xff", 0x6ae84401 },
	{ "\x16\x27", 0x00233fc6 },
	{ "\xe2\x56\xb4", 0xd3f61ec1 },
	{ "\xc9\x4d\x9c\xda", 0xfa10da80 },
	{ "\x79\xf1\x29\x69\x5d", 0xcd5e7468 },
	{ "\xff\x7e\xdf\x1e\x31\x1c", 0xe135b2e2 },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 0xbf2129d0 },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 0xc3d26370 },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 0xa6840538 },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 0x36115feb },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\xff\xb7\xae", 0xef961a79 },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 0x23412f54 },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 0x04332eac },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 0xd3025b16 },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 0x4ce44257 },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 0x47d86f33 },
	{ 0, 0 }
};

struct strhash64_test {
	char* data;
	tommy_uint64_t hash;
} STRHASH64[] = {
	{ "", 0xf1999eea212f89e6ULL },
	{ "a", 0x6af55c57ef3cc6e2ULL },
	{ "abc", 0x8a09859573f3ba72ULL },
	{ "message digest", 0x08a48193f341bef5ULL },
	{ "abcdefghijklmnopqrstuvwxyz", 0x8761b9f83f99b36dULL },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0x6f9a23bb4217ad12ULL },
	{ "The quick brown fox jumps over the lazy dog", 0x33e3651d073f9942ULL },
	{ "\xff", 0xfcb8ac990a86ae22ULL },
	{ "\x16\x27", 0x9d411c7d088f8c1eULL },
	{ "\xe2\x56\xb4", 0xa20865547db581afULL },
	{ "\xc9\x4d\x9c\xda", 0xb4eb664b81e7058dULL },
	{ "\x79\xf1\x29\x69\x5d", 0xf0b386ad052571b4ULL },
	{ "\xff\x7e\xdf\x1e\x31\x1c", 0x366d27a142088121ULL },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 0xa8b94da524aabf49ULL },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 0xb913a803aea6d51dULL },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 0xb6961500a25f63beULL },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 0xfa6dccb9650a1d28ULL },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\xff\xb7\xae", 0x4bdd7dea7cd363a4ULL },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 0x15901950afd60cd2ULL },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 0xf1affd3121a833b1ULL },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 0x59d83bcc1fa82a8fULL },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 0xd77da4fa5676c552ULL },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 0xa02e9cfd1b99ae12ULL },
	{ 0, 0 }
};

struct hash64_test {
	char* data;
	tommy_uint32_t len;
	tommy_uint64_t hash;
} HASH64[] = {
	{ "", 0, 0xf1999eea212f89e6ULL },
	{ "a", 1, 0x6af55c57ef3cc6e2ULL },
	{ "abc", 3, 0x8a09859573f3ba72ULL },
	{ "message digest", 14, 0x08a48193f341bef5ULL },
	{ "abcdefghijklmnopqrstuvwxyz", 26, 0x8761b9f83f99b36dULL },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62, 0x6f9a23bb4217ad12ULL },
	{ "The quick brown fox jumps over the lazy dog", 43, 0x33e3651d073f9942ULL },
	{ "\x00", 1, 0x9360785dad01a14eULL },
	{ "\x16\x27", 2, 0x9d411c7d088f8c1eULL },
	{ "\xe2\x56\xb4", 3, 0xa20865547db581afULL },
	{ "\xc9\x4d\x9c\xda", 4, 0xb4eb664b81e7058dULL },
	{ "\x79\xf1\x29\x69\x5d", 5, 0xf0b386ad052571b4ULL },
	{ "\x00\x7e\xdf\x1e\x31\x1c", 6, 0xdd3b8851d859c7d4ULL },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 7, 0xa8b94da524aabf49ULL },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 8, 0xb913a803aea6d51dULL },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 9, 0xb6961500a25f63beULL },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 10, 0xfa6dccb9650a1d28ULL },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\x00\xb7\xae", 11, 0xc95d0a87ec8c2a98ULL },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 12, 0x15901950afd60cd2ULL },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 13, 0xf1affd3121a833b1ULL },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 14, 0x59d83bcc1fa82a8fULL },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 15, 0xd77da4fa5676c552ULL },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 16, 0xa02e9cfd1b99ae12ULL },
	{ 0, 0, 0 }
};

struct inthash32_test {
	tommy_uint32_t value;
	tommy_uint32_t hash;
} INTHASH32[] = {
	{ 0x00000000, 0x00000000 },
	{ 0x00000001, 0xc2b73583 },
	{ 0x00000002, 0xe90f1258 },
	{ 0x00000004, 0x7a10c2d3 },
	{ 0x00000008, 0x200c3457 },
	{ 0x00000010, 0xeb97690a },
	{ 0x00000020, 0x7fb291d3 },
	{ 0x00000040, 0xf50601d8 },
	{ 0x00000080, 0x727dbaed },
	{ 0x00000100, 0x7ef5f77d },
	{ 0x00000200, 0x91a480dc },
	{ 0x00000400, 0x2bad9acc },
	{ 0x00000800, 0xfe4d150e },
	{ 0x00001000, 0xc3add476 },
	{ 0x00002000, 0x23946174 },
	{ 0x00004000, 0x987cfc43 },
	{ 0x00008000, 0x630cdf68 },
	{ 0x00010000, 0x0ac3a767 },
	{ 0x00020000, 0xad086d5b },
	{ 0x00040000, 0x1126ccdf },
	{ 0x00080000, 0x4370dbc4 },
	{ 0x00100000, 0xefd6e5e6 },
	{ 0x00200000, 0x9a93c1b5 },
	{ 0x00400000, 0x10114902 },
	{ 0x00800000, 0x96117e60 },
	{ 0x01000000, 0x5dec9f58 },
	{ 0x02000000, 0xfee234c7 },
	{ 0x04000000, 0x36137e26 },
	{ 0x08000000, 0x6c26fc4c },
	{ 0x10000000, 0xd84df898 },
	{ 0x20000000, 0xb099f131 },
	{ 0x40000000, 0x6131e262 },
	{ 0x80000000, 0xc263c4c4 },
	{ 0x00204a16, 0xd8b97461 },
	{ 0x05542a27, 0x65d0057a },
	{ 0x169c39e2, 0x7c2ff59a },
	{ 0x2eab4956, 0xa8ba89bd },
	{ 0x0bb0b8b4, 0xb790c8de },
	{ 0x0bd068c9, 0x92d30546 },
	{ 0x3e5d224d, 0xd610bf1d },
	{ 0x436c8d9c, 0x27b09019 },
	{ 0x3a2adfda, 0xdfde9385 },
	{ 0x1dd8ca79, 0xc7c10d7b },
	{ 0x6a67c4f1, 0xf92a788d },
	{ 0x7742fa29, 0x892c3519 },
	{ 0x48b62d69, 0x7f642f55 },
	{ 0x472e195d, 0xea2c49e5 },
	{ 0x0681a900, 0x4de5c929 },
	{ 0x622ebb7e, 0x35a7e306 },
	{ 0x026bccdf, 0xa7e4b630 },
	{ 0x204d531e, 0x43ebd664 },
	{ 0x262b5331, 0x7a9a161f },
	{ 0x7020241c, 0xbaed3ef7 },
	{ 0x440a0e2a, 0x0b8c5b29 },
	{ 0x75cb1c4c, 0x19555414 },
	{ 0x41f9a5e1, 0xc6acbc6b },
	{ 0x67bc26ff, 0x54e4411f },
	{ 0x181e279e, 0x979c834f },
	{ 0x7172c06f, 0xe6a179ff },
	{ 0x4909e153, 0x198e5e0f },
	{ 0x09d3bfba, 0x109a6f17 },
	{ 0x685ae502, 0xf9d57a4b },
	{ 0x7e10e8ab, 0x09765bec },
	{ 0x0f262618, 0xe16404cd },
	{ 0x726b8230, 0x55e478b4 },
	{ 0, 0 }
};

struct inthash64_test {
	tommy_uint64_t value;
	tommy_uint64_t hash;
} INTHASH64[] = {
	{ 0x0000000000000000ULL, 0x77cfa1eef01bca90ULL },
	{ 0x0000000000000001ULL, 0x5bca7c69b794f8ceULL },
	{ 0x0000000000000002ULL, 0xb795033f6f2a0674ULL },
	{ 0x0000000000000004ULL, 0x6f2a25235e544a31ULL },
	{ 0x0000000000000008ULL, 0xde543f7b3ca87ecbULL },
	{ 0x0000000000000010ULL, 0xbca87eec7950fd82ULL },
	{ 0x0000000000000020ULL, 0x7950fddef2a1fb10ULL },
	{ 0x0000000000000040ULL, 0xf2a1fbbde543f620ULL },
	{ 0x0000000000000080ULL, 0xe543f9324a87efadULL },
	{ 0x0000000000000100ULL, 0xca87f25e950fdf4eULL },
	{ 0x0000000000000200ULL, 0x950fe4bd2a1fbe9cULL },
	{ 0x0000000000000400ULL, 0x2a1fc968543f7d14ULL },
	{ 0x0000000000000800ULL, 0x543f92d0a87efa28ULL },
	{ 0x0000000000001000ULL, 0xa87f259f50fdf44cULL },
	{ 0x0000000000002000ULL, 0x50fe4b3ea1fbe898ULL },
	{ 0x0000000000004000ULL, 0xa1fc967bc3f7d12dULL },
	{ 0x0000000000008000ULL, 0x43f92da207efa3afULL },
	{ 0x0000000000010000ULL, 0x87f25b4e8fdf4773ULL },
	{ 0x0000000000020000ULL, 0x0fe4b6a71fbe8efaULL },
	{ 0x0000000000040000ULL, 0x1fc96d4ebf7d1df5ULL },
	{ 0x0000000000080000ULL, 0x3f92da9dfefa3bebULL },
	{ 0x0000000000100000ULL, 0x7f25b53c7df477d7ULL },
	{ 0x0000000000200000ULL, 0xfe4b6a797be8efafULL },
	{ 0x0000000000400000ULL, 0xfc96d4f377d1df5fULL },
	{ 0x0000000000800000ULL, 0xf92da9e76fa3bebfULL },
	{ 0x0000000001000000ULL, 0xf25b39f0df4749c2ULL },
	{ 0x0000000002000000ULL, 0xe4b673e1be8e9384ULL },
	{ 0x0000000004000000ULL, 0xc96ce7c3fd1d2709ULL },
	{ 0x0000000008000000ULL, 0x92d9cf87fa3a4e12ULL },
	{ 0x0000000010000000ULL, 0x25b39f0ff4749c24ULL },
	{ 0x0000000020000000ULL, 0x4b673e1fe8e93848ULL },
	{ 0x0000000040000000ULL, 0x96ce7c3a51d27085ULL },
	{ 0x0000000080000000ULL, 0x2d9cf88523a4e10bULL },
	{ 0x0000000100000000ULL, 0x5b39f10ac749c217ULL },
	{ 0x0000000200000000ULL, 0xb673e2060e93842fULL },
	{ 0x0000000400000000ULL, 0x6ce7c40c9d27085fULL },
	{ 0x0000000800000000ULL, 0xdc8387ff3f0e10aaULL },
	{ 0x0000001000000000ULL, 0xb9070fee7e1c2154ULL },
	{ 0x0000002000000000ULL, 0x720e1fdcfc3842a8ULL },
	{ 0x0000004000000000ULL, 0xe41c3fa478708545ULL },
	{ 0x0000008000000000ULL, 0xc8387f58f0e10a8aULL },
	{ 0x0000010000000000ULL, 0x9364fec267021515ULL },
	{ 0x0000020000000000ULL, 0x26c9fd954e042a2bULL },
	{ 0x0000040000000000ULL, 0x4d93fb2a9c085456ULL },
	{ 0x0000080000000000ULL, 0x9b27f6553810a8acULL },
	{ 0x0000100000000000ULL, 0xae84159b600d1cc8ULL },
	{ 0x0000200000000000ULL, 0x455932ec903fc254ULL },
	{ 0x0000400000000000ULL, 0xa2d36aa0d044b36fULL },
	{ 0x0000800000000000ULL, 0x7b8ef94d0d2d2844ULL },
	{ 0x0001000000000000ULL, 0xd802248d5ba62df7ULL },
	{ 0x0002000000000000ULL, 0x2c298f47af1d015eULL },
	{ 0x0004000000000000ULL, 0x93a4e2a055abc2f5ULL },
	{ 0x0008000000000000ULL, 0x6f9511373b7145b2ULL },
	{ 0x0010000000000000ULL, 0x6305c0717e1d40d4ULL },
	{ 0x0020000000000000ULL, 0x6237df27b416b76bULL },
	{ 0x0040000000000000ULL, 0x042bc5ffe77f439eULL },
	{ 0x0080000000000000ULL, 0x7241900621a8c72bULL },
	{ 0x0100000000000000ULL, 0x6a298a4b4ecb2ec6ULL },
	{ 0x0200000000000000ULL, 0x789742a5659f92fbULL },
	{ 0x0400000000000000ULL, 0x794ee951db0365e6ULL },
	{ 0x0800000000000000ULL, 0x745424e0b94aec3cULL },
	{ 0x1000000000000000ULL, 0x726e27d005120de7ULL },
	{ 0x2000000000000000ULL, 0x6898adb511c8513eULL },
	{ 0x4000000000000000ULL, 0x59dbb96b3414d7ecULL },
	{ 0x8000000000000000ULL, 0x3be7d0f7780de548ULL },
	{ 0x0cead30e6469f5c5ULL, 0x0f3b67e6407d0ed9ULL },
	{ 0x028a2bec206c7b8aULL, 0xa9be0155bf452972ULL },
	{ 0x56e5747a306eac4eULL, 0x308451cac91706c3ULL },
	{ 0x6058b11e57287c72ULL, 0x6f8b3e2b8bf1abc4ULL },
	{ 0x4fec8f2a00cc2071ULL, 0x11f1e965f20a45a4ULL },
	{ 0x4f0bdf33102febc9ULL, 0x6102a774aa4baacfULL },
	{ 0x17e067e262adc6fdULL, 0x5878e9adb48c4d7cULL },
	{ 0x41374f0f3f94939cULL, 0x55d43d2febf8b7dfULL },
	{ 0x59d163b72870b072ULL, 0x98ee8d1508e8ff20ULL },
	{ 0x702b00ea2f01f008ULL, 0x1abf9db8b3306341ULL },
	{ 0x433b78782f7cc0d0ULL, 0xd33358a085d11f6aULL },
	{ 0x45789bc436c34865ULL, 0xa573c24e116ccf0dULL },
	{ 0x15e7f9b85580528aULL, 0x6d594ec1bb5651dfULL },
	{ 0x6e52ea866a56f880ULL, 0x5dac61fbbe7d64ffULL },
	{ 0x06baec796275d69aULL, 0x193424cca0b43145ULL },
	{ 0x2ac9d0b77bbdcc00ULL, 0x44b2175a2c313151ULL },
	{ 0x2299ab770a8223aeULL, 0x404514aa9d8e5c65ULL },
	{ 0x568e90d709816de9ULL, 0x08907c8b20d9fb59ULL },
	{ 0x3b6b460e67b34680ULL, 0xfc77f1ea859b024eULL },
	{ 0x418fde5c48db0e3fULL, 0x2e6a2d8cbe2d4412ULL },
	{ 0x5e8fa3c84b12c543ULL, 0xa9354017412aba12ULL },
	{ 0x49fad5466f937fc2ULL, 0xe81aeb4429b6264aULL },
	{ 0x2ab116877b7b6839ULL, 0xb189fec8b0994d6eULL },
	{ 0x16a5916132482fd8ULL, 0xa7c48eb9f12b7d37ULL },
	{ 0x6d24a54c0e51b961ULL, 0x701631d776a5b0e2ULL },
	{ 0x7a39bf1767d47a89ULL, 0x6bd08da7d7095b7aULL },
	{ 0x11c7f9a173bf8d4eULL, 0x451390b96dcad08dULL },
	{ 0x10411fef57d4fca4ULL, 0x49bd093dc8d7e040ULL },
	{ 0x001079a900860713ULL, 0x81ab2baf1bf59632ULL },
	{ 0x4b1b9ca618b2a5feULL, 0x152cb5a46ef65f99ULL },
	{ 0x036db8c22ffea95bULL, 0x08a263ec4de57fa9ULL },
	{ 0x64861ee83b7d6ddaULL, 0xd344a694800cea8cULL },
	{ 0, 0 }
};

void test_strhash_alignment(void)
{
	const tommy_uint32_t seed[] = { 0, 1, 0xa766795d, 0xffffffff };
	unsigned char buffer[4][256 + 8];
	unsigned char* key[4];

	for (unsigned alignment = 0; alignment < 4; ++alignment) {
		unsigned char* base = buffer[alignment];
		key[alignment] = base + ((4 - ((tommy_uintptr_t)base & 3)) & 3) + alignment;
	}

	/* verify the published vectors at every alignment */
	for (unsigned i = 0; STRHASH32[i].data; ++i) {
		for (unsigned alignment = 0; alignment < 4; ++alignment) {
			memset(buffer[alignment], 0xa5, sizeof(buffer[alignment]));
			memcpy(key[alignment], STRHASH32[i].data, strlen(STRHASH32[i].data) + 1);
			if (tommy_strhash_u32(0xa766795d, key[alignment]) != STRHASH32[i].hash) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
	}

	/* cover all tails, complete blocks and non-ASCII bytes with several seeds */
	for (unsigned len = 0; len < 256; ++len) {
		for (unsigned alignment = 0; alignment < 4; ++alignment) {
			memset(buffer[alignment], 0xa5, sizeof(buffer[alignment]));
			for (unsigned i = 0; i < len; ++i)
				key[alignment][i] = (i * 73 + len * 11) % 255 + 1;
			key[alignment][len] = 0;
		}
		for (unsigned s = 0; s < sizeof(seed) / sizeof(seed[0]); ++s) {
			tommy_uint32_t expected = tommy_strhash_u32(seed[s], key[0]);
			for (unsigned alignment = 0; alignment < 4; ++alignment) {
				if (tommy_strhash_u32(seed[s], key[alignment]) != expected) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				if (tommy_hash_u32(seed[s], key[alignment], len) != expected) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}
		}
	}
}

void test_strhash_u64_alignment(void)
{
	const tommy_uint64_t seed[] = { 0, 1, 0xa766795dULL, 0x2f022773a766795dULL, 0xffffffffffffffffULL };
	unsigned char buffer[8][256 + 16];
	unsigned char* key[8];

	for (unsigned alignment = 0; alignment < 8; ++alignment) {
		unsigned char* base = buffer[alignment];
		key[alignment] = base + ((8 - ((tommy_uintptr_t)base & 7)) & 7) + alignment;
	}

	/* verify the published vectors at every alignment */
	for (unsigned i = 0; STRHASH64[i].data; ++i) {
		for (unsigned alignment = 0; alignment < 8; ++alignment) {
			memset(buffer[alignment], 0xa5, sizeof(buffer[alignment]));
			memcpy(key[alignment], STRHASH64[i].data, strlen(STRHASH64[i].data) + 1);
			if (tommy_strhash_u64(0x2f022773a766795dULL, key[alignment]) != STRHASH64[i].hash) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
	}

	/* cover all tails, complete blocks and non-ASCII bytes with several seeds across all 8 alignments */
	for (unsigned len = 0; len < 256; ++len) {
		for (unsigned alignment = 0; alignment < 8; ++alignment) {
			memset(buffer[alignment], 0xa5, sizeof(buffer[alignment]));
			for (unsigned i = 0; i < len; ++i)
				key[alignment][i] = (i * 73 + len * 11) % 255 + 1;
			key[alignment][len] = 0;
		}
		for (unsigned s = 0; s < sizeof(seed) / sizeof(seed[0]); ++s) {
			tommy_uint64_t expected = tommy_strhash_u64(seed[s], key[0]);
			for (unsigned alignment = 0; alignment < 8; ++alignment) {
				if (tommy_strhash_u64(seed[s], key[alignment]) != expected) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				if (tommy_hash_u64(seed[s], key[alignment], len) != expected) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}
		}
	}
}

void test_hash(void)
{
	unsigned char buffer[129];
	unsigned COUNT = 1024 * 128;

	START("hash_test_vectors");

	/* test approximation retry in rnd() */
	tommy_uint64_t saved_seed = SEED;
	SEED = 0x1865d6a7bc16fecbULL;
	if (rnd(7) >= 7) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	SEED = saved_seed;

	test_strhash_alignment();
	test_strhash_u64_alignment();

	if (tommy_hash_u32(0xa766795d, 0, 0) != 0x3ba63d24) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_hash_u64(0x2f022773a766795dULL, 0, 0) != 0xf1999eea212f89e6ULL) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	for (unsigned i = 0; HASH32[i].data; ++i) {
		if (tommy_hash_u32(0xa766795d, HASH32[i].data, HASH32[i].len) != HASH32[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	for (unsigned i = 0; STRHASH32[i].data; ++i) {
		if (tommy_strhash_u32(0xa766795d, STRHASH32[i].data) != STRHASH32[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	for (unsigned i = 0; HASH64[i].data; ++i) {
		if (tommy_hash_u64(0x2f022773a766795dULL, HASH64[i].data, HASH64[i].len) != HASH64[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	for (unsigned i = 0; STRHASH64[i].data; ++i) {
		if (tommy_strhash_u64(0x2f022773a766795dULL, STRHASH64[i].data) != STRHASH64[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	for (unsigned i = 0; INTHASH32[i].value || !i; ++i) {
		if (tommy_inthash_u32(INTHASH32[i].value) != INTHASH32[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	for (unsigned i = 0; INTHASH64[i].value || !i; ++i) {
		if (tommy_inthash_u64(INTHASH64[i].value) != INTHASH64[i].hash) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	STOP();

	memset(buffer, 0xAA, sizeof(buffer));
	buffer[sizeof(buffer) - 1] = 0;

	tommy_uint32_t hash32 = 0;
	tommy_uint64_t hash64 = 0;

	START("hash_u32");

	for (unsigned i = 0; i < COUNT; ++i) {
		for (unsigned j = 0; j < sizeof(buffer); ++j)
			hash32 = tommy_hash_u32(hash32, buffer, j);
	}

	STOP();

	START("strhash_u32");

	for (unsigned i = 0; i < COUNT; ++i) {
		for (unsigned j = 0; j < sizeof(buffer); ++j) {
			buffer[j] = 0;
			hash32 = tommy_strhash_u32(hash32, buffer);
			buffer[j] = 0xAA;
		}
	}

	STOP();

	START("hash_u64");

	for (unsigned i = 0; i < COUNT; ++i) {
		for (unsigned j = 0; j < sizeof(buffer); ++j)
			hash64 = tommy_hash_u64(hash64, buffer, j);
	}

	STOP();

	START("strhash_u64");

	for (unsigned i = 0; i < COUNT; ++i) {
		for (unsigned j = 0; j < sizeof(buffer); ++j) {
			buffer[j] = 0;
			hash64 = tommy_strhash_u64(hash64, buffer);
			buffer[j] = 0xAA;
		}
	}

	STOP();
}

void test_alloc(void)
{
	const unsigned size = 10 * TOMMY_SIZE;
	tommy_allocator alloc;
	void** PTR = malloc(size * sizeof(void*));

	/* ensure at least pointer alignment */
	tommy_allocator_init(&alloc, sizeof(void*), 1);
	if (alloc.align_size < sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_allocator_done(&alloc);

	/* ensure correct alignment */
	tommy_allocator_init(&alloc, sizeof(void*) - 1, sizeof(void*));
	if (alloc.block_size != sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_allocator_done(&alloc);

	/* check big blocks */
	tommy_allocator_init(&alloc, 128000, 64);
	if (tommy_allocator_alloc(&alloc) == 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_allocator_done(&alloc);

	tommy_allocator_init(&alloc, 64, 64);

	START("alloc");
	for (unsigned i = 0; i < size; ++i) {
		PTR[i] = tommy_allocator_alloc(&alloc);
	}
	STOP();

	START("free");
	for (unsigned i = 0; i < size; ++i) {
		tommy_allocator_free(&alloc, PTR[i]);
	}
	STOP();

	tommy_allocator_done(&alloc);

	free(PTR);
}

void test_list_order(tommy_node* list)
{
	tommy_node* node;

	node = list;
	while (node) {
		if (node->next) {
			const struct object* a = node->data;
			const struct object* b = node->next->data;
			/* check order */
			if (a->value > b->value) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			/* check order for stable sort */
			if (a->value == b->value && a > b) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		node = node->next;
	}
}

/* check exact node identity, both traversal directions and list invariants. */
void test_list_sequence(tommy_list* list, struct object* obj, const unsigned* order, unsigned size)
{
	tommy_node* node = tommy_list_head(list);
	tommy_node* prev = tommy_list_tail(list);

	if (tommy_list_empty(list) != (size == 0)
		|| tommy_list_tail(list) != (size ? &obj[order[size - 1]].node : 0)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	for (unsigned i = 0; i < size; ++i) {
		if (node != &obj[order[i]].node || node->prev != prev
			|| node->data != &obj[order[i]] || node->index != order[i] + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		prev = node;
		node = node->next;
	}

	if (node != 0 || tommy_list_count(list) != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	node = tommy_list_tail(list);
	for (unsigned i = size; i > 0; --i) {
		if (node != &obj[order[i - 1]].node) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		node = tommy_list_prev(list, node);
	}

	if (node != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_list_build(tommy_list* list, struct object* obj, unsigned start, unsigned size)
{
	tommy_list_init(list);
	for (unsigned i = start; i < start + size; ++i) {
		obj[i].node.index = i + 1;
		tommy_list_insert_tail(list, &obj[i].node, &obj[i]);
	}
}

void test_list_insert_head(void)
{
	struct object obj[8];
	unsigned order[8] = { 0 };
	tommy_list list;

	for (unsigned n = 0; n <= 4; ++n) {
		tommy_list_init(&list);
		for (unsigned i = 0; i < n; ++i) {
			obj[i].node.index = i + 1;
			tommy_list_insert_head(&list, &obj[i].node, &obj[i]);
			order[n - 1 - i] = i;
		}
		test_list_sequence(&list, obj, order, n);
	}
}

void test_list_merge(void)
{
	struct object obj[8];
	unsigned order[8];
	const unsigned combinations[] = { 1, 3, 9, 27, 81 };
	tommy_list first;
	tommy_list second;

	/* All sorted sequences of up to four nodes with keys in [0, 2].
	 * Object identity distinguishes duplicates, including duplicates across lists.
	 * The oracle groups by key, retaining the original first-then-second order.
	 */
	for (unsigned n = 0; n <= 4; ++n) {
		for (unsigned m = 0; m <= 4; ++m) {
			for (unsigned a = 0; a < combinations[n]; ++a) {
				for (unsigned b = 0; b < combinations[m]; ++b) {
					tommy_bool_t sorted = 1;
					unsigned code = a;
					for (unsigned i = 0; i < n; ++i) {
						obj[i].value = code % 3;
						code /= 3;
						if (i && obj[i - 1].value > obj[i].value)
							sorted = 0;
					}
					code = b;
					for (unsigned i = n; i < n + m; ++i) {
						obj[i].value = code % 3;
						code /= 3;
						if (i != n && obj[i - 1].value > obj[i].value)
							sorted = 0;
					}
					if (!sorted)
						continue;

					test_list_build(&first, obj, 0, n);
					test_list_build(&second, obj, n, m);
					unsigned size = 0;
					for (unsigned key = 0; key < 3; ++key)
						for (unsigned i = 0; i < n + m; ++i)
							if (obj[i].value == (int)key)
								order[size++] = i;

					tommy_list_merge(&first, &second, compare);
					test_list_sequence(&first, obj, order, size);
				}
			}
		}
	}
}

void test_list_split(void)
{
	struct object obj[6];
	const unsigned order[] = { 0, 1, 2, 3, 4 };
	const unsigned previous_order[] = { 5 };
	tommy_list first;
	tommy_list second;

	/* split at every position, including the head, tail and null node. */
	for (unsigned n = 0; n <= 5; ++n) {
		for (unsigned pos = 0; pos <= n; ++pos) {
			test_list_build(&first, obj, 0, n);
			/* the previous destination value is overwritten, but its nodes are untouched. */
			test_list_build(&second, obj, 5, 1);
			tommy_list previous = second;
			tommy_list_split(&first, pos == n ? 0 : &obj[pos].node, &second);
			test_list_sequence(&first, obj, order, pos);
			test_list_sequence(&second, obj, order + pos, n - pos);
			test_list_sequence(&previous, obj, previous_order, 1);

			/* rejoin the lists and check that no node was lost. */
			tommy_list_concat(&first, &second);
			test_list_sequence(&first, obj, order, n);
		}

		/* an uninitialized output is valid, including when the input node is 0. */
		{
			tommy_list output;
			test_list_build(&first, obj, 0, n);
			tommy_list_split(&first, tommy_list_head(&first), &output);
			test_list_sequence(&first, obj, 0, 0);
			test_list_sequence(&output, obj, order, n);
		}
	}
}

void test_list_splice(void)
{
	struct object obj[8];
	unsigned order[8];
	tommy_list first;
	tommy_list second;

	/* both splice operations, every valid reference and empty/singleton cases. */
	for (unsigned n = 1; n <= 4; ++n) {
		for (unsigned m = 0; m <= 3; ++m) {
			for (unsigned operation = 0; operation < 2; ++operation) {
				for (unsigned pos = 0; pos < n; ++pos) {
					test_list_build(&first, obj, 0, n);
					test_list_build(&second, obj, n, m);
					unsigned insert = operation == 0 ? pos : pos + 1;
					unsigned size = 0;
					for (unsigned i = 0; i < insert; ++i)
						order[size++] = i;
					for (unsigned i = n; i < n + m; ++i)
						order[size++] = i;
					for (unsigned i = insert; i < n; ++i)
						order[size++] = i;

					switch (operation) {
					case 0 :
						tommy_list_splice_before(&first, &obj[pos].node, &second);
						break;
					case 1 :
						tommy_list_splice_after(&first, &obj[pos].node, &second);
						break;
					}
					test_list_sequence(&first, obj, order, size);

					/* the source can be reused after reinitialization without affecting the destination. */
					unsigned reuse[1] = { n + m };
					test_list_build(&second, obj, reuse[0], 1);
					test_list_sequence(&second, obj, reuse, 1);
					test_list_sequence(&first, obj, order, size);
				}
			}
		}
	}
}

void test_list_concat(void)
{
	struct object obj[8];
	unsigned order[8];
	tommy_list first;
	tommy_list second;

	for (unsigned n = 0; n <= 4; ++n) {
		for (unsigned m = 0; m <= 3; ++m) {
			test_list_build(&first, obj, 0, n);
			test_list_build(&second, obj, n, m);
			unsigned size = 0;
			for (unsigned i = 0; i < n + m; ++i)
				order[size++] = i;
			tommy_list_concat(&first, &second);
			test_list_sequence(&first, obj, order, size);
		}
	}
}

void test_list_prepend(void)
{
	struct object obj[8];
	unsigned order[8];
	tommy_list first;
	tommy_list second;

	for (unsigned n = 0; n <= 4; ++n) {
		for (unsigned m = 0; m <= 3; ++m) {
			test_list_build(&first, obj, 0, n);
			test_list_build(&second, obj, n, m);
			unsigned size = 0;
			for (unsigned i = n; i < n + m; ++i)
				order[size++] = i;
			for (unsigned i = 0; i < n; ++i)
				order[size++] = i;
			tommy_list_prepend(&first, &second);
			test_list_sequence(&first, obj, order, size);
		}
	}
}

void test_list_reverse_move(void)
{
	struct object obj[6];
	const unsigned forward[] = { 0, 1, 2, 3, 4, 5 };
	unsigned order[6];
	tommy_list list;

	for (unsigned n = 0; n <= 6; ++n) {
		test_list_build(&list, obj, 0, n);
		for (unsigned i = 0; i < n; ++i)
			order[i] = n - 1 - i;
		tommy_list_reverse(&list);
		test_list_sequence(&list, obj, order, n);
		tommy_list_reverse(&list);
		test_list_sequence(&list, obj, forward, n);

		/* move every node to either end, including nodes already at that end. */
		for (unsigned pos = 0; pos < n; ++pos) {
			for (unsigned tail = 0; tail < 2; ++tail) {
				test_list_build(&list, obj, 0, n);
				unsigned size = 0;
				if (!tail)
					order[size++] = pos;
				for (unsigned i = 0; i < n; ++i)
					if (i != pos)
						order[size++] = i;
				if (tail)
					order[size++] = pos;

				if (tail)
					tommy_list_move_tail(&list, &obj[pos].node);
				else
					tommy_list_move_head(&list, &obj[pos].node);
				test_list_sequence(&list, obj, order, n);
			}
		}
	}

	/* moving a node with null data preserves it. */
	test_list_build(&list, obj, 0, 3);
	obj[1].node.data = 0;
	tommy_list_move_head(&list, &obj[1].node);
	tommy_list_move_tail(&list, &obj[1].node);
	if (tommy_list_tail(&list) != &obj[1].node || obj[1].node.data != 0 || obj[1].node.index != 2) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_list_removed(tommy_node* node, tommy_node* saved)
{
	if (node->next != saved->next || node->prev != saved->prev
		|| node->data != saved->data || node->index != saved->index) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_list_remove_ends(void)
{
	struct object obj[6];
	const unsigned order[] = { 0, 1, 2, 3, 4, 5 };
	tommy_list list;

	/* drain from the head, from the tail, and alternating between both. */
	for (unsigned n = 0; n <= 6; ++n) {
		for (unsigned mode = 0; mode < 3; ++mode) {
			test_list_build(&list, obj, 0, n);
			unsigned head = 0;
			unsigned tail = n;
			while (head != tail) {
				tommy_bool_t remove_tail = mode == 1 || (mode == 2 && (tail - head) % 2 == 0);
				tommy_node* node = &obj[remove_tail ? tail - 1 : head].node;
				tommy_node saved = *node;
				void* data;
				if (remove_tail) {
					data = tommy_list_remove_tail(&list);
					--tail;
				} else {
					data = tommy_list_remove_head(&list);
					++head;
				}
				if (data != saved.data) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				test_list_removed(node, &saved);
				test_list_sequence(&list, obj, order + head, tail - head);
			}
			if (tommy_list_remove_head(&list) != 0 || tommy_list_remove_tail(&list) != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			test_list_sequence(&list, obj, 0, 0);
		}
	}

	/* null data still removes the node, despite returning 0. */
	for (unsigned mode = 0; mode < 2; ++mode) {
		test_list_build(&list, obj, 0, 1);
		obj[0].node.data = 0;
		tommy_node saved = obj[0].node;
		void* data = mode ? tommy_list_remove_tail(&list) : tommy_list_remove_head(&list);
		if (data != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		test_list_removed(&obj[0].node, &saved);
		test_list_sequence(&list, obj, 0, 0);
	}
}

void test_list_swap(void)
{
	struct object obj[8];
	const unsigned order[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	tommy_list first;
	tommy_list second;

	for (unsigned n = 0; n <= 4; ++n) {
		for (unsigned m = 0; m <= 4; ++m) {
			test_list_build(&first, obj, 0, n);
			test_list_build(&second, obj, n, m);
			tommy_list_swap(&first, &second);
			test_list_sequence(&first, obj, order + n, m);
			test_list_sequence(&second, obj, order, n);
			tommy_list_swap(&first, &first);
			tommy_list_swap(&second, &second);
			test_list_sequence(&first, obj, order + n, m);
			test_list_sequence(&second, obj, order, n);
			tommy_list_swap(&first, &second);
			test_list_sequence(&first, obj, order, n);
			test_list_sequence(&second, obj, order + n, m);
		}
	}
}

void test_list(void)
{
	const unsigned size = TOMMY_SIZE;
	tommy_node* list;

	test_list_insert_head();
	test_list_merge();
	test_list_split();
	test_list_splice();
	test_list_concat();
	test_list_prepend();
	test_list_reverse_move();
	test_list_remove_ends();
	test_list_swap();

	struct object* LIST = malloc(size * sizeof(struct object));
	struct object_vector* VECTOR = malloc(size * sizeof(struct object_vector));

	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = 0;
	}

	tommy_list_init(&list);

	/* sort an empty list */
	tommy_list_sort(&list, compare);

	if (!tommy_list_empty(&list)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_list_tail(&list) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_list_head(&list) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = rnd(size);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	if (tommy_list_tail(&list) == 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_list_head(&list) == 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	START("sort random");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort random");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* forward order with some (1%) random values */
	list = 0;
	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = i;
		if (rnd(100) == 0)
			VECTOR[i].value = LIST[i].value = rnd(size);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort partially ordered");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort partially ordered");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* forward order */
	list = 0;
	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort forward");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort forward");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* backward order */
	list = 0;
	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = size - 1 - i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort backward");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort backward");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* use a small range of random value to insert a lot of duplicates */
	list = 0;
	for (unsigned i = 0; i < size; ++i) {
		VECTOR[i].value = LIST[i].value = rnd(size / 1000 + 2);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort random duplicate");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort random duplicate");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	free(LIST);
	free(VECTOR);
}

void test_tree(void)
{
	const unsigned size = TOMMY_SIZE / 4;
	struct object_tree* OBJ = malloc(size * sizeof(struct object_tree));
	tommy_tree tree;

	START("tree");
	tommy_tree_init(&tree, &compare);

	/* forward order */
	for (unsigned i = 0; i < size; ++i)
		OBJ[i].value = i;

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]);

	if (tommy_tree_memory_usage(&tree) < size * sizeof(tommy_tree_node)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_tree_count(&tree) != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	the_count = 0;
	tommy_tree_foreach(&tree, count_callback);
	if (the_count != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	the_count = 0;
	tommy_tree_foreach_arg(&tree, count_arg_callback, &the_count);
	if (the_count != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* search present */
	for (unsigned i = 0; i < size / 2; ++i) {
		if (tommy_tree_search(&tree, &OBJ[i]) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_compare(&tree, &compare, &OBJ[i]) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* insert existing */
	for (unsigned i = 0; i < size; ++i) {
		struct object_tree EXTRA;
		EXTRA.value = i;
		if (tommy_tree_insert_unique(&tree, &EXTRA.node, &EXTRA) == &EXTRA) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* remove existing */
	for (unsigned i = 0; i < size / 2; ++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	/* remove missing */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_tree_remove(&tree, &OBJ[i]) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* search missing */
	for (unsigned i = 0; i < size / 2; ++i) {
		if (tommy_tree_search(&tree, &OBJ[i]) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_compare(&tree, &compare, &OBJ[i]) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* remove present */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_tree_remove(&tree, &OBJ[size / 2 + i]) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* reverse order */
	for (unsigned i = 0; i < size; ++i)
		OBJ[i].value = size - i;

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]);

	/* remove all */
	for (unsigned i = 0; i < size; ++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	/* random order */
	for (unsigned i = 0; i < size; ++i)
		OBJ[i].value = tommy_inthash_u32(i);

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]);

	/* remove all */
	for (unsigned i = 0; i < size; ++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	STOP();
}

void test_tree_swap(void)
{
	tommy_tree first;
	tommy_tree second;
	tommy_tree empty;
	struct object_tree objects[5];
	tommy_tree_node* first_root;
	tommy_tree_node* second_root;
	const int values[] = { 1, 3, 5, 4, 2 };

	tommy_tree_init(&first, &compare);
	tommy_tree_init(&second, &compare_tree_reverse);
	tommy_tree_init(&empty, &compare);
	for (unsigned i = 0; i < 5; ++i)
		objects[i].value = values[i];
	for (unsigned i = 0; i < 3; ++i)
		tommy_tree_insert(&first, &objects[i].node, &objects[i]);
	for (unsigned i = 3; i < 5; ++i)
		tommy_tree_insert(&second, &objects[i].node, &objects[i]);
	first_root = first.root;
	second_root = second.root;

	tommy_tree_swap(&first, &second);
	if (first.root != second_root || first.cmp != &compare_tree_reverse || tommy_tree_count(&first) != 2
		|| second.root != first_root || second.cmp != &compare || tommy_tree_count(&second) != 3
		|| tommy_tree_head(&first) != &objects[3].node
		|| tommy_tree_next(tommy_tree_head(&first)) != &objects[4].node
		|| tommy_tree_search(&first, &objects[2]) != 0
		|| tommy_tree_search_greater(&first, &objects[1]) != &objects[4]
		|| tommy_tree_search_less(&first, &objects[1]) != &objects[3]
		|| tommy_tree_head(&second) != &objects[0].node
		|| tommy_tree_next(tommy_tree_head(&second)) != &objects[1].node
		|| tommy_tree_tail(&second) != &objects[2].node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_tree_swap(&first, &first);
	if (first.root != second_root || first.cmp != &compare_tree_reverse || tommy_tree_count(&first) != 2) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_remove(&first, &objects[4]) != &objects[4]
		|| tommy_tree_remove(&second, &objects[2]) != &objects[2]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_tree_swap(&first, &empty);
	if (!tommy_tree_empty(&first) || first.cmp != &compare || tommy_tree_count(&first) != 0
		|| empty.root != second_root || empty.cmp != &compare_tree_reverse || tommy_tree_count(&empty) != 1
		|| tommy_tree_head(&empty) != &objects[3].node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_tree_duplicates(void)
{
	tommy_tree tree;
	struct object_tree objects[96];

	tommy_tree_init(&tree, &compare);

	for (unsigned i = 0; i < 96; ++i) {
		objects[i].value = i % 4;
		tommy_tree_insert(&tree, &objects[i].node, &objects[i]);
	}

	if (tommy_tree_count(&tree) != 96) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_tree_node* node = tommy_tree_head(&tree);
	for (unsigned group = 0; group < 4; ++group) {
		for (unsigned i = group; i < 96; i += 4) {
			if (node != &objects[i].node) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			node = tommy_tree_next(node);
		}
	}
	if (node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	for (unsigned group = 0; group < 4; ++group) {
		const struct object_tree key = { .value = group };
		void* greater = group < 3 ? &objects[group + 1] : 0;
		void* less = group ? &objects[91 + group] : 0;
		if (tommy_tree_search(&tree, &key) != &objects[group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_compare(&tree, &compare, &key) != &objects[group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_greater_equal(&tree, &key) != &objects[group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_greater_equal_compare(&tree, &compare, &key) != &objects[group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_less_equal(&tree, &key) != &objects[92 + group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_less_equal_compare(&tree, &compare, &key) != &objects[92 + group]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_tree_search_greater(&tree, &key) != greater
			|| tommy_tree_search_greater_compare(&tree, &compare, &key) != greater
			|| tommy_tree_search_less(&tree, &key) != less
			|| tommy_tree_search_less_compare(&tree, &compare, &key) != less) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	struct object_tree key;
	key.value = -1;
	if (tommy_tree_search_greater(&tree, &key) != &objects[0]
		|| tommy_tree_search_greater_compare(&tree, &compare, &key) != &objects[0]
		|| tommy_tree_search_less(&tree, &key) != 0
		|| tommy_tree_search_less_compare(&tree, &compare, &key) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	key.value = 4;
	if (tommy_tree_search_greater(&tree, &key) != 0
		|| tommy_tree_search_greater_compare(&tree, &compare, &key) != 0
		|| tommy_tree_search_less(&tree, &key) != &objects[95]
		|| tommy_tree_search_less_compare(&tree, &compare, &key) != &objects[95]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	key.value = 2;
	if (tommy_tree_search_compare(&tree, &compare_tree_group, &key) != &objects[2]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_greater_equal_compare(&tree, &compare_tree_group, &key) != &objects[2]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_less_equal_compare(&tree, &compare_tree_group, &key) != &objects[95]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_greater_compare(&tree, &compare_tree_group, &key) != 0
		|| tommy_tree_search_less_compare(&tree, &compare_tree_group, &key) != &objects[93]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	key.value = 0;
	if (tommy_tree_search_greater_compare(&tree, &compare_tree_group, &key) != &objects[2]
		|| tommy_tree_search_less_compare(&tree, &compare_tree_group, &key) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	int value = 2;
	if (tommy_tree_search_greater_compare(&tree, &compare_tree_int, &value) != &objects[3]
		|| tommy_tree_search_less_compare(&tree, &compare_tree_int, &value) != &objects[93]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	struct object_tree candidate;
	candidate.value = 2;
	memset(&candidate.node, 0xa5, sizeof(candidate.node));
	unsigned char before[sizeof(candidate.node)];
	memcpy(before, &candidate.node, sizeof(before));
	if (tommy_tree_insert_unique(&tree, &candidate.node, &candidate) != &objects[2]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (memcmp(&candidate.node, &before, sizeof(before)) != 0 || tommy_tree_count(&tree) != 96) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	for (unsigned i = 2; i < 96; i += 4) {
		if (tommy_tree_remove(&tree, &candidate) != &objects[i]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	if (tommy_tree_remove(&tree, &candidate) != 0 || tommy_tree_count(&tree) != 72) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_greater_equal(&tree, &candidate) != &objects[3]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_less_equal(&tree, &candidate) != &objects[93]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 3; i < 96; i += 4) {
		if (tommy_tree_remove_compare(&tree, &compare_tree_group, &candidate) != &objects[i]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	if (tommy_tree_remove_compare(&tree, &compare_tree_group, &candidate) != 0 || tommy_tree_count(&tree) != 48) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_compare(&tree, &compare_tree_group, &candidate) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	candidate.value = 4;
	if (tommy_tree_insert_unique(&tree, &candidate.node, &candidate) != &candidate) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_tail(&tree) != &candidate.node || tommy_tree_count(&tree) != 49) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	key.value = -2;
	if (tommy_tree_search_less_equal(&tree, &key) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_compare(&tree, &compare_tree_group, &key) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	key.value = 5;
	if (tommy_tree_search_greater_equal(&tree, &key) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	value = 4;
	if (tommy_tree_remove_compare(&tree, &compare_tree_int, &value) != &candidate) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_remove_compare(&tree, &compare_tree_int, &value) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	int val_neg = -1;
	if (tommy_tree_remove_compare(&tree, &compare_tree_int, &val_neg) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned group = 0; group < 4; ++group) {
		if (group >= 2)
			continue;
		for (unsigned i = group; i < 96; i += 4)
			tommy_tree_remove_existing(&tree, &objects[i].node);
	}
	if (!tommy_tree_empty(&tree)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

static void tree_foreach_free_arg_callback(void* arg, void* data)
{
	int* expected = arg;
	struct object_tree* object = data;

	if (object->value != *expected) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	++*expected;
	free(object);
}

void test_tree_foreach(void)
{
	tommy_tree tree;

	for (unsigned pass = 0; pass < 2; ++pass) {
		int expected = 0;

		tommy_tree_init(&tree, &compare);
		tommy_tree_foreach(&tree, free);
		tommy_tree_foreach_arg(&tree, tree_foreach_free_arg_callback, &expected);
		for (unsigned i = 0; i < 64; ++i) {
			struct object_tree* object = malloc(sizeof(*object));

			object->value = i * 37 % 64;
			tommy_tree_insert(&tree, &object->node, object);
		}
		if (pass == 0) {
			tommy_tree_foreach(&tree, free);
		} else {
			tommy_tree_foreach_arg(&tree, tree_foreach_free_arg_callback, &expected);
			if (expected != 64) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		tommy_tree_clear(&tree);
	}
}

void test_tree_clear(void)
{
	tommy_tree tree;
	struct object_tree objects[2];

	tommy_tree_init(&tree, &compare);
	objects[0].value = 1;
	objects[1].value = 2;
	tommy_tree_insert(&tree, &objects[0].node, &objects[0]);
	tommy_tree_insert(&tree, &objects[1].node, &objects[1]);
	tommy_tree_clear(&tree);
	if (!tommy_tree_empty(&tree) || tommy_tree_count(&tree) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_head(&tree) != 0 || tommy_tree_tail(&tree) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_tree_search_greater(&tree, &objects[0]) != 0
		|| tommy_tree_search_greater_compare(&tree, &compare, &objects[0]) != 0
		|| tommy_tree_search_less(&tree, &objects[0]) != 0
		|| tommy_tree_search_less_compare(&tree, &compare, &objects[0]) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_tree_insert(&tree, &objects[0].node, &objects[0]);
	if (tommy_tree_search(&tree, &objects[0]) != &objects[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_tree_remove_existing(&tree, &objects[0].node);

	struct object_tree* allocated = malloc(sizeof(*allocated));
	allocated->value = 3;
	tommy_tree_insert(&tree, &allocated->node, allocated);
	tommy_tree_foreach(&tree, free);
	tommy_tree_clear(&tree);
	if (!tommy_tree_empty(&tree)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_tree_insert(&tree, &objects[1].node, &objects[1]);
	if (tommy_tree_remove(&tree, &objects[1]) != &objects[1]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_tree_to_list(void)
{
	tommy_tree tree;
	tommy_list list;
	struct object_tree objects[96];
	struct object_tree prefix[2];
	struct object_tree reused;
	tommy_size_t index[96];

	tommy_tree_init(&tree, &compare);
	tommy_list_init(&list);
	tommy_tree_to_list(&tree, &list);
	if (!tommy_tree_empty(&tree) || !tommy_list_empty(&list)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	reused.value = 7;
	tommy_tree_insert(&tree, &reused.node, &reused);
	tommy_tree_to_list(&tree, &list);
	if (tommy_list_head(&list) != &reused.node || tommy_list_tail(&list) != &reused.node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_list_init(&list);

	for (unsigned i = 0; i < 2; ++i)
		tommy_list_insert_tail(&list, &prefix[i].node, &prefix[i]);
	for (unsigned i = 0; i < 96; ++i) {
		objects[i].value = i % 5;
		tommy_tree_insert(&tree, &objects[i].node, &objects[i]);
	}
	for (unsigned i = 0; i < 96; ++i)
		index[i] = objects[i].node.index;

	tommy_tree_to_list(&tree, &list);
	if (!tommy_tree_empty(&tree) || tommy_tree_count(&tree) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	if (tommy_list_count(&list) != 98) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_node* node = tommy_list_head(&list);
	for (unsigned i = 0; i < 2; ++i) {
		if (node != &prefix[i].node) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		node = node->next;
	}
	for (unsigned group = 0; group < 5; ++group) {
		for (unsigned i = group; i < 96; i += 5) {
			if (node != &objects[i].node || node->data != &objects[i] || node->index != index[i]) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			node = node->next;
		}
	}
	if (node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	node = tommy_list_tail(&list);
	for (unsigned i = 0; i < 98; ++i) {
		if (!node) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		node = tommy_list_prev(&list, node);
	}
	if (node) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_tree_to_list(&tree, &list);
	if (tommy_list_count(&list) != 98) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	reused.value = 8;
	tommy_tree_insert(&tree, &reused.node, &reused);
	if (tommy_tree_remove(&tree, &reused) != &reused) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_array(void)
{
	const unsigned size = 50 * TOMMY_SIZE;
	tommy_array array;

	tommy_array_init(&array);

	/* no op */
	tommy_array_grow(&array, 0);

	START("array init");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		tommy_array_grow(&array, i + 1);
		if (tommy_array_get(&array, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	START("array set");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		tommy_array_set(&array, i, (void*)i);
	}
	STOP();

	START("array get");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		if (tommy_array_get(&array, i) != (void*)i) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	if (tommy_array_memory_usage(&array) < size * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_array_done(&array);
}

void test_arrayof(void)
{
	const unsigned size = 50 * TOMMY_SIZE;
	tommy_arrayof arrayof;

	tommy_arrayof_init(&arrayof, sizeof(unsigned));

	/* no op */
	tommy_arrayof_grow(&arrayof, 0);

	START("arrayof init");
	for (unsigned i = 0; i < size; ++i) {
		tommy_arrayof_grow(&arrayof, i + 1);
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	START("arrayof set");
	for (unsigned i = 0; i < size; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		*ref = i;
	}
	STOP();

	START("arrayof get");
	for (unsigned i = 0; i < size; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != i) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	if (tommy_arrayof_memory_usage(&arrayof) < size * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayof_done(&arrayof);
}

void test_arrayblk(void)
{
	const unsigned size = 50 * TOMMY_SIZE;
	tommy_arrayblk arrayblk;
	void** first_ref = 0;

	tommy_arrayblk_init(&arrayblk);

	/* no op */
	tommy_arrayblk_grow(&arrayblk, 0);

	START("arrayblk init");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		tommy_arrayblk_grow(&arrayblk, i + 1);
		if (i == 0)
			first_ref = tommy_arrayblk_ref(&arrayblk, i);
		if (tommy_arrayblk_get(&arrayblk, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();
	if (first_ref != tommy_arrayblk_ref(&arrayblk, 0)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	START("arrayblk set");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		tommy_arrayblk_set(&arrayblk, i, (void*)i);
	}
	STOP();

	START("arrayblk get");
	for (tommy_uintptr_t i = 0; i < size; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != (void*)i) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	if (tommy_arrayblk_memory_usage(&arrayblk) < size * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblk_done(&arrayblk);
}

void test_arrayblkof(void)
{
	const unsigned size = 50 * TOMMY_SIZE;
	tommy_arrayblkof arrayblkof;
	unsigned* first_ref = 0;

	tommy_arrayblkof_init(&arrayblkof, sizeof(unsigned));

	/* no op */
	tommy_arrayblkof_grow(&arrayblkof, 0);

	START("arrayblkof init");
	for (unsigned i = 0; i < size; ++i) {
		tommy_arrayblkof_grow(&arrayblkof, i + 1);
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (i == 0)
			first_ref = ref;
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();
	if (first_ref != tommy_arrayblkof_ref(&arrayblkof, 0)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	START("arrayblkof set");
	for (unsigned i = 0; i < size; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		*ref = i;
	}
	STOP();

	START("arrayblkof get");
	for (unsigned i = 0; i < size; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != i) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	STOP();

	if (tommy_arrayblkof_memory_usage(&arrayblkof) < size * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblkof_done(&arrayblkof);
}

void test_array_tail_ops(void)
{
	unsigned value = 7;

	{
		tommy_array array;
		tommy_array_init(&array);
		if (!tommy_array_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		for (tommy_size_t i = 0; i < 65; ++i)
			tommy_array_insert_tail(&array, &value);
		if (tommy_array_empty(&array) || tommy_array_size(&array) != 65 || tommy_array_tail(&array) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_array_remove_tail(&array) != &value || tommy_array_size(&array) != 64) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_grow(&array, 65);
		if (tommy_array_tail(&array) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_clear(&array);
		if (!tommy_array_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_done(&array);
	}

	{
		tommy_arrayof array;
		tommy_arrayof_init(&array, sizeof(unsigned));
		if (!tommy_arrayof_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		for (tommy_size_t i = 0; i < 65; ++i) {
			unsigned* ref = tommy_arrayof_insert_tail(&array);
			if (ref != tommy_arrayof_tail(&array) || *ref != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			*ref = value;
		}
		unsigned* tail = tommy_arrayof_tail(&array);
		if (tommy_arrayof_empty(&array) || tommy_arrayof_size(&array) != 65 || *tail != value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_remove_tail(&array);
		tail = tommy_arrayof_insert_tail(&array);
		if (tommy_arrayof_size(&array) != 65 || *tail != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_clear(&array);
		if (!tommy_arrayof_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_done(&array);
	}

	{
		tommy_arrayblk array;
		tommy_arrayblk_init(&array);
		if (!tommy_arrayblk_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		for (tommy_size_t i = 0; i < TOMMY_ARRAYBLK_SIZE + 1; ++i)
			tommy_arrayblk_insert_tail(&array, &value);
		if (tommy_arrayblk_empty(&array) || tommy_arrayblk_size(&array) != TOMMY_ARRAYBLK_SIZE + 1 || tommy_arrayblk_tail(&array) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		if (tommy_arrayblk_remove_tail(&array) != &value || tommy_arrayblk_size(&array) != TOMMY_ARRAYBLK_SIZE) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_grow(&array, TOMMY_ARRAYBLK_SIZE + 1);
		if (tommy_arrayblk_tail(&array) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_clear(&array);
		if (!tommy_arrayblk_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_done(&array);
	}

	{
		tommy_arrayblkof array;
		tommy_arrayblkof_init(&array, sizeof(unsigned));
		if (!tommy_arrayblkof_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		for (tommy_size_t i = 0; i < TOMMY_ARRAYBLKOF_SIZE + 1; ++i) {
			unsigned* ref = tommy_arrayblkof_insert_tail(&array);
			if (ref != tommy_arrayblkof_tail(&array) || *ref != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			*ref = value;
		}
		unsigned* tail = tommy_arrayblkof_tail(&array);
		if (tommy_arrayblkof_empty(&array) || tommy_arrayblkof_size(&array) != TOMMY_ARRAYBLKOF_SIZE + 1 || *tail != value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_remove_tail(&array);
		tail = tommy_arrayblkof_insert_tail(&array);
		if (tommy_arrayblkof_size(&array) != TOMMY_ARRAYBLKOF_SIZE + 1 || *tail != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_clear(&array);
		if (!tommy_arrayblkof_empty(&array)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_done(&array);
	}
}

void test_array_capacity_swap(void)
{
	unsigned value = 7;

	{
		tommy_array first;
		tommy_array second;
		tommy_array_init(&first);
		tommy_array_init(&second);
		if (tommy_array_capacity(&first) != 64 || tommy_array_capacity(&second) != 64) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_reserve(&first, 65);
		tommy_array_grow(&first, 65);
		tommy_array_set(&first, 0, &value);
		tommy_array_set(&first, 64, &value);
		void** head = tommy_array_ref(&first, 0);
		void** tail = tommy_array_ref(&first, 64);
		if (tommy_array_capacity(&first) != 128) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_swap(&first, &second);
		tommy_array_swap(&second, &second);
		if (tommy_array_size(&first) != 0 || tommy_array_capacity(&first) != 64 || tommy_array_size(&second) != 65 || tommy_array_capacity(&second) != 128 || tommy_array_ref(&second, 0) != head || tommy_array_ref(&second, 64) != tail || tommy_array_get(&second, 64) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_resize(&second, 1);
		tommy_array_shrink(&second);
		if (tommy_array_capacity(&second) != 64 || tommy_array_ref(&second, 0) != head || tommy_array_get(&second, 0) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_done(&first);
		tommy_array_done(&second);
	}

	{
		tommy_arrayblk first;
		tommy_arrayblk second;
		tommy_arrayblk_init(&first);
		tommy_arrayblk_init(&second);
		if (tommy_arrayblk_capacity(&first) != 0 || tommy_arrayblk_capacity(&second) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_reserve(&first, TOMMY_ARRAYBLK_SIZE + 1);
		tommy_arrayblk_grow(&first, TOMMY_ARRAYBLK_SIZE + 1);
		tommy_arrayblk_set(&first, 0, &value);
		tommy_arrayblk_set(&first, TOMMY_ARRAYBLK_SIZE, &value);
		void** head = tommy_arrayblk_ref(&first, 0);
		void** tail = tommy_arrayblk_ref(&first, TOMMY_ARRAYBLK_SIZE);
		if (tommy_arrayblk_capacity(&first) != 2 * TOMMY_ARRAYBLK_SIZE) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_swap(&first, &second);
		tommy_arrayblk_swap(&second, &second);
		if (tommy_arrayblk_size(&first) != 0 || tommy_arrayblk_capacity(&first) != 0 || tommy_arrayblk_size(&second) != TOMMY_ARRAYBLK_SIZE + 1 || tommy_arrayblk_capacity(&second) != 2 * TOMMY_ARRAYBLK_SIZE || tommy_arrayblk_ref(&second, 0) != head || tommy_arrayblk_ref(&second, TOMMY_ARRAYBLK_SIZE) != tail || tommy_arrayblk_get(&second, TOMMY_ARRAYBLK_SIZE) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_resize(&second, 1);
		tommy_arrayblk_shrink(&second);
		if (tommy_arrayblk_capacity(&second) != TOMMY_ARRAYBLK_SIZE || tommy_arrayblk_ref(&second, 0) != head || tommy_arrayblk_get(&second, 0) != &value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_done(&first);
		tommy_arrayblk_done(&second);
	}

	{
		tommy_arrayof first;
		tommy_arrayof second;
		tommy_arrayof_init(&first, sizeof(unsigned));
		tommy_arrayof_init(&second, 2 * sizeof(unsigned));
		tommy_arrayof_reserve(&first, 65);
		tommy_arrayof_grow(&first, 65);
		tommy_arrayof_grow(&second, 1);
		unsigned* head = tommy_arrayof_ref(&first, 0);
		unsigned* tail = tommy_arrayof_ref(&first, 64);
		unsigned* pair = tommy_arrayof_ref(&second, 0);
		*head = value;
		*tail = value + 1;
		pair[0] = value + 2;
		pair[1] = value + 3;
		if (tommy_arrayof_capacity(&first) != 128 || tommy_arrayof_capacity(&second) != 64) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_swap(&first, &second);
		tommy_arrayof_swap(&second, &second);
		tommy_arrayof_grow(&first, 2);
		if (tommy_arrayof_size(&first) != 2 || tommy_arrayof_size(&second) != 65 || tommy_arrayof_capacity(&first) != 64 || tommy_arrayof_capacity(&second) != 128 || tommy_arrayof_ref(&first, 0) != pair || tommy_arrayof_ref(&first, 1) != pair + 2 || tommy_arrayof_ref(&second, 64) != tail || pair[0] != value + 2 || pair[1] != value + 3 || *tail != value + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_resize(&second, 1);
		tommy_arrayof_shrink(&second);
		if (tommy_arrayof_capacity(&second) != 64 || tommy_arrayof_ref(&second, 0) != head || *head != value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayof_done(&first);
		tommy_arrayof_done(&second);
	}

	{
		tommy_arrayblkof first;
		tommy_arrayblkof second;
		tommy_arrayblkof_init(&first, sizeof(unsigned));
		tommy_arrayblkof_init(&second, 2 * sizeof(unsigned));
		if (tommy_arrayblkof_capacity(&first) != 0 || tommy_arrayblkof_capacity(&second) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_reserve(&first, TOMMY_ARRAYBLKOF_SIZE + 1);
		tommy_arrayblkof_grow(&first, TOMMY_ARRAYBLKOF_SIZE + 1);
		tommy_arrayblkof_grow(&second, 1);
		unsigned* head = tommy_arrayblkof_ref(&first, 0);
		unsigned* tail = tommy_arrayblkof_ref(&first, TOMMY_ARRAYBLKOF_SIZE);
		unsigned* pair = tommy_arrayblkof_ref(&second, 0);
		*head = value;
		*tail = value + 1;
		pair[0] = value + 2;
		pair[1] = value + 3;
		if (tommy_arrayblkof_capacity(&first) != 2 * TOMMY_ARRAYBLKOF_SIZE || tommy_arrayblkof_capacity(&second) != TOMMY_ARRAYBLKOF_SIZE) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_swap(&first, &second);
		tommy_arrayblkof_swap(&second, &second);
		tommy_arrayblkof_grow(&first, 2);
		if (tommy_arrayblkof_size(&first) != 2 || tommy_arrayblkof_size(&second) != TOMMY_ARRAYBLKOF_SIZE + 1 || tommy_arrayblkof_capacity(&first) != TOMMY_ARRAYBLKOF_SIZE || tommy_arrayblkof_capacity(&second) != 2 * TOMMY_ARRAYBLKOF_SIZE || tommy_arrayblkof_ref(&first, 0) != pair || tommy_arrayblkof_ref(&first, 1) != pair + 2 || tommy_arrayblkof_ref(&second, TOMMY_ARRAYBLKOF_SIZE) != tail || pair[0] != value + 2 || pair[1] != value + 3 || *tail != value + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_resize(&second, 1);
		tommy_arrayblkof_shrink(&second);
		if (tommy_arrayblkof_capacity(&second) != TOMMY_ARRAYBLKOF_SIZE || tommy_arrayblkof_ref(&second, 0) != head || *head != value) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblkof_done(&first);
		tommy_arrayblkof_done(&second);
	}
}

void test_array_ops(void)
{
	tommy_array array;

	tommy_array_init(&array);

	/* foreach on empty array */
	the_count = 0;
	tommy_array_foreach(&array, count_callback);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	the_count = 0;
	tommy_array_foreach_arg(&array, count_arg_callback, &the_count);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* reserve on empty array */
	tommy_array_reserve(&array, 0);
	if (tommy_array_size(&array) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_array_reserve(&array, 50);
	if (tommy_array_size(&array) != 0 || tommy_array_memory_usage(&array) < 64 * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_array_reserve(&array, 100);
	if (tommy_array_size(&array) != 0 || tommy_array_memory_usage(&array) < 128 * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* resize growing */
	tommy_array_resize(&array, 100);
	if (tommy_array_size(&array) != 100) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < 100; ++i) {
		if (tommy_array_get(&array, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_array_set(&array, i, (void*)(i + 1));
	}

	/* resize shrinking and invariant check */
	tommy_array_resize(&array, 50);
	if (tommy_array_size(&array) != 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < 50; ++i) {
		if (tommy_array_get(&array, i) != (void*)(i + 1)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	tommy_array_resize(&array, 100);
	for (tommy_uintptr_t i = 50; i < 100; ++i) {
		if (tommy_array_get(&array, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* boundary resizes */
	tommy_array_resize(&array, 63);
	tommy_array_resize(&array, 64);
	tommy_array_resize(&array, 65);
	tommy_array_resize(&array, 127);
	tommy_array_resize(&array, 128);
	tommy_array_resize(&array, 129);
	tommy_array_resize(&array, 63);

	/* clear */
	tommy_array_set(&array, 0, (void*)1);
	tommy_array_clear(&array);
	if (tommy_array_size(&array) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_array_resize(&array, 10);
	for (tommy_uintptr_t i = 0; i < 10; ++i) {
		if (tommy_array_get(&array, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink */
	tommy_array_resize(&array, 200);
	for (tommy_uintptr_t i = 0; i < 70; ++i)
		tommy_array_set(&array, i, (void*)(i + 1));
	tommy_array_resize(&array, 70);
	tommy_array_shrink(&array);
	if (tommy_array_size(&array) != 70 || tommy_array_memory_usage(&array) != 128 * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < 70; ++i) {
		if (tommy_array_get(&array, i) != (void*)(i + 1)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_array_clear(&array);
	tommy_array_shrink(&array);
	if (tommy_array_size(&array) != 0 || tommy_array_memory_usage(&array) != 64 * sizeof(void*)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_array_grow(&array, 1);
	if (tommy_array_get(&array, 0) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* foreach and foreach_arg on populated array */
	tommy_array_resize(&array, 200);
	for (tommy_uintptr_t i = 0; i < 200; ++i)
		tommy_array_set(&array, i, (void*)(i + 1));
	the_count = 0;
	tommy_array_foreach(&array, array_foreach_check_callback);
	if (the_count != 200) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	{
		tommy_uintptr_t expected = 0;
		tommy_array_foreach_arg(&array, array_foreach_check_arg_callback, &expected);
		if (expected != 200) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_array_done(&array);
}

void test_arrayof_ops(void)
{
	tommy_arrayof arrayof;

	tommy_arrayof_init(&arrayof, sizeof(unsigned));

	/* foreach on empty array */
	the_count = 0;
	tommy_arrayof_foreach(&arrayof, count_callback);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	the_count = 0;
	tommy_arrayof_foreach_arg(&arrayof, count_arg_callback, &the_count);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* reserve on empty array */
	tommy_arrayof_reserve(&arrayof, 0);
	if (tommy_arrayof_size(&arrayof) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayof_reserve(&arrayof, 50);
	if (tommy_arrayof_size(&arrayof) != 0 || tommy_arrayof_memory_usage(&arrayof) < 64 * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayof_reserve(&arrayof, 100);
	if (tommy_arrayof_size(&arrayof) != 0 || tommy_arrayof_memory_usage(&arrayof) < 128 * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* resize growing */
	tommy_arrayof_resize(&arrayof, 100);
	if (tommy_arrayof_size(&arrayof) != 100) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < 100; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		*ref = i + 1;
	}

	/* resize shrinking and invariant check */
	tommy_arrayof_resize(&arrayof, 50);
	if (tommy_arrayof_size(&arrayof) != 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < 50; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != i + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}
	tommy_arrayof_resize(&arrayof, 100);
	for (unsigned i = 50; i < 100; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* boundary resizes */
	tommy_arrayof_resize(&arrayof, 63);
	tommy_arrayof_resize(&arrayof, 64);
	tommy_arrayof_resize(&arrayof, 65);
	tommy_arrayof_resize(&arrayof, 127);
	tommy_arrayof_resize(&arrayof, 128);
	tommy_arrayof_resize(&arrayof, 129);
	tommy_arrayof_resize(&arrayof, 63);

	/* clear */
	{
		unsigned* ref = tommy_arrayof_ref(&arrayof, 0);
		*ref = 1;
	}
	tommy_arrayof_clear(&arrayof);
	if (tommy_arrayof_size(&arrayof) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_arrayof_resize(&arrayof, 10);
	for (unsigned i = 0; i < 10; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink */
	tommy_arrayof_resize(&arrayof, 200);
	for (unsigned i = 0; i < 70; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		*ref = i + 1;
	}
	tommy_arrayof_resize(&arrayof, 70);
	tommy_arrayof_shrink(&arrayof);
	if (tommy_arrayof_size(&arrayof) != 70 || tommy_arrayof_memory_usage(&arrayof) != 128 * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < 70; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != i + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_arrayof_clear(&arrayof);
	tommy_arrayof_shrink(&arrayof);
	if (tommy_arrayof_size(&arrayof) != 0 || tommy_arrayof_memory_usage(&arrayof) != 64 * sizeof(unsigned)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayof_grow(&arrayof, 1);
	{
		unsigned* ref = tommy_arrayof_ref(&arrayof, 0);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* foreach and foreach_arg on populated array */
	tommy_arrayof_resize(&arrayof, 200);
	for (unsigned i = 0; i < 200; ++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		*ref = i + 1;
	}
	the_count = 0;
	tommy_arrayof_foreach(&arrayof, arrayof_foreach_check_callback);
	if (the_count != 200) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	{
		unsigned expected = 0;
		tommy_arrayof_foreach_arg(&arrayof, arrayof_foreach_check_arg_callback, &expected);
		if (expected != 200) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_arrayof_done(&arrayof);
}

void test_arrayblk_ops(void)
{
	tommy_arrayblk arrayblk;

	tommy_arrayblk_init(&arrayblk);

	/* foreach on empty array */
	the_count = 0;
	tommy_arrayblk_foreach(&arrayblk, count_callback);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	the_count = 0;
	tommy_arrayblk_foreach_arg(&arrayblk, count_arg_callback, &the_count);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* reserve on empty array */
	tommy_arrayblk_reserve(&arrayblk, 0);
	if (tommy_arrayblk_size(&arrayblk) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblk_reserve(&arrayblk, 100);
	if (tommy_arrayblk_size(&arrayblk) != 0 || arrayblk.block_count != 1) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblk_reserve(&arrayblk, TOMMY_ARRAYBLK_SIZE + 1);
	if (tommy_arrayblk_size(&arrayblk) != 0 || arrayblk.block_count != 2) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* test directory expansion beyond initial 64 block entries */
	tommy_arrayblk_reserve(&arrayblk, 65 * TOMMY_ARRAYBLK_SIZE);
	if (tommy_arrayblk_size(&arrayblk) != 0 || arrayblk.block_capacity < 65) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* resize growing across block boundary */
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE + 50);
	if (tommy_arrayblk_size(&arrayblk) != TOMMY_ARRAYBLK_SIZE + 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < TOMMY_ARRAYBLK_SIZE + 50; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_arrayblk_set(&arrayblk, i, (void*)(i + 1));
	}

	/* resize shrinking across block boundary */
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE - 10);
	if (tommy_arrayblk_size(&arrayblk) != TOMMY_ARRAYBLK_SIZE - 10) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < TOMMY_ARRAYBLK_SIZE - 10; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != (void*)(i + 1)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* regrowing and invariant check across block boundary */
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE + 50);
	for (tommy_uintptr_t i = TOMMY_ARRAYBLK_SIZE - 10; i < TOMMY_ARRAYBLK_SIZE + 50; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* boundary resizes */
	tommy_arrayblk_resize(&arrayblk, 4095);
	tommy_arrayblk_resize(&arrayblk, 4096);
	tommy_arrayblk_resize(&arrayblk, 4097);
	tommy_arrayblk_resize(&arrayblk, 4095);

	/* clear */
	tommy_arrayblk_set(&arrayblk, 0, (void*)1);
	tommy_arrayblk_clear(&arrayblk);
	if (tommy_arrayblk_size(&arrayblk) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_arrayblk_resize(&arrayblk, 10);
	for (tommy_uintptr_t i = 0; i < 10; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink with directory reduction */
	tommy_arrayblk_reserve(&arrayblk, 65 * TOMMY_ARRAYBLK_SIZE);
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE + 50);
	for (tommy_uintptr_t i = 0; i < TOMMY_ARRAYBLK_SIZE + 10; ++i)
		tommy_arrayblk_set(&arrayblk, i, (void*)(i + 1));
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE + 10);
	tommy_arrayblk_shrink(&arrayblk);
	if (tommy_arrayblk_size(&arrayblk) != TOMMY_ARRAYBLK_SIZE + 10 || arrayblk.block_count != 2 || arrayblk.block_capacity != 64) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < TOMMY_ARRAYBLK_SIZE + 10; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != (void*)(i + 1)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink with directory reduction while capacity > min_capacity */
	tommy_arrayblk_reserve(&arrayblk, 200 * TOMMY_ARRAYBLK_SIZE);
	tommy_arrayblk_resize(&arrayblk, 70 * TOMMY_ARRAYBLK_SIZE);
	for (tommy_uintptr_t i = 0; i < 70 * TOMMY_ARRAYBLK_SIZE; ++i)
		tommy_arrayblk_set(&arrayblk, i, (void*)(i + 1));
	tommy_arrayblk_shrink(&arrayblk);
	if (tommy_arrayblk_size(&arrayblk) != 70 * TOMMY_ARRAYBLK_SIZE || arrayblk.block_count != 70 || arrayblk.block_capacity != 128) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (tommy_uintptr_t i = 0; i < 70 * TOMMY_ARRAYBLK_SIZE; ++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != (void*)(i + 1)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink to empty */
	tommy_arrayblk_clear(&arrayblk);
	tommy_arrayblk_shrink(&arrayblk);
	if (tommy_arrayblk_size(&arrayblk) != 0 || arrayblk.block_count != 0 || arrayblk.block_capacity != 64) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblk_grow(&arrayblk, 1);
	if (tommy_arrayblk_get(&arrayblk, 0) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* foreach and foreach_arg on populated array spanning multiple blocks */
	tommy_arrayblk_resize(&arrayblk, TOMMY_ARRAYBLK_SIZE + 50);
	for (tommy_uintptr_t i = 0; i < TOMMY_ARRAYBLK_SIZE + 50; ++i)
		tommy_arrayblk_set(&arrayblk, i, (void*)(i + 1));
	the_count = 0;
	tommy_arrayblk_foreach(&arrayblk, array_foreach_check_callback);
	if (the_count != TOMMY_ARRAYBLK_SIZE + 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	{
		tommy_uintptr_t expected = 0;
		tommy_arrayblk_foreach_arg(&arrayblk, array_foreach_check_arg_callback, &expected);
		if (expected != TOMMY_ARRAYBLK_SIZE + 50) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_arrayblk_done(&arrayblk);
}

void test_arrayblkof_ops(void)
{
	tommy_arrayblkof arrayblkof;

	tommy_arrayblkof_init(&arrayblkof, sizeof(unsigned));

	/* foreach on empty array */
	the_count = 0;
	tommy_arrayblkof_foreach(&arrayblkof, count_callback);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	the_count = 0;
	tommy_arrayblkof_foreach_arg(&arrayblkof, count_arg_callback, &the_count);
	if (the_count != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* reserve on empty array */
	tommy_arrayblkof_reserve(&arrayblkof, 0);
	if (tommy_arrayblkof_size(&arrayblkof) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblkof_reserve(&arrayblkof, 100);
	if (tommy_arrayblkof_size(&arrayblkof) != 0 || arrayblkof.block_count != 1) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblkof_reserve(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 1);
	if (tommy_arrayblkof_size(&arrayblkof) != 0 || arrayblkof.block_count != 2) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* test directory expansion beyond initial 64 block entries */
	tommy_arrayblkof_reserve(&arrayblkof, 65 * TOMMY_ARRAYBLKOF_SIZE);
	if (tommy_arrayblkof_size(&arrayblkof) != 0 || arrayblkof.block_capacity < 65) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* resize growing across block boundary */
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 50);
	if (tommy_arrayblkof_size(&arrayblkof) != TOMMY_ARRAYBLKOF_SIZE + 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < TOMMY_ARRAYBLKOF_SIZE + 50; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		*ref = i + 1;
	}

	/* resize shrinking across block boundary */
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE - 10);
	if (tommy_arrayblkof_size(&arrayblkof) != TOMMY_ARRAYBLKOF_SIZE - 10) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < TOMMY_ARRAYBLKOF_SIZE - 10; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != i + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* regrowing and invariant check across block boundary */
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 50);
	for (unsigned i = TOMMY_ARRAYBLKOF_SIZE - 10; i < TOMMY_ARRAYBLKOF_SIZE + 50; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* boundary resizes */
	tommy_arrayblkof_resize(&arrayblkof, 4095);
	tommy_arrayblkof_resize(&arrayblkof, 4096);
	tommy_arrayblkof_resize(&arrayblkof, 4097);
	tommy_arrayblkof_resize(&arrayblkof, 4095);

	/* clear */
	{
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, 0);
		*ref = 1;
	}
	tommy_arrayblkof_clear(&arrayblkof);
	if (tommy_arrayblkof_size(&arrayblkof) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_arrayblkof_resize(&arrayblkof, 10);
	for (unsigned i = 0; i < 10; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink with directory reduction */
	tommy_arrayblkof_reserve(&arrayblkof, 65 * TOMMY_ARRAYBLKOF_SIZE);
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 50);
	for (unsigned i = 0; i < TOMMY_ARRAYBLKOF_SIZE + 10; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		*ref = i + 1;
	}
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 10);
	tommy_arrayblkof_shrink(&arrayblkof);
	if (tommy_arrayblkof_size(&arrayblkof) != TOMMY_ARRAYBLKOF_SIZE + 10 || arrayblkof.block_count != 2 || arrayblkof.block_capacity != 64) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < TOMMY_ARRAYBLKOF_SIZE + 10; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != i + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink with directory reduction while capacity > min_capacity */
	tommy_arrayblkof_reserve(&arrayblkof, 200 * TOMMY_ARRAYBLKOF_SIZE);
	tommy_arrayblkof_resize(&arrayblkof, 70 * TOMMY_ARRAYBLKOF_SIZE);
	for (unsigned i = 0; i < 70 * TOMMY_ARRAYBLKOF_SIZE; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		*ref = i + 1;
	}
	tommy_arrayblkof_shrink(&arrayblkof);
	if (tommy_arrayblkof_size(&arrayblkof) != 70 * TOMMY_ARRAYBLKOF_SIZE || arrayblkof.block_count != 70 || arrayblkof.block_capacity != 128) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	for (unsigned i = 0; i < 70 * TOMMY_ARRAYBLKOF_SIZE; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != i + 1) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* shrink to empty */
	tommy_arrayblkof_clear(&arrayblkof);
	tommy_arrayblkof_shrink(&arrayblkof);
	if (tommy_arrayblkof_size(&arrayblkof) != 0 || arrayblkof.block_count != 0 || arrayblkof.block_capacity != 64) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	tommy_arrayblkof_grow(&arrayblkof, 1);
	{
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, 0);
		if (*ref != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	/* foreach and foreach_arg on populated array spanning multiple blocks */
	tommy_arrayblkof_resize(&arrayblkof, TOMMY_ARRAYBLKOF_SIZE + 50);
	for (unsigned i = 0; i < TOMMY_ARRAYBLKOF_SIZE + 50; ++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		*ref = i + 1;
	}
	the_count = 0;
	tommy_arrayblkof_foreach(&arrayblkof, arrayof_foreach_check_callback);
	if (the_count != TOMMY_ARRAYBLKOF_SIZE + 50) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	{
		unsigned expected = 0;
		tommy_arrayblkof_foreach_arg(&arrayblkof, arrayof_foreach_check_arg_callback, &expected);
		if (expected != TOMMY_ARRAYBLKOF_SIZE + 50) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	tommy_arrayblkof_done(&arrayblkof);
}

/* verify node identity, metadata, both list directions and per-bucket order. */
void test_hash_list(tommy_list* list, struct object_hash* obj, tommy_node* saved, unsigned size, unsigned prefix)
{
	unsigned order[130], position[130];
	unsigned seen[130] = { 0 };
	tommy_node* node = tommy_list_head(list);
	tommy_node* prev = tommy_list_tail(list);

	for (unsigned i = 0; i < size; ++i) {
		unsigned j;
		for (j = 0; j < size; ++j)
			if (node == &obj[j].node)
				break;
		if (j == size || seen[j] || (i < prefix && j != i)
			|| node->prev != prev || node->data != saved[j].data
			|| node->index != saved[j].index) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		seen[j] = 1;
		order[i] = j;
		position[j] = i;
		prev = node;
		node = node->next;
	}
	if (node != 0 || tommy_list_tail(list) != (size ? prev : 0)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* each original successor must still follow its node, regardless of bucket order. */
	for (unsigned i = 0; i < size; ++i) {
		if (saved[i].next == 0)
			continue;
		unsigned j;
		for (j = 0; j < size; ++j)
			if (saved[i].next == &obj[j].node)
				break;
		if (j == size || position[i] >= position[j]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
	}

	node = tommy_list_tail(list);
	for (unsigned i = size; i > 0; --i) {
		if (node != &obj[order[i - 1]].node) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		node = tommy_list_prev(list, node);
	}
	if (node != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

/* verify the exact bucket order and circular head/tail links. */
void test_hash_bucket(tommy_node* node, struct object_hash* obj, const unsigned* order, unsigned size)
{
	tommy_node* prev = size ? &obj[order[size - 1]].node : 0;

	for (unsigned i = 0; i < size; ++i) {
		if (node != &obj[order[i]].node || node->prev != prev || node->data != &obj[order[i]]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		prev = node;
		node = node->next;
	}
	if (node != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
}

void test_hashtable_insert_unique(void)
{
	struct object_hash obj[257], candidate;
	tommy_hashtable table;
	const unsigned order[] = { 0, 1, 2, 3, 4, 5 };

	tommy_hashtable_init(&table, 16);
	obj[0].value = 1;
	compare_counter = 0;
	if (tommy_hashtable_insert_unique(&table, &obj[0].node, &obj[0], search_hash_value_callback, &obj[0].value, 0) != &obj[0]
		|| compare_counter != 0 || tommy_hashtable_count(&table) != 1
		|| tommy_hashtable_search(&table, search_hash_value_callback, &obj[0].value, 0) != &obj[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashtable_remove_existing(&table, &obj[0].node);

	tommy_size_t bucket_max = table.bucket_max;
	obj[0].value = 10;
	obj[1].value = 20;
	obj[2].value = 30;
	obj[3].value = 20;
	for (unsigned i = 0; i < 4; ++i)
		tommy_hashtable_insert(&table, &obj[i].node, &obj[i], i == 0 ? bucket_max : 0);

	/* the first matching duplicate is returned; the rejected candidate stays intact. */
	obj[4].value = 20;
	obj[4].node.next = &obj[4].node;
	obj[4].node.prev = &obj[4].node;
	obj[4].node.data = &obj[4];
	obj[4].node.index = ~(tommy_hash_t)0;
	tommy_node saved = obj[4].node;
	compare_counter = 0;
	if (tommy_hashtable_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[1]
		|| compare_counter != 1 || tommy_hashtable_count(&table) != 4
		|| obj[4].value != 20 || obj[4].node.next != saved.next || obj[4].node.prev != saved.prev
		|| obj[4].node.data != saved.data || obj[4].node.index != saved.index) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashtable_bucket(&table, 0), obj, order, 4);

	/* equal hashes with different keys must allow insertion at the tail. */
	obj[4].value = 40;
	compare_counter = 0;
	if (tommy_hashtable_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[4]
		|| compare_counter != 3 || tommy_hashtable_count(&table) != 5
		|| obj[4].node.data != &obj[4] || obj[4].node.index != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* different full hashes in the same bucket must never invoke the comparator. */
	obj[5].value = 50;
	compare_counter = 0;
	if (tommy_hashtable_insert_unique(&table, &obj[5].node, &obj[5], search_hash_value_callback, &obj[5].value, 2 * bucket_max) != &obj[5]
		|| compare_counter != 0 || tommy_hashtable_count(&table) != 6) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashtable_bucket(&table, 0), obj, order, 6);
	tommy_hashtable_remove_existing(&table, &obj[1].node);
	obj[6].value = 20;
	if (tommy_hashtable_insert_unique(&table, &obj[6].node, &obj[6], search_hash_value_callback, &obj[6].value, 0) != &obj[3]
		|| tommy_hashtable_count(&table) != 5) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashtable_done(&table);

	/* insert unique keys, reject duplicates, then repeat during contraction. */
	tommy_hashtable_init(&table, 16);
	candidate.node.next = &candidate.node;
	candidate.node.prev = &candidate.node;
	candidate.node.data = &candidate;
	candidate.node.index = ~(tommy_hash_t)0;
	for (unsigned phase = 0; phase < 2; ++phase) {
		for (unsigned step = 0; step < 256; ++step) {
			unsigned i = phase == 0 ? step : 255 - step;
			tommy_hash_t hash = tommy_inthash_u32(i);
			if (phase == 0) {
				obj[i].value = i;
				if (tommy_hashtable_insert_unique(&table, &obj[i].node, &obj[i], search_hash_value_callback, &obj[i].value, hash) != &obj[i]
					|| tommy_hashtable_count(&table) != i + 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}

			candidate.value = i;
			saved = obj[i].node;
			tommy_size_t count = tommy_hashtable_count(&table);
			bucket_max = table.bucket_max;
			tommy_size_t bucket_mask = table.bucket_mask;
			tommy_hashtable_node** bucket = table.bucket;
			compare_counter = 0;
			if (tommy_hashtable_insert_unique(&table, &candidate.node, &candidate, search_hash_value_callback, &candidate.value, hash) != &obj[i]
				|| compare_counter != 1 || tommy_hashtable_count(&table) != count
				|| table.bucket_max != bucket_max || table.bucket_mask != bucket_mask
				|| table.bucket != bucket
				|| candidate.value != (int)i || candidate.node.next != &candidate.node
				|| candidate.node.prev != &candidate.node || candidate.node.data != &candidate
				|| candidate.node.index != ~(tommy_hash_t)0 || obj[i].node.next != saved.next
				|| obj[i].node.prev != saved.prev || obj[i].node.data != saved.data || obj[i].node.index != saved.index) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			if (phase != 0) {
				if (tommy_hashtable_remove_existing(&table, &obj[i].node) != &obj[i]
					|| tommy_hashtable_count(&table) != count - 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also insert a new key while hashlin is shrinking progressively. */
				if (i == 24) {
					obj[256].value = 256;
					if (tommy_hashtable_insert_unique(&table, &obj[256].node, &obj[256], search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashtable_count(&table) != count
						|| tommy_hashtable_search(&table, search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashtable_remove_existing(&table, &obj[256].node) != &obj[256]
						|| tommy_hashtable_count(&table) != count - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
			}
		}
	}
	tommy_hashtable_done(&table);
}

void test_hashtable_rehash_existing(void)
{
	const unsigned counts[][2] = { { 1, 1 }, { 4, 4 }, { 128, 128 }, { 128, 31 } };
	struct object_hash obj[130];
	tommy_hash_t hashes[130];
	tommy_hashtable table;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c][1];
		unsigned source_size = n < 3 ? n : 3;
		for (unsigned target = 0; target < source_size; ++target) {
			for (unsigned mode = 0; mode < 4; ++mode) {
				unsigned source_count = 0, destination_count = 0;
				unsigned source[3], destination[2];

				tommy_hashtable_init(&table, 16);
				for (unsigned i = 0; i < counts[c][0]; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashtable_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = counts[c][0]; i > n; --i)
					tommy_hashtable_remove_existing(&table, &obj[i - 1].node);

				tommy_size_t bucket_max = table.bucket_max;
				tommy_size_t bucket_mask = table.bucket_mask;
				tommy_hashtable_node** bucket = table.bucket;
				/* exercise unchanged hash, same bucket, occupied bucket and empty bucket. */
				tommy_hash_t hash = mode == 0 ? 0 : mode == 1 ? bucket_max : mode == 2 ? 1 : ~(tommy_hash_t)0;
				tommy_node saved = obj[target].node;
				obj[target].value = 1000 + target;
				tommy_hashtable_rehash_existing(&table, &obj[target].node, hash);
				hashes[target] = hash;

				if (tommy_hashtable_count(&table) != n || table.bucket_max != bucket_max
					|| table.bucket_mask != bucket_mask
					|| table.bucket != bucket
					|| obj[target].node.index != hash || obj[target].node.data != saved.data
					|| obj[target].value != (int)(1000 + target)
					|| (mode == 0 && (obj[target].node.next != saved.next || obj[target].node.prev != saved.prev))) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

				for (unsigned i = 0; i < source_size; ++i)
					if (mode == 0 || i != target)
						source[source_count++] = i;
				if (mode == 1)
					source[source_count++] = target;
				test_hash_bucket(tommy_hashtable_bucket(&table, 0), obj, source, source_count);

				if (n > 3)
					destination[destination_count++] = 3;
				if (mode == 2)
					destination[destination_count++] = target;
				test_hash_bucket(tommy_hashtable_bucket(&table, 1), obj, destination, destination_count);
				if (mode == 3)
					test_hash_bucket(tommy_hashtable_bucket(&table, hash), obj, &target, 1);
				if (mode != 0 && tommy_hashtable_search(&table, search_callback, &obj[target], 0) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also unlink from the new hash, including an unsplit high position. */
				if (mode != 0) {
					tommy_hashtable_rehash_existing(&table, &obj[target].node, 0);
					if (tommy_hashtable_count(&table) != n || obj[target].node.index != 0
						|| tommy_hashtable_search(&table, search_callback, &obj[target], 0) != &obj[target]
						|| tommy_hashtable_search(&table, search_callback, &obj[target], hash) != 0) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
					tommy_hashtable_rehash_existing(&table, &obj[target].node, hash);
				}
				for (unsigned i = 0; i < n; ++i)
					if (tommy_hashtable_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}

				/* subsequent resizing and removals must use the updated stored hash. */
				for (unsigned i = n; i < 130; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashtable_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = 0; i < 130; ++i)
					if (tommy_hashtable_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				for (unsigned i = 130; i > 0; --i) {
					void* data;
					if (i % 2)
						data = tommy_hashtable_remove_existing(&table, &obj[i - 1].node);
					else
						data = tommy_hashtable_remove(&table, search_callback, &obj[i - 1], hashes[i - 1]);
					if (data != &obj[i - 1] || tommy_hashtable_count(&table) != i - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
				tommy_hashtable_done(&table);
			}
		}
	}
}

void test_hashtable_to_list(void)
{
	const unsigned counts[] = { 0, 1, 16, 128 };
	struct object_hash obj[130];
	tommy_node saved[130];
	tommy_hashtable hashtable;
	tommy_list list;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		for (unsigned prefix = 0; prefix <= 2; prefix += 2) {
			unsigned size = prefix + counts[c];
			tommy_hashtable_init(&hashtable, counts[c] + 1);
			tommy_list_init(&list);
			for (unsigned i = 0; i < size; ++i) {
				obj[i].value = i / 2;
				obj[i].node.index = tommy_inthash_u32(obj[i].value);
				if (i < prefix)
					tommy_list_insert_tail(&list, &obj[i].node, &obj[i]);
				else
					tommy_hashtable_insert(&hashtable, &obj[i].node, &obj[i], obj[i].node.index);
			}
			for (unsigned i = 0; i < size; ++i)
				saved[i] = obj[i].node;
			tommy_hashtable_node** bucket = hashtable.bucket;
			tommy_size_t bucket_max = hashtable.bucket_max;
			tommy_size_t bucket_mask = hashtable.bucket_mask;

			tommy_hashtable_to_list(&hashtable, &list);
			test_hash_list(&list, obj, saved, size, prefix);
			if (tommy_hashtable_count(&hashtable) != 0 || hashtable.bucket != bucket
				|| hashtable.bucket_max != bucket_max || hashtable.bucket_mask != bucket_mask
				|| tommy_hashtable_memory_usage(&hashtable) != bucket_max * sizeof(*bucket)) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < bucket_max; ++i)
				if (tommy_hashtable_bucket(&hashtable, i) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			the_count = 0;
			tommy_hashtable_foreach(&hashtable, count_callback);
			if (the_count != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = prefix; i < size; ++i)
				if (tommy_hashtable_search(&hashtable, search_callback, &obj[i], obj[i].node.index) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			/* a repeated call on the empty table must preserve the complete list order. */
			for (unsigned i = 0; i < size; ++i)
				saved[i] = obj[i].node;
			tommy_hashtable_to_list(&hashtable, &list);
			test_hash_list(&list, obj, saved, size, prefix);

			/* transfer the detached nodes back, then search and remove each one. */
			while (!tommy_list_empty(&list)) {
				struct object_hash* data = tommy_list_remove_head(&list);
				tommy_hashtable_insert(&hashtable, &data->node, data, data->node.index);
			}
			if (tommy_hashtable_count(&hashtable) != size) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < size; ++i)
				if (tommy_hashtable_search(&hashtable, search_callback, &obj[i], obj[i].node.index) != &obj[i]
					|| tommy_hashtable_remove_existing(&hashtable, &obj[i].node) != &obj[i]
					|| tommy_hashtable_count(&hashtable) != size - i - 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			tommy_hashtable_done(&hashtable);
		}
	}
}

void test_hashtable_clear(void)
{
	const unsigned counts[] = { 0, 1, 8, 32, 128 };
	struct object_hash obj[128];
	tommy_hashtable hashtable;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c];
		tommy_hashtable_init(&hashtable, n + 1);

		/* include duplicates and different hashes sharing a bucket. */
		for (unsigned i = 0; i < n; ++i) {
			obj[i].value = i / 2;
			tommy_hashtable_insert(&hashtable, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
		}

		tommy_hashtable_node** bucket = hashtable.bucket;
		tommy_size_t bucket_max = hashtable.bucket_max;
		tommy_size_t bucket_mask = hashtable.bucket_mask;
		tommy_size_t memory_usage = tommy_hashtable_memory_usage(&hashtable) - n * sizeof(tommy_hashtable_node);

		/* clearing an empty table again must preserve its capacity too. */
		for (unsigned j = 0; j < 2; ++j) {
			tommy_hashtable_clear(&hashtable);
			if (tommy_hashtable_count(&hashtable) != 0
				|| hashtable.bucket != bucket || hashtable.bucket_max != bucket_max
				|| hashtable.bucket_mask != bucket_mask
				|| tommy_hashtable_memory_usage(&hashtable) != memory_usage) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < bucket_max; ++i)
				if (tommy_hashtable_bucket(&hashtable, i) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			the_count = 0;
			tommy_hashtable_foreach(&hashtable, count_callback);
			tommy_hashtable_foreach_arg(&hashtable, count_arg_callback, &the_count);
			if (the_count != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < n; ++i) {
				tommy_hash_t hash = tommy_inthash_u32(obj[i].value);
				if (obj[i].value != (int)(i / 2)
					|| obj[i].node.data != &obj[i] || obj[i].node.index != hash
					|| tommy_hashtable_search(&hashtable, search_callback, &obj[i], hash) != 0
					|| tommy_hashtable_remove(&hashtable, search_callback, &obj[i], hash) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}
		}

		/* reuse the same nodes without initializing their old links. */
		for (unsigned i = 0; i < 128; ++i) {
			obj[i].value = i / 2;
			tommy_hashtable_insert(&hashtable, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
		}
		if (tommy_hashtable_count(&hashtable) != 128) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		for (unsigned i = 0; i < 128; ++i)
			if (tommy_hashtable_search(&hashtable, search_callback, &obj[i], tommy_inthash_u32(obj[i].value)) != &obj[i]) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		for (unsigned i = 128; i > 0; --i) {
			void* data;
			if (i % 2)
				data = tommy_hashtable_remove_existing(&hashtable, &obj[i - 1].node);
			else
				data = tommy_hashtable_remove(&hashtable, search_callback, &obj[i - 1], tommy_inthash_u32(obj[i - 1].value));
			if (data != &obj[i - 1] || tommy_hashtable_count(&hashtable) != i - 1) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		tommy_hashtable_done(&hashtable);
	}

	/* clear must not access objects already freed by foreach. */
	tommy_hashtable_init(&hashtable, 128);
	for (unsigned i = 0; i < 128; ++i) {
		struct object_hash* allocated = malloc(sizeof(struct object_hash));
		allocated->value = i;
		tommy_hashtable_insert(&hashtable, &allocated->node, allocated, tommy_inthash_u32(i));
	}
	tommy_hashtable_foreach(&hashtable, free);
	tommy_hashtable_clear(&hashtable);
	obj[0].value = 1;
	tommy_hashtable_insert(&hashtable, &obj[0].node, &obj[0], 1);
	if (tommy_hashtable_count(&hashtable) != 1
		|| tommy_hashtable_remove_existing(&hashtable, &obj[0].node) != &obj[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashtable_done(&hashtable);
}

void test_hashtable(void)
{
	tommy_hashtable hashtable;
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;

	test_hashtable_clear();
	test_hashtable_insert_unique();
	test_hashtable_rehash_existing();
	test_hashtable_to_list();

	struct object_hash* HASH = malloc(size * sizeof(struct object_hash));

	for (unsigned i = 0; i < size; ++i)
		HASH[i].value = i % module;

	/* initialize a very small hashtable */
	tommy_hashtable_init(&hashtable, 1);

	/* check that we allocated space for more elements */
	if (hashtable.bucket_max == 1) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* destroy it as empty */
	tommy_hashtable_done(&hashtable);

	START("hashtable stack");
	unsigned limit = 5 * isqrt(size);
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashtable_init(&hashtable, limit / 2);

		/* insert */
		for (unsigned i = 0; i < n; ++i)
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashtable_memory_usage(&hashtable) < n * sizeof(void*)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		if (tommy_hashtable_count(&hashtable) != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		the_count = 0;
		tommy_hashtable_foreach(&hashtable, count_callback);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* remove in backward order */
		for (unsigned i = 0; i < n / 2; ++i)
			tommy_hashtable_remove_existing(&hashtable, &HASH[n - i - 1].node);

		/* remove missing */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[n - i - 1], HASH[n - i - 1].value) != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		/* remove search */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[n / 2 - i - 1], HASH[n / 2 - i - 1].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashtable_done(&hashtable);
	}
	STOP();

	START("hashtable queue");
	limit = isqrt(size) / 16;
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashtable_init(&hashtable, limit / 2);

		/* insert first run */
		unsigned j = 0, i = 0;
		for (; i < n; ++i)
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashtable_foreach_arg(&hashtable, count_arg_callback, &the_count);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* insert all the others */
		for (; i < size; ++i, ++j) {
			/* insert one */
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashtable_remove_existing(&hashtable, &HASH[j].node);
		}

		for (; j < size; ++j)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[j], HASH[j].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashtable_done(&hashtable);
	}
	STOP();
}

void test_hashdyn_shrink(void)
{
	const unsigned counts[][2] = {
		{ 0, 16 }, { 1, 16 }, { 7, 16 }, { 8, 32 },
		{ 15, 32 }, { 16, 64 }, { 31, 64 }, { 32, 128 },
		{ 63, 128 }, { 64, 256 }, { 65, 256 }, { 128, 512 }
	};
	struct object_hash obj[130];
	tommy_node saved[130];

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		for (unsigned reserved = 0; reserved < 3; ++reserved) {
			unsigned n = counts[c][0];
			tommy_hashdyn table;
			tommy_hashdyn_init(&table);
			tommy_hashdyn_reserve(&table, 0);
			/* reserve far beyond the contents to exercise multi-bit contractions. */
			if (reserved == 1)
				tommy_hashdyn_reserve(&table, 1024);
			for (unsigned i = 0; i < n; ++i) {
				obj[i].value = i / 2;
				tommy_hashdyn_insert(&table, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
			}
			if (reserved == 2)
				tommy_hashdyn_reserve(&table, 1024);
			for (unsigned i = 0; i < n; ++i)
				saved[i] = obj[i].node;
			tommy_hashdyn_node** bucket = table.bucket;
			tommy_hashdyn_shrink(&table);
			if (tommy_hashdyn_bucket_count(&table) != counts[c][1]
				|| table.bucket_mask != counts[c][1] - 1
				|| ((tommy_size_t)1 << table.bucket_bit) != counts[c][1]
				|| tommy_hashdyn_count(&table) != n
				|| tommy_hashdyn_memory_usage(&table) != counts[c][1] * sizeof(*bucket) + n * sizeof(tommy_hashdyn_node)
				|| (!reserved && table.bucket != bucket)) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			/* repeated shrinking must not allocate or disturb the contents. */
			bucket = table.bucket;
			tommy_hashdyn_shrink(&table);
			if (table.bucket != bucket || tommy_hashdyn_count(&table) != n) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < n; ++i) {
				if (obj[i].node.index != saved[i].index || obj[i].node.data != saved[i].data
					|| obj[i].value != (int)(i / 2)
					|| tommy_hashdyn_search(&table, search_callback, &obj[i], saved[i].index) != &obj[i]
					|| tommy_hashdyn_search(&table, search_hash_value_callback, &obj[i].value, saved[i].index) != &obj[i - i % 2]) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}

			/* check all links and original bucket ordering, then shrink the drained table. */
			tommy_list list;
			tommy_list_init(&list);
			tommy_hashdyn_to_list(&table, &list);
			tommy_hashdyn_shrink(&table);
			if (!tommy_hashdyn_empty(&table) || tommy_hashdyn_bucket_count(&table) != 16) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			test_hash_list(&list, obj, saved, n, 0);

			/* reuse the shrunken table, grow it and remove duplicates in insertion order. */
			while (!tommy_list_empty(&list)) {
				struct object_hash* data = tommy_list_remove_head(&list);
				tommy_hashdyn_insert(&table, &data->node, data, data->node.index);
			}
			for (unsigned i = n; i < 130; ++i) {
				obj[i].value = i / 2;
				tommy_hashdyn_insert(&table, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
			}
			if (tommy_hashdyn_count(&table) != 130 || tommy_hashdyn_bucket_count(&table) != 512) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < 130; ++i)
				if (tommy_hashdyn_remove(&table, search_hash_value_callback, &obj[i].value, obj[i].node.index) != &obj[i]
					|| tommy_hashdyn_count(&table) != 129 - i) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			tommy_hashdyn_done(&table);
		}
	}
}

void test_hashdyn_insert_unique(void)
{
	struct object_hash obj[257];
	tommy_hashdyn table;
	const unsigned order[] = { 0, 1, 2, 3, 4, 5 };

	tommy_hashdyn_init(&table);
	obj[0].value = 1;
	compare_counter = 0;
	if (tommy_hashdyn_insert_unique(&table, &obj[0].node, &obj[0], search_hash_value_callback, &obj[0].value, 0) != &obj[0]
		|| compare_counter != 0 || tommy_hashdyn_count(&table) != 1
		|| tommy_hashdyn_search(&table, search_hash_value_callback, &obj[0].value, 0) != &obj[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashdyn_remove_existing(&table, &obj[0].node);

	tommy_size_t bucket_max = table.bucket_max;
	obj[0].value = 10;
	obj[1].value = 20;
	obj[2].value = 30;
	obj[3].value = 20;
	for (unsigned i = 0; i < 4; ++i)
		tommy_hashdyn_insert(&table, &obj[i].node, &obj[i], i == 0 ? bucket_max : 0);

	/* the first matching duplicate is returned; the rejected candidate stays intact. */
	obj[4].value = 20;
	obj[4].node.next = &obj[4].node;
	obj[4].node.prev = &obj[4].node;
	obj[4].node.data = &obj[4];
	obj[4].node.index = ~(tommy_hash_t)0;
	tommy_node saved = obj[4].node;
	compare_counter = 0;
	if (tommy_hashdyn_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[1]
		|| compare_counter != 1 || tommy_hashdyn_count(&table) != 4
		|| obj[4].value != 20 || obj[4].node.next != saved.next || obj[4].node.prev != saved.prev
		|| obj[4].node.data != saved.data || obj[4].node.index != saved.index) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashdyn_bucket(&table, 0), obj, order, 4);

	/* equal hashes with different keys must allow insertion at the tail. */
	obj[4].value = 40;
	compare_counter = 0;
	if (tommy_hashdyn_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[4]
		|| compare_counter != 3 || tommy_hashdyn_count(&table) != 5
		|| obj[4].node.data != &obj[4] || obj[4].node.index != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* different full hashes in the same bucket must never invoke the comparator. */
	obj[5].value = 50;
	compare_counter = 0;
	if (tommy_hashdyn_insert_unique(&table, &obj[5].node, &obj[5], search_hash_value_callback, &obj[5].value, 2 * bucket_max) != &obj[5]
		|| compare_counter != 0 || tommy_hashdyn_count(&table) != 6) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashdyn_bucket(&table, 0), obj, order, 6);
	tommy_hashdyn_remove_existing(&table, &obj[1].node);
	obj[6].value = 20;
	if (tommy_hashdyn_insert_unique(&table, &obj[6].node, &obj[6], search_hash_value_callback, &obj[6].value, 0) != &obj[3]
		|| tommy_hashdyn_count(&table) != 5) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashdyn_done(&table);

	/* insert unique keys, reject duplicates, then repeat during contraction. */
	tommy_hashdyn_init(&table);
	struct object_hash candidate;
	candidate.node.next = &candidate.node;
	candidate.node.prev = &candidate.node;
	candidate.node.data = &candidate;
	candidate.node.index = ~(tommy_hash_t)0;
	for (unsigned phase = 0; phase < 2; ++phase) {
		for (unsigned step = 0; step < 256; ++step) {
			unsigned i = phase == 0 ? step : 255 - step;
			tommy_hash_t hash = tommy_inthash_u32(i);
			if (phase == 0) {
				obj[i].value = i;
				if (tommy_hashdyn_insert_unique(&table, &obj[i].node, &obj[i], search_hash_value_callback, &obj[i].value, hash) != &obj[i]
					|| tommy_hashdyn_count(&table) != i + 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}

			candidate.value = i;
			saved = obj[i].node;
			tommy_size_t count = tommy_hashdyn_count(&table);
			bucket_max = table.bucket_max;
			tommy_size_t bucket_mask = table.bucket_mask;
			tommy_hashdyn_node** bucket = table.bucket;
			tommy_uint_t bucket_bit = table.bucket_bit;
			compare_counter = 0;
			if (tommy_hashdyn_insert_unique(&table, &candidate.node, &candidate, search_hash_value_callback, &candidate.value, hash) != &obj[i]
				|| compare_counter != 1 || tommy_hashdyn_count(&table) != count
				|| table.bucket_max != bucket_max || table.bucket_mask != bucket_mask
				|| table.bucket != bucket || table.bucket_bit != bucket_bit
				|| candidate.value != (int)i || candidate.node.next != &candidate.node
				|| candidate.node.prev != &candidate.node || candidate.node.data != &candidate
				|| candidate.node.index != ~(tommy_hash_t)0 || obj[i].node.next != saved.next
				|| obj[i].node.prev != saved.prev || obj[i].node.data != saved.data || obj[i].node.index != saved.index) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			if (phase != 0) {
				if (tommy_hashdyn_remove_existing(&table, &obj[i].node) != &obj[i]
					|| tommy_hashdyn_count(&table) != count - 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also insert a new key while hashlin is shrinking progressively. */
				if (i == 24) {
					obj[256].value = 256;
					if (tommy_hashdyn_insert_unique(&table, &obj[256].node, &obj[256], search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashdyn_count(&table) != count
						|| tommy_hashdyn_search(&table, search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashdyn_remove_existing(&table, &obj[256].node) != &obj[256]
						|| tommy_hashdyn_count(&table) != count - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
			}
		}
	}
	tommy_hashdyn_done(&table);
}

void test_hashdyn_rehash_existing(void)
{
	const unsigned counts[][2] = { { 1, 1 }, { 4, 4 }, { 128, 128 }, { 128, 31 } };
	struct object_hash obj[130];
	tommy_hash_t hashes[130];
	tommy_hashdyn table;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c][1];
		unsigned source_size = n < 3 ? n : 3;
		for (unsigned target = 0; target < source_size; ++target) {
			for (unsigned mode = 0; mode < 4; ++mode) {
				unsigned source[3], destination[2];
				unsigned source_count = 0, destination_count = 0;

				tommy_hashdyn_init(&table);
				if (n == 4)
					tommy_hashdyn_reserve(&table, 128);
				for (unsigned i = 0; i < counts[c][0]; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashdyn_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = counts[c][0]; i > n; --i)
					tommy_hashdyn_remove_existing(&table, &obj[i - 1].node);

				tommy_size_t bucket_max = table.bucket_max;
				tommy_size_t bucket_mask = table.bucket_mask;
				tommy_hashdyn_node** bucket = table.bucket;
				tommy_uint_t bucket_bit = table.bucket_bit;
				/* exercise unchanged hash, same bucket, occupied bucket and empty bucket. */
				tommy_hash_t hash = mode == 0 ? 0 : mode == 1 ? bucket_max : mode == 2 ? 1 : ~(tommy_hash_t)0;
				tommy_node saved = obj[target].node;
				obj[target].value = 1000 + target;
				tommy_hashdyn_rehash_existing(&table, &obj[target].node, hash);
				hashes[target] = hash;

				if (tommy_hashdyn_count(&table) != n || table.bucket_max != bucket_max
					|| table.bucket_mask != bucket_mask
					|| table.bucket != bucket || table.bucket_bit != bucket_bit
					|| obj[target].node.index != hash || obj[target].node.data != saved.data
					|| obj[target].value != (int)(1000 + target)
					|| (mode == 0 && (obj[target].node.next != saved.next || obj[target].node.prev != saved.prev))) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

				for (unsigned i = 0; i < source_size; ++i)
					if (mode == 0 || i != target)
						source[source_count++] = i;
				if (mode == 1)
					source[source_count++] = target;
				test_hash_bucket(tommy_hashdyn_bucket(&table, 0), obj, source, source_count);

				if (n > 3)
					destination[destination_count++] = 3;
				if (mode == 2)
					destination[destination_count++] = target;
				test_hash_bucket(tommy_hashdyn_bucket(&table, 1), obj, destination, destination_count);
				if (mode == 3)
					test_hash_bucket(tommy_hashdyn_bucket(&table, hash), obj, &target, 1);
				if (mode != 0 && tommy_hashdyn_search(&table, search_callback, &obj[target], 0) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also unlink from the new hash, including an unsplit high position. */
				if (mode != 0) {
					tommy_hashdyn_rehash_existing(&table, &obj[target].node, 0);
					if (tommy_hashdyn_count(&table) != n || obj[target].node.index != 0
						|| tommy_hashdyn_search(&table, search_callback, &obj[target], 0) != &obj[target]
						|| tommy_hashdyn_search(&table, search_callback, &obj[target], hash) != 0) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
					tommy_hashdyn_rehash_existing(&table, &obj[target].node, hash);
				}
				for (unsigned i = 0; i < n; ++i)
					if (tommy_hashdyn_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}

				/* subsequent resizing and removals must use the updated stored hash. */
				for (unsigned i = n; i < 130; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashdyn_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = 0; i < 130; ++i)
					if (tommy_hashdyn_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				for (unsigned i = 130; i > 0; --i) {
					void* data;
					if (i % 2)
						data = tommy_hashdyn_remove_existing(&table, &obj[i - 1].node);
					else
						data = tommy_hashdyn_remove(&table, search_callback, &obj[i - 1], hashes[i - 1]);
					if (data != &obj[i - 1] || tommy_hashdyn_count(&table) != i - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
				tommy_hashdyn_done(&table);
			}
		}
	}
}

void test_hashdyn_to_list(void)
{
	const unsigned counts[] = { 0, 1, 8, 128 };
	struct object_hash obj[130];
	tommy_node saved[130];

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		for (unsigned reserved = 0; reserved < 2; ++reserved) {
			for (unsigned prefix = 0; prefix <= 2; prefix += 2) {
				unsigned size = prefix + counts[c];
				tommy_hashdyn hashdyn;
				tommy_hashdyn_init(&hashdyn);
				/* include sparse tables with capacity reserved beyond their contents. */
				if (reserved)
					tommy_hashdyn_reserve(&hashdyn, 512);
				tommy_list list;
				tommy_list_init(&list);
				for (unsigned i = 0; i < size; ++i) {
					obj[i].value = i / 2;
					obj[i].node.index = tommy_inthash_u32(obj[i].value);
					if (i < prefix)
						tommy_list_insert_tail(&list, &obj[i].node, &obj[i]);
					else
						tommy_hashdyn_insert(&hashdyn, &obj[i].node, &obj[i], obj[i].node.index);
				}
				for (unsigned i = 0; i < size; ++i)
					saved[i] = obj[i].node;
				tommy_hashdyn_node** bucket = hashdyn.bucket;
				tommy_size_t bucket_max = hashdyn.bucket_max;
				tommy_size_t bucket_mask = hashdyn.bucket_mask;
				tommy_uint_t bucket_bit = hashdyn.bucket_bit;

				tommy_hashdyn_to_list(&hashdyn, &list);
				test_hash_list(&list, obj, saved, size, prefix);
				if (tommy_hashdyn_count(&hashdyn) != 0 || hashdyn.bucket != bucket
					|| hashdyn.bucket_max != bucket_max || hashdyn.bucket_mask != bucket_mask
					|| hashdyn.bucket_bit != bucket_bit
					|| tommy_hashdyn_memory_usage(&hashdyn) != bucket_max * sizeof(*bucket)) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				for (unsigned i = 0; i < bucket_max; ++i)
					if (tommy_hashdyn_bucket(&hashdyn, i) != 0) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				the_count = 0;
				tommy_hashdyn_foreach(&hashdyn, count_callback);
				if (the_count != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				for (unsigned i = prefix; i < size; ++i)
					if (tommy_hashdyn_search(&hashdyn, search_callback, &obj[i], obj[i].node.index) != 0) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}

				for (unsigned i = 0; i < size; ++i)
					saved[i] = obj[i].node;
				tommy_hashdyn_to_list(&hashdyn, &list);
				test_hash_list(&list, obj, saved, size, prefix);

				/* reuse the nodes and verify removals across shrink thresholds. */
				while (!tommy_list_empty(&list)) {
					struct object_hash* data = tommy_list_remove_head(&list);
					tommy_hashdyn_insert(&hashdyn, &data->node, data, data->node.index);
				}
				if (tommy_hashdyn_count(&hashdyn) != size) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				for (unsigned i = 0; i < size; ++i)
					if (tommy_hashdyn_search(&hashdyn, search_callback, &obj[i], obj[i].node.index) != &obj[i]
						|| tommy_hashdyn_remove_existing(&hashdyn, &obj[i].node) != &obj[i]
						|| tommy_hashdyn_count(&hashdyn) != size - i - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				tommy_hashdyn_done(&hashdyn);
			}
		}
	}
}

void test_hashdyn_clear(void)
{
	const unsigned counts[] = { 0, 1, 8, 32, 128 };
	struct object_hash obj[128];

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c];
		tommy_hashdyn hashdyn;
		tommy_hashdyn_init(&hashdyn);
		/* also exercise an empty table with reserved capacity. */
		if (n == 0)
			tommy_hashdyn_reserve(&hashdyn, 128);

		for (unsigned i = 0; i < n; ++i) {
			obj[i].value = i / 2;
			tommy_hashdyn_insert(&hashdyn, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
		}

		tommy_hashdyn_node** bucket = hashdyn.bucket;
		tommy_size_t bucket_max = hashdyn.bucket_max;
		tommy_size_t bucket_mask = hashdyn.bucket_mask;
		tommy_uint_t bucket_bit = hashdyn.bucket_bit;
		tommy_size_t memory_usage = tommy_hashdyn_memory_usage(&hashdyn) - n * sizeof(tommy_hashdyn_node);

		for (unsigned j = 0; j < 2; ++j) {
			tommy_hashdyn_clear(&hashdyn);
			if (tommy_hashdyn_count(&hashdyn) != 0
				|| hashdyn.bucket != bucket || hashdyn.bucket_max != bucket_max
				|| hashdyn.bucket_mask != bucket_mask || hashdyn.bucket_bit != bucket_bit
				|| tommy_hashdyn_memory_usage(&hashdyn) != memory_usage) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < bucket_max; ++i)
				if (tommy_hashdyn_bucket(&hashdyn, i) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			the_count = 0;
			tommy_hashdyn_foreach(&hashdyn, count_callback);
			tommy_hashdyn_foreach_arg(&hashdyn, count_arg_callback, &the_count);
			if (the_count != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < n; ++i) {
				tommy_hash_t hash = tommy_inthash_u32(obj[i].value);
				if (obj[i].value != (int)(i / 2)
					|| obj[i].node.data != &obj[i] || obj[i].node.index != hash
					|| tommy_hashdyn_search(&hashdyn, search_callback, &obj[i], hash) != 0
					|| tommy_hashdyn_remove(&hashdyn, search_callback, &obj[i], hash) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}
		}

		/* reinsert old nodes and exercise growth after clear. */
		for (unsigned i = 0; i < 128; ++i) {
			obj[i].value = i / 2;
			tommy_hashdyn_insert(&hashdyn, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
		}
		if (tommy_hashdyn_count(&hashdyn) != 128) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		for (unsigned i = 0; i < 128; ++i)
			if (tommy_hashdyn_search(&hashdyn, search_callback, &obj[i], tommy_inthash_u32(obj[i].value)) != &obj[i]) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		/* both removal paths must still work across shrink thresholds. */
		for (unsigned i = 128; i > 0; --i) {
			void* data;
			if (i % 2)
				data = tommy_hashdyn_remove_existing(&hashdyn, &obj[i - 1].node);
			else
				data = tommy_hashdyn_remove(&hashdyn, search_callback, &obj[i - 1], tommy_inthash_u32(obj[i - 1].value));
			if (data != &obj[i - 1] || tommy_hashdyn_count(&hashdyn) != i - 1) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		tommy_hashdyn_done(&hashdyn);
	}

	/* clear must not access objects already freed by foreach. */
	tommy_hashdyn hashdyn;
	tommy_hashdyn_init(&hashdyn);
	for (unsigned i = 0; i < 128; ++i) {
		struct object_hash* allocated = malloc(sizeof(struct object_hash));
		allocated->value = i;
		tommy_hashdyn_insert(&hashdyn, &allocated->node, allocated, tommy_inthash_u32(i));
	}
	tommy_hashdyn_foreach(&hashdyn, free);
	tommy_hashdyn_clear(&hashdyn);
	obj[0].value = 1;
	tommy_hashdyn_insert(&hashdyn, &obj[0].node, &obj[0], 1);
	if (tommy_hashdyn_count(&hashdyn) != 1
		|| tommy_hashdyn_remove_existing(&hashdyn, &obj[0].node) != &obj[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashdyn_done(&hashdyn);
}

void test_hashdyn(void)
{
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;

	test_hashdyn_clear();
	test_hashdyn_shrink();
	test_hashdyn_insert_unique();
	test_hashdyn_rehash_existing();
	test_hashdyn_to_list();

	struct object_hash* HASH = malloc(size * sizeof(struct object_hash));

	for (unsigned i = 0; i < size; ++i)
		HASH[i].value = i % module;

	START("hashdyn stack");
	unsigned limit = 5 * isqrt(size);
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashdyn hashdyn;
		tommy_hashdyn_init(&hashdyn);

		/* insert */
		for (unsigned i = 0; i < n; ++i)
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashdyn_memory_usage(&hashdyn) < n * sizeof(void*)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		if (tommy_hashdyn_count(&hashdyn) != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		the_count = 0;
		tommy_hashdyn_foreach(&hashdyn, count_callback);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* remove in backward order */
		for (unsigned i = 0; i < n / 2; ++i)
			tommy_hashdyn_remove_existing(&hashdyn, &HASH[n - i - 1].node);

		/* remove missing */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[n - i - 1], HASH[n - i - 1].value) != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		/* remove search */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[n / 2 - i - 1], HASH[n / 2 - i - 1].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();

	START("hashdyn queue");
	limit = isqrt(size) / 16;
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashdyn hashdyn;
		tommy_hashdyn_init(&hashdyn);

		/* insert first run */
		unsigned j = 0;
		unsigned i = 0;
		for (; i < n; ++i)
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashdyn_foreach_arg(&hashdyn, count_arg_callback, &the_count);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* insert all the others */
		for (; i < size; ++i, ++j) {
			/* insert one */
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashdyn_remove_existing(&hashdyn, &HASH[j].node);
		}

		for (; j < size; ++j)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[j], HASH[j].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();
}

void test_hashlin_insert_unique(void)
{
	struct object_hash obj[257];
	tommy_hashlin table;
	const unsigned order[] = { 0, 1, 2, 3, 4, 5 };

	tommy_hashlin_init(&table);
	obj[0].value = 1;
	compare_counter = 0;
	if (tommy_hashlin_insert_unique(&table, &obj[0].node, &obj[0], search_hash_value_callback, &obj[0].value, 0) != &obj[0]
		|| compare_counter != 0 || tommy_hashlin_count(&table) != 1
		|| tommy_hashlin_search(&table, search_hash_value_callback, &obj[0].value, 0) != &obj[0]) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashlin_remove_existing(&table, &obj[0].node);

	tommy_size_t bucket_max = table.bucket_max;
	obj[0].value = 10;
	obj[1].value = 20;
	obj[2].value = 30;
	obj[3].value = 20;
	for (unsigned i = 0; i < 4; ++i)
		tommy_hashlin_insert(&table, &obj[i].node, &obj[i], i == 0 ? bucket_max : 0);

	/* the first matching duplicate is returned; the rejected candidate stays intact. */
	obj[4].value = 20;
	obj[4].node.next = &obj[4].node;
	obj[4].node.prev = &obj[4].node;
	obj[4].node.data = &obj[4];
	obj[4].node.index = ~(tommy_hash_t)0;
	tommy_node saved = obj[4].node;
	compare_counter = 0;
	if (tommy_hashlin_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[1]
		|| compare_counter != 1 || tommy_hashlin_count(&table) != 4
		|| obj[4].value != 20 || obj[4].node.next != saved.next || obj[4].node.prev != saved.prev
		|| obj[4].node.data != saved.data || obj[4].node.index != saved.index) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashlin_bucket(&table, 0), obj, order, 4);

	/* equal hashes with different keys must allow insertion at the tail. */
	obj[4].value = 40;
	compare_counter = 0;
	if (tommy_hashlin_insert_unique(&table, &obj[4].node, &obj[4], search_hash_value_callback, &obj[4].value, 0) != &obj[4]
		|| compare_counter != 3 || tommy_hashlin_count(&table) != 5
		|| obj[4].node.data != &obj[4] || obj[4].node.index != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* different full hashes in the same bucket must never invoke the comparator. */
	obj[5].value = 50;
	compare_counter = 0;
	if (tommy_hashlin_insert_unique(&table, &obj[5].node, &obj[5], search_hash_value_callback, &obj[5].value, 2 * bucket_max) != &obj[5]
		|| compare_counter != 0 || tommy_hashlin_count(&table) != 6) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	test_hash_bucket(tommy_hashlin_bucket(&table, 0), obj, order, 6);
	tommy_hashlin_remove_existing(&table, &obj[1].node);
	obj[6].value = 20;
	if (tommy_hashlin_insert_unique(&table, &obj[6].node, &obj[6], search_hash_value_callback, &obj[6].value, 0) != &obj[3]
		|| tommy_hashlin_count(&table) != 5) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}
	tommy_hashlin_done(&table);

	/* insert unique keys, reject duplicates, then repeat during contraction. */
	tommy_hashlin_init(&table);
	struct object_hash candidate;
	candidate.node.next = &candidate.node;
	candidate.node.prev = &candidate.node;
	candidate.node.data = &candidate;
	candidate.node.index = ~(tommy_hash_t)0;
	for (unsigned phase = 0; phase < 2; ++phase) {
		for (unsigned step = 0; step < 256; ++step) {
			unsigned i = phase == 0 ? step : 255 - step;
			tommy_hash_t hash = tommy_inthash_u32(i);
			if (phase == 0) {
				obj[i].value = i;
				if (tommy_hashlin_insert_unique(&table, &obj[i].node, &obj[i], search_hash_value_callback, &obj[i].value, hash) != &obj[i]
					|| tommy_hashlin_count(&table) != i + 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}

			candidate.value = i;
			saved = obj[i].node;
			tommy_size_t count = tommy_hashlin_count(&table);
			bucket_max = table.bucket_max;
			tommy_size_t bucket_mask = table.bucket_mask;
			tommy_hashlin_node** bucket = table.bucket[0];
			tommy_uint_t bucket_bit = table.bucket_bit;
			tommy_uint_t state = table.state;
			tommy_size_t low_max = table.low_max;
			tommy_size_t low_mask = table.low_mask;
			tommy_size_t split = table.split;
			compare_counter = 0;
			if (tommy_hashlin_insert_unique(&table, &candidate.node, &candidate, search_hash_value_callback, &candidate.value, hash) != &obj[i]
				|| compare_counter != 1 || tommy_hashlin_count(&table) != count
				|| table.bucket_max != bucket_max || table.bucket_mask != bucket_mask
				|| table.bucket[0] != bucket || table.bucket_bit != bucket_bit || table.state != state
				|| table.low_max != low_max || table.low_mask != low_mask || table.split != split
				|| candidate.value != (int)i || candidate.node.next != &candidate.node
				|| candidate.node.prev != &candidate.node || candidate.node.data != &candidate
				|| candidate.node.index != ~(tommy_hash_t)0 || obj[i].node.next != saved.next
				|| obj[i].node.prev != saved.prev || obj[i].node.data != saved.data || obj[i].node.index != saved.index) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			if (phase != 0) {
				if (tommy_hashlin_remove_existing(&table, &obj[i].node) != &obj[i]
					|| tommy_hashlin_count(&table) != count - 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also insert a new key while hashlin is shrinking progressively. */
				if (i == 24) {
					obj[256].value = 256;
					if (tommy_hashlin_insert_unique(&table, &obj[256].node, &obj[256], search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashlin_count(&table) != count
						|| tommy_hashlin_search(&table, search_hash_value_callback, &obj[256].value, tommy_inthash_u32(256)) != &obj[256]
						|| tommy_hashlin_remove_existing(&table, &obj[256].node) != &obj[256]
						|| tommy_hashlin_count(&table) != count - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
			}
		}
	}
	tommy_hashlin_done(&table);
}

void test_hashlin_rehash_existing(void)
{
	const unsigned counts[][2] = { { 1, 1 }, { 4, 4 }, { 33, 33 }, { 48, 48 }, { 63, 63 }, { 64, 64 },
				       { 65, 65 }, { 128, 128 }, { 128, 31 }, { 128, 24 }, { 128, 17 },
				       { 128, 16 }, { 128, 15 }, { 128, 9 }, { 128, 8 } };
	struct object_hash obj[130];
	tommy_hash_t hashes[130];
	tommy_hashlin table;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c][1];
		unsigned source_size = n < 3 ? n : 3;
		for (unsigned target = 0; target < source_size; ++target) {
			for (unsigned mode = 0; mode < 4; ++mode) {
				unsigned source[3], destination[2];
				unsigned source_count = 0, destination_count = 0;

				tommy_hashlin_init(&table);
				for (unsigned i = 0; i < counts[c][0]; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashlin_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = counts[c][0]; i > n; --i)
					tommy_hashlin_remove_existing(&table, &obj[i - 1].node);

				tommy_size_t bucket_max = table.bucket_max;
				tommy_size_t bucket_mask = table.bucket_mask;
				tommy_uint_t bucket_bit = table.bucket_bit;
				tommy_uint_t state = table.state;
				tommy_size_t low_max = table.low_max;
				tommy_size_t low_mask = table.low_mask;
				tommy_size_t split = table.split;
				tommy_hashlin_node** segments[TOMMY_SIZE_BIT];
				for (unsigned i = 0; i < bucket_bit; ++i)
					segments[i] = table.bucket[i];
				/* exercise unchanged hash, same bucket, occupied bucket and empty bucket. */
				tommy_hash_t hash = mode == 0 ? 0 : mode == 1 ? bucket_max : mode == 2 ? 1 : ~(tommy_hash_t)0;
				tommy_node saved = obj[target].node;
				obj[target].value = 1000 + target;
				tommy_hashlin_rehash_existing(&table, &obj[target].node, hash);
				hashes[target] = hash;

				if (tommy_hashlin_count(&table) != n || table.bucket_max != bucket_max
					|| table.bucket_mask != bucket_mask
					|| table.bucket_bit != bucket_bit || table.state != state
					|| table.low_max != low_max || table.low_mask != low_mask || table.split != split
					|| obj[target].node.index != hash || obj[target].node.data != saved.data
					|| obj[target].value != (int)(1000 + target)
					|| (mode == 0 && (obj[target].node.next != saved.next || obj[target].node.prev != saved.prev))) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				for (unsigned i = 0; i < bucket_bit; ++i)
					if (table.bucket[i] != segments[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}

				for (unsigned i = 0; i < source_size; ++i)
					if (mode == 0 || i != target)
						source[source_count++] = i;
				if (mode == 1)
					source[source_count++] = target;
				test_hash_bucket(tommy_hashlin_bucket(&table, 0), obj, source, source_count);

				if (n > 3)
					destination[destination_count++] = 3;
				if (mode == 2)
					destination[destination_count++] = target;
				test_hash_bucket(tommy_hashlin_bucket(&table, 1), obj, destination, destination_count);
				if (mode == 3)
					test_hash_bucket(tommy_hashlin_bucket(&table, hash), obj, &target, 1);
				if (mode != 0 && tommy_hashlin_search(&table, search_callback, &obj[target], 0) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
				/* also unlink from the new hash, including an unsplit high position. */
				if (mode != 0) {
					tommy_hashlin_rehash_existing(&table, &obj[target].node, 0);
					if (tommy_hashlin_count(&table) != n || obj[target].node.index != 0
						|| tommy_hashlin_search(&table, search_callback, &obj[target], 0) != &obj[target]
						|| tommy_hashlin_search(&table, search_callback, &obj[target], hash) != 0) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
					tommy_hashlin_rehash_existing(&table, &obj[target].node, hash);
				}
				for (unsigned i = 0; i < n; ++i)
					if (tommy_hashlin_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}

				/* subsequent resizing and removals must use the updated stored hash. */
				for (unsigned i = n; i < 130; ++i) {
					obj[i].value = i;
					hashes[i] = i < 3 ? 0 : i == 3 ? 1 : 2;
					tommy_hashlin_insert(&table, &obj[i].node, &obj[i], hashes[i]);
				}
				for (unsigned i = 0; i < 130; ++i)
					if (tommy_hashlin_search(&table, search_callback, &obj[i], hashes[i]) != &obj[i]) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				for (unsigned i = 130; i > 0; --i) {
					void* data;
					if (i % 2)
						data = tommy_hashlin_remove_existing(&table, &obj[i - 1].node);
					else
						data = tommy_hashlin_remove(&table, search_callback, &obj[i - 1], hashes[i - 1]);
					if (data != &obj[i - 1] || tommy_hashlin_count(&table) != i - 1) {
						/* LCOV_EXCL_START */
						abort();
						/* LCOV_EXCL_STOP */
					}
				}
				tommy_hashlin_done(&table);
			}
		}
	}
}

void test_hashlin_to_list(void)
{
	const unsigned counts[][2] = {
		{ 0, 0 }, { 1, 1 }, { 33, 33 }, { 48, 48 }, { 63, 63 }, { 64, 64 },
		{ 65, 65 }, { 128, 128 }, { 128, 31 }, { 128, 24 }, { 128, 17 },
		{ 128, 16 }, { 128, 15 }, { 128, 9 }, { 128, 8 }
	};
	struct object_hash obj[130];
	tommy_node saved[130];

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		for (unsigned prefix = 0; prefix <= 2; prefix += 2) {
			unsigned size = prefix + counts[c][1];
			tommy_hashlin hashlin;
			tommy_hashlin_init(&hashlin);
			tommy_uint_t stable_state = hashlin.state;
			tommy_list list;
			tommy_list_init(&list);
			for (unsigned i = 0; i < prefix + counts[c][0]; ++i) {
				obj[i].value = i / 2;
				obj[i].node.index = tommy_inthash_u32(obj[i].value);
				if (i < prefix)
					tommy_list_insert_tail(&list, &obj[i].node, &obj[i]);
				else
					tommy_hashlin_insert(&hashlin, &obj[i].node, &obj[i], obj[i].node.index);
			}
			for (unsigned i = prefix + counts[c][0]; i > size; --i)
				tommy_hashlin_remove_existing(&hashlin, &obj[i - 1].node);
			for (unsigned i = 0; i < size; ++i)
				saved[i] = obj[i].node;
			tommy_size_t bucket_max = hashlin.bucket_max;
			tommy_size_t bucket_mask = hashlin.bucket_mask;
			tommy_uint_t bucket_bit = hashlin.bucket_bit;
			tommy_hashlin_node** segments[TOMMY_SIZE_BIT];
			for (unsigned i = 0; i < bucket_bit; ++i)
				segments[i] = hashlin.bucket[i];

			/* inactive slots must never be transferred, including stale shrink pointers. */
			for (unsigned i = hashlin.low_max + hashlin.split; i < bucket_max; ++i)
				*tommy_hashlin_pos(&hashlin, i) = &obj[prefix].node;

			tommy_hashlin_to_list(&hashlin, &list);
			test_hash_list(&list, obj, saved, size, prefix);
			if (tommy_hashlin_count(&hashlin) != 0 || hashlin.bucket_max != bucket_max
				|| hashlin.bucket_mask != bucket_mask || hashlin.bucket_bit != bucket_bit
				|| hashlin.state != stable_state || hashlin.low_max != bucket_max
				|| hashlin.low_mask != bucket_mask || hashlin.split != 0
				|| tommy_hashlin_memory_usage(&hashlin) != bucket_max * sizeof(tommy_hashlin_node*)) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < bucket_bit; ++i)
				if (hashlin.bucket[i] != segments[i]) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			for (unsigned i = 0; i < bucket_max; ++i)
				if (tommy_hashlin_bucket(&hashlin, i) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			the_count = 0;
			tommy_hashlin_foreach(&hashlin, count_callback);
			if (the_count != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = prefix; i < size; ++i)
				if (tommy_hashlin_search(&hashlin, search_callback, &obj[i], obj[i].node.index) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			for (unsigned i = 0; i < size; ++i)
				saved[i] = obj[i].node;
			tommy_hashlin_to_list(&hashlin, &list);
			test_hash_list(&list, obj, saved, size, prefix);

			/* reinsert all transferred nodes, then grow beyond the retained capacity. */
			while (!tommy_list_empty(&list)) {
				struct object_hash* data = tommy_list_remove_head(&list);
				tommy_hashlin_insert(&hashlin, &data->node, data, data->node.index);
			}
			for (unsigned i = size; i < 130; ++i) {
				obj[i].value = i / 2;
				tommy_hashlin_insert(&hashlin, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
			}
			if (tommy_hashlin_count(&hashlin) != 130 || hashlin.bucket_max <= bucket_max) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			for (unsigned i = 0; i < 130; ++i)
				if (tommy_hashlin_search(&hashlin, search_callback, &obj[i], obj[i].node.index) != &obj[i]
					|| tommy_hashlin_remove_existing(&hashlin, &obj[i].node) != &obj[i]
					|| tommy_hashlin_count(&hashlin) != 130 - i - 1) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			if (hashlin.bucket_max != (tommy_size_t)1 << TOMMY_HASHLIN_BIT) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
			tommy_hashlin_done(&hashlin);
		}
	}
}

void test_hashlin_clear(void)
{
	const unsigned counts[][2] = {
		{ 0, 0 }, { 1, 1 }, { 32, 32 },
		{ 33, 33 }, { 48, 48 }, { 63, 63 }, { 64, 64 },
		{ 65, 65 }, { 96, 96 }, { 127, 127 }, { 128, 128 },
		{ 128, 31 }, { 128, 24 }, { 128, 17 }, { 128, 16 },
		{ 128, 15 }, { 128, 12 }, { 128, 9 }, { 128, 8 }
	};
	struct object_hash obj[256];
	unsigned stable = 0, grow = 0, shrink = 0;

	for (unsigned c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
		unsigned n = counts[c][0];
		unsigned retained = counts[c][1];
		tommy_hashlin hashlin;
		tommy_hashlin_init(&hashlin);
		tommy_uint_t stable_state = hashlin.state;

		for (unsigned i = 0; i < n; ++i) {
			obj[i].value = i / 2;
			tommy_hashlin_insert(&hashlin, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
		}
		/* stop at different points of a progressive shrink. */
		for (unsigned i = n; i > retained; --i)
			tommy_hashlin_remove_existing(&hashlin, &obj[i - 1].node);

		if (hashlin.state == stable_state)
			++stable;
		else if (n == retained)
			++grow;
		else
			++shrink;

		tommy_size_t bucket_max = hashlin.bucket_max;
		tommy_size_t bucket_mask = hashlin.bucket_mask;
		tommy_uint_t bucket_bit = hashlin.bucket_bit;
		tommy_hashlin_node** bucket[TOMMY_SIZE_BIT];
		for (unsigned i = 0; i < bucket_bit; ++i)
			bucket[i] = hashlin.bucket[i];
		tommy_size_t memory_usage = tommy_hashlin_memory_usage(&hashlin) - retained * sizeof(tommy_hashlin_node);

		/* inactive slots may contain arbitrary values or stale node pointers. */
		for (unsigned i = hashlin.low_max + hashlin.split; i < bucket_max; ++i)
			*tommy_hashlin_pos(&hashlin, i) = &obj[0].node;

		for (unsigned j = 0; j < 2; ++j) {
			tommy_hashlin_clear(&hashlin);
			if (tommy_hashlin_count(&hashlin) != 0
				|| hashlin.bucket_max != bucket_max || hashlin.bucket_mask != bucket_mask
				|| hashlin.bucket_bit != bucket_bit || hashlin.state != stable_state
				|| hashlin.low_max != bucket_max || hashlin.low_mask != bucket_mask
				|| hashlin.split != 0 || tommy_hashlin_memory_usage(&hashlin) != memory_usage) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < bucket_bit; ++i)
				if (hashlin.bucket[i] != bucket[i]) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			for (unsigned i = 0; i < bucket_max; ++i)
				if (tommy_hashlin_bucket(&hashlin, i) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}

			the_count = 0;
			tommy_hashlin_foreach(&hashlin, count_callback);
			tommy_hashlin_foreach_arg(&hashlin, count_arg_callback, &the_count);
			if (the_count != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

			for (unsigned i = 0; i < retained; ++i) {
				tommy_hash_t hash = tommy_inthash_u32(obj[i].value);
				if (obj[i].value != (int)(i / 2)
					|| obj[i].node.data != &obj[i] || obj[i].node.index != hash
					|| tommy_hashlin_search(&hashlin, search_callback, &obj[i], hash) != 0
					|| tommy_hashlin_remove(&hashlin, search_callback, &obj[i], hash) != 0) {
					/* LCOV_EXCL_START */
					abort();
					/* LCOV_EXCL_STOP */
				}
			}
		}

		/* reuse old nodes and grow beyond the preserved capacity. */
		for (unsigned i = 0; i < 256; ++i) {
			obj[i].value = i / 2;
			tommy_hashlin_insert(&hashlin, &obj[i].node, &obj[i], tommy_inthash_u32(obj[i].value));
			if (tommy_hashlin_count(&hashlin) != i + 1) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		if (hashlin.bucket_max <= bucket_max) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		for (unsigned i = 0; i < 256; ++i)
			if (tommy_hashlin_search(&hashlin, search_callback, &obj[i], tommy_inthash_u32(obj[i].value)) != &obj[i]) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		/* verify both removal paths while the table shrinks again. */
		for (unsigned i = 256; i > 0; --i) {
			void* data;
			if (i % 2)
				data = tommy_hashlin_remove_existing(&hashlin, &obj[i - 1].node);
			else
				data = tommy_hashlin_remove(&hashlin, search_callback, &obj[i - 1], tommy_inthash_u32(obj[i - 1].value));
			if (data != &obj[i - 1] || tommy_hashlin_count(&hashlin) != i - 1) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}
		}
		if (hashlin.bucket_max != (tommy_size_t)1 << TOMMY_HASHLIN_BIT) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_hashlin_done(&hashlin);
	}

	if (stable == 0 || grow == 0 || shrink == 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* free objects during a partial grow or shrink, then clear and reuse. */
	for (unsigned c = 0; c < 2; ++c) {
		struct object_hash* allocated[128];
		unsigned n = c == 0 ? 33 : 128;
		unsigned retained = c == 0 ? 33 : 24;
		tommy_hashlin hashlin;
		tommy_hashlin_init(&hashlin);
		for (unsigned i = 0; i < n; ++i) {
			allocated[i] = malloc(sizeof(struct object_hash));
			allocated[i]->value = i;
			tommy_hashlin_insert(&hashlin, &allocated[i]->node, allocated[i], tommy_inthash_u32(i));
		}
		for (unsigned i = n; i > retained; --i)
			free(tommy_hashlin_remove_existing(&hashlin, &allocated[i - 1]->node));
		tommy_hashlin_foreach(&hashlin, free);
		tommy_hashlin_clear(&hashlin);
		obj[0].value = 1;
		tommy_hashlin_insert(&hashlin, &obj[0].node, &obj[0], 1);
		if (tommy_hashlin_count(&hashlin) != 1
			|| tommy_hashlin_remove_existing(&hashlin, &obj[0].node) != &obj[0]) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}
		tommy_hashlin_done(&hashlin);
	}
}

void test_hashlin(void)
{
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;

	test_hashlin_clear();
	test_hashlin_insert_unique();
	test_hashlin_rehash_existing();
	test_hashlin_to_list();

	struct object_hash* HASH = malloc(size * sizeof(struct object_hash));

	for (unsigned i = 0; i < size; ++i)
		HASH[i].value = i % module;

	tommy_hashlin hashlin;
	tommy_hashlin_init(&hashlin);

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

	/* get the bucket of the last element */
	tommy_hashlin_node* bucket = tommy_hashlin_bucket(&hashlin, module - 1);
	if (bucket == 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* deinitialize without removing elements to force deallocation */
	tommy_hashlin_done(&hashlin);

	START("hashlin stack");
	unsigned limit = 5 * isqrt(size);
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashlin_init(&hashlin);

		/* insert */
		for (unsigned i = 0; i < n; ++i)
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashlin_memory_usage(&hashlin) < n * sizeof(void*)) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		if (tommy_hashlin_count(&hashlin) != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		the_count = 0;
		tommy_hashlin_foreach(&hashlin, count_callback);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* remove in backward order */
		for (unsigned i = 0; i < n / 2; ++i)
			tommy_hashlin_remove_existing(&hashlin, &HASH[n - i - 1].node);

		/* remove missing */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[n - i - 1], HASH[n - i - 1].value) != 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		/* remove search */
		for (unsigned i = 0; i < n / 2; ++i)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[n / 2 - i - 1], HASH[n / 2 - i - 1].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashlin_done(&hashlin);
	}
	STOP();

	START("hashlin queue");
	limit = isqrt(size) / 16;
	for (unsigned n = 0; n <= limit; ++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashlin_init(&hashlin);

		/* insert first run */
		unsigned j = 0;
		unsigned i = 0;
		for (; i < n; ++i)
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashlin_foreach_arg(&hashlin, count_arg_callback, &the_count);
		if (the_count != n) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

		/* insert all the others */
		for (; i < size; ++i, ++j) {
			/* insert one */
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashlin_remove_existing(&hashlin, &HASH[j].node);
		}

		for (; j < size; ++j)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[j], HASH[j].value) == 0) {
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			}

		tommy_hashlin_done(&hashlin);
	}
	STOP();
}

void test_trie(void)
{
	const unsigned size = TOMMY_SIZE * 4;

	struct object_trie* OBJ = malloc(size * sizeof(struct object_trie));

	for (unsigned i = 0; i < size; ++i)
		OBJ[i].value = i;

	START("trie");
	tommy_allocator alloc;
	tommy_allocator_init(&alloc, TOMMY_TRIE_BLOCK_SIZE, TOMMY_TRIE_BLOCK_SIZE);
	tommy_trie trie;
	tommy_trie_init(&trie, &alloc);

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_trie_insert(&trie, &OBJ[i].node, &OBJ[i], OBJ[i].value);

	if (tommy_trie_memory_usage(&trie) < size * sizeof(tommy_trie_node)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_allocator_memory_usage(&alloc) < trie.node_count * TOMMY_TRIE_BLOCK_SIZE) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_trie_count(&trie) != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* insert duplicate */
	struct object_trie DUP[2];
	for (unsigned i = 0; i < 2; ++i) {
		DUP[i].value = 0;
		tommy_trie_insert(&trie, &DUP[i].node, &DUP[i], DUP[i].value);
	}

	/* search present */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_search(&trie, OBJ[i].value) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* remove first duplicate */
	tommy_trie_remove_existing(&trie, &DUP[0].node);

	/* remove existing */
	for (unsigned i = 0; i < size / 2; ++i)
		tommy_trie_remove_existing(&trie, &OBJ[i].node);

	/* remove missing using the same bucket of the duplicate */
	if (tommy_trie_remove(&trie, 1) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* search missing using the same bucket of the duplicate */
	if (tommy_trie_search(&trie, 1) != 0) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* remove second duplicate */
	tommy_trie_remove_existing(&trie, &DUP[1].node);

	/* remove missing */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_remove(&trie, OBJ[i].value) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* search missing */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_search(&trie, OBJ[i].value) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* remove present */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_remove(&trie, OBJ[size / 2 + i].value) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	tommy_allocator_done(&alloc);
	STOP();
}

void test_trie_inplace(void)
{
	const unsigned size = TOMMY_SIZE * 4;

	struct object_trie_inplace* OBJ = malloc(size * sizeof(struct object_trie_inplace));

	for (unsigned i = 0; i < size; ++i)
		OBJ[i].value = i;

	START("trie_inplace");
	tommy_trie_inplace trie_inplace;
	tommy_trie_inplace_init(&trie_inplace);

	/* insert */
	for (unsigned i = 0; i < size; ++i)
		tommy_trie_inplace_insert(&trie_inplace, &OBJ[i].node, &OBJ[i], OBJ[i].value);

	if (tommy_trie_inplace_memory_usage(&trie_inplace) < size * sizeof(tommy_trie_inplace_node)) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	if (tommy_trie_inplace_count(&trie_inplace) != size) {
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	}

	/* insert duplicates */
	struct object_trie_inplace DUP[2];
	for (unsigned i = 0; i < 2; ++i) {
		DUP[i].value = 0;
		tommy_trie_inplace_insert(&trie_inplace, &DUP[i].node, &DUP[i], DUP[i].value);
	}

	/* search present */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_inplace_search(&trie_inplace, OBJ[i].value) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* remove first duplicate */
	tommy_trie_inplace_remove_existing(&trie_inplace, &DUP[0].node);

	/* remove existing */
	for (unsigned i = 0; i < size / 2; ++i)
		tommy_trie_inplace_remove_existing(&trie_inplace, &OBJ[i].node);

	/* remove second duplicate */
	tommy_trie_inplace_remove_existing(&trie_inplace, &DUP[1].node);

	/* remove missing */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_inplace_remove(&trie_inplace, OBJ[i].value) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* search missing */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_inplace_search(&trie_inplace, OBJ[i].value) != 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	/* remove present */
	for (unsigned i = 0; i < size / 2; ++i)
		if (tommy_trie_inplace_remove(&trie_inplace, OBJ[size / 2 + i].value) == 0) {
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		}

	STOP();
}

int main()
{
	nano_init();

	printf("Tommy check program.\n");

	test_hash();
	test_alloc();
	test_list();
	test_tree();
	test_tree_swap();
	test_tree_duplicates();
	test_tree_foreach();
	test_tree_clear();
	test_tree_to_list();
	test_array();
	test_array_ops();
	test_arrayof();
	test_arrayof_ops();
	test_arrayblk();
	test_arrayblk_ops();
	test_arrayblkof();
	test_arrayblkof_ops();
	test_array_tail_ops();
	test_array_capacity_swap();
	test_hashtable();
	test_hashdyn();
	test_hashlin();
	test_trie();
	test_trie_inplace();

	printf("OK\n");

	return EXIT_SUCCESS;
}

