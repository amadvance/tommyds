// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2013 Andrea Mazzoleni

/** \file
 * Dynamic array based on fixed-size blocks.
 *
 * This array is able to grow dynamically without reallocating stored elements.
 *
 * Growth allocates new blocks without moving existing ones. The directory of
 * block pointers may be reallocated, but element addresses never change.
 *
 * Allocated blocks are always of the same fixed size of 4 Ki pointers.
 */

#ifndef TOMMY_ARRAYBLK_H
#define TOMMY_ARRAYBLK_H

#include "tommytypes.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* array */

/**
 * Initial and minimal capacity of the block directory expressed as a power of 2.
 */
#define TOMMY_ARRAYBLK_BIT 6

/**
 * Elements for each block.
 */
#define TOMMY_ARRAYBLK_SIZE (4 * 1024)

/**
 * Array container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_arrayblk_struct {
	void*** block; /**< Directory of blocks. */
	tommy_size_t block_count; /**< Number of allocated blocks. */
	tommy_size_t block_capacity; /**< Number of directory entries allocated. */
	tommy_size_t count; /**< Number of initialized elements in the array. */
} tommy_arrayblk;

/**
 * Initializes the array.
 * \param array The array.
 */
TOMMY_API void tommy_arrayblk_init(tommy_arrayblk* array);

/**
 * Deinitializes the array.
 * \param array The array.
 */
TOMMY_API void tommy_arrayblk_done(tommy_arrayblk* array);

/**
 * Allocates space for at least the specified number of elements.
 * The initialized size and existing elements are unchanged.
 * Existing element references remain valid.
 * \param array The array.
 * \param size Number of elements to reserve space for.
 */
TOMMY_API void tommy_arrayblk_reserve(tommy_arrayblk* array, tommy_size_t size);

/**
 * Grows the size up to the specified value.
 * All the new elements in the array are initialized with the 0 value.
 * \param array The array.
 * \param size New size of the array.
 */
tommy_inline void tommy_arrayblk_grow(tommy_arrayblk* array, tommy_size_t size)
{
	if (size > array->count) {
		array->count = size;

		if (size > array->block_count * TOMMY_ARRAYBLK_SIZE)
			tommy_arrayblk_reserve(array, size);
	}
}

/**
 * Changes the initialized size, preserving the common prefix.
 * New elements are initialized to zero.
 * Reducing the size does not release allocated capacity.
 * \param array The array.
 * \param size New size of the array.
 */
TOMMY_API void tommy_arrayblk_resize(tommy_arrayblk* array, tommy_size_t size);

/**
 * Removes all elements, preserving the allocated capacity.
 * Call tommy_arrayblk_shrink() after clearing to restore the initial capacity.
 * Pointed-to elements are not freed.
 * The array remains initialized and can be reused immediately.
 * \param array The array.
 */
tommy_inline void tommy_arrayblk_clear(tommy_arrayblk* array)
{
	tommy_arrayblk_resize(array, 0);
}

/**
 * Releases unused allocated memory.
 * Preserves the size, values, and addresses of existing elements.
 * \param array The array.
 */
TOMMY_API void tommy_arrayblk_shrink(tommy_arrayblk* array);

/**
 * Gets a reference to the element at the specified position.
 * You must be sure that space for this position is already
 * allocated by calling tommy_arrayblk_grow().
 * \param array The array.
 * \param pos Position of the element.
 * \return Pointer to the slot at the specified position.
 */
tommy_inline void** tommy_arrayblk_ref(tommy_arrayblk* array, tommy_size_t pos)
{
	assert(pos < array->count);

	return &array->block[pos / TOMMY_ARRAYBLK_SIZE][pos % TOMMY_ARRAYBLK_SIZE];
}

/**
 * Sets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated by calling tommy_arrayblk_grow().
 * \param array The array.
 * \param pos Position of the element.
 * \param element Element to set.
 */
tommy_inline void tommy_arrayblk_set(tommy_arrayblk* array, tommy_size_t pos, void* element)
{
	*tommy_arrayblk_ref(array, pos) = element;
}

/**
 * Gets the element at the specified position.
 * You must be sure that space for this position is already
 * allocated by calling tommy_arrayblk_grow().
 * \param array The array.
 * \param pos Position of the element.
 * \return The element at the specified position.
 */
tommy_inline void* tommy_arrayblk_get(tommy_arrayblk* array, tommy_size_t pos)
{
	return *tommy_arrayblk_ref(array, pos);
}

/**
 * Gets the last element.
 * The array must not be empty.
 * \param array The array.
 * \return The last element.
 */
tommy_inline void* tommy_arrayblk_tail(tommy_arrayblk* array)
{
	assert(array->count != 0);
	return tommy_arrayblk_get(array, array->count - 1);
}

/**
 * Grows and inserts a new element at the end of the array.
 * \param array The array.
 * \param element Element to insert.
 */
tommy_inline void tommy_arrayblk_insert_tail(tommy_arrayblk* array, void* element)
{
	tommy_size_t pos = array->count;

	++array->count;
	if (array->count > array->block_count * TOMMY_ARRAYBLK_SIZE)
		tommy_arrayblk_reserve(array, array->count);

	tommy_arrayblk_set(array, pos, element);
}

/**
 * Removes and returns the last element.
 * The array must not be empty.
 * The removed slot is cleared, and allocated capacity is preserved.
 * \param array The array.
 * \return The removed element.
 */
tommy_inline void* tommy_arrayblk_remove_tail(tommy_arrayblk* array)
{
	assert(array->count != 0);

	tommy_size_t pos = array->count - 1;
	void** ptr = tommy_arrayblk_ref(array, pos);
	void* element = *ptr;

	/* keep unused slots zero so growing within capacity needs no initialization */
	*ptr = 0;
	array->count = pos;

	return element;
}

/**
 * Checks whether the array is empty.
 * \param array The array.
 * \return Nonzero if the array is empty, 0 otherwise.
 */
tommy_inline tommy_bool_t tommy_arrayblk_empty(const tommy_arrayblk* array)
{
	return array->count == 0;
}

/**
 * Gets the initialized size of the array.
 * \param array The array.
 * \return The initialized size of the array.
 */
tommy_inline tommy_size_t tommy_arrayblk_size(const tommy_arrayblk* array)
{
	return array->count;
}

/**
 * Gets the number of elements that fit without further allocation.
 * \param array The array.
 * \return The capacity of the array.
 */
tommy_inline tommy_size_t tommy_arrayblk_capacity(const tommy_arrayblk* array)
{
	return array->block_count * TOMMY_ARRAYBLK_SIZE;
}

/**
 * Exchanges two initialized arrays without copying their elements.
 * Element references remain valid and belong to the other array.
 * Passing the same array twice has no effect.
 * \param first The first array.
 * \param second The second array.
 * \note This operation is O(1).
 */
tommy_inline void tommy_arrayblk_swap(tommy_arrayblk* first, tommy_arrayblk* second)
{
	tommy_arrayblk tmp = *first;
	*first = *second;
	*second = tmp;
}

/**
 * Calls the specified function for each element in the array.
 *
 * You cannot add or remove elements, nor change the size of the array,
 * from inside the callback.
 *
 * If the callback frees the elements, the array retains dangling
 * pointers. Call tommy_arrayblk_clear() before reusing it, or
 * tommy_arrayblk_done() to release its storage.
 * \param array The array.
 * \param func Function to call with each element.
 */
TOMMY_API void tommy_arrayblk_foreach(tommy_arrayblk* array, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the array.
 *
 * \param array The array.
 * \param func Function to call with each element.
 * \param arg Argument to pass to the function.
 */
TOMMY_API void tommy_arrayblk_foreach_arg(tommy_arrayblk* array, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the size of allocated memory.
 * \param array The array.
 * \return Total memory allocated by the array, in bytes.
 */
TOMMY_API tommy_size_t tommy_arrayblk_memory_usage(const tommy_arrayblk* array);

#endif

