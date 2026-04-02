// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommyalloc.h"

/******************************************************************************/
/* allocator */

/**
 * Basic allocation segment.
 * Smaller of a memory page, to allow also a little heap overread.
 * The heap manager may put it in a single memory page.
 */
#define TOMMY_ALLOCATOR_BLOCK_SIZE (4096 - 64)

TOMMY_API void tommy_allocator_init(tommy_allocator* alloc, tommy_size_t block_size, tommy_size_t align_size)
{
	/* setup the minimal alignment */
	if (align_size < sizeof(void*))
		align_size = sizeof(void*);

	/* ensure that the block_size keeps the alignment */
	if (block_size % align_size != 0)
		block_size += align_size - block_size % align_size;

	alloc->block_size = block_size;
	alloc->align_size = align_size;

	alloc->count = 0;
	alloc->free_block = 0;
	alloc->used_segment = 0;
}

/**
 * Reset the allocator and free all.
 */
static void allocator_reset(tommy_allocator* alloc)
{
	tommy_allocator_entry* block = alloc->used_segment;

	while (block) {
		tommy_allocator_entry* block_next = block->next;
		tommy_free(block);
		block = block_next;
	}

	alloc->count = 0;
	alloc->free_block = 0;
	alloc->used_segment = 0;
}

TOMMY_API void tommy_allocator_done(tommy_allocator* alloc)
{
	allocator_reset(alloc);
}

TOMMY_API void* tommy_allocator_alloc(tommy_allocator* alloc)
{
	void* ptr;

	/* if no free block available */
	if (!alloc->free_block) {
		tommy_uintptr_t off, mis;
		tommy_size_t size;
		char* data;
		tommy_allocator_entry* segment;

		/* default allocation size */
		size = TOMMY_ALLOCATOR_BLOCK_SIZE;

		/* ensure that we can allocate at least one block */
		if (size < sizeof(tommy_allocator_entry) + alloc->align_size + alloc->block_size)
			size = sizeof(tommy_allocator_entry) + alloc->align_size + alloc->block_size;

		data = tommy_cast(char*, tommy_malloc(size));
		segment = (tommy_allocator_entry*)data;

		/* put in the segment list */
		segment->next = alloc->used_segment;
		alloc->used_segment = segment;
		data += sizeof(tommy_allocator_entry);

		/* align if not aligned */
		off = (tommy_uintptr_t)data;
		mis = off % alloc->align_size;
		if (mis != 0) {
			data += alloc->align_size - mis;
			size -= alloc->align_size - mis;
		}

		/* insert in free list */
		do {
			tommy_allocator_entry* free_block = (tommy_allocator_entry*)data;
			free_block->next = alloc->free_block;
			alloc->free_block = free_block;

			data += alloc->block_size;
			size -= alloc->block_size;
		} while (size >= alloc->block_size);
	}

	/* remove one from the free list */
	ptr = alloc->free_block;
	alloc->free_block = alloc->free_block->next;

	++alloc->count;

	return ptr;
}

TOMMY_API void tommy_allocator_free(tommy_allocator* alloc, void* ptr)
{
	tommy_allocator_entry* free_block = tommy_cast(tommy_allocator_entry*, ptr);

	/* put it in the free list */
	free_block->next = alloc->free_block;
	alloc->free_block = free_block;

	--alloc->count;
}

TOMMY_API tommy_size_t tommy_allocator_memory_usage(tommy_allocator* alloc)
{
	return alloc->count * (tommy_size_t)alloc->block_size;
}

