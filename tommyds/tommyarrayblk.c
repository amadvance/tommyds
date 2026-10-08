// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

#include "tommyarrayblk.h"

#include <string.h> /* for memset */

/******************************************************************************/
/* array */

TOMMY_API void tommy_arrayblk_init(tommy_arrayblk* array)
{
	array->block_capacity = (tommy_size_t)1 << TOMMY_ARRAY_BIT;
	array->block = tommy_cast(void***, tommy_malloc(array->block_capacity * sizeof(array->block[0])));
	array->block_count = 0;

	array->count = 0;
}

TOMMY_API void tommy_arrayblk_done(tommy_arrayblk* array)
{
	for (tommy_size_t i = 0; i < array->block_count; ++i)
		tommy_free(array->block[i]);

	tommy_free(array->block);
}

TOMMY_API void tommy_arrayblk_reserve(tommy_arrayblk* array, tommy_size_t size)
{
	if (size <= array->block_count * TOMMY_ARRAYBLK_SIZE)
		return;

	tommy_size_t block_max = (size - 1) / TOMMY_ARRAYBLK_SIZE + 1;

	if (array->block_capacity < block_max) {
		/* only the directory moves; addresses inside existing blocks stay valid */
		tommy_size_t capacity = array->block_capacity;
		while (capacity < block_max)
			capacity *= 2;
		array->block = tommy_cast(void***, tommy_realloc(array->block, capacity * sizeof(array->block[0])));
		array->block_capacity = capacity;
	}

	/* allocate new blocks */
	while (array->block_count < block_max) {
		void** ptr = tommy_cast(void**, tommy_calloc(TOMMY_ARRAYBLK_SIZE, sizeof(void*)));

		array->block[array->block_count] = ptr;
		++array->block_count;
	}
}

TOMMY_API void tommy_arrayblk_resize(tommy_arrayblk* array, tommy_size_t size)
{
	if (size >= array->count) {
		tommy_arrayblk_grow(array, size);
		return;
	}

	/* clear the unused elements to maintain the invariant that unused slots are zero */
	tommy_size_t pos = size;
	while (pos < array->count) {
		tommy_size_t blk_idx = pos / TOMMY_ARRAYBLK_SIZE;
		tommy_size_t blk_offset = pos % TOMMY_ARRAYBLK_SIZE;
		tommy_size_t blk_rem = TOMMY_ARRAYBLK_SIZE - blk_offset;
		tommy_size_t count_rem = array->count - pos;
		tommy_size_t chunk = blk_rem < count_rem ? blk_rem : count_rem;

		memset(&array->block[blk_idx][blk_offset], 0, chunk * sizeof(void*));
		pos += chunk;
	}

	array->count = size;
}

TOMMY_API void tommy_arrayblk_shrink(tommy_arrayblk* array)
{
	tommy_size_t block_max = (array->count == 0) ? 0 : ((array->count - 1) / TOMMY_ARRAYBLK_SIZE + 1);

	while (array->block_count > block_max) {
		--array->block_count;
		tommy_free(array->block[array->block_count]);
	}

	tommy_size_t min_capacity = (tommy_size_t)1 << TOMMY_ARRAY_BIT;
	tommy_size_t capacity = min_capacity;
	while (capacity < block_max)
		capacity *= 2;
	if (capacity < array->block_capacity) {
		array->block = tommy_cast(void***, tommy_realloc(array->block, capacity * sizeof(array->block[0])));
		array->block_capacity = capacity;
	}
}

TOMMY_API void tommy_arrayblk_foreach(tommy_arrayblk* array, tommy_foreach_func* func)
{
	tommy_size_t pos = 0;

	while (pos < array->count) {
		tommy_size_t blk_idx = pos / TOMMY_ARRAYBLK_SIZE;
		tommy_size_t chunk = array->count - pos;
		void** ptr = array->block[blk_idx];

		if (chunk > TOMMY_ARRAYBLK_SIZE)
			chunk = TOMMY_ARRAYBLK_SIZE;

		for (tommy_size_t i = 0; i < chunk; ++i)
			func(ptr[i]);

		pos += chunk;
	}
}

TOMMY_API void tommy_arrayblk_foreach_arg(tommy_arrayblk* array, tommy_foreach_arg_func* func, void* arg)
{
	tommy_size_t pos = 0;

	while (pos < array->count) {
		tommy_size_t blk_idx = pos / TOMMY_ARRAYBLK_SIZE;
		tommy_size_t chunk = array->count - pos;
		void** ptr = array->block[blk_idx];

		if (chunk > TOMMY_ARRAYBLK_SIZE)
			chunk = TOMMY_ARRAYBLK_SIZE;

		for (tommy_size_t i = 0; i < chunk; ++i)
			func(arg, ptr[i]);

		pos += chunk;
	}
}

TOMMY_API tommy_size_t tommy_arrayblk_memory_usage(tommy_arrayblk* array)
{
	return array->block_capacity * sizeof(array->block[0]) + array->block_count * TOMMY_ARRAYBLK_SIZE * sizeof(void*);
}

