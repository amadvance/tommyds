// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

#include "tommyarrayblk.h"

/******************************************************************************/
/* array */

void tommy_arrayblk_init(tommy_arrayblk* array)
{
	tommy_array_init(&array->block);

	array->count = 0;
}

TOMMY_API void tommy_arrayblk_done(tommy_arrayblk* array)
{
	tommy_size_t i;

	for (i = 0; i < tommy_array_size(&array->block); ++i)
		tommy_free(tommy_array_get(&array->block, i));

	tommy_array_done(&array->block);
}

TOMMY_API void tommy_arrayblk_grow(tommy_arrayblk* array, tommy_size_t count)
{
	tommy_size_t block_max;
	tommy_size_t block_mac;

	if (array->count >= count)
		return;
	array->count = count;

	block_max = (count + TOMMY_ARRAYBLK_SIZE - 1) / TOMMY_ARRAYBLK_SIZE;
	block_mac = tommy_array_size(&array->block);

	if (block_mac < block_max) {
		/* grow the block array */
		tommy_array_grow(&array->block, block_max);

		/* allocate new blocks */
		while (block_mac < block_max) {
			void** ptr = tommy_cast(void**, tommy_calloc(TOMMY_ARRAYBLK_SIZE, sizeof(void*)));

			/* set the new block */
			tommy_array_set(&array->block, block_mac, ptr);

			++block_mac;
		}
	}
}

TOMMY_API tommy_size_t tommy_arrayblk_memory_usage(tommy_arrayblk* array)
{
	return tommy_array_memory_usage(&array->block) + tommy_array_size(&array->block) * TOMMY_ARRAYBLK_SIZE * sizeof(void*);
}

