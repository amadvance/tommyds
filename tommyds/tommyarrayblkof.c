// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

#include "tommyarrayblkof.h"

/******************************************************************************/
/* array */

TOMMY_API void tommy_arrayblkof_init(tommy_arrayblkof* array, tommy_size_t element_size)
{
	array->block_capacity = (tommy_size_t)1 << TOMMY_ARRAY_BIT;
	array->block = tommy_cast(unsigned char**, tommy_malloc(array->block_capacity * sizeof(array->block[0])));
	array->block_count = 0;

	array->element_size = element_size;
	array->count = 0;
}

TOMMY_API void tommy_arrayblkof_done(tommy_arrayblkof* array)
{
	tommy_size_t i;

	for (i = 0; i < array->block_count; ++i)
		tommy_free(array->block[i]);

	tommy_free(array->block);
}

TOMMY_API void tommy_arrayblkof_grow(tommy_arrayblkof* array, tommy_size_t count)
{
	tommy_size_t block_max;
	tommy_size_t capacity;

	if (array->count >= count)
		return;
	array->count = count;

	if (count <= array->block_count * TOMMY_ARRAYBLKOF_SIZE)
		return;
	block_max = (count - 1) / TOMMY_ARRAYBLKOF_SIZE + 1;

	if (array->block_capacity < block_max) {
		/* only the directory moves; addresses inside existing blocks stay valid */
		capacity = array->block_capacity;
		while (capacity < block_max)
			capacity *= 2;
		array->block = tommy_cast(unsigned char**, tommy_realloc(array->block, capacity * sizeof(array->block[0])));
		array->block_capacity = capacity;
	}

	/* allocate new blocks */
	while (array->block_count < block_max) {
		unsigned char* ptr = tommy_cast(unsigned char*, tommy_calloc(TOMMY_ARRAYBLKOF_SIZE, array->element_size));

		array->block[array->block_count] = ptr;
		++array->block_count;
	}
}

TOMMY_API tommy_size_t tommy_arrayblkof_memory_usage(tommy_arrayblkof* array)
{
	return array->block_capacity * sizeof(array->block[0]) + array->block_count * TOMMY_ARRAYBLKOF_SIZE * array->element_size;
}

