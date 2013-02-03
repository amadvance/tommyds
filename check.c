/*
 * Copyright 2010 Andrea Mazzoleni. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY ANDREA MAZZOLENI AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANDREA MAZZOLENI OR CONTRIBUTORS BE
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

#include "tommy.h"

#define MAX 1000000

#define PAYLOAD 16 /**< Size of payload data for objects */

struct object {
	tommy_node node;
	int value;
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
	tommy_node hashnode;
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

const char* the_str;
tommy_uint64_t the_start;

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

void start(const char* str)
{
	cache_clear();
	compare_counter = 0;
	the_str = str;
	the_start = nano();
}

void stop()
{
	tommy_uint64_t stop = nano();
	printf("%25s %8u [ms], %8u [compare]\n", the_str, (unsigned)((stop - the_start) / 1000000), compare_counter);
}

#define START(s) start(s)
#define STOP() stop()

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
				abort();
			/* check order for stable sort */
			if (a->value == b->value && a > b)
				abort();
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

	LIST = malloc(MAX * sizeof(struct object));
	VECTOR = malloc(MAX * sizeof(struct object_vector));

	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = 0;
	}

	list = 0;
	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = rnd(MAX);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort random");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort random");
	qsort(VECTOR, MAX, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* forward order with some (1%) random values */
	list = 0;
	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = i;
		if (rnd(100) == 0)
			VECTOR[i].value = LIST[i].value = rnd(MAX);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort partially ordered");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort partially ordered");
	qsort(VECTOR, MAX, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* forward order */
	list = 0;
	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort forward");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort forward");
	qsort(VECTOR, MAX, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* backward order */
	list = 0;
	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = MAX - 1 - i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort backward");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort backward");
	qsort(VECTOR, MAX, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	/* use a small range of random value to insert a lot of duplicates */
	list = 0;
	for(i=0;i<MAX;++i) {
		VECTOR[i].value = LIST[i].value = rnd(MAX / 1000 + 2);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort random duplicate");
	tommy_list_sort(&list, compare);
	STOP();

	START("C qsort random duplicate");
	qsort(VECTOR, MAX, sizeof(VECTOR[0]), compare_vector);
	STOP();

	test_list_order(list);

	free(LIST);
	free(VECTOR);
}

void test_array(void)
{
	tommy_array array;
	unsigned i;

	tommy_array_init(&array);

	START("array init");
	for(i=0;i<MAX*10;++i) {
		tommy_array_grow(&array, i + 1);
		if (tommy_array_get(&array, i) != 0)
			abort();
	}
	STOP();

	START("array set");
	for(i=0;i<MAX*10;++i) {
		tommy_array_set(&array, i, (void*)i);
	}
	STOP();

	START("array get");
	for(i=0;i<MAX*10;++i) {
		if (tommy_array_get(&array, i) != (void*)i)
			abort();
	}
	STOP();

	tommy_array_done(&array);
}

void test_hashdyn(void)
{
	tommy_list list;
	tommy_hashdyn hashdyn;
	struct object_hash* HASH;
	unsigned i, n;
	tommy_node* p;
	unsigned limit;

	HASH = malloc(MAX * sizeof(struct object_hash));

	for(i=0;i<MAX;++i) {
		HASH[i].value = i;
	}

	START("hashdyn stack");
	limit = 10 * sqrt(MAX);
	for(n=0;n<limit;++n) {
		tommy_list_init(&list);
		tommy_hashdyn_init(&hashdyn);

		/* insert */
		for(i=0;i<n;++i) {
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashdyn_insert(&hashdyn, &HASH[i].hashnode, &HASH[i], HASH[i].value);
		}

		/* remove */
		p = tommy_list_head(&list);
		while (p) {
			struct object_hash* obj = p->data;
			p = p->next;
			tommy_hashdyn_remove_existing(&hashdyn, &obj->hashnode);
		}

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();

	START("hashdyn queue");
	limit = sqrt(MAX) / 2;
	for(n=0;n<limit;++n) {
		tommy_list_init(&list);
		tommy_hashdyn_init(&hashdyn);

		/* insert first run */
		for(i=0;i<n;++i) {
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashdyn_insert(&hashdyn, &HASH[i].hashnode, &HASH[i], HASH[i].value);
		}

		/* insert all the others */
		for(;i<MAX;++i) {
			struct object_hash* obj;
			
			/* insert one */
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashdyn_insert(&hashdyn, &HASH[i].hashnode, &HASH[i], HASH[i].value);

			/* remove one */
			p = tommy_list_head(&list);
			obj = p->data;
			tommy_list_remove_existing(&list, p);
			tommy_hashdyn_remove_existing(&hashdyn, &obj->hashnode);
		}

		/* remove remaining */
		p = tommy_list_head(&list);
		while (p) {
			struct object_hash* obj = p->data;
			p = p->next;
			tommy_hashdyn_remove_existing(&hashdyn, &obj->hashnode);
		}

		tommy_hashdyn_done(&hashdyn);
	}
	STOP();
}

void test_hashlin(void)
{
	tommy_list list;
	tommy_hashlin hashlin;
	struct object_hash* HASH;
	unsigned i, n;
	tommy_node* p;
	unsigned limit;

	HASH = malloc(MAX * sizeof(struct object_hash));

	for(i=0;i<MAX;++i) {
		HASH[i].value = i;
	}

	START("hashlin stack");
	limit = 10 * sqrt(MAX);
	for(n=0;n<limit;++n) {
		tommy_list_init(&list);
		tommy_hashlin_init(&hashlin);

		/* insert */
		for(i=0;i<n;++i) {
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashlin_insert(&hashlin, &HASH[i].hashnode, &HASH[i], HASH[i].value);
		}

		/* remove */
		p = tommy_list_head(&list);
		while (p) {
			struct object_hash* obj = p->data;
			p = p->next;
			tommy_hashlin_remove_existing(&hashlin, &obj->hashnode);
		}

		tommy_hashlin_done(&hashlin);
	}
	STOP();

	START("hashlin queue");
	limit = sqrt(MAX) / 2;
	for(n=0;n<limit;++n) {
		tommy_list_init(&list);
		tommy_hashlin_init(&hashlin);

		/* insert first run */
		for(i=0;i<n;++i) {
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashlin_insert(&hashlin, &HASH[i].hashnode, &HASH[i], HASH[i].value);
		}

		/* insert all the others */
		for(;i<MAX;++i) {
			struct object_hash* obj;
			
			/* insert one */
			tommy_list_insert_head(&list, &HASH[i].node, &HASH[i]);
			tommy_hashlin_insert(&hashlin, &HASH[i].hashnode, &HASH[i], HASH[i].value);

			/* remove one */
			p = tommy_list_head(&list);
			obj = p->data;
			tommy_list_remove_existing(&list, p);
			tommy_hashlin_remove_existing(&hashlin, &obj->hashnode);
		}

		/* remove remaining */
		p = tommy_list_head(&list);
		while (p) {
			struct object_hash* obj = p->data;
			p = p->next;
			tommy_hashlin_remove_existing(&hashlin, &obj->hashnode);
		}

		tommy_hashlin_done(&hashlin);
	}
	STOP();
}

int main() {
	nano_init();

	printf("Tommy check program.\n");

	test_list();
	test_array();
	test_hashdyn();
	test_hashlin();

	printf("OK\n");

	return EXIT_SUCCESS;
}

