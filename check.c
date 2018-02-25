/*
 * Copyright (c) 2010, Andrea Mazzoleni. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
	tommy_uint64_t ret;
#if defined(_WIN32)   
	LARGE_INTEGER t;

	if (!QueryPerformanceCounter(&t))
		return 0;

	ret = (t.QuadPart / win_frequency.QuadPart) * 1000000000;

	ret += (t.QuadPart % win_frequency.QuadPart) * 1000000000 / win_frequency.QuadPart;
#elif defined(__MACH__)
	mach_timebase_info_data_t info;
	kern_return_t r;
	tommy_uint64_t t;

	t = mach_absolute_time();

	r = mach_timebase_info(&info);
	if (r != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	ret = (t / info.denom) * info.numer;
	
	ret += (t % info.denom) * info.numer / info.denom;
#elif defined(__linux)
	struct timespec ts;
	int r;

	r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	ret = ts.tv_sec * (tommy_uint64_t)1000000000 + ts.tv_nsec;
#else
	struct timeval tv;
	int r;

	r = gettimeofday(&tv, 0);
	if (r != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	ret = tv.tv_sec * (tommy_uint64_t)1000000000 + tv.tv_usec * 1000;
#endif
	return ret;
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
	unsigned r;
	tommy_uint64_t divider;
    
loop:    
	/* linear congruential generator from MMIX by Donald Knuth, http://en.wikipedia.org/wiki/Linear_congruential_generator */
#ifdef _MSC_VER
	divider = 0xFFFFFFFFFFFFFFFF / max;
	SEED = SEED * 6364136223846793005 + 1442695040888963407;
#else
	divider = 0xFFFFFFFFFFFFFFFFULL / max;
	SEED = SEED * 6364136223846793005LL + 1442695040888963407LL;
#endif
 
	r = (unsigned)(SEED / divider);

	/* it may happen as the divider is approximated down */
	if (r >= max)
		/* LCOV_EXCL_START */
		goto loop;
		/* LCOV_EXCL_STOP */

	return r;
}

/******************************************************************************/
/* helper */

unsigned isqrt(unsigned n)
{
	unsigned root, remain, place;

	root = 0;

	remain = n;

	place = 0x40000000;

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
static unsigned char the_cache[16*1024*1024];
static const char* the_str;
static tommy_uint64_t the_start;

void cache_clear(void)
{
	unsigned i;

	/* read & write */
	for(i=0;i<sizeof(the_cache);i += 32)
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

static int search_callback(const void* arg, const void* obj)
{
	return arg != obj;
}

struct hash32_test {
	char* data;
	tommy_uint32_t len;
	tommy_uint32_t hash;
} HASH32[] = {
	{ "", 0, 0x8614384c },
	{ "a", 1, 0x12c16c36 },
	{ "abc", 3, 0xc58e8af5 },
	{ "message digest", 14, 0x006b32f1 },
	{ "abcdefghijklmnopqrstuvwxyz", 26, 0x7e6fcfe0 },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62, 0x8604adf8 },
	{ "The quick brown fox jumps over the lazy dog", 43, 0xdeba3d3a },
	{ "\x00", 1, 0x4a7d1c33 },
	{ "\x16\x27", 2, 0x8b50899b },
	{ "\xe2\x56\xb4", 3, 0x60406493 },
	{ "\xc9\x4d\x9c\xda", 4, 0xa049144a },
	{ "\x79\xf1\x29\x69\x5d", 5, 0x4da2c2f1 },
	{ "\x00\x7e\xdf\x1e\x31\x1c", 6, 0x59de30cf },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 7, 0x219e149c },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 8, 0x25067520 },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 9, 0xa1f368d8 },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 10, 0x805fc63d },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\x00\xb7\xae", 11, 0x7f75dd0f },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 12, 0xb9154382 },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 13, 0x2bdd05d7 },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 14, 0xabffeb9f },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 15, 0x886da0b4 },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 16, 0x34ed2af3 },
	{ 0, 0, 0 }
};

struct strhash32_test {
	char* data;
	tommy_uint32_t hash;
} STRHASH32[] = {
	{ "", 0x0af1416d },
	{ "a", 0x68fa0f3f },
	{ "abc", 0xfc68ffc5 },
	{ "message digest", 0x08477b63 },
	{ "abcdefghijklmnopqrstuvwxyz", 0x5b9c25e5 },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 0x1e530ce7 },
	{ "The quick brown fox jumps over the lazy dog", 0xaf93eefe },
	{ "\xff", 0xfc88801b },
	{ "\x16\x27", 0xcd7216db },
	{ "\xe2\x56\xb4", 0x05f98d02 },
	{ "\xc9\x4d\x9c\xda", 0xf65206f8 },
	{ "\x79\xf1\x29\x69\x5d", 0x72bd6bda },
	{ "\xff\x7e\xdf\x1e\x31\x1c", 0x57dfb9b4 },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 0x499ff634 },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 0xe896b7ce },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 0xfe3939f0 },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 0x4351d482 },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\xff\xb7\xae", 0x88e92135 },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 0x01109c16 },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 0xbcb050dc },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 0xbe5e1fd5 },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 0x70d8c97f },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 0x957440a9 },
	{ 0, 0 }
};

struct hash64_test {
	char* data;
	tommy_uint32_t len;
	tommy_uint64_t hash;
} HASH64[] = {
	{ "", 0, 0x8614384cb5165fbfULL },
	{ "a", 1, 0x1a2e0298a8e94a3dULL },
	{ "abc", 3, 0x7555796b7a7d21ebULL },
	{ "message digest", 14, 0x9411a57d04b92fb4ULL },
	{ "abcdefghijklmnopqrstuvwxyz", 26, 0x3ca3f8d2b4e69832ULL },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 62, 0x6dae542ba0015a4dULL },
	{ "The quick brown fox jumps over the lazy dog", 43, 0xe06d8cbb3d2ea1a6ULL },
	{ "\x00", 1, 0x201e664fb5f2c021ULL },
	{ "\x16\x27", 2, 0xef42fa8032c4b775ULL },
	{ "\xe2\x56\xb4", 3, 0x6e6c498a6688466cULL },
	{ "\xc9\x4d\x9c\xda", 4, 0x5195005419905423ULL },
	{ "\x79\xf1\x29\x69\x5d", 5, 0x221235b48afee7c1ULL },
	{ "\x00\x7e\xdf\x1e\x31\x1c", 6, 0x1b1f18b9266f095bULL },
	{ "\x2a\x4c\xe1\xff\x9e\x6f\x53", 7, 0x2cbafa8e741d49caULL },
	{ "\xba\x02\xab\x18\x30\xc5\x0e\x8a", 8, 0x4677f04c06e0758dULL },
	{ "\xec\x4e\x7a\x72\x1e\x71\x2a\xc9\x33", 9, 0x5afe09e8214e2163ULL },
	{ "\xfd\xe2\x9c\x0f\x72\xb7\x08\xea\xd0\x78", 10, 0x115b6276d209fab6ULL },
	{ "\x65\xc4\x8a\xb8\x80\x86\x9a\x79\x00\xb7\xae", 11, 0xd0636d2f01cf3a3eULL },
	{ "\x77\xe9\xd7\x80\x0e\x3f\x5c\x43\xc8\xc2\x46\x39", 12, 0x6d259f5fef74f93eULL },
	{ "\x87\xd8\x61\x61\x4c\x89\x17\x4e\xa1\xa4\xef\x13\xa9", 13, 0x23449c3baf93ac39ULL },
	{ "\xfe\xa6\x5b\xc2\xda\xe8\x95\xd4\x64\xab\x4c\x39\x58\x29", 14, 0x9b85ba28d7854d69ULL },
	{ "\x94\x49\xc0\x78\xa0\x80\xda\xc7\x71\x4e\x17\x37\xa9\x7c\x40", 15, 0x3617c833193a359fULL },
	{ "\x53\x7e\x36\xb4\x2e\xc9\xb9\xcc\x18\x3e\x9a\x5f\xfc\xb7\xb0\x61", 16, 0x5dbf9ff58e274dd9ULL },
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

void test_hash(void)
{
	unsigned i;
	unsigned char buffer[16];
	unsigned COUNT = 1024*1024*16;
	tommy_uint32_t hash32;
	tommy_uint64_t hash64;

	START("hash_test_vectors");

	for(i=0;HASH32[i].data;++i) {
		if (tommy_hash_u32(0xa766795d, HASH32[i].data, HASH32[i].len) != HASH32[i].hash)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	for(i=0;STRHASH32[i].data;++i) {
		if (tommy_strhash_u32(0xa766795d, STRHASH32[i].data) != STRHASH32[i].hash)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	for(i=0;HASH64[i].data;++i) {
		if (tommy_hash_u64(0x2f022773a766795dULL, HASH64[i].data, HASH64[i].len) != HASH64[i].hash)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	for(i=0;INTHASH32[i].value || !i;++i) {
		if (tommy_inthash_u32(INTHASH32[i].value) != INTHASH32[i].hash)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	for(i=0;INTHASH64[i].value || !i;++i) {
		if (tommy_inthash_u64(INTHASH64[i].value) != INTHASH64[i].hash)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	STOP();

	memset(buffer, 0xAA, sizeof(buffer));
	buffer[sizeof(buffer) - 1] = 0;

	hash32 = 0;
	hash64 = 0;

	START("hash_u32");

	for(i=0;i<COUNT;++i) {
		hash32 = tommy_hash_u32(hash32, buffer, sizeof(buffer));
	}

	STOP();

	START("strhash_u32");

	for(i=0;i<COUNT;++i) {
		hash32 = tommy_strhash_u32(hash32, buffer);
	}

	STOP();

	START("hash_u64");

	for(i=0;i<COUNT;++i) {
		hash64 = tommy_hash_u64(hash64, buffer, sizeof(buffer));
	}

	STOP();
}

void test_alloc(void)
{
	const unsigned size = 10 * TOMMY_SIZE;
	unsigned i;
	tommy_allocator alloc;
	void** PTR;

	PTR = malloc(size * sizeof(void*));

	/* ensure at least pointer alignment */
	tommy_allocator_init(&alloc, sizeof(void*), 1);
	if (alloc.align_size < sizeof(void*))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	tommy_allocator_done(&alloc);

	/* ensure correct alignment */
	tommy_allocator_init(&alloc, sizeof(void*) - 1, sizeof(void*));
	if (alloc.block_size != sizeof(void*))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	tommy_allocator_done(&alloc);

	/* check big blocks */
	tommy_allocator_init(&alloc, 128000, 64);
	if (tommy_allocator_alloc(&alloc) == 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */
	tommy_allocator_done(&alloc);

	tommy_allocator_init(&alloc, 64, 64);

	START("alloc");
	for(i=0;i<size;++i) {
		PTR[i] = tommy_allocator_alloc(&alloc);
	}
	STOP();

	START("free");
	for(i=0;i<size;++i) {
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
			if (a->value > b->value)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
			/* check order for stable sort */
			if (a->value == b->value && a > b)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */
		}
		node = node->next;
	}
}

void test_list(void)
{
	struct object* LIST;
	struct object_vector* VECTOR;
	tommy_node* list;
	unsigned i;
	const unsigned size = TOMMY_SIZE;

	LIST = malloc(size * sizeof(struct object));
	VECTOR = malloc(size * sizeof(struct object_vector));

	for(i=0;i<size;++i) {
		VECTOR[i].value = LIST[i].value = 0;
	}

	tommy_list_init(&list);

	/* sort an empty list */
	tommy_list_sort(&list, compare);

	if (!tommy_list_empty(&list))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_list_tail(&list) != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_list_head(&list) != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	for(i=0;i<size;++i) {
		VECTOR[i].value = LIST[i].value = rnd(size);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	if (tommy_list_tail(&list) == 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_list_head(&list) == 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	START("sort random");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort random");
	qsort(VECTOR, size, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* forward order with some (1%) random values */
	list = 0;
	for(i=0;i<size;++i) {
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
	for(i=0;i<size;++i) {
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
	for(i=0;i<size;++i) {
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
	for(i=0;i<size;++i) {
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
	tommy_tree tree;
	struct object_tree* OBJ;
	unsigned i;
	const unsigned size = TOMMY_SIZE / 4;

	OBJ = malloc(size * sizeof(struct object_tree));

	START("tree");
	tommy_tree_init(&tree, &compare);

	/* forward order */
	for(i=0;i<size;++i)
		OBJ[i].value = i;

	/* insert */
	for(i=0;i<size;++i)
		if (tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]) != &OBJ[i])
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	if (tommy_tree_memory_usage(&tree) < size * sizeof(tommy_tree_node))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_tree_count(&tree) != size)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	the_count = 0;
	tommy_tree_foreach(&tree, count_callback);
	if (the_count != size)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	the_count = 0;
	tommy_tree_foreach_arg(&tree, count_arg_callback, &the_count);
	if (the_count != size)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* search present */
	for(i=0;i<size/2;++i) {
		if (tommy_tree_search(&tree, &OBJ[i]) == 0)
				/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		if (tommy_tree_search_compare(&tree, &compare, &OBJ[i]) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	/* insert existing */
	for(i=0;i<size;++i) {
		struct object_tree EXTRA;
		EXTRA.value = i;
		if (tommy_tree_insert(&tree, &EXTRA.node, &EXTRA) == &EXTRA)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	/* remove existing */
	for(i=0;i<size/2;++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	/* remove missing */
	for(i=0;i<size/2;++i)
		if (tommy_tree_remove(&tree, &OBJ[i]) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* search missing */
	for(i=0;i<size/2;++i) {
		if (tommy_tree_search(&tree, &OBJ[i]) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
		if (tommy_tree_search_compare(&tree, &compare, &OBJ[i]) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}

	/* remove present */
	for(i=0;i<size/2;++i)
		if (tommy_tree_remove(&tree, &OBJ[size/2+i]) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* reverse order */
	for(i=0;i<size;++i)
		OBJ[i].value = size - i;

	/* insert */
	for(i=0;i<size;++i)
		if (tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]) != &OBJ[i])
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove all */
	for(i=0;i<size;++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	/* random order */
	for(i=0;i<size;++i)
		OBJ[i].value = tommy_inthash_u32(i);

	/* insert */
	for(i=0;i<size;++i)
		if (tommy_tree_insert(&tree, &OBJ[i].node, &OBJ[i]) != &OBJ[i])
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove all */
	for(i=0;i<size;++i)
		tommy_tree_remove_existing(&tree, &OBJ[i].node);

	STOP();
}

void test_array(void)
{
	tommy_array array;
	tommy_uintptr_t i;
	const unsigned size = 50 * TOMMY_SIZE;

	tommy_array_init(&array);

	/* no op */
	tommy_array_grow(&array, 0);

	START("array init");
	for(i=0;i<size;++i) {
		tommy_array_grow(&array, i + 1);
		if (tommy_array_get(&array, i) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	START("array set");
	for(i=0;i<size;++i) {
		tommy_array_set(&array, i, (void*)i);
	}
	STOP();

	START("array get");
	for(i=0;i<size;++i) {
		if (tommy_array_get(&array, i) != (void*)i)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	if (tommy_array_memory_usage(&array) < size * sizeof(void*))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	tommy_array_done(&array);
}

void test_arrayof(void)
{
	tommy_arrayof arrayof;
	unsigned i;
	const unsigned size = 50 * TOMMY_SIZE;

	tommy_arrayof_init(&arrayof, sizeof(unsigned));

	/* no op */
	tommy_arrayof_grow(&arrayof, 0);

	START("arrayof init");
	for(i=0;i<size;++i) {
		tommy_arrayof_grow(&arrayof, i + 1);
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	START("arrayof set");
	for(i=0;i<size;++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		*ref = i;
	}
	STOP();

	START("arrayof get");
	for(i=0;i<size;++i) {
		unsigned* ref = tommy_arrayof_ref(&arrayof, i);
		if (*ref != i)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	if (tommy_arrayof_memory_usage(&arrayof) < size * sizeof(unsigned))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	tommy_arrayof_done(&arrayof);
}

void test_arrayblk(void)
{
	tommy_arrayblk arrayblk;
	tommy_uintptr_t i;
	const unsigned size = 50 * TOMMY_SIZE;

	tommy_arrayblk_init(&arrayblk);

	/* no op */
	tommy_arrayblk_grow(&arrayblk, 0);

	START("arrayblk init");
	for(i=0;i<size;++i) {
		tommy_arrayblk_grow(&arrayblk, i + 1);
		if (tommy_arrayblk_get(&arrayblk, i) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	START("arrayblk set");
	for(i=0;i<size;++i) {
		tommy_arrayblk_set(&arrayblk, i, (void*)i);
	}
	STOP();

	START("arrayblk get");
	for(i=0;i<size;++i) {
		if (tommy_arrayblk_get(&arrayblk, i) != (void*)i)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	if (tommy_arrayblk_memory_usage(&arrayblk) < size * sizeof(void*))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	tommy_arrayblk_done(&arrayblk);
}

void test_arrayblkof(void)
{
	tommy_arrayblkof arrayblkof;
	unsigned i;
	const unsigned size = 50 * TOMMY_SIZE;

	tommy_arrayblkof_init(&arrayblkof, sizeof(unsigned));

	/* no op */
	tommy_arrayblkof_grow(&arrayblkof, 0);

	START("arrayblkof init");
	for(i=0;i<size;++i) {
		tommy_arrayblkof_grow(&arrayblkof, i + 1);
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	START("arrayblkof set");
	for(i=0;i<size;++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		*ref = i;
	}
	STOP();

	START("arrayblkof get");
	for(i=0;i<size;++i) {
		unsigned* ref = tommy_arrayblkof_ref(&arrayblkof, i);
		if (*ref != i)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */
	}
	STOP();

	if (tommy_arrayblkof_memory_usage(&arrayblkof) < size * sizeof(unsigned))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	tommy_arrayblkof_done(&arrayblkof);
}

void test_hashtable(void)
{
	tommy_hashtable hashtable;
	struct object_hash* HASH;
	unsigned i, j, n;
	unsigned limit;
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;

	HASH = malloc(size * sizeof(struct object_hash));

	for(i=0;i<size;++i)
		HASH[i].value = i % module;

	/* initialize a very small hashtable */
	tommy_hashtable_init(&hashtable, 1);

	/* check that we allocated space for more elements */
	if (hashtable.bucket_max == 1)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* destroy it as empty */
	tommy_hashtable_done(&hashtable);

	START("hashtable stack");
	limit = 5 * isqrt(size);
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashtable_init(&hashtable, limit / 2);

		/* insert */
		for(i=0;i<n;++i)
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashtable_memory_usage(&hashtable) < n * sizeof(void*))
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		if (tommy_hashtable_count(&hashtable) != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		the_count = 0;
		tommy_hashtable_foreach(&hashtable, count_callback);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* remove in backward order */
		for(i=0;i<n/2;++i)
			tommy_hashtable_remove_existing(&hashtable, &HASH[n-i-1].node);

		/* remove missing */
		for(i=0;i<n/2;++i)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[n-i-1], HASH[n-i-1].value) != 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		/* remove search */
		for(i=0;i<n/2;++i)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[n/2-i-1], HASH[n/2-i-1].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashtable_done(&hashtable);
	}
	STOP();

	START("hashtable queue");
	limit = isqrt(size) / 16;
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashtable_init(&hashtable, limit / 2);

		/* insert first run */
		for(j=0,i=0;i<n;++i)
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashtable_foreach_arg(&hashtable, count_arg_callback, &the_count);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* insert all the others */
		for(;i<size;++i,++j) {
			/* insert one */
			tommy_hashtable_insert(&hashtable, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashtable_remove_existing(&hashtable, &HASH[j].node);
		}

		for(;j<size;++j)
			if (tommy_hashtable_remove(&hashtable, search_callback, &HASH[j], HASH[j].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashtable_done(&hashtable);
	}
	STOP();
}

void test_hashdyn(void)
{
	tommy_hashdyn hashdyn;
	struct object_hash* HASH;
	unsigned i, j, n;
	unsigned limit;
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;

	HASH = malloc(size * sizeof(struct object_hash));

	for(i=0;i<size;++i)
		HASH[i].value = i % module;

	START("hashdyn stack");
	limit = 5 * isqrt(size);
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashdyn_init(&hashdyn);

		/* insert */
		for(i=0;i<n;++i)
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashdyn_memory_usage(&hashdyn) < n * sizeof(void*))
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		if (tommy_hashdyn_count(&hashdyn) != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		the_count = 0;
		tommy_hashdyn_foreach(&hashdyn, count_callback);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* remove in backward order */
		for(i=0;i<n/2;++i)
			tommy_hashdyn_remove_existing(&hashdyn, &HASH[n-i-1].node);

		/* remove missing */
		for(i=0;i<n/2;++i)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[n-i-1], HASH[n-i-1].value) != 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		/* remove search */
		for(i=0;i<n/2;++i)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[n/2-i-1], HASH[n/2-i-1].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();

	START("hashdyn queue");
	limit = isqrt(size) / 16;
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashdyn_init(&hashdyn);

		/* insert first run */
		for(j=0,i=0;i<n;++i)
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashdyn_foreach_arg(&hashdyn, count_arg_callback, &the_count);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* insert all the others */
		for(;i<size;++i,++j) {
			/* insert one */
			tommy_hashdyn_insert(&hashdyn, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashdyn_remove_existing(&hashdyn, &HASH[j].node);
		}

		for(;j<size;++j)
			if (tommy_hashdyn_remove(&hashdyn, search_callback, &HASH[j], HASH[j].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();
}

void test_hashlin(void)
{
	tommy_hashlin hashlin;
	struct object_hash* HASH;
	unsigned i, j, n;
	unsigned limit;
	const unsigned size = TOMMY_SIZE;
	const unsigned module = TOMMY_SIZE / 4;
	tommy_hashlin_node* bucket;

	HASH = malloc(size * sizeof(struct object_hash));

	for(i=0;i<size;++i)
		HASH[i].value = i % module;

	tommy_hashlin_init(&hashlin);

	/* insert */
	for(i=0;i<size;++i)
		tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

	/* get the bucket of the last element */
	bucket = tommy_hashlin_bucket(&hashlin, module - 1);
	if (bucket == 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* deinitialize without removing elements to force deallocation */
	tommy_hashlin_done(&hashlin);

	START("hashlin stack");
	limit = 5 * isqrt(size);
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashlin_init(&hashlin);

		/* insert */
		for(i=0;i<n;++i)
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

		if (tommy_hashlin_memory_usage(&hashlin) < n * sizeof(void*))
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		if (tommy_hashlin_count(&hashlin) != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		the_count = 0;
		tommy_hashlin_foreach(&hashlin, count_callback);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* remove in backward order */
		for(i=0;i<n/2;++i)
			tommy_hashlin_remove_existing(&hashlin, &HASH[n-i-1].node);

		/* remove missing */
		for(i=0;i<n/2;++i)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[n-i-1], HASH[n-i-1].value) != 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		/* remove search */
		for(i=0;i<n/2;++i)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[n/2-i-1], HASH[n/2-i-1].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashlin_done(&hashlin);
	}
	STOP();

	START("hashlin queue");
	limit = isqrt(size) / 16;
	for(n=0;n<=limit;++n) {
		/* last iteration is full size */
		if (n == limit)
			n = limit = size;

		tommy_hashlin_init(&hashlin);

		/* insert first run */
		for(j=0,i=0;i<n;++i)
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

		the_count = 0;
		tommy_hashlin_foreach_arg(&hashlin, count_arg_callback, &the_count);
		if (the_count != n)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

		/* insert all the others */
		for(;i<size;++i,++j) {
			/* insert one */
			tommy_hashlin_insert(&hashlin, &HASH[i].node, &HASH[i], HASH[i].value);

			/* remove one */
			tommy_hashlin_remove_existing(&hashlin, &HASH[j].node);
		}

		for(;j<size;++j)
			if (tommy_hashlin_remove(&hashlin, search_callback, &HASH[j], HASH[j].value) == 0)
				/* LCOV_EXCL_START */
				abort();
				/* LCOV_EXCL_STOP */

		tommy_hashlin_done(&hashlin);
	}
	STOP();
}

void test_trie(void)
{
	tommy_trie trie;
	tommy_allocator alloc;
	struct object_trie* OBJ;
	struct object_trie DUP[2];
	unsigned i;
	const unsigned size = TOMMY_SIZE * 4;

	OBJ = malloc(size * sizeof(struct object_trie));

	for(i=0;i<size;++i)
		OBJ[i].value = i;

	START("trie");
	tommy_allocator_init(&alloc, TOMMY_TRIE_BLOCK_SIZE, TOMMY_TRIE_BLOCK_SIZE);
	tommy_trie_init(&trie, &alloc);

	/* insert */
	for(i=0;i<size;++i)
		tommy_trie_insert(&trie, &OBJ[i].node, &OBJ[i], OBJ[i].value);

	if (tommy_trie_memory_usage(&trie) < size * sizeof(tommy_trie_node))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_allocator_memory_usage(&alloc) < trie.node_count * TOMMY_TRIE_BLOCK_SIZE)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_trie_count(&trie) != size)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* insert duplicate */
	for(i=0;i<2;++i) {
		DUP[i].value = 0;
		tommy_trie_insert(&trie, &DUP[i].node, &DUP[i], DUP[i].value);
	}

	/* search present */
	for(i=0;i<size/2;++i)
		if (tommy_trie_search(&trie, OBJ[i].value) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove first duplicate */
	tommy_trie_remove_existing(&trie, &DUP[0].node);

	/* remove existing */
	for(i=0;i<size/2;++i)
		tommy_trie_remove_existing(&trie, &OBJ[i].node);

	/* remove missing using the same bucket of the duplicate */
	if (tommy_trie_remove(&trie, 1) != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* search missing using the same bucket of the duplicate */
	if (tommy_trie_search(&trie, 1) != 0)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* remove second duplicate */
	tommy_trie_remove_existing(&trie, &DUP[1].node);

	/* remove missing */
	for(i=0;i<size/2;++i)
		if (tommy_trie_remove(&trie, OBJ[i].value) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* search missing */
	for(i=0;i<size/2;++i)
		if (tommy_trie_search(&trie, OBJ[i].value) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove present */
	for(i=0;i<size/2;++i)
		if (tommy_trie_remove(&trie, OBJ[size/2+i].value) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	tommy_allocator_done(&alloc);
	STOP();
}

void test_trie_inplace(void)
{
	tommy_trie_inplace trie_inplace;
	struct object_trie_inplace* OBJ;
	struct object_trie_inplace DUP[2];
	unsigned i;
	const unsigned size = TOMMY_SIZE * 4;

	OBJ = malloc(size * sizeof(struct object_trie_inplace));

	for(i=0;i<size;++i)
		OBJ[i].value = i;

	START("trie_inplace");
	tommy_trie_inplace_init(&trie_inplace);

	/* insert */
	for(i=0;i<size;++i)
		tommy_trie_inplace_insert(&trie_inplace, &OBJ[i].node, &OBJ[i], OBJ[i].value);

	if (tommy_trie_inplace_memory_usage(&trie_inplace) < size * sizeof(tommy_trie_inplace_node))
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	if (tommy_trie_inplace_count(&trie_inplace) != size)
		/* LCOV_EXCL_START */
		abort();
		/* LCOV_EXCL_STOP */

	/* insert duplicates */
	for(i=0;i<2;++i) {
		DUP[i].value = 0;
		tommy_trie_inplace_insert(&trie_inplace, &DUP[i].node, &DUP[i], DUP[i].value);
	}

	/* search present */
	for(i=0;i<size/2;++i)
		if (tommy_trie_inplace_search(&trie_inplace, OBJ[i].value) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove first duplicate */
	tommy_trie_inplace_remove_existing(&trie_inplace, &DUP[0].node);

	/* remove existing */
	for(i=0;i<size/2;++i)
		tommy_trie_inplace_remove_existing(&trie_inplace, &OBJ[i].node);

	/* remove second duplicate */
	tommy_trie_inplace_remove_existing(&trie_inplace, &DUP[1].node);

	/* remove missing */
	for(i=0;i<size/2;++i)
		if (tommy_trie_inplace_remove(&trie_inplace, OBJ[i].value) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* search missing */
	for(i=0;i<size/2;++i)
		if (tommy_trie_inplace_search(&trie_inplace, OBJ[i].value) != 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	/* remove present */
	for(i=0;i<size/2;++i)
		if (tommy_trie_inplace_remove(&trie_inplace, OBJ[size/2+i].value) == 0)
			/* LCOV_EXCL_START */
			abort();
			/* LCOV_EXCL_STOP */

	STOP();
}

int main() {
	nano_init();

	printf("Tommy check program.\n");

	test_hash();
	test_alloc();
	test_list();
	test_tree();
	test_array();
	test_arrayof();
	test_arrayblk();
	test_arrayblkof();
	test_hashtable();
	test_hashdyn();
	test_hashlin();
	test_trie();
	test_trie_inplace();

	printf("OK\n");

	return EXIT_SUCCESS;
}

