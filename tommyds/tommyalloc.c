// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommyalloc.h"

#include <assert.h> /* for assert */

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

	/* blocks store free-list pointers and must preserve their alignment */
	assert(align_size % sizeof(void*) == 0);

	/* ensure the minimum block size and alignment */
	if (block_size < align_size)
		block_size = align_size;
	else if (block_size % align_size != 0)
		block_size += align_size - block_size % align_size;

	alloc->block_size = block_size;
	alloc->align_size = align_size;

	alloc->count = 0;
	alloc->free_block = 0;
	alloc->used_segment = 0;
}

TOMMY_API void tommy_allocator_done(tommy_allocator* alloc)
{
	tommy_allocator_entry* block = alloc->used_segment;

	while (block) {
		tommy_allocator_entry* block_next = block->next;
		tommy_free(block);
		block = block_next;
	}
}

TOMMY_API void tommy_allocator_clear(tommy_allocator* alloc)
{
	tommy_allocator_done(alloc);

	alloc->count = 0;
	alloc->free_block = 0;
	alloc->used_segment = 0;
}

TOMMY_API void* tommy_allocator_alloc(tommy_allocator* alloc)
{
	/* if no free block available */
	if (!alloc->free_block) {
		/* default allocation size */
		tommy_size_t size = TOMMY_ALLOCATOR_BLOCK_SIZE;

		/* ensure that we can allocate at least one block */
		if (size < sizeof(tommy_allocator_entry) + alloc->align_size + alloc->block_size)
			size = sizeof(tommy_allocator_entry) + alloc->align_size + alloc->block_size;

		char* data = tommy_cast(char*, tommy_malloc(size));
		tommy_allocator_entry* segment = (tommy_allocator_entry*)data;

		/* put in the segment list */
		segment->next = alloc->used_segment;
		alloc->used_segment = segment;
		data += sizeof(tommy_allocator_entry);

		/* exclude the header so every free block fits within the segment */
		size -= sizeof(tommy_allocator_entry);

		/* align if not aligned */
		tommy_uintptr_t off = (tommy_uintptr_t)data;
		tommy_uintptr_t mis = off % alloc->align_size;
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
	void* ptr = alloc->free_block;
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

TOMMY_API tommy_size_t tommy_allocator_memory_usage(const tommy_allocator* alloc)
{
	return alloc->count * (tommy_size_t)alloc->block_size;
}

