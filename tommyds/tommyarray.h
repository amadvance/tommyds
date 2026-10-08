// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2011 Andrea Mazzoleni

/** \file
 * Dynamic array based on segments of exponentially growing size.
 *
 * This array is able to grow dynamically upon request, without any reallocation.
 *
 * The grow operation involves an allocation of a new array segment, without reallocating
 * the already used memory, and thus **not increasing** the heap fragmentation.
 * This also implies that the address of the stored elements never change.
 *
 * Allocated segments grow in size exponentially.
 */

#ifndef __TOMMYARRAY_H
#define __TOMMYARRAY_H

#include "tommytypes.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* array */

/**
 * Initial and minimal size of the array expressed as a power of 2.
 * The initial size is 2^TOMMY_ARRAY_BIT.
 */
#define TOMMY_ARRAY_BIT 6

/**
 * Array container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_array_struct {
	void** bucket[TOMMY_SIZE_BIT]; /**< Dynamic array of buckets. */
	tommy_size_t bucket_max; /**< Number of buckets. */
	tommy_size_t count; /**< Number of initialized elements in the array. */
	tommy_uint_t bucket_bit; /**< Bits used in the bit mask. */
} tommy_array;

/**
 * Initializes the array.
 */
TOMMY_API void tommy_array_init(tommy_array* array);

/**
 * Deinitializes the array.
 */
TOMMY_API void tommy_array_done(tommy_array* array);

/**
 * Allocates space for at least the specified number of elements.
 * The initialized size and existing elements are unchanged.
 * Existing element references remain valid.
 */
TOMMY_API void tommy_array_reserve(tommy_array* array, tommy_size_t size);

/**
 * Grows the size up to the specified value.
 * All the new elements in the array are initialized with the 0 value.
 */
tommy_inline void tommy_array_grow(tommy_array* array, tommy_size_t size)
{
	if (size > array->count) {
		array->count = size;

		if (size > array->bucket_max)
			tommy_array_reserve(array, size);
	}
}

/**
 * Changes the initialized size, preserving the common prefix.
 * New elements are initialized to zero.
 * Reducing the size does not release allocated capacity.
 */
TOMMY_API void tommy_array_resize(tommy_array* array, tommy_size_t size);

/**
 * Removes all elements, preserving the allocated capacity.
 * Pointed-to objects are not freed.
 * The array remains initialized and can be reused immediately.
 */
tommy_inline void tommy_array_clear(tommy_array* array)
{
	tommy_array_resize(array, 0);
}

/**
 * Releases unused allocated memory.
 * Preserves the size, values, and addresses of existing elements.
 */
TOMMY_API void tommy_array_shrink(tommy_array* array);

/**
 * Gets a reference of the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_array_grow().
 */
tommy_inline void** tommy_array_ref(tommy_array* array, tommy_size_t pos)
{
	assert(pos < array->count);

	/* get the highest bit set, in case of all 0, return 0 */
	tommy_uint_t bsr = tommy_ilog2(pos | 1);

	return &array->bucket[bsr][pos];
}

/**
 * Sets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_array_grow().
 */
tommy_inline void tommy_array_set(tommy_array* array, tommy_size_t pos, void* element)
{
	*tommy_array_ref(array, pos) = element;
}

/**
 * Gets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_array_grow().
 */
tommy_inline void* tommy_array_get(tommy_array* array, tommy_size_t pos)
{
	return *tommy_array_ref(array, pos);
}

/**
 * Grows and inserts a new element at the end of the array.
 */
tommy_inline void tommy_array_insert(tommy_array* array, void* element)
{
	tommy_size_t pos = array->count;

	tommy_array_grow(array, pos + 1);

	tommy_array_set(array, pos, element);
}

/**
 * Gets the initialized size of the array.
 */
tommy_inline tommy_size_t tommy_array_size(tommy_array* array)
{
	return array->count;
}

/**
 * Calls the specified function for each element in the array.
 *
 * You cannot add or remove elements, nor change the size of the array,
 * from inside the callback.
 *
 * \param array Array to iterate.
 * \param func Function to call with each element.
 */
TOMMY_API void tommy_array_foreach(tommy_array* array, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the array.
 *
 * \param array Array to iterate.
 * \param func Function to call with each element.
 * \param arg Argument to pass to the function.
 */
TOMMY_API void tommy_array_foreach_arg(tommy_array* array, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the size of allocated memory.
 */
TOMMY_API tommy_size_t tommy_array_memory_usage(tommy_array* array);

#endif

