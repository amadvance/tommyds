// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

#include "tommyarrayof.h"

/******************************************************************************/
/* array */

TOMMY_API void tommy_arrayof_init(tommy_arrayof* array, tommy_size_t element_size)
{
	tommy_uint_t i;

	/* fixed initial size */
	array->element_size = element_size;
	array->bucket_bit = TOMMY_ARRAYOF_BIT;
	array->bucket_max = (tommy_size_t)1 << array->bucket_bit;
	array->bucket[0] = tommy_calloc(array->bucket_max, array->element_size);
	for (i = 1; i < TOMMY_ARRAYOF_BIT; ++i)
		array->bucket[i] = array->bucket[0];

	array->count = 0;
}

TOMMY_API void tommy_arrayof_done(tommy_arrayof* array)
{
	tommy_uint_t i;

	tommy_free(array->bucket[0]);
	for (i = TOMMY_ARRAYOF_BIT; i < array->bucket_bit; ++i) {
		unsigned char* segment = tommy_cast(unsigned char*, array->bucket[i]);
		tommy_free(segment + ((tommy_ptrdiff_t)1 << i) * array->element_size);
	}
}

TOMMY_API void tommy_arrayof_grow(tommy_arrayof* array, tommy_size_t count)
{
	if (array->count >= count)
		return;
	array->count = count;

	while (count > array->bucket_max) {
		unsigned char* segment;

		/* allocate one more segment */
		segment = tommy_cast(unsigned char*, tommy_calloc(array->bucket_max, array->element_size));

		/* store it adjusting the offset */
		/* cast to ptrdiff_t to ensure to get a negative value */
		array->bucket[array->bucket_bit] = segment - (tommy_ptrdiff_t)array->bucket_max * array->element_size;

		++array->bucket_bit;
		array->bucket_max = (tommy_size_t)1 << array->bucket_bit;
	}
}

TOMMY_API tommy_size_t tommy_arrayof_memory_usage(tommy_arrayof* array)
{
	return array->bucket_max * (tommy_size_t)array->element_size;
}

