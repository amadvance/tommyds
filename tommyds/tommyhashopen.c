/*
 * Copyright (c) 2010, Andrea Mazzoleni. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "tommyhashopen.h"
#include "tommylist.h"

/******************************************************************************/
/* hashopen */

/**
 * Allocate a new table.
 *
 * The alignment is choosen to exactly fit a typical cache line of 64 bytes.
 */
static void tommy_hashopen_alloc(tommy_hashopen* hashopen, tommy_uint_t bit)
{
	size_t entries_for_cache_line;
	tommy_uintptr_t ptr;

	assert(64 % sizeof(tommy_hashopen_pos) == 0);

	/* number of entry for each cache line */
	entries_for_cache_line = 64 / sizeof(tommy_hashopen_pos);

	hashopen->bucket_bit = bit;
	hashopen->bucket_max = 1 << hashopen->bucket_bit;
	hashopen->bucket_mask = hashopen->bucket_max - 1;
	hashopen->bucket_alloc = tommy_calloc(hashopen->bucket_max + entries_for_cache_line, sizeof(tommy_hashopen_pos));

	/* setup a bit mask always getting the first entry in the cache line */
	/* clearing the lower bits */
	hashopen->bucket_mask_cache = hashopen->bucket_mask & ~(entries_for_cache_line - 1);

	/* align the bucket pointer at the cache line */
	ptr = (tommy_uintptr_t)hashopen->bucket_alloc;
	ptr = (ptr + 63) & ~63;
	hashopen->bucket = (tommy_hashopen_pos*)ptr;
}

void tommy_hashopen_init(tommy_hashopen* hashopen)
{
	/* fixed initial size */
	hashopen->bucket_bit = TOMMY_HASHOPEN_BIT;

	tommy_hashopen_alloc(hashopen, TOMMY_HASHOPEN_BIT);

	hashopen->count = 0;
	hashopen->filled_count = 0;
}

void tommy_hashopen_done(tommy_hashopen* hashopen)
{
	tommy_free(hashopen->bucket_alloc);
}

/**
 * Resize the bucket vector.
 */
static void tommy_hashopen_resize(tommy_hashopen* hashopen, tommy_uint_t new_bucket_bit)
{
	tommy_count_t old_bucket_max;
	tommy_hashopen_pos* old_bucket;
	void* old_bucket_alloc;
	tommy_count_t i;

	/* save the old table */
	old_bucket_max = hashopen->bucket_max;
	old_bucket = hashopen->bucket;
	old_bucket_alloc = hashopen->bucket_alloc;

	/* allocate the new table */
	tommy_hashopen_alloc(hashopen, new_bucket_bit);

	/* reinsert all the elements */
	for (i = 0; i < old_bucket_max; ++i) {
		tommy_hashopen_pos* old_pos = &old_bucket[i];

		if (old_pos->ptr != TOMMY_HASHOPEN_EMPTY) {
			tommy_hashopen_pos* pos;
			tommy_count_t j = old_pos->hash & hashopen->bucket_mask_cache;

			/* search the first empty bucket */
			/* we don't consider the same hash, because it cannot yet */
			/* exists an equal hash. */
			while (1) {
				pos = &hashopen->bucket[j];

				if (pos->ptr == TOMMY_HASHOPEN_EMPTY)
					break;

				/* go to the next bucket */
				j = (j + 1) & hashopen->bucket_mask;
			}

			/* assign the new position */
			*pos = *old_pos;
		}
	}

	tommy_free(old_bucket_alloc);
}

/**
 * Grow.
 */
tommy_inline void hashopen_grow_step(tommy_hashopen* hashopen)
{
	/* grow if more than 50% full */
	if (hashopen->filled_count >= hashopen->bucket_max / 2)
		tommy_hashopen_resize(hashopen, hashopen->bucket_bit + 1);
}

/**
 * Shrink.
 */
tommy_inline void hashopen_shrink_step(tommy_hashopen* hashopen)
{
	/* shrink if less than 12.5% full */
	if (hashopen->filled_count <= hashopen->bucket_max / 8 && hashopen->bucket_bit > TOMMY_HASHOPEN_BIT)
		tommy_hashopen_resize(hashopen, hashopen->bucket_bit - 1);
}

void tommy_hashopen_insert(tommy_hashopen* hashopen, tommy_hashopen_node* node, void* data, tommy_hash_t hash)
{
	tommy_hashopen_pos* pos = tommy_hashopen_bucket(hashopen, hash);

	/* if the bucket is empty or deleted */
	if (pos->ptr == TOMMY_HASHOPEN_EMPTY) {
		tommy_list_insert_first(&pos->ptr, node);
		pos->hash = hash;
		++hashopen->filled_count;
	} else {
		/* otherwise it already contains elements with the correct hash */
		tommy_list_insert_tail_not_empty(pos->ptr, node);
	}

	node->data = data;
	node->key = hash;

	++hashopen->count;

	hashopen_grow_step(hashopen);
}

/**
 * Refill the specified bucket shifting backward following elements.
 *
 * It's a linear search of the next empty bucket, with a shift of
 * all the elements that can be shifted.
 */
tommy_inline void tommy_hashopen_refill(tommy_hashopen* hashopen, tommy_hashopen_pos* refill_pos)
{
	tommy_count_t refill = refill_pos - hashopen->bucket;
	tommy_count_t candidate = (refill + 1) & hashopen->bucket_mask;
	while (1) {
		tommy_count_t target;

		/* candidate element */
		tommy_hashopen_pos* pos = &hashopen->bucket[candidate];

		/* if it's empty, we don't need to search more */
		if (pos->ptr == TOMMY_HASHOPEN_EMPTY) {
			/* set the bucket as empty */
			refill_pos->ptr = TOMMY_HASHOPEN_EMPTY;
			return;
		}

		/* get the target position where the candidate should go */
		target = pos->hash & hashopen->bucket_mask_cache;

		/* identify if candidate overflowed the table respect to refill */
		if (refill < candidate) {
			/* if the candidate can be moved */
			if (target <= refill || target > candidate) {
				/* move it */
				*refill_pos = *pos;

				/* set the new refill position */
				refill_pos = pos;
				refill = candidate;
			}
		} else {
			/* we had an overflow on the table size */

			/* if the candidate can be moved */
			if (target <= refill && target > candidate) {
				/* move it */
				*refill_pos = *pos;

				/* set the new refill position */
				refill_pos = pos;
				refill = candidate;
			}
		}

		/* check the next candidate */
		candidate = (candidate + 1) & hashopen->bucket_mask;
	}
}

void* tommy_hashopen_remove_existing(tommy_hashopen* hashopen, tommy_hashopen_node* node)
{
	tommy_hashopen_pos* pos = tommy_hashopen_bucket(hashopen, node->key);

	/* we don't check for empty bucket, because we know that it's an existing element */
	tommy_list_remove_existing(&pos->ptr, node);

	/* if it's empty */
	if (!pos->ptr) {
		tommy_hashopen_refill(hashopen, pos);
		--hashopen->filled_count;
	}

	--hashopen->count;

	hashopen_shrink_step(hashopen);

	return node->data;
}

void* tommy_hashopen_remove(tommy_hashopen* hashopen, tommy_compare_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashopen_pos* pos = tommy_hashopen_bucket(hashopen, hash);
	tommy_hashopen_node* j;

	/* if empty bucket, it's missing */
	if (pos->ptr == TOMMY_HASHOPEN_EMPTY)
		return 0;

	/* for sure we have at least one object */
	j = pos->ptr;
	do {
		if (cmp(cmp_arg, j->data) == 0) {
			tommy_list_remove_existing(&pos->ptr, j);

			/* if it's empty */
			if (!pos->ptr) {
				tommy_hashopen_refill(hashopen, pos);
				--hashopen->filled_count;
			}

			--hashopen->count;

			hashopen_shrink_step(hashopen);

			return j->data;
		}
		j = j->next;
	} while (j);

	return 0;
}

tommy_size_t tommy_hashopen_memory_usage(tommy_hashopen* hashopen)
{
	return hashopen->bucket_max * (tommy_size_t)sizeof(hashopen->bucket[0])
	       + tommy_hashopen_count(hashopen) * (tommy_size_t)sizeof(tommy_hashopen_node);
}

