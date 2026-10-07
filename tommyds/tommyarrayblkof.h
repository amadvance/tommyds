// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

/** \file
 * Dynamic array based on blocks of fixed size.
 *
 * This array is able to grow dynamically without reallocating stored elements.
 * The directory of block pointers may be reallocated, but element addresses
 * never change.
 *
 * This is very similar to ::tommy_arrayblk, but it allows to store elements of any
 * size and not just pointers.
 *
 * Note that in this case tommy_arrayblkof_ref() returns a pointer to the element,
 * that should be used for getting and setting elements in the array,
 * as generic getter and setter are not available.
 */

#ifndef __TOMMYARRAYBLKOF_H
#define __TOMMYARRAYBLKOF_H

#include "tommytypes.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* array */

/**
 * Elements for each block.
 */
#define TOMMY_ARRAYBLKOF_SIZE (4 * 1024)

/**
 * Array container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_arrayblkof_struct {
	unsigned char** block; /**< Directory of blocks. */
	tommy_size_t block_count; /**< Number of allocated blocks. */
	tommy_size_t block_capacity; /**< Number of directory entries allocated. */
	tommy_size_t element_size; /**< Size of the stored element in bytes. */
	tommy_size_t count; /**< Number of initialized elements in the array. */
} tommy_arrayblkof;

/**
 * Initializes the array.
 * \param element_size Size in byte of the element to store in the array.
 */
TOMMY_API void tommy_arrayblkof_init(tommy_arrayblkof* array, tommy_size_t element_size);

/**
 * Deinitializes the array.
 */
TOMMY_API void tommy_arrayblkof_done(tommy_arrayblkof* array);

/**
 * Grows the size up to the specified value.
 * All the new elements in the array are initialized with the 0 value.
 */
TOMMY_API void tommy_arrayblkof_grow(tommy_arrayblkof* array, tommy_size_t size);

/**
 * Gets a reference of the element at the specified position.
 * You must be sure that space for this position is already
 * allocated calling tommy_arrayblkof_grow().
 */
tommy_inline void* tommy_arrayblkof_ref(tommy_arrayblkof* array, tommy_size_t pos)
{
	assert(pos < array->count);

	return array->block[pos / TOMMY_ARRAYBLKOF_SIZE] + (pos % TOMMY_ARRAYBLKOF_SIZE) * array->element_size;
}

/**
 * Gets the initialized size of the array.
 */
tommy_inline tommy_size_t tommy_arrayblkof_size(tommy_arrayblkof* array)
{
	return array->count;
}

/**
 * Gets the size of allocated memory.
 */
TOMMY_API tommy_size_t tommy_arrayblkof_memory_usage(tommy_arrayblkof* array);

#endif

