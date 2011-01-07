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

struct object {
	tommy_node node;
	int value;
};

int compare(const void* void_a, const void* void_b)
{
	const struct object* a = void_a;
	const struct object* b = void_b;

	if (a->value < b->value)
		return -1;
	if (a->value > b->value)
		return 1;
	return 0;
}

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

void start(const char* str)
{
	the_str = str;
	the_start = nano();
}

void stop()
{
	tommy_uint64_t stop = nano();
	printf("%23s %8u [ms]\n", the_str, (unsigned)((stop - the_start) / 1000000));
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
	tommy_node* list;
	unsigned i;

	LIST = malloc(MAX * sizeof(struct object));

	for(i=0;i<MAX;++i) {
		LIST[i].value = 0;
	}

	list = 0;
	for(i=0;i<MAX;++i) {
		LIST[i].value = rnd(MAX);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort random");
	tommy_list_sort(&list, compare);
	STOP();

	test_list_order(list);

	/* forward order with some (1%) random values */
	list = 0;
	for(i=0;i<MAX;++i) {
		LIST[i].value = i;
		if (rnd(100) == 0)
			LIST[i].value = rnd(MAX);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort partially ordered");
	tommy_list_sort(&list, compare);
	STOP();

	test_list_order(list);

	/* forward order */
	list = 0;
	for(i=0;i<MAX;++i) {
		LIST[i].value = i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort forward");
	tommy_list_sort(&list, compare);
	STOP();

	test_list_order(list);

	/* backward order */
	list = 0;
	for(i=0;i<MAX;++i) {
		LIST[i].value = MAX - 1 - i;
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort backward");
	tommy_list_sort(&list, compare);
	STOP();

	test_list_order(list);

	/* use a small range of random value to insert a lot of duplicates */
	list = 0;
	for(i=0;i<MAX;++i) {
		LIST[i].value = rnd(MAX / 1000 + 2);
		tommy_list_insert_tail(&list, &LIST[i].node, &LIST[i]);
	}

	START("sort random duplicate");
	tommy_list_sort(&list, compare);
	STOP();

	test_list_order(list);

	free(LIST);
}

int main() {
	nano_init();

	printf("Tommy check program.\n");

	test_list();

	printf("OK\n");

	return EXIT_SUCCESS;
}

