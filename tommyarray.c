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

#include "tommyarray.h"

#include <string.h> /* for memset */

/******************************************************************************/
/* array */

/**
 * Initial and minimal size of the array expressed as a power of 2.
 * The initial size is 2^TOMMY_ARRAY_BIT.
 */
#define TOMMY_ARRAY_BIT 4

void tommy_array_init(tommy_array* array)
{
	/* fixed initial size */
	array->bucket_bit = TOMMY_ARRAY_BIT;
	array->bucket_max = 1 << array->bucket_bit;
	array->bucket[0] = tommy_cast(void**, tommy_malloc(array->bucket_max * sizeof(void*)));
	memset(array->bucket[0], 0, array->bucket_max * sizeof(void*));
	array->bucket_mac = 1;
}

void tommy_array_done(tommy_array* array)
{
	unsigned i;
	for(i=0;i<array->bucket_mac;++i)
		tommy_free(array->bucket[i]);
}

/**
 * Return the bucket at the specified pos.
 */
tommy_inline void** tommy_array_pos(tommy_array* array, unsigned pos)
{  
	unsigned bsr;  
 
	/* special case for the first bucket */
	if (pos < (1 << TOMMY_ARRAY_BIT)) {
		return &array->bucket[0][pos];
	}

	/* get the highest bit set */
	bsr = tommy_ilog2_u32(pos);

	/* clear the highest bit */
	pos -= 1 << bsr;

	return &array->bucket[bsr - TOMMY_ARRAY_BIT + 1][pos];
}

tommy_inline void tommy_array_reserve(tommy_array* array, unsigned pos)
{
	while (pos >= array->bucket_max) {
		/* grow the hash size and allocate */
		array->bucket[array->bucket_mac] = tommy_cast(void**, tommy_malloc(array->bucket_max * sizeof(void*)));
		memset(array->bucket[array->bucket_mac], 0, array->bucket_max * sizeof(void*));
		++array->bucket_mac;
		++array->bucket_bit;
		array->bucket_max = 1 << array->bucket_bit;
	}
}

void tommy_array_set(tommy_array* array, unsigned pos, void* node)
{
	tommy_array_reserve(array, pos);

	*tommy_array_pos(array, pos) = node;
}

void* tommy_array_get(tommy_array* array, unsigned pos)
{
	tommy_array_reserve(array, pos);

	return *tommy_array_pos(array, pos);
}

tommy_size_t tommy_array_memory_usage(tommy_array* array)
{
	return array->bucket_max * (tommy_size_t)sizeof(array->bucket[0][0]);
}

