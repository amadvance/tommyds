// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

/** \file
 * Dynamic array based on blocks of fixed size.
 *
 * This array is able to grow dynamically upon request, without any reallocation.
 *
 * The grow operation involves an allocation of a new array block, without reallocating
 * the already used memory, and thus **not increasing** the heap fragmentation,
 * and **minimizing** the space occupation.
 * This also implies that the address of the stored elements never change.
 *
 * Allocated blocks are always of the same fixed size of 4 Ki pointers.
 */

#ifndef __TOMMYARRAYBLK_H
#define __TOMMYARRAYBLK_H

#include "tommytypes.h"
#include "tommyarray.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* array */

/**
 * Elements for each block.
 */
#define TOMMY_ARRAYBLK_SIZE (4 * 1024)

/**
 * Array container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_arrayblk_struct {
	tommy_array block; /**< Array of blocks. */
	tommy_size_t count; /**< Number of initialized elements in the array. */
} tommy_arrayblk;

/**
 * Initializes the array.
 */
TOMMY_API void tommy_arrayblk_init(tommy_arrayblk* array);

/**
 * Deinitializes the array.
 */
TOMMY_API void tommy_arrayblk_done(tommy_arrayblk* array);

/**
 * Grows the size up to the specified value.
 * All the new elements in the array are initialized with the 0 value.
 */
TOMMY_API void tommy_arrayblk_grow(tommy_arrayblk* array, tommy_size_t size);

/**
 * Gets a reference of the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_arrayblk_grow().
 */
tommy_inline void** tommy_arrayblk_ref(tommy_arrayblk* array, tommy_size_t pos)
{
	void** ptr;

	assert(pos < array->count);

	ptr = tommy_cast(void**, tommy_array_get(&array->block, pos / TOMMY_ARRAYBLK_SIZE));

	return &ptr[pos % TOMMY_ARRAYBLK_SIZE];
}

/**
 * Sets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_arrayblk_grow().
 */
tommy_inline void tommy_arrayblk_set(tommy_arrayblk* array, tommy_size_t pos, void* element)
{
	*tommy_arrayblk_ref(array, pos) = element;
}

/**
 * Gets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_arrayblk_grow().
 */
tommy_inline void* tommy_arrayblk_get(tommy_arrayblk* array, tommy_size_t pos)
{
	return *tommy_arrayblk_ref(array, pos);
}

/**
 * Grows and inserts a new element at the end of the array.
 */
tommy_inline void tommy_arrayblk_insert(tommy_arrayblk* array, void* element)
{
	tommy_size_t pos = array->count;

	tommy_arrayblk_grow(array, pos + 1);

	tommy_arrayblk_set(array, pos, element);
}

/**
 * Gets the initialized size of the array.
 */
tommy_inline tommy_size_t tommy_arrayblk_size(tommy_arrayblk* array)
{
	return array->count;
}

/**
 * Gets the size of allocated memory.
 */
TOMMY_API tommy_size_t tommy_arrayblk_memory_usage(tommy_arrayblk* array);

#endif
