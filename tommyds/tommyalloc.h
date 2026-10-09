// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Allocator of fixed-size blocks.
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
 * Allocator of fixed-size blocks.
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
 * \param align_size Minimum alignment requirement. Values smaller than sizeof(void*)
 * are raised to sizeof(void*). After this adjustment, align_size must be a multiple
 * of sizeof(void*) because allocator blocks store free-list pointers.
 */
TOMMY_API void tommy_allocator_init(tommy_allocator* alloc, tommy_size_t block_size, tommy_size_t align_size);

/**
 * Deinitializes the allocator.
 * It also releases all the allocated memory to the heap.
 * \param alloc Allocator to deinitialize.
 */
TOMMY_API void tommy_allocator_done(tommy_allocator* alloc);

/**
 * Releases all the allocated memory to the heap, including blocks still in use.
 * All previously allocated block pointers become invalid and must not be used or freed.
 * The allocator remains initialized and can be reused immediately with the same
 * block size and alignment. Its memory usage becomes zero.
 * Calling this function on an empty allocator has no effect.
 * \param alloc Allocator to clear.
 */
TOMMY_API void tommy_allocator_clear(tommy_allocator* alloc);

/**
 * Allocates a block.
 * \param alloc Allocator to use.
 * \return Pointer to the allocated block.
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
 * Gets the total size of allocated blocks currently in use.
 * Excludes free blocks retained by the allocator and segment overhead.
 * \param alloc Allocator to use.
 * \return Size of active blocks in bytes.
 */
TOMMY_API tommy_size_t tommy_allocator_memory_usage(const tommy_allocator* alloc);

#endif

