// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

#include "tommyarrayblk.h"

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
	tommy_size_t i;

	for (i = 0; i < array->block_count; ++i)
		tommy_free(array->block[i]);

	tommy_free(array->block);
}

TOMMY_API void tommy_arrayblk_grow(tommy_arrayblk* array, tommy_size_t count)
{
	tommy_size_t block_max;
	tommy_size_t capacity;

	if (array->count >= count)
		return;
	array->count = count;

	if (count <= array->block_count * TOMMY_ARRAYBLK_SIZE)
		return;
	block_max = (count - 1) / TOMMY_ARRAYBLK_SIZE + 1;

	if (array->block_capacity < block_max) {
		/* only the directory moves; addresses inside existing blocks stay valid */
		capacity = array->block_capacity;
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

TOMMY_API tommy_size_t tommy_arrayblk_memory_usage(tommy_arrayblk* array)
{
	return array->block_capacity * sizeof(array->block[0]) + array->block_count * TOMMY_ARRAYBLK_SIZE * sizeof(void*);
}

