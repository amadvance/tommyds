// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Allocator of fixed size blocks.
 */

#ifndef __TOMMYALLOC_H
#define __TOMMYALLOC_H

#include "tommytypes.h"

/******************************************************************************/
/* allocator */

/** \internal
 * Allocator entry.
 */
struct tommy_allocator_entry_struct {
	struct tommy_allocator_entry_struct* next; /**< Pointer to the next entry. 0 for last. */
};
typedef struct tommy_allocator_entry_struct tommy_allocator_entry;

/**
 * Allocator of fixed size blocks.
 */
typedef struct tommy_allocator_struct {
	struct tommy_allocator_entry_struct* free_block; /**< List of free blocks. */
	struct tommy_allocator_entry_struct* used_segment; /**< List of allocated segments. */
	tommy_size_t block_size; /**< Block size. */
	tommy_size_t align_size; /**< Alignment size. */
	tommy_size_t count; /**< Number of allocated elements. */
} tommy_allocator;

/**
 * Initializes the allocator.
 * \param alloc Allocator to initialize.
 * \param block_size Size of the block to allocate.
 * \param align_size Minimum alignment requirement. No less than sizeof(void*).
 */
TOMMY_API void tommy_allocator_init(tommy_allocator* alloc, tommy_size_t block_size, tommy_size_t align_size);

/**
 * Deinitializes the allocator.
 * It also releases all the allocated memory to the heap.
 * \param alloc Allocator to deinitialize.
 */
TOMMY_API void tommy_allocator_done(tommy_allocator* alloc);

/**
 * Allocates a block.
 * \param alloc Allocator to use.
 */
TOMMY_API void* tommy_allocator_alloc(tommy_allocator* alloc);

/**
 * Deallocates a block.
 * You must use the same allocator used in the tommy_allocator_alloc() call.
 * \param alloc Allocator to use.
 * \param ptr Block to free.
 */
TOMMY_API void tommy_allocator_free(tommy_allocator* alloc, void* ptr);

/**
 * Gets the size of allocated memory.
 * \param alloc Allocator to use.
 */
TOMMY_API tommy_size_t tommy_allocator_memory_usage(tommy_allocator* alloc);

#endif
