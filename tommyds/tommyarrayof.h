// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

/** \file
 * Dynamic array based on segments of exponentially growing size.
 *
 * This array is able to grow dynamically upon request, without any reallocation.
 *
 * This is very similar to ::tommy_array, but it allows to store elements of any
 * size and not just pointers.
 *
 * Note that in this case tommy_arrayof_ref() returns a pointer to the element,
 * that should be used for getting and setting elements in the array,
 * as generic getter and setter are not available.
 */

#ifndef __TOMMYARRAYOF_H
#define __TOMMYARRAYOF_H

#include "tommytypes.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* array */

/**
 * Initial and minimal size of the array expressed as a power of 2.
 * The initial size is 2^TOMMY_ARRAYOF_BIT.
 */
#define TOMMY_ARRAYOF_BIT 6

/**
 * Array container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_arrayof_struct {
	void* bucket[TOMMY_SIZE_BIT]; /**< Dynamic array of buckets. */
	tommy_size_t element_size; /**< Size of the stored element in bytes. */
	tommy_size_t bucket_max; /**< Number of buckets. */
	tommy_size_t count; /**< Number of initialized elements in the array. */
	tommy_uint_t bucket_bit; /**< Bits used in the bit mask. */
} tommy_arrayof;

/**
 * Initializes the array.
 * \param array Array to initialize.
 * \param element_size Size in bytes of the element to store in the array.
 */
TOMMY_API void tommy_arrayof_init(tommy_arrayof* array, tommy_size_t element_size);

/**
 * Deinitializes the array.
 */
TOMMY_API void tommy_arrayof_done(tommy_arrayof* array);

/**
 * Allocates space for at least the specified number of elements.
 * The initialized size and existing elements are unchanged.
 * Existing element references remain valid.
 * \param array Array to reserve space for.
 * \param size Number of elements to reserve space for.
 */
TOMMY_API void tommy_arrayof_reserve(tommy_arrayof* array, tommy_size_t size);

/**
 * Grows the size up to the specified value.
 * All the new elements in the array are initialized with the 0 value.
 * \param array Array to grow.
 * \param size New size of the array.
 */
tommy_inline void tommy_arrayof_grow(tommy_arrayof* array, tommy_size_t size)
{
	if (size > array->count) {
		array->count = size;

		if (size > array->bucket_max)
			tommy_arrayof_reserve(array, size);
	}
}

/**
 * Changes the initialized size, preserving the common prefix.
 * New elements are initialized to zero.
 * Reducing the size does not release allocated capacity.
 * \param array Array to resize.
 * \param size New size of the array.
 */
TOMMY_API void tommy_arrayof_resize(tommy_arrayof* array, tommy_size_t size);

/**
 * Removes all elements, preserving the allocated capacity.
 * The array remains initialized and can be reused immediately.
 * \param array Array to clear.
 */
tommy_inline void tommy_arrayof_clear(tommy_arrayof* array)
{
	tommy_arrayof_resize(array, 0);
}

/**
 * Releases unused allocated memory.
 * Preserves the size, values, and addresses of existing elements.
 * \param array Array to shrink.
 */
TOMMY_API void tommy_arrayof_shrink(tommy_arrayof* array);

/**
 * Gets a reference of the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_arrayof_grow().
 * \param array Array to reference.
 * \param pos Position of the element.
 */
tommy_inline void* tommy_arrayof_ref(tommy_arrayof* array, tommy_size_t pos)
{
	assert(pos < array->count);

	/* get the highest bit set, in case of all 0, return 0 */
	tommy_uint_t bsr = tommy_ilog2(pos | 1);

	unsigned char* ptr = tommy_cast(unsigned char*, array->bucket[bsr]);

	return ptr + pos * array->element_size;
}

/**
 * Gets the initialized size of the array.
 * \param array Array to query.
 */
tommy_inline tommy_size_t tommy_arrayof_size(tommy_arrayof* array)
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
TOMMY_API void tommy_arrayof_foreach(tommy_arrayof* array, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the array.
 *
 * \param array Array to iterate.
 * \param func Function to call with each element.
 * \param arg Argument to pass to the function.
 */
TOMMY_API void tommy_arrayof_foreach_arg(tommy_arrayof* array, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the size of allocated memory.
 * \param array Array to query.
 */
TOMMY_API tommy_size_t tommy_arrayof_memory_usage(tommy_arrayof* array);

#endif

