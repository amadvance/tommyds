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
 * Tommy benchmark program.
 *
 * Simply run it without any options. It will generate a set of *.lst files
 * suitable for producing graphs with gnuplot or another program.
 *
 * This program is supposed to be also compiled with a C++ compiler,
 * to be able to use the Google dense_hash_map templates.
 * For this reason it contains a lot of unnecessary casting for C of void* pointers.
 */

#define __STDC_LIMIT_MACROS 1 /* needed by ck */

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

/* Judy available on in x86 */
#if defined(_WIN32)
#include <windows.h>
#if defined(_M_IX86) || defined(__i386__)
#define USE_JUDY
#endif
#else
#if defined(__i386__)
#define USE_JUDY
#endif
#endif

/* Available only in C++ */
#ifdef __cplusplus
#define USE_GOOGLEDENSEHASH
#define USE_GOOGLEBTREE
#define USE_STXBTREE
#define USE_CPPMAP
#define USE_CPPUNORDEREDMAP
#endif

/******************************************************************************/
/* data structure */

/* Tommy data structures */
/* We directly include the C file to have functions automatically */
/* expanded inline by the compiler like other implementations */
#include "tommyds/tommy.h"
#include "tommyds/tommy.c"

/* C++ Btree */
#ifdef USE_CPPMAP
#include <map>
#endif

/* C++ Hashtable */
#ifdef USE_CPPUNORDEREDMAP
#include <unordered_map>
#endif

/* Google C dense hash table */
/* http://code.google.com/p/google-sparsehash/ in the experimental/ directory */
/* Disabled by default because it's superseeded by the C++ version. */
/* Note that it has a VERY BAD performance on the "Change" test, */
/* so we disable it in the general graphs */
/* #define USE_GOOGLELIBCHASH */
#ifdef USE_GOOGLELIBCHASH
#define htonl(x) 0 /* used for serialization, not needed here */
#define ntohl(x) 0 /* used for serialization, not needed here */
#define Table(x) Dense##x /* Use google dense tables */
#include "benchmark/lib/google/libchash.c" 
#endif

/* Google C++ dense hash table. */
/* http://code.google.com/p/google-sparsehash/ */
/* Note that after erasing we always call resize(0) to possibly trigger a table resize to free some space */
/* Otherwise, it would be an unfair advantage never shrinking on deletion. */
/* The shrink is triggered only sometimes, so the performance doesn't suffer to much */
#ifdef USE_GOOGLEDENSEHASH
#include <google/dense_hash_map>
#endif

/* Google BTree */
/* https://code.google.com/p/cpp-btree/ */
#ifdef USE_GOOGLEBTREE
#if defined(_MSC_VER) /* Allow compilation in MSVC 2010 */
#include <stdint.h>
typedef size_t ssize_t;
#endif
#undef min
#undef max
#include "benchmark/lib/cpp-btree/btree_map.h"
#endif

/* STX BTree */
/* http://panthema.net/2007/stx-btree/ */
#ifdef USE_STXBTREE
#include "benchmark/lib/stx/btree_map.h"
#endif

/* UTHASH */
/* http://uthash.sourceforge.net/ */
#include "benchmark/lib/uthash/uthash.h"

/* Nedtries */
/* http://www.nedprod.com/programs/portable/nedtries/ */
/* Note that I fixed a crash bug when inserting objects with 0 key */
/* This fix is now in the nedtries github */
/* https://github.com/ned14/nedtries/commit/21039696f27db4ffac70a82f89dc5d00ae74b332 */
/* We do not use the C++ implementation as it doesn't compile with gcc 4.4.3 */
#include "benchmark/lib/nedtries/nedtrie.h"

/* KHash */
/* http://attractivechaos.awardspace.com/ */
/* It has the unfair advantage on remove by never shrinking when deleting objects. */
#define inline __inline
#include "benchmark/lib/khash/khash.h"

/* RBtree */
/* http://www.canonware.com/rb/ */
/* We are using the most recent implementation rb_never/rb.h. Not rb_old/ or rb_new/ */
#define RB_COMPACT
#define ssize_t size_t
#define bool int
#define true 1
#define false 0
#include "benchmark/lib/rb/rb_newer.h"

/* Judy */
/* http://judy.sourceforge.net/ */
/* Disabled on 64 bits platform in Windows as it's not supported */
#ifdef USE_JUDY
#include "benchmark/lib/judy/Judy.h"
#endif

/* JudyArray */
/* http://code.google.com/p/judyarray/ */
/* Ensure to use the version "judy64na.c". previous ones are not not working with integers key. */
#include "benchmark/lib/judyarray/judy64na.c"
#define USE_JUDYARRAY

/* Binary Search Cube */
/* https://sites.google.com/site/binarysearchcube/ */
#include "benchmark/lib/cube/binary-search-tesseract-1.0.c"
#define USE_CUBE

/* libdynamic */
/* https://github.com/fredrikwidlund/libdynamic */
#define USE_LIBDYNAMIC
#ifdef USE_LIBDYNAMIC
#define new c_new
#include "benchmark/lib/libdynamic/map.c"
#undef new
#endif

/* Concurrency Kit Hash Set */
/* http://concurrencykit.org/ */
/* Note that it has a VERY BAD performance on the "Change" test, */
/* so we disable it in the graphs becasue makes it unreadable */
/*
$ ./tommybench -n 63095 -d ck
Tommy benchmark program.
63095 ck forward
   forward,     insert,           ck,  193 [ns]
   forward,     change,           ck, 3102 [ns] <<<<<
   forward,        hit,           ck,  344 [ns]
   forward,       miss,           ck, 3330 [ns] <<<<<
   forward,     remove,           ck,  327 [ns]
63095 ck random
    random,     insert,           ck,  193 [ns]
    random,     change,           ck, 2984 [ns] <<<<<
    random,        hit,           ck,  340 [ns]
    random,       miss,           ck, 3261 [ns] <<<<<
    random,     remove,           ck,  341 [ns]
OK
*/
/* #define USE_CK */
#if defined(USE_CK) && defined(__linux)
/* if you enable it, ensure to link also with the -lck option */
extern "C" {
#include <ck_hs.h>
}
#endif

/* default hash function used */
#define hash(v) tommy_inthash_u32(v)

/******************************************************************************/
/* objects */

#define PAYLOAD 16 /**< Size of payload data for objects */

struct rbt_object {
	rb_node(struct rbt_object) link;
	unsigned value;
	char payload[PAYLOAD];
};

struct hashtable_object {
	tommy_node node;
	unsigned value;
	char payload[PAYLOAD];
};

struct uthash_object {
	UT_hash_handle hh;
	unsigned value;
	char payload[PAYLOAD];
};

struct khash_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct google_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct stx_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct trie_object {
	tommy_trie_node node;
	unsigned value;
	char payload[PAYLOAD];
};

struct trie_inplace_object {
	tommy_trie_inplace_node node;
	unsigned value;
	char payload[PAYLOAD];
};

struct judy_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct judyarray_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct nedtrie_object {
	NEDTRIE_ENTRY(nedtrie_object) link;
	unsigned value;   
	char payload[PAYLOAD];
};

struct cpp_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct cube_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct libdynamic_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct ck_object {
	unsigned value;
	char payload[PAYLOAD];
};

struct rbt_object* RBTREE;
struct hashtable_object* HASHTABLE;
struct hashtable_object* HASHDYN;
struct hashtable_object* HASHLIN;
struct trie_object* TRIE;
struct trie_inplace_object* TRIE_INPLACE;
struct khash_object* KHASH;
struct google_object* GOOGLE;
struct stx_object* STX;
struct uthash_object* UTHASH;
struct nedtrie_object* NEDTRIE;
struct cpp_object* CPP;
#ifdef USE_JUDY
struct judy_object* JUDY;
#endif
#ifdef USE_JUDYARRAY
struct judyarray_object* JUDYARRAY;
#endif
#ifdef USE_CUBE
struct cube_object* CUBE;
#endif
#ifdef USE_LIBDYNAMIC
/* We insert in the hashtable pointer to objects, because our objects are big */
size_t libdynamic_hash(map*, void* void_obj)
{
	libdynamic_object** obj = (libdynamic_object**)void_obj;

	return hash((*obj)->value);
}

int libdynamic_equal(map*, void* void_obj1, void* void_obj2)
{
	libdynamic_object** obj1 = (libdynamic_object**)void_obj1;
	libdynamic_object** obj2 = (libdynamic_object**)void_obj2;

	return (*obj1)->value == (*obj2)->value;
}

void libdynamic_set(map*, void* void_obj1, void* void_obj2)
{
	libdynamic_object** obj1 = (libdynamic_object**)void_obj1;
	libdynamic_object** obj2 = (libdynamic_object**)void_obj2;

	*obj1 = *obj2;
}

struct libdynamic_object* LIBDYNAMIC_RELEASE;

void libdynamic_release(map*, void* void_obj)
{
	libdynamic_object** obj = (libdynamic_object**)void_obj;

	LIBDYNAMIC_RELEASE = *obj;
}

struct libdynamic_object* LIBDYNAMIC;
#endif

#ifdef USE_CK
struct ck_object* CK;

static void* hs_malloc(size_t r)
{
	return malloc(r);
}

static void hs_free(void* p, size_t b, bool r)
{
	(void)b;
	(void)r;
	free(p);
}

static struct ck_malloc my_allocator = {
	hs_malloc,
	0,
	hs_free
};

static unsigned long hs_hash(const void* void_object, unsigned long seed)
{
	const struct ck_object* object = void_object;
	(void)seed;
	return hash(object->value);
}

static bool hs_compare(const void *previous, const void *compare)
{
	const struct ck_object* object_1 = previous;
	const struct ck_object* object_2 = compare;
	return object_1->value == object_2->value;
}
#endif

/******************************************************************************/
/* containers */

int rbt_compare(const void* void_a, const void* void_b)
{
	const struct rbt_object* a = (const struct rbt_object*)void_a;
	const struct rbt_object* b = (const struct rbt_object*)void_b;

	int va = a->value;
	int vb = b->value;

	if (va < vb)
		return -1;
	if (va > vb)
		return 1;
	return 0;
}

int tommy_hashtable_compare(const void* void_arg, const void* void_obj)
{
	const unsigned* arg = (const unsigned*)void_arg;
	const struct hashtable_object* obj = (const struct hashtable_object*)void_obj;

	if (*arg == obj->value)
		return 0;

	return 1;
}

typedef rbt(struct rbt_object) rbtree_t;

rb_gen(static, rbt_, rbtree_t, struct rbt_object, link, rbt_compare)

NEDTRIE_HEAD(nedtrie_t, nedtrie_object);

static size_t nedtrie_func(const struct nedtrie_object* r)
{
	return r->value;
}

NEDTRIE_GENERATE(static, nedtrie_t, nedtrie_object, link, nedtrie_func, NEDTRIE_NOBBLEZEROS(nedtrie_t));

KHASH_MAP_INIT_INT(word, struct khash_object*)

rbtree_t tree;
tommy_hashtable hashtable;
tommy_hashdyn hashdyn;
tommy_hashlin hashlin;
tommy_allocator trie_allocator;
tommy_trie trie;
tommy_trie_inplace trie_inplace;
struct uthash_object* uthash = 0;
struct nedtrie_t nedtrie;
khash_t(word)* khash;
#ifdef __cplusplus
/* use a specialized hash, otherwise the performance depends on the STL implementation used. */
class cpp_hash {
public:
	unsigned operator()(unsigned key) const { return hash(key); }
};
#endif
#ifdef USE_CPPMAP
typedef std::map<unsigned, struct cpp_object*> cppmap_t;
cppmap_t* cppmap;
#endif
#ifdef USE_CPPUNORDEREDMAP
typedef std::unordered_map<unsigned, struct cpp_object*, cpp_hash> cppunorderedmap_t;
cppunorderedmap_t* cppunorderedmap;
#endif
#ifdef USE_GOOGLELIBCHASH
struct HashTable* googlelibhash;
#endif
#ifdef USE_GOOGLEDENSEHASH
typedef google::dense_hash_map<unsigned, struct google_object*, cpp_hash> googledensehash_t;
googledensehash_t* googledensehash;
#endif
#ifdef USE_GOOGLEBTREE
typedef btree::btree_map<unsigned, struct google_object*> googlebtree_t;
googlebtree_t* googlebtree;
#endif
#ifdef USE_STXBTREE
typedef stx::btree_map<unsigned, struct stx_object*> stxbtree_t;
stxbtree_t* stxbtree;
#endif
#ifdef USE_JUDY
Pvoid_t judy = 0;
#endif
#ifdef USE_JUDYARRAY
Judy* judyarray = 0;
#endif
#ifdef USE_CUBE
struct cube* cube = 0;
#endif
#ifdef USE_LIBDYNAMIC
struct map libdynamic;
struct libdynamic_object libdynamic_empty = { (unsigned)-1 };
struct libdynamic_object* libdynamic_empty_ptr = &libdynamic_empty;
#endif
#ifdef USE_CK
ck_hs_t ck;
#endif

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
	if (r != 0) {
		abort();
	}

	ret = (t / info.denom) * info.numer;
	
	ret += (t % info.denom) * info.numer / info.denom;
#elif defined(__linux)
	struct timespec ts;
	int r;

	r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r != 0) {
		abort();
	}

	ret = ts.tv_sec * (tommy_uint64_t)1000000000 + ts.tv_nsec;
#else
	struct timeval tv;
	int r;

	r = gettimeofday(&tv, 0);
	if (r != 0) {
		abort();
	}

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
		goto loop;

	return r;
}

/******************************************************************************/
/* test */

#ifdef _DEBUG
#define MAX 100000
#else
#if defined(__x86_64__) || defined(_M_X64)
#define MAX 100000000
#else
#define MAX 10000000
#endif
#endif

/**
 * Max number of retries.
 */
#define RETRY_MAX 7

/**
 * Max total number of test for retries.
 */
#define MIN_TRY 2000000

/**
 * Operations.
 */
#define OPERATION_INSERT 0
#define OPERATION_HIT 1
#define OPERATION_MISS 2
#define OPERATION_SIZE 3
#define OPERATION_CHANGE 4
#define OPERATION_REMOVE 5
#define OPERATION_MAX 6

const char* OPERATION_NAME[OPERATION_MAX] = {
	"insert",
	"hit",
	"miss",
	"size",
	"change",
	"remove",
};

/**
 * Orders.
 */
#define ORDER_FORWARD 0
#define ORDER_RANDOM 1
#define ORDER_MAX 2

const char* ORDER_NAME[ORDER_MAX] = {
	"forward",
	"random",
};

/**
 * Data structures.
 */
#define DATA_HASHTABLE 0
#define DATA_HASHDYN 1
#define DATA_HASHLIN 2
#define DATA_TRIE 3
#define DATA_TRIE_INPLACE 4
#define DATA_TREE 5
#define DATA_NEDTRIE 6
#define DATA_KHASH 7
#define DATA_UTHASH 8
#define DATA_JUDY 9
#define DATA_JUDYARRAY 10
#define DATA_GOOGLEDENSEHASH 11
#define DATA_GOOGLEBTREE 12
#define DATA_STXBTREE 13
#define DATA_CPPUNORDEREDMAP 14
#define DATA_CPPMAP 15
#define DATA_CUBE 16
#define DATA_LIBDYNAMIC 17
#ifdef USE_GOOGLELIBCHASH
#define DATA_GOOGLELIBCHASH 18
#endif
#ifdef USE_CK
#define DATA_CK 19
#endif
#define DATA_MAX 20

const char* DATA_NAME[DATA_MAX] = {
	"tommy-hashtable",
	"tommy-hashdyn",
	"tommy-hashlin",
	"tommy-trie",
	"tommy-trie-inplace",
	"rbtree",
	"nedtrie",
	"khash",
	"uthash",
	"judy",
	"judyarray",
	"googledensehash",
	"googlebtree",
	"stxbtree",
	"c++unorderedmap",
	"c++map",
	"tesseract",
	"libdynamic",
	"googlelibchash",
	"concurrencykit",
};

/** 
 * Logged data.
 */
unsigned LOG[RETRY_MAX][DATA_MAX][ORDER_MAX][OPERATION_MAX];

/**
 * Time limit in nanosecond. 
 * We stop measuring degenerated cases after this limit.
 */
#define TIME_MAX_NS 1500

/**
 * Last measure.
 * Used to stop degenerated cases.
 */
unsigned LAST[DATA_MAX][ORDER_MAX];

/**
 * Test state.
 */
unsigned the_retry; /**< Number of retry. */
unsigned the_data;  /**< Data struture in test. */
unsigned the_operation; /**< Operation in test. */
unsigned the_order; /**< Order in test. */
unsigned the_max; /**< Number of elements in test. */
tommy_uint64_t the_time; /**< Start time of the test. */
unsigned the_start_data; /**< Saved data structure in the last START command, simply to avoid to repeat it for STOP. */

/**
 * Control flow state.
 */
tommy_bool_t the_log;

/** 
 * If the data structure should be in the graph, even if with no data.
 * It allows to have equal graphs also if in some platforms a data structure is not available.
 */
tommy_bool_t is_listed(unsigned data)
{
	(void)data;

	/* always have all the columns, we exclude them in the graphs */
	return 1;
}

/**
 * If the data structure should be measured.
 */
tommy_bool_t is_select(unsigned data)
{
	return the_data == data;
}

tommy_bool_t start(unsigned data)
{
	the_start_data = data;

	if (!is_select(the_start_data))
		return 0;

	if (!the_log)
		printf("%10s, %10s, %12s, ", ORDER_NAME[the_order], OPERATION_NAME[the_operation], DATA_NAME[data]);

	the_time = nano();
	return 1;
}

void stop(void)
{
	tommy_uint64_t elapsed = nano() - the_time;

	if (!is_select(the_start_data))
		return;

	if (!the_log) {
		printf("%4u [ns]\n", (unsigned)(elapsed / the_max));
	} 

	LOG[the_retry][the_data][the_order][the_operation] = (unsigned)(elapsed / the_max);
}

void mem(unsigned data, tommy_size_t v)
{
	if (!the_log) {
		printf("%10s, %10s, %12s, ", ORDER_NAME[the_order], OPERATION_NAME[the_operation], DATA_NAME[data]);
		printf("%4u [byte]\n", (unsigned)(v / the_max));
	} 

	LOG[the_retry][the_data][the_order][the_operation] = (unsigned)(v / the_max);
}

#define COND(s) if (is_select(s))
#define START(s) if (start(s)) for(i=0;i<the_max;++i)
#define STOP() stop()
#define MEM(s, v) if (is_select(s)) mem(s, v)
#define OPERATION(operation) the_operation = operation
#define ORDER(order) the_order = order
#define DATA(data) the_data = data

FILE* open(const char* mode)
{
	char buf[128];
	sprintf(buf, "dat_%s_%s.lst", ORDER_NAME[the_order], OPERATION_NAME[the_operation]);
	return fopen(buf, mode);
}

/******************************************************************************/
/* test */

/**
 * Cache clearing buffer.
 */
unsigned char CACHE[8*1024*1024];

void cache_clear(void)
{
	unsigned i;

	/* read & write */
	for(i=0;i<sizeof(CACHE);i += 32)
		CACHE[i] += 1;

#ifdef WIN32
	Sleep(0);
#endif
}

/**
 * Different orders used.
 */ 
unsigned* FORWARD;
unsigned* RAND0;
unsigned* RAND1;

void order_init(int max, int sparse)
{
	int i;

	FORWARD = (unsigned*)malloc(max * sizeof(unsigned));
	RAND0 = (unsigned*)malloc(max * sizeof(unsigned));
	RAND1 = (unsigned*)malloc(max * sizeof(unsigned));

	/* generated forward orders */
	if (sparse) {
		for(i=0;i<max;++i) {
			FORWARD[i] = 0xffffffff / max * i;
			RAND0[i] = FORWARD[i];
			RAND1[i] = FORWARD[i];
		}
	} else {
		for(i=0;i<max;++i) {
			FORWARD[i] = 0x80000000 + i * 2;
			RAND0[i] = FORWARD[i];
			RAND1[i] = FORWARD[i];
		}
	}

	/* randomize using the Fisher-Yates shuffle, http://en.wikipedia.org/wiki/Fisher-Yates_shuffle */
	for(i=max-1;i>=0;--i) {
		unsigned j, t;
		j = rnd(i+1);
		t = RAND0[i];
		RAND0[i] = RAND0[j];
		RAND0[j] = t;

		j = rnd(i+1);
		t = RAND1[i];
		RAND1[i] = RAND1[j];
		RAND1[j] = t;
	}
}

void order_done(void)
{
	free(FORWARD);
	free(RAND0);
	free(RAND1); 
}

void test_alloc(void)
{
	COND(DATA_TREE) {
		rbt_new(&tree);
		RBTREE = (struct rbt_object*)malloc(sizeof(struct rbt_object) * the_max);
	}

	COND(DATA_HASHTABLE) {
		tommy_hashtable_init(&hashtable, 2 * the_max);
		HASHTABLE = (struct hashtable_object*)malloc(sizeof(struct hashtable_object) * the_max);
	}

	COND(DATA_HASHDYN) {
		tommy_hashdyn_init(&hashdyn);
		HASHDYN = (struct hashtable_object*)malloc(sizeof(struct hashtable_object) * the_max);
	}

	COND(DATA_HASHLIN) {
		tommy_hashlin_init(&hashlin);
		HASHLIN = (struct hashtable_object*)malloc(sizeof(struct hashtable_object) * the_max);
	}

	COND(DATA_TRIE) {
		tommy_allocator_init(&trie_allocator, TOMMY_TRIE_BLOCK_SIZE, TOMMY_TRIE_BLOCK_SIZE);
		tommy_trie_init(&trie, &trie_allocator);
		TRIE = (struct trie_object*)malloc(sizeof(struct trie_object) * the_max);
	}

	COND(DATA_TRIE_INPLACE) {
		tommy_trie_inplace_init(&trie_inplace);
		TRIE_INPLACE = (struct trie_inplace_object*)malloc(sizeof(struct trie_inplace_object) * the_max);
	}

	COND(DATA_KHASH) {
		KHASH = (struct khash_object*)malloc(sizeof(struct khash_object) * the_max);
		khash = kh_init(word);
	}

#ifdef USE_GOOGLELIBCHASH
	COND(DATA_GOOGLELIBCHASH) {
		GOOGLE = (struct google_object*)malloc(sizeof(struct google_object) * the_max);
		googlelibhash = AllocateHashTable(sizeof(void*), 0);
	}
#endif

#ifdef USE_GOOGLEDENSEHASH
	COND(DATA_GOOGLEDENSEHASH) {
		GOOGLE = (struct google_object*)malloc(sizeof(struct google_object) * the_max);
		googledensehash = new googledensehash_t;
		googledensehash->set_empty_key(-1);
		googledensehash->set_deleted_key(-2);
	}
#endif

#ifdef USE_GOOGLEBTREE
	COND(DATA_GOOGLEBTREE) {
		GOOGLE = (struct google_object*)malloc(sizeof(struct google_object) * the_max);
		googlebtree = new googlebtree_t;
	}
#endif

#ifdef USE_STXBTREE
	COND(DATA_STXBTREE) {
		STX = (struct stx_object*)malloc(sizeof(struct stx_object) * the_max);
		stxbtree = new stxbtree_t;
	}
#endif

#ifdef USE_CPPMAP
	COND(DATA_CPPMAP) {
		CPP = (struct cpp_object*)malloc(sizeof(struct cpp_object) * the_max);
		cppmap = new cppmap_t;
	}
#endif

#ifdef USE_CPPUNORDEREDMAP
	COND(DATA_CPPUNORDEREDMAP) {
		CPP = (struct cpp_object*)malloc(sizeof(struct cpp_object) * the_max);
		cppunorderedmap = new cppunorderedmap_t;
	}
#endif

	COND(DATA_UTHASH) {
		UTHASH = (struct uthash_object*)malloc(sizeof(struct uthash_object) * the_max);
	}

	COND(DATA_NEDTRIE) {
		NEDTRIE_INIT(&nedtrie);
		NEDTRIE = (struct nedtrie_object*)malloc(sizeof(struct nedtrie_object) * the_max);
	}

#ifdef USE_JUDY
	COND(DATA_JUDY) {
		JUDY = (struct judy_object*)malloc(sizeof(struct judy_object) * the_max);
	}
#endif

#ifdef USE_JUDYARRAY
	COND(DATA_JUDYARRAY) {
		JUDYARRAY = (struct judyarray_object*)malloc(sizeof(struct judyarray_object) * the_max);
		judyarray = (Judy*)judy_open(1024, 1);
	}
#endif

#ifdef USE_CUBE
	COND(DATA_CUBE) {
		CUBE = (struct cube_object*)malloc(sizeof(struct cube_object) * the_max);
		cube = create_cube();
	}
#endif

#ifdef USE_LIBDYNAMIC
	COND(DATA_LIBDYNAMIC) {
		LIBDYNAMIC = (struct libdynamic_object*)malloc(sizeof(struct libdynamic_object) * the_max);
		map_construct(&libdynamic, sizeof(struct libdynamic_object*), &libdynamic_empty_ptr, libdynamic_set);
	}
#endif

#ifdef USE_CK
	COND(DATA_CK) {
		CK = (struct ck_object*)malloc(sizeof(struct ck_object) * the_max);
		/* Adding CK_HS_MODE_DELETE makes the performance worse */
		/* when the number of elements is near and just a little lower than a */
		/* power of 2. For example, with 63095 elements: */
/*
63095 ck forward
   forward,     insert,           ck,  200 [ns]
   forward,     change,           ck, 23977 [ns] <<<<<
   forward,        hit,           ck,  338 [ns]
   forward,       miss,           ck,  209 [ns]
   forward,     remove,           ck,  325 [ns]
63095 ck random
    random,     insert,           ck,  197 [ns]
    random,     change,           ck, 24025 [ns] <<<<<
    random,        hit,           ck,  342 [ns]
    random,       miss,           ck,  206 [ns]
    random,     remove,           ck,  337 [ns]
*/
		/* Without CK_HS_MODE_DELETE performance are better, but still */
		/* very slow: */
/*
63095 ck forward
   forward,     insert,           ck,  193 [ns]
   forward,     change,           ck, 3102 [ns] <<<<<
   forward,        hit,           ck,  344 [ns]
   forward,       miss,           ck, 3330 [ns] <<<<<
   forward,     remove,           ck,  327 [ns]
63095 ck random
    random,     insert,           ck,  193 [ns]
    random,     change,           ck, 2984 [ns] <<<<<
    random,        hit,           ck,  340 [ns]
    random,       miss,           ck, 3261 [ns] <<<<<
    random,     remove,           ck,  341 [ns]
*/
		ck_hs_init(&ck, CK_HS_MODE_OBJECT, hs_hash, hs_compare, &my_allocator, 32, 0);
	}
#endif
}

void test_free(void)
{
	COND(DATA_TREE) {
		free(RBTREE);
	}

	COND(DATA_HASHTABLE) {
		if (tommy_hashtable_count(&hashtable) != 0)
			abort();
		tommy_hashtable_done(&hashtable);
		free(HASHTABLE);
	}

	COND(DATA_HASHDYN) {
		if (tommy_hashdyn_count(&hashdyn) != 0)
			abort();
		tommy_hashdyn_done(&hashdyn);
		free(HASHDYN);
	}

	COND(DATA_HASHLIN) {
		if (tommy_hashlin_count(&hashlin) != 0)
			abort();
		tommy_hashlin_done(&hashlin);
		free(HASHLIN);
	}

	COND(DATA_TRIE) {
		if (tommy_trie_count(&trie) != 0)
			abort();
		tommy_allocator_done(&trie_allocator);
		free(TRIE);
	}

	COND(DATA_TRIE_INPLACE) {
		if (tommy_trie_inplace_count(&trie_inplace) != 0)
			abort();
		free(TRIE_INPLACE);
	}

	COND(DATA_KHASH) {
		kh_destroy(word, khash);
		free(KHASH);
	}

#ifdef USE_GOOGLELIBCHASH
	COND(DATA_GOOGLELIBCHASH) {
		FreeHashTable(googlelibhash);
		free(GOOGLE);
	}
#endif

#ifdef USE_GOOGLEDENSEHASH
	COND(DATA_GOOGLEDENSEHASH) {
		free(GOOGLE);
		delete googledensehash;
	}
#endif

#ifdef USE_GOOGLEBTREE
	COND(DATA_GOOGLEBTREE) {
		free(GOOGLE);
		delete googlebtree;
	}
#endif

#ifdef USE_STXBTREE
	COND(DATA_STXBTREE) {
		free(STX);
		delete stxbtree;
	}
#endif

#ifdef USE_CPPMAP
	COND(DATA_CPPMAP) {
		free(CPP);
		delete cppmap;
	}
#endif

#ifdef USE_CPPUNORDEREDMAP
	COND(DATA_CPPUNORDEREDMAP) {
		free(CPP);
		delete cppunorderedmap;
	}
#endif

	COND(DATA_UTHASH) {
		free(UTHASH);
	}

	COND(DATA_NEDTRIE) {
		free(NEDTRIE);
	}

#ifdef USE_JUDY
	COND(DATA_JUDY) {
		free(JUDY);
	}
#endif

#ifdef USE_JUDYARRAY
	COND(DATA_JUDYARRAY) {
		free(JUDYARRAY);
		judy_close(judyarray);
	}
#endif

#ifdef USE_CUBE
	COND(DATA_CUBE) {
		free(CUBE);
		destroy_cube(cube);
	}
#endif

#ifdef USE_LIBDYNAMIC
	COND(DATA_LIBDYNAMIC) {
		free(LIBDYNAMIC);
		map_destruct(&libdynamic, &libdynamic_equal, 0);
	}
#endif

#ifdef USE_CK
	COND(DATA_CK) {
		free(CK);
		ck_hs_destroy(&ck);
	}
#endif
}

void test_insert(unsigned* INSERT)
{
	unsigned i;

	START(DATA_TREE) {
		unsigned key = INSERT[i];
		RBTREE[i].value = key;
		rbt_insert(&tree, &RBTREE[i]);
	} STOP();

	START(DATA_HASHTABLE) {
		unsigned key = INSERT[i];
		unsigned hash_key = hash(key);
		HASHTABLE[i].value = key;
		tommy_hashtable_insert(&hashtable, &HASHTABLE[i].node, &HASHTABLE[i], hash_key);
	} STOP();

	START(DATA_HASHDYN) {
		unsigned key = INSERT[i];
		unsigned hash_key = hash(key);
		HASHDYN[i].value = key;
		tommy_hashdyn_insert(&hashdyn, &HASHDYN[i].node, &HASHDYN[i], hash_key);
	} STOP();

	START(DATA_HASHLIN) {
		unsigned key = INSERT[i];
		unsigned hash_key = hash(key);
		HASHLIN[i].value = key;
		tommy_hashlin_insert(&hashlin, &HASHLIN[i].node, &HASHLIN[i], hash_key);
	} STOP();

	START(DATA_TRIE) {
		unsigned key = INSERT[i];
		TRIE[i].value = key;
		tommy_trie_insert(&trie, &TRIE[i].node, &TRIE[i], key);
	} STOP();

	START(DATA_TRIE_INPLACE) {
		unsigned key = INSERT[i];
		TRIE_INPLACE[i].value = key;
		tommy_trie_inplace_insert(&trie_inplace, &TRIE_INPLACE[i].node, &TRIE_INPLACE[i], key);
	} STOP();

	/* for khash we hash the key because internally khash doesn't use any hash for integer keys */
	START(DATA_KHASH) {
		unsigned key = INSERT[i];
		unsigned hash_key = hash(key);
		khiter_t k;
		int r;
		KHASH[i].value = key;
		k = kh_put(word, khash, hash_key, &r);
		if (!r)
			abort();
		kh_value(khash, k) = &KHASH[i];
	} STOP();

#ifdef USE_GOOGLELIBCHASH
	START(DATA_GOOGLELIBCHASH) {
		unsigned key = INSERT[i];
		HTItem* r;
		u_long ptr_value = (u_long)&GOOGLE[i];
		GOOGLE[i].value = key;
		r = HashInsert(googlelibhash, key, ptr_value);
		if (!r)
			abort();
	} STOP();
#endif

#ifdef USE_GOOGLEDENSEHASH
	START(DATA_GOOGLEDENSEHASH) {
		unsigned key = INSERT[i];
		struct google_object* obj = &GOOGLE[i];
		GOOGLE[i].value = key;
		(*googledensehash)[key] = obj;
	} STOP();
#endif

#ifdef USE_GOOGLEBTREE
	START(DATA_GOOGLEBTREE) {
		unsigned key = INSERT[i];
		struct google_object* obj = &GOOGLE[i];
		GOOGLE[i].value = key;
		(*googlebtree)[key] = obj;
	} STOP();
#endif

#ifdef USE_STXBTREE
	START(DATA_STXBTREE) {
		unsigned key = INSERT[i];
		struct stx_object* obj = &STX[i];
		STX[i].value = key;
		(*stxbtree)[key] = obj;
	} STOP();
#endif

#ifdef USE_CPPMAP
	START(DATA_CPPMAP) {
		unsigned key = INSERT[i];
		struct cpp_object* obj = &CPP[i];
		CPP[i].value = key;
		(*cppmap)[key] = obj;
	} STOP();
#endif

#ifdef USE_CPPUNORDEREDMAP
	START(DATA_CPPUNORDEREDMAP) {
		unsigned key = INSERT[i];
		struct cpp_object* obj = &CPP[i];
		CPP[i].value = key;
		(*cppunorderedmap)[key] = obj;
	} STOP();
#endif

	START(DATA_UTHASH) {
		unsigned key = INSERT[i];
		struct uthash_object* obj = &UTHASH[i];
		obj->value = key;
		HASH_ADD_INT(uthash, value, obj);
	} STOP();

	START(DATA_NEDTRIE) {
		unsigned key = INSERT[i];
		NEDTRIE[i].value = key;
		NEDTRIE_INSERT(nedtrie_t, &nedtrie, &NEDTRIE[i]);
	} STOP();

#ifdef USE_JUDY
	START(DATA_JUDY) {
		unsigned key = INSERT[i];
		Pvoid_t PValue;
		JUDY[i].value = key;
		JLI(PValue, judy, key);
		*(struct judy_object**)PValue = &JUDY[i];
	} STOP();
#endif

#ifdef USE_JUDYARRAY
	START(DATA_JUDYARRAY) {
		judyvalue key = INSERT[i];
		JudySlot* pvalue;
		JUDYARRAY[i].value = key;
		pvalue = judy_cell(judyarray, (uchar*)&key, 0);
		*(struct judyarray_object**)pvalue = &JUDYARRAY[i];
	} STOP();
#endif

#ifdef USE_CUBE
	START(DATA_CUBE) {
		unsigned key = INSERT[i];
		CUBE[i].value = key;
		set_key(cube, key, &CUBE[i]);
	} STOP();
#endif

#ifdef USE_LIBDYNAMIC
	START(DATA_LIBDYNAMIC) {
		unsigned key = INSERT[i];
		struct libdynamic_object* obj = &LIBDYNAMIC[i];
		obj->value = key;
		map_insert(&libdynamic, &obj, libdynamic_hash, libdynamic_equal, libdynamic_set, 0);
	} STOP();
#endif

#ifdef USE_CK
	START(DATA_CK) {
		unsigned key = INSERT[i];
		unsigned hash_key;
		CK[i].value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, &CK[i]);
		ck_hs_put(&ck, hash_key, &CK[i]);
	} STOP();
#endif
}

void test_hit(unsigned* SEARCH)
{
	unsigned i;

	/* always dereference the object found. It has cache effect. */
	/* considering we are dealing with objects, it makes sense to simulate an access to it */
	/* this favorites data structures that store part of the information in the object itself */
	const int dereference = 1;

	const unsigned DELTA = 1;

	START(DATA_TREE) {
		unsigned key = SEARCH[i] + DELTA;
		struct rbt_object key_obj;
		struct rbt_object* obj;
		key_obj.value = key;
		obj = rbt_search(&tree, &key_obj); 
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_HASHTABLE) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashtable_search(&hashtable, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_HASHDYN) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashdyn_search(&hashdyn, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_HASHLIN) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashlin_search(&hashlin, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_TRIE) {
		unsigned key = SEARCH[i] + DELTA;
		struct trie_object* obj;
		obj = (struct trie_object*)tommy_trie_search(&trie, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_TRIE_INPLACE) {
		unsigned key = SEARCH[i] + DELTA;
		struct trie_inplace_object* obj;
		obj = (struct trie_inplace_object*)tommy_trie_inplace_search(&trie_inplace, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_KHASH) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		khiter_t k;
		k = kh_get(word, khash, hash_key);
		if (k == kh_end(khash))
			abort();
		if (dereference) {
			struct khash_object* obj = kh_value(khash, k);
			if (obj->value != key)
				abort();
		}
	} STOP();

#ifdef USE_GOOGLELIBCHASH
	START(DATA_GOOGLELIBCHASH) {
		unsigned key = SEARCH[i] + DELTA;
		HTItem* ptr;
		ptr = HashFind(googlelibhash, key);
		if (!ptr)
			abort();
		if (dereference) {
			struct google_object* obj = (void*)ptr->data;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_GOOGLEDENSEHASH
	START(DATA_GOOGLEDENSEHASH) {
		unsigned key = SEARCH[i] + DELTA;
		googledensehash_t::const_iterator ptr = googledensehash->find(key);
		if (ptr == googledensehash->end())
			abort();
		if (dereference) {
			struct google_object* obj = ptr->second;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_GOOGLEBTREE
	START(DATA_GOOGLEBTREE) {
		unsigned key = SEARCH[i] + DELTA;
		googlebtree_t::const_iterator ptr = googlebtree->find(key);
		if (ptr == googlebtree->end())
			abort();
		if (dereference) {
			struct google_object* obj = ptr->second;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_STXBTREE
	START(DATA_STXBTREE) {
		unsigned key = SEARCH[i] + DELTA;
		stxbtree_t::const_iterator ptr = stxbtree->find(key);
		if (ptr == stxbtree->end())
			abort();
		if (dereference) {
			struct stx_object* obj = ptr->second;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CPPMAP
	START(DATA_CPPMAP) {
		unsigned key = SEARCH[i] + DELTA;
		cppmap_t::const_iterator ptr = cppmap->find(key);
		if (ptr == cppmap->end())
			abort();
		if (dereference) {
			struct cpp_object* obj = ptr->second;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CPPUNORDEREDMAP
	START(DATA_CPPUNORDEREDMAP) {
		unsigned key = SEARCH[i] + DELTA;
		cppunorderedmap_t::const_iterator ptr = cppunorderedmap->find(key);
		if (ptr == cppunorderedmap->end())
			abort();
		if (dereference) {
			struct cpp_object* obj = ptr->second;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

	START(DATA_UTHASH) {
		unsigned key = SEARCH[i] + DELTA;
		struct uthash_object* obj;
		HASH_FIND_INT(uthash, &key, obj);  
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_NEDTRIE) {
		unsigned key = SEARCH[i] + DELTA;
		struct nedtrie_object key_obj;
		struct nedtrie_object* obj;
		key_obj.value = key;
		obj = NEDTRIE_FIND(nedtrie_t, &nedtrie, &key_obj);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

#ifdef USE_JUDY
	START(DATA_JUDY) {
		Word_t key = SEARCH[i] + DELTA;
		Pvoid_t PValue;
		JLG(PValue, judy, key);
		if (!PValue)
			abort();
		if (dereference) {
			struct judy_object* obj = *(struct judy_object**)PValue;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_JUDYARRAY
	START(DATA_JUDYARRAY) {
		judyvalue key = SEARCH[i] + DELTA;
		JudySlot* pvalue;
		pvalue = judy_slot(judyarray, (uchar*)&key, 0);
		if (!pvalue)
			abort();
		if (dereference) {
			struct judyarray_object* obj = *(struct judyarray_object**)pvalue;
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CUBE
	START(DATA_CUBE) {
		unsigned key = SEARCH[i] + DELTA;
		struct cube_object* obj;
		obj = (struct cube_object*)get_key(cube, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_LIBDYNAMIC
	START(DATA_LIBDYNAMIC) {
		unsigned key = SEARCH[i] + DELTA;
		struct libdynamic_object key_obj;
		struct libdynamic_object* key_ptr = &key_obj;
		struct libdynamic_object* obj;
		key_obj.value = key;
		obj = *(struct libdynamic_object**)map_at(&libdynamic, &key_ptr, libdynamic_hash, libdynamic_equal);
		if (obj == &libdynamic_empty)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CK
	START(DATA_CK) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key;
		struct ck_object obj_key;
		struct ck_object* obj;
		obj_key.value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, &obj_key);
		obj = (struct ck_object*)ck_hs_get(&ck, hash_key, &obj_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif
}

void test_miss(unsigned* SEARCH)
{
	unsigned i;

	const unsigned DELTA = 0;

	START(DATA_TREE) {
		struct rbt_object key;
		key.value = SEARCH[i] + DELTA;
		if (rbt_search(&tree, &key))
			abort();
	} STOP();

	START(DATA_HASHTABLE) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashtable_search(&hashtable, tommy_hashtable_compare, &key, hash_key);
		if (obj)
			abort();
	} STOP();

	START(DATA_HASHDYN) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashdyn_search(&hashdyn, tommy_hashtable_compare, &key, hash_key);
		if (obj)
			abort();
	} STOP();

	START(DATA_HASHLIN) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashlin_search(&hashlin, tommy_hashtable_compare, &key, hash_key);
		if (obj)
			abort();
	} STOP();

	START(DATA_TRIE) {
		struct trie_object* obj;
		obj = (struct trie_object*)tommy_trie_search(&trie, SEARCH[i] + DELTA);
		if (obj)
			abort();
	} STOP();

	START(DATA_TRIE_INPLACE) {
		struct trie_inplace_object* obj;
		obj = (struct trie_inplace_object*)tommy_trie_inplace_search(&trie_inplace, SEARCH[i] + DELTA);
		if (obj)
			abort();
	} STOP();

	START(DATA_KHASH) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key = hash(key);
		khiter_t k;
		k = kh_get(word, khash, hash_key);
		if (k != kh_end(khash))
			abort();
	} STOP();

#ifdef USE_GOOGLELIBCHASH
	START(DATA_GOOGLELIBCHASH) {
		unsigned key = SEARCH[i] + DELTA;
		HTItem* ptr;
		ptr = HashFind(googlelibhash, key);
		if (ptr)
			abort();
	} STOP();
#endif

#ifdef USE_GOOGLEDENSEHASH
	START(DATA_GOOGLEDENSEHASH) {
		unsigned key = SEARCH[i] + DELTA;
		googledensehash_t::const_iterator ptr = googledensehash->find(key);
		if (ptr != googledensehash->end())
			abort();
	} STOP();
#endif

#ifdef USE_GOOGLEBTREE
	START(DATA_GOOGLEBTREE) {
		unsigned key = SEARCH[i] + DELTA;
		googlebtree_t::const_iterator ptr = googlebtree->find(key);
		if (ptr != googlebtree->end())
			abort();
	} STOP();
#endif

#ifdef USE_STXBTREE
	START(DATA_STXBTREE) {
		unsigned key = SEARCH[i] + DELTA;
		stxbtree_t::const_iterator ptr = stxbtree->find(key);
		if (ptr != stxbtree->end())
			abort();
	} STOP();
#endif


#ifdef USE_CPPMAP
	START(DATA_CPPMAP) {
		unsigned key = SEARCH[i] + DELTA;
		cppmap_t::const_iterator ptr = cppmap->find(key);
		if (ptr != cppmap->end())
			abort();
	} STOP();
#endif

#ifdef USE_CPPUNORDEREDMAP
	START(DATA_CPPUNORDEREDMAP) {
		unsigned key = SEARCH[i] + DELTA;
		cppunorderedmap_t::const_iterator ptr = cppunorderedmap->find(key);
		if (ptr != cppunorderedmap->end())
			abort();
	} STOP();
#endif

	START(DATA_UTHASH) {
		unsigned key = SEARCH[i] + DELTA;
		struct uthash_object* obj;
		HASH_FIND_INT(uthash, &key, obj);  
		if (obj)
			abort();
	} STOP();

	START(DATA_NEDTRIE) {
		unsigned key = SEARCH[i] + DELTA;
		struct nedtrie_object key_obj;
		struct nedtrie_object* obj;
		key_obj.value = key;
		obj = NEDTRIE_FIND(nedtrie_t, &nedtrie, &key_obj);
		if (obj)
			abort();
	} STOP();

#ifdef USE_JUDY
	START(DATA_JUDY) {
		Word_t key = SEARCH[i] + DELTA;
		Pvoid_t PValue;
		JLG(PValue, judy, key);
		if (PValue)
			abort();
	} STOP();
#endif

#ifdef USE_JUDYARRAY
	START(DATA_JUDYARRAY) {
		judyvalue key = SEARCH[i] + DELTA;
		JudySlot* pvalue;
		pvalue = judy_slot(judyarray, (uchar*)&key, 0);
		if (pvalue) {
			/* workaround for a judyarray bug. Sometimes it returns a pvalue pointing to NULL */
			void* obj = *(void**)pvalue;
			if (obj)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CUBE
	START(DATA_CUBE) {
		unsigned key = SEARCH[i] + DELTA;
		struct cube_obj* obj;
		obj = (struct cube_obj*)get_key(cube, key);
		if (obj)
			abort();
	} STOP();
#endif

#ifdef USE_LIBDYNAMIC
	START(DATA_LIBDYNAMIC) {
		unsigned key = SEARCH[i] + DELTA;
		struct libdynamic_object key_obj;
		struct libdynamic_object* key_ptr = &key_obj;
		struct libdynamic_object* obj;
		key_obj.value = key;
		obj = *(struct libdynamic_object**)map_at(&libdynamic, &key_ptr, libdynamic_hash, libdynamic_equal);
		if (obj != &libdynamic_empty)
			abort();
	} STOP();
#endif

#ifdef USE_CK
	START(DATA_CK) {
		unsigned key = SEARCH[i] + DELTA;
		unsigned hash_key;
		struct ck_object obj_key;
		struct ck_object* obj;
		obj_key.value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, &obj_key);
		obj = (struct ck_object*)ck_hs_get(&ck, hash_key, &obj_key);
		if (obj)
			abort();
	} STOP();
#endif
}

void test_change(unsigned* REMOVE, unsigned* INSERT)
{
	unsigned i;

	const unsigned DELTA = 1;

	START(DATA_TREE) {
		unsigned key = REMOVE[i];
		struct rbt_object key_obj;
		struct rbt_object* obj;
		key_obj.value = REMOVE[i];
		obj = rbt_search(&tree, &key_obj); 
		if (!obj)
			abort();
		rbt_remove(&tree, obj);

		key = INSERT[i] + DELTA;  
		obj->value = key;
		rbt_insert(&tree, obj);
	} STOP();

	START(DATA_HASHTABLE) {
		unsigned key = REMOVE[i];
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashtable_remove(&hashtable, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		hash_key = hash(key);
		obj->value = key;
		tommy_hashtable_insert(&hashtable, &obj->node, obj, hash_key);
	} STOP();

	START(DATA_HASHDYN) {
		unsigned key = REMOVE[i];
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashdyn_remove(&hashdyn, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		hash_key = hash(key);
		obj->value = key;
		tommy_hashdyn_insert(&hashdyn, &obj->node, obj, hash_key);
	} STOP();

	START(DATA_HASHLIN) {
		unsigned key = REMOVE[i];
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashlin_remove(&hashlin, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		hash_key = hash(key);
		obj->value = key;
		tommy_hashlin_insert(&hashlin, &obj->node, obj, hash_key);
	} STOP();

	START(DATA_TRIE) {
		unsigned key = REMOVE[i];
		struct trie_object* obj;
		obj = (struct trie_object*)tommy_trie_remove(&trie, key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		tommy_trie_insert(&trie, &obj->node, obj, key);
	} STOP();

	START(DATA_TRIE_INPLACE) {
		unsigned key = REMOVE[i];
		struct trie_inplace_object* obj;
		obj = (struct trie_inplace_object*)tommy_trie_inplace_remove(&trie_inplace, key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		tommy_trie_inplace_insert(&trie_inplace, &obj->node, obj, key);
	} STOP();

	START(DATA_KHASH) {
		unsigned key = REMOVE[i];
		unsigned hash_key = hash(key);
		khiter_t k;
		int r;
		struct khash_object* obj;
		k = kh_get(word, khash, hash_key);
		if (k == kh_end(khash))
			abort();
		obj = kh_value(khash, k);
		kh_del(word, khash, k);

		key = INSERT[i] + DELTA;
		hash_key = hash(key);
		obj->value = key;
		k = kh_put(word, khash, hash_key, &r);
		if (!r)
			abort();
		kh_value(khash, k) = obj;
	} STOP();

#ifdef USE_GOOGLELIBCHASH
	START(DATA_GOOGLELIBCHASH) {
		unsigned key = REMOVE[i];
		HTItem* ptr;
		struct google_object* obj;
		u_long ptr_value;
		ptr = HashFind(googlelibhash, key);
		if (!ptr)
			abort();
		obj = (void*)ptr->data;
		HashDeleteLast(googlelibhash);

		key = INSERT[i] + DELTA;
		obj->value = key;
		ptr_value = (u_long)obj;
		ptr = HashInsert(googlelibhash, key, ptr_value);
		if (!ptr)
			abort();
	} STOP();
#endif

#ifdef USE_GOOGLEDENSEHASH
	START(DATA_GOOGLEDENSEHASH) {
		unsigned key = REMOVE[i];
		googledensehash_t::iterator ptr = googledensehash->find(key);
		struct google_object* obj;
		if (ptr == googledensehash->end())
			abort();
		obj = ptr->second;
		googledensehash->erase(ptr);

		key = INSERT[i] + DELTA;
		obj->value = key;
		(*googledensehash)[key] = obj;
	} STOP();
#endif

#ifdef USE_GOOGLEBTREE
	START(DATA_GOOGLEBTREE) {
		unsigned key = REMOVE[i];
		googlebtree_t::iterator ptr = googlebtree->find(key);
		struct google_object* obj;
		if (ptr == googlebtree->end())
			abort();
		obj = ptr->second;
		googlebtree->erase(ptr);

		key = INSERT[i] + DELTA;
		obj->value = key;
		(*googlebtree)[key] = obj;
	} STOP();
#endif

#ifdef USE_STXBTREE
	START(DATA_STXBTREE) {
		unsigned key = REMOVE[i];
		stxbtree_t::iterator ptr = stxbtree->find(key);
		struct stx_object* obj;
		if (ptr == stxbtree->end())
			abort();
		obj = ptr->second;
		stxbtree->erase(ptr);

		key = INSERT[i] + DELTA;
		obj->value = key;
		(*stxbtree)[key] = obj;
	} STOP();
#endif

#ifdef USE_CPPMAP
	START(DATA_CPPMAP) {
		unsigned key = REMOVE[i];
		cppmap_t::iterator ptr = cppmap->find(key);
		struct cpp_object* obj;
		if (ptr == cppmap->end())
			abort();
		obj = ptr->second;
		cppmap->erase(ptr);

		key = INSERT[i] + DELTA;
		obj->value = key;
		(*cppmap)[key] = obj;
	} STOP();
#endif

#ifdef USE_CPPUNORDEREDMAP
	START(DATA_CPPUNORDEREDMAP) {
		unsigned key = REMOVE[i];
		cppunorderedmap_t::iterator ptr = cppunorderedmap->find(key);
		struct cpp_object* obj;
		if (ptr == cppunorderedmap->end())
			abort();
		obj = ptr->second;
		cppunorderedmap->erase(ptr);

		key = INSERT[i] + DELTA;
		obj->value = key;
		(*cppunorderedmap)[key] = obj;
	} STOP();
#endif

	START(DATA_UTHASH) {
		unsigned key = REMOVE[i];
		struct uthash_object* obj;
		HASH_FIND_INT(uthash, &key, obj);
		if (!obj)
			abort();
		HASH_DEL(uthash, obj);

		key = INSERT[i] + DELTA;
		obj->value = key;
		HASH_ADD_INT(uthash, value, obj);
	} STOP();

	START(DATA_NEDTRIE) {
		unsigned key = REMOVE[i];
		struct nedtrie_object key_obj;
		struct nedtrie_object* obj;
		key_obj.value = key;
		obj = NEDTRIE_FIND(nedtrie_t, &nedtrie, &key_obj);
		if (!obj)
			abort();
		NEDTRIE_REMOVE(nedtrie_t, &nedtrie, obj);

		key = INSERT[i] + DELTA;
		obj->value = key;
		NEDTRIE_INSERT(nedtrie_t, &nedtrie, obj);
	} STOP();

#ifdef USE_JUDY
	START(DATA_JUDY) {
		Word_t key = REMOVE[i];
		struct judy_object* obj;
		int r;
		Pvoid_t PValue;
		JLG(PValue, judy, key);
		if (!PValue)
			abort();
		obj = *(struct judy_object**)PValue;
		JLD(r, judy, key);
		if (r != 1)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		JLI(PValue, judy, key);
		*(struct judy_object**)PValue = obj;
	} STOP();
#endif

#ifdef USE_JUDYARRAY
	START(DATA_JUDYARRAY) {
		judyvalue key = REMOVE[i];
		struct judyarray_object* obj;
		JudySlot* pvalue;
		pvalue = judy_slot(judyarray, (uchar*)&key, 0);
		if (!pvalue)
			abort();
		obj = *(struct judyarray_object**)pvalue;
		judy_del(judyarray);

		key = INSERT[i] + DELTA;
		obj->value = key;
		pvalue = judy_cell(judyarray, (uchar*)&key, 0);
		*(struct judyarray_object**)pvalue = obj;
	} STOP();
#endif

#ifdef USE_CUBE
	START(DATA_CUBE) {
		unsigned key = REMOVE[i];
		struct cube_object* obj;
		obj = (struct cube_object*)del_key(cube, key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		set_key(cube, key, obj);
	} STOP();
#endif

#ifdef USE_LIBDYNAMIC
	START(DATA_LIBDYNAMIC) {
		unsigned key = REMOVE[i];
		struct libdynamic_object key_obj;
		struct libdynamic_object* key_ptr = &key_obj;
		struct libdynamic_object* obj;
		key_obj.value = key;
		LIBDYNAMIC_RELEASE = 0;
		map_erase(&libdynamic, &key_ptr, libdynamic_hash, libdynamic_equal, libdynamic_set, libdynamic_release);
		obj = LIBDYNAMIC_RELEASE;
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		map_insert(&libdynamic, &obj, libdynamic_hash, libdynamic_equal, libdynamic_set, 0);
	} STOP();
#endif

#ifdef USE_CK
	START(DATA_CK) {
		unsigned key = REMOVE[i];
		unsigned hash_key;
		struct ck_object obj_key;
		struct ck_object* obj;
		obj_key.value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, &obj_key);
		obj = (struct ck_object*)ck_hs_remove(&ck, hash_key, &obj_key);
		if (!obj)
			abort();

		key = INSERT[i] + DELTA;
		obj->value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, obj);
		ck_hs_put(&ck, hash_key, obj);
	} STOP();
#endif
}

void test_remove(unsigned* REMOVE)
{
	unsigned i;

	/* always dereference the object deleted. It has cache effect. */
	/* considering we are dealing with objects, it makes sense to simulate an access to it */
	/* even on deletion, because you have at least to do a free() call. */
	/* this favorites data structures that store part of the information in the object itself */
	const int dereference = 1;

	const unsigned DELTA = 1;

	START(DATA_TREE) {
		unsigned key = REMOVE[i] + DELTA;
		struct rbt_object key_obj;
		struct rbt_object* obj;
		key_obj.value = key;
		obj = rbt_search(&tree, &key_obj); 
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
		rbt_remove(&tree, obj);
	} STOP();

	START(DATA_HASHTABLE) {
		unsigned key = REMOVE[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashtable_remove(&hashtable, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_HASHDYN) {
		unsigned key = REMOVE[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashdyn_remove(&hashdyn, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_HASHLIN) {
		unsigned key = REMOVE[i] + DELTA;
		unsigned hash_key = hash(key);
		struct hashtable_object* obj;
		obj = (struct hashtable_object*)tommy_hashlin_remove(&hashlin, tommy_hashtable_compare, &key, hash_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_TRIE) {
		unsigned key = REMOVE[i] + DELTA;
		struct trie_object* obj;
		obj = (struct trie_object*)tommy_trie_remove(&trie, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_TRIE_INPLACE) {
		unsigned key = REMOVE[i] + DELTA;
		struct trie_inplace_object* obj;
		obj = (struct trie_inplace_object*)tommy_trie_inplace_remove(&trie_inplace, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_KHASH) {
		unsigned key = REMOVE[i] + DELTA;
		unsigned hash_key = hash(key);
		struct khash_object* obj;
		khiter_t k;
		k = kh_get(word, khash, hash_key);
		if (k == kh_end(khash))
			abort();
		if (dereference) {
			obj = kh_value(khash, k);
		}
		kh_del(word, khash, k);
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

#ifdef USE_GOOGLELIBCHASH
	START(DATA_GOOGLELIBCHASH) {
		unsigned key = REMOVE[i] + DELTA;
		HTItem* ptr;
		struct google_object* obj;
		ptr = HashFind(googlelibhash, key);
		if (!ptr)
			abort();
		obj = (struct google_object*)ptr->data;
		HashDeleteLast(googlelibhash);
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_GOOGLEDENSEHASH
	START(DATA_GOOGLEDENSEHASH) {
		unsigned key = REMOVE[i] + DELTA;
		struct google_object* obj;
		googledensehash_t::iterator ptr = googledensehash->find(key);
		if (ptr == googledensehash->end())
			abort();
		obj = ptr->second;
		googledensehash->erase(ptr);

		/* force a resize when we reach 20% load factor */
		googledensehash->resize(0);

		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_GOOGLEBTREE
	START(DATA_GOOGLEBTREE) {
		unsigned key = REMOVE[i] + DELTA;
		struct google_object* obj;
		googlebtree_t::iterator ptr = googlebtree->find(key);
		if (ptr == googlebtree->end())
			abort();
		obj = ptr->second;
		googlebtree->erase(ptr);

		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_STXBTREE
	START(DATA_STXBTREE) {
		unsigned key = REMOVE[i] + DELTA;
		struct stx_object* obj;
		stxbtree_t::iterator ptr = stxbtree->find(key);
		if (ptr == stxbtree->end())
			abort();
		obj = ptr->second;
		stxbtree->erase(ptr);

		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CPPMAP
	START(DATA_CPPMAP) {
		unsigned key = REMOVE[i] + DELTA;
		struct cpp_object* obj;
		cppmap_t::iterator ptr = cppmap->find(key);
		if (ptr == cppmap->end())
			abort();
		obj = ptr->second;
		cppmap->erase(ptr);

		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CPPUNORDEREDMAP
	START(DATA_CPPUNORDEREDMAP) {
		unsigned key = REMOVE[i] + DELTA;
		struct cpp_object* obj;
		cppunorderedmap_t::iterator ptr = cppunorderedmap->find(key);
		if (ptr == cppunorderedmap->end())
			abort();
		obj = ptr->second;
		cppunorderedmap->erase(ptr);

		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

	START(DATA_UTHASH) {
		unsigned key = REMOVE[i] + DELTA;
		struct uthash_object* obj;
		HASH_FIND_INT(uthash, &key, obj);  
		if (!obj)
			abort();
		HASH_DEL(uthash, obj);  
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

	START(DATA_NEDTRIE) {
		unsigned key = REMOVE[i] + DELTA;
		struct nedtrie_object key_obj;
		struct nedtrie_object* obj;
		key_obj.value = key;
		obj = NEDTRIE_FIND(nedtrie_t, &nedtrie, &key_obj);
		if (!obj)
			abort();
		NEDTRIE_REMOVE(nedtrie_t, &nedtrie, obj);
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();

#ifdef USE_JUDY
	START(DATA_JUDY) {
		Word_t key = REMOVE[i] + DELTA;
		struct judy_object* obj;
		int r;
		if (dereference) {
			Pvoid_t PValue;
			JLG(PValue, judy, key);
			if (!PValue)
				abort();
			obj = *(struct judy_object**)PValue;
		}
		JLD(r, judy, key);
		if (r != 1)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_JUDYARRAY
	START(DATA_JUDYARRAY) {
		judyvalue key = REMOVE[i] + DELTA;
		struct judyarray_object* obj;
		JudySlot* pvalue;
		pvalue = judy_slot(judyarray, (uchar*)&key, 0);
		if (!pvalue)
			abort();
		obj = *(struct judyarray_object**)pvalue;
		judy_del(judyarray);
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CUBE
	START(DATA_CUBE) {
		unsigned key = REMOVE[i] + DELTA;
		struct cube_object* obj;
		obj = (struct cube_object*)del_key(cube, key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_LIBDYNAMIC
	START(DATA_LIBDYNAMIC) {
		unsigned key = REMOVE[i] + DELTA;
		struct libdynamic_object key_obj;
		struct libdynamic_object* key_ptr = &key_obj;
		struct libdynamic_object* obj;
		key_obj.value = key;
		LIBDYNAMIC_RELEASE = 0;
		map_erase(&libdynamic, &key_ptr, libdynamic_hash, libdynamic_equal, libdynamic_set, libdynamic_release);
		obj = LIBDYNAMIC_RELEASE;
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif

#ifdef USE_CK
	START(DATA_CK) {
		unsigned key = REMOVE[i] + DELTA;
		unsigned hash_key;
		struct ck_object obj_key;
		struct ck_object* obj;
		obj_key.value = key;
		hash_key = CK_HS_HASH(&ck, hs_hash, &obj_key);
		obj = (struct ck_object*)ck_hs_remove(&ck, hash_key, &obj_key);
		if (!obj)
			abort();
		if (dereference) {
			if (obj->value != key)
				abort();
		}
	} STOP();
#endif
}

tommy_size_t uthash_size(struct uthash_object* obj)
{
	UT_hash_table* table;
	if (!obj)
		return 0;
	table = obj->hh.tbl;
	if (!table)
		return 0;
	return table->num_buckets * (tommy_size_t)sizeof(table->buckets[0])
		+ table->num_items * (tommy_size_t)sizeof(UT_hash_handle);
}

tommy_size_t nedtrie_size(struct nedtrie_t* col)
{
	struct nedtrie_object element;
	(void)element;
	return col->count * sizeof(element.link);
}

tommy_size_t khash_size(khash_t(word)* col)
{
	return col->n_buckets * sizeof(void*) /* val */
		+ col->n_buckets * sizeof(uint32_t) /* key */
		+ (col->n_buckets >> 4) * sizeof(uint32_t); /* flags */
}

#ifdef USE_GOOGLEDENSEHASH
tommy_size_t googledensehash_size(googledensehash_t* col)
{
	googledensehash_t::value_type element;
	(void)element;
	return col->bucket_count() * sizeof(element);
}
#endif

#ifdef USE_GOOGLEBTREE
tommy_size_t googlebtree_size(googlebtree_t* col)
{
	return col->bytes_used();
}
#endif

#ifdef USE_STXBTREE
tommy_size_t stxbtree_size(stxbtree_t* col)
{
	return col->get_stats().leaves * sizeof(struct stxbtree_t::btree_impl::leaf_node)
		+ col->get_stats().innernodes * sizeof(struct stxbtree_t::btree_impl::inner_node);
	return 0;
}
#endif

tommy_size_t rbt_size(rbtree_t* col, unsigned count)
{
	struct rbt_object element;
	(void)col;
	(void)element;
	return count * sizeof(element.link);
}

tommy_size_t libdynamic_size(map* ld)
{
	return sizeof(map) + ld->element_size * ld->elements_capacity;
}

void test_size(void)
{
#ifdef USE_JUDY
	Word_t w;
#endif

	MEM(DATA_TREE, rbt_size(&tree, the_max));
	MEM(DATA_HASHTABLE, tommy_hashtable_memory_usage(&hashtable));
	MEM(DATA_HASHDYN, tommy_hashdyn_memory_usage(&hashdyn));
	MEM(DATA_HASHLIN, tommy_hashlin_memory_usage(&hashlin));
	MEM(DATA_TRIE, tommy_trie_memory_usage(&trie));
	MEM(DATA_TRIE_INPLACE, tommy_trie_inplace_memory_usage(&trie_inplace));
	MEM(DATA_KHASH, khash_size(khash));
#ifdef USE_GOOGLEDENSEHASH
	MEM(DATA_GOOGLEDENSEHASH, googledensehash_size(googledensehash));
#endif
#ifdef USE_GOOGLEBTREE
	MEM(DATA_GOOGLEBTREE, googlebtree_size(googlebtree));
#endif
#ifdef USE_STXBTREE
	MEM(DATA_STXBTREE, stxbtree_size(stxbtree));
#endif
	MEM(DATA_UTHASH, uthash_size(uthash));
	MEM(DATA_NEDTRIE, nedtrie_size(&nedtrie));
#ifdef USE_JUDY
	JLMU(w, judy);
	MEM(DATA_JUDY, w);
#endif
#ifdef USE_JUDYARRAY
	MEM(DATA_JUDYARRAY,judy_size(judyarray));
#endif
#ifdef USE_CUBE
	MEM(DATA_CUBE, size_cube(cube));
#endif
#ifdef USE_LIBDYNAMIC
	MEM(DATA_LIBDYNAMIC, libdynamic_size(&libdynamic));
#endif
}

void test_operation(unsigned* INSERT, unsigned* SEARCH)
{
	cache_clear();

	OPERATION(OPERATION_INSERT);
	test_insert(INSERT);

	OPERATION(OPERATION_CHANGE);
	test_change(SEARCH, INSERT);

	OPERATION(OPERATION_HIT);
	test_hit(SEARCH);

	OPERATION(OPERATION_MISS);
	test_miss(SEARCH);

	OPERATION(OPERATION_SIZE);
	test_size();

	OPERATION(OPERATION_REMOVE);
	test_remove(SEARCH);
}

void test(unsigned size, unsigned data, int log, int sparse)
{
	double base;
	double fact;

	base = 1000;
	fact = pow(10, 0.1);

	/* log if batch test or requested */
	the_log = (size == 0 && data == DATA_MAX) || log;

	/* write the header */
	if (the_log)
	for(the_order=0;the_order<ORDER_MAX;++the_order) {
		for(the_operation=0;the_operation<OPERATION_MAX;++the_operation) {
			FILE* f = open("wt");
			fprintf(f, "0\t");
			for(the_data=0;the_data<DATA_MAX;++the_data) {
				if (is_listed(the_data))
					fprintf(f, "%s\t", DATA_NAME[the_data]);
			}
			fprintf(f, "\n");
			fclose(f);
		}
	}

	if (size != 0)
		the_max = size;
	else
		the_max = (unsigned)base;

	while (the_max <= MAX) {
		unsigned retry;
	
		/* number of retries to avoid spikes */
		retry = MIN_TRY / the_max;
		if (retry < 1)
			retry = 1;
		if (retry > RETRY_MAX)
			retry = RETRY_MAX;
		if (size != 0)
			retry = 1;

		/* clear the log */
		memset(LOG, 0, sizeof(LOG));

		order_init(the_max, sparse);

		/* run the test */
		for(the_retry=0;the_retry<retry;++the_retry) {
			/* data */
			for(the_data=0;the_data<DATA_MAX;++the_data) {
				if (!is_listed(the_data))
					continue;

				if (data != DATA_MAX && data != the_data)
					continue;

				for(the_order=0;the_order<ORDER_MAX;++the_order) {
					unsigned i;

					printf("%12u", the_max);

					printf(" %18s %10s", DATA_NAME[the_data], ORDER_NAME[the_order]);

					/* skip degenerated cases */
					if (LAST[the_data][the_order] > TIME_MAX_NS) {
						printf(" (skipped, too slow)\n");
						continue;
					}

					if (!the_log)
						printf("\n");

					test_alloc();
					if (the_order == ORDER_FORWARD)
						test_operation(FORWARD, FORWARD);
					else
						test_operation(RAND0, RAND1);
					test_free();

					if (the_log) {
						for(i=0;i<OPERATION_MAX;++i)
							printf(" %4u", LOG[the_retry][the_data][the_order][i]);
						printf(" [ns]\n");
					}
				}
			}
		}

		order_done();

		/* write the data */
		if (the_log)
		for(the_order=0;the_order<ORDER_MAX;++the_order) {
			for(the_operation=0;the_operation<OPERATION_MAX;++the_operation) {
				FILE* f = open("at");

				fprintf(f, "%u\t", the_max);
				
				/* data */
				for(the_data=0;the_data<DATA_MAX;++the_data) {
					unsigned i, v;

					if (!is_listed(the_data))
						continue;

					/* get the minimum */
					v = LOG[0][the_data][the_order][the_operation];
					for(i=1;i<retry;++i) {
						if (LOG[i][the_data][the_order][the_operation] < v)
							v = LOG[i][the_data][the_order][the_operation];
					}

					if (the_operation != OPERATION_SIZE && v != 0) {
						/* keep the longest operation measure */
						if (v > LAST[the_data][the_order]) {
							LAST[the_data][the_order] = v;
						}
					}

					fprintf(f, "%u\t", v);
				}

				fprintf(f, "\n");
				fclose(f);
			}
		}

		if (size != 0)
			break;

		/* new max */
		base *= fact;
		the_max = (unsigned)base;
	}
}

void help(void)
{
	printf("Options\n");
	printf("-n NUMBER Run the test for the specified number of objects.\n");
	printf("-m        Run the test for the maximum number of objects.\n");
	printf("-d DATA   Run the test for the specified data structure.\n");
	printf("-s        Use a sparse dataset intead of a compact one.\n");
	printf("-l        Logs results into file for graphs creation.\n");
}

int main(int argc, char * argv[])
{
	int i;
	int flag_data = DATA_MAX;
	int flag_size = 0;
	int flag_log = 0;
	int flag_sparse = 0;

	nano_init();

	printf("Tommy benchmark program.\n");

	for(i=1;i<argc;++i) {
		if (strcmp(argv[i], "-l") == 0) {
			flag_log = 1;
		} else if (strcmp(argv[i], "-s") == 0) {
			flag_sparse = 1;
		} else if (strcmp(argv[i], "-m") == 0) {
			flag_size = MAX;
		} else if (strcmp(argv[i], "-n") == 0) {
			if (i+1 >= argc) {
				printf("Missing number of objects in %s\n", argv[i]);
				exit(EXIT_FAILURE);
			}
			flag_size = atoi(argv[i+1]);
			++i;
		} else if (strcmp(argv[i], "-d") == 0) {
			int j;
			if (i+1 >= argc) {
				printf("Missing data in %s\n", argv[i]);
				exit(EXIT_FAILURE);
			}
			flag_data = DATA_MAX;
			for(j=0;j<DATA_MAX;++j) {
				if (strcmp(argv[i+1], DATA_NAME[j]) == 0) {
					flag_data = j;
				}
			}
			if (flag_data == DATA_MAX) {
				printf("Unknown data name '%s'\n", argv[i+1]);
				printf("Possible values are:\n");
				for(j=0;j<DATA_MAX;++j) {
					printf("\t%s\n", DATA_NAME[j]);
				}
				exit(EXIT_FAILURE);
			}

			++i;
		} else {
			printf("Unknown option %s\n", argv[i]);
			help();
			exit(EXIT_FAILURE);
		} 
	}

	test(flag_size, flag_data, flag_log, flag_sparse);

	printf("OK\n");

	return EXIT_SUCCESS;
}

