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

void tommy_hashopen_init(tommy_hashopen* hashopen)
{
	/* fixed initial size */
	hashopen->bucket_bit = TOMMY_HASHOPEN_BIT;
	hashopen->bucket_max = 1 << hashopen->bucket_bit;
	hashopen->bucket_mask = hashopen->bucket_max - 1;
	hashopen->bucket = tommy_cast(tommy_hashopen_pos*, tommy_calloc(hashopen->bucket_max, sizeof(tommy_hashopen_pos)));
	hashopen->bucket_last = hashopen->bucket + hashopen->bucket_max;

	hashopen->count = 0;
	hashopen->filled_count = 0;
	hashopen->deleted_count = 0;
}

void tommy_hashopen_done(tommy_hashopen* hashopen)
{
	tommy_free(hashopen->bucket);
}

/**
 * Resize the bucket vector.
 */
static void tommy_hashopen_resize(tommy_hashopen* hashopen, tommy_uint_t new_bucket_bit)
{
	tommy_count_t bucket_max;
	tommy_count_t new_bucket_max;
	tommy_count_t new_bucket_mask;
	tommy_hashopen_pos* new_bucket;
	tommy_hashopen_pos* new_bucket_last;
	tommy_count_t i;

	bucket_max = hashopen->bucket_max;

	new_bucket_max = 1 << new_bucket_bit;
	new_bucket_mask = new_bucket_max - 1;
	new_bucket = tommy_cast(tommy_hashopen_pos*, tommy_calloc(new_bucket_max, sizeof(tommy_hashopen_pos)));
	new_bucket_last = new_bucket + new_bucket_max;

	/* reset the countes */
	hashopen->filled_count = 0;
	hashopen->deleted_count = 0;

	/* reinsert all the elements */
	for(i=0;i<bucket_max;++i) {
		tommy_hashopen_pos* j = &hashopen->bucket[i];
		if (j->ptr != TOMMY_HASHOPEN_EMPTY && j->ptr != TOMMY_HASHOPEN_DELETED) {
			tommy_hashopen_node* k = j->ptr;
			while (k) {
				tommy_hashopen_node* k_next = k->next;
				tommy_hashopen_pos* n = &new_bucket[k->key & new_bucket_mask];

				while (1) {
					/* if the bucket is empty, the element is missing */
					if (n->ptr == TOMMY_HASHOPEN_EMPTY) {
						tommy_list_insert_first(&n->ptr, k);
						n->hash = k->key;
						++hashopen->filled_count;
						break;
					}

					/* if the hash matches, it's the right one */
					if (n->hash == k->key) {
						tommy_list_insert_tail_not_empty(n->ptr, k);
						break;
					}

					/* go to the next bucket */;
					++n;
					if (n == new_bucket_last)
						n = new_bucket;
				}

				k = k_next;
			}
		}
	}

	tommy_free(hashopen->bucket);

	/* setup */
	hashopen->bucket_bit = new_bucket_bit;
	hashopen->bucket_max = new_bucket_max;
	hashopen->bucket_mask = new_bucket_mask;
	hashopen->bucket = new_bucket;
	hashopen->bucket_last = hashopen->bucket + hashopen->bucket_max;
}

/**
 * Grow.
 */
tommy_inline void hashopen_grow_step(tommy_hashopen* hashopen)
{
	/* grow if more than 50% full */
	if (hashopen->filled_count + hashopen->deleted_count >= hashopen->bucket_max / 2) {
		tommy_hashopen_resize(hashopen, hashopen->bucket_bit + 1);
	}
}

/**
 * Shrink.
 */
tommy_inline void hashopen_shrink_step(tommy_hashopen* hashopen)
{
	/* shrink if less than 12.5% full */
	if (hashopen->filled_count <= hashopen->bucket_max / 8 && hashopen->bucket_bit > TOMMY_HASHOPEN_BIT) {
		tommy_hashopen_resize(hashopen, hashopen->bucket_bit - 1);
	}
}

void tommy_hashopen_insert(tommy_hashopen* hashopen, tommy_hashopen_node* node, void* data, tommy_hash_t hash)
{
	tommy_hashopen_pos* i = tommy_hashopen_bucket(hashopen, hash);

	/* if the bucket is empty */
	if (i->ptr == TOMMY_HASHOPEN_EMPTY) {
		tommy_list_insert_first(&i->ptr, node);
		i->hash = hash;
		++hashopen->filled_count;
	} else {
		/* otherwise it already contains elements with the correct hash */
		tommy_list_insert_tail_not_empty(i->ptr, node);
	}

	node->data = data;
	node->key = hash;

	++hashopen->count;

	hashopen_grow_step(hashopen);
}

void* tommy_hashopen_remove_existing(tommy_hashopen* hashopen, tommy_hashopen_node* node)
{
	tommy_hashopen_pos* i = tommy_hashopen_bucket(hashopen, node->key);

	/* we don't check for empty bucket, because we know that it's an existing element */
	tommy_list_remove_existing(&i->ptr, node);

	/* if it's empty */
	if (!i->ptr) {
		/* set it as deleted */
		i->ptr = TOMMY_HASHOPEN_DELETED;
		--hashopen->filled_count;
		++hashopen->deleted_count;
	}

	--hashopen->count;

	hashopen_shrink_step(hashopen);

	return node->data;
}

void* tommy_hashopen_remove(tommy_hashopen* hashopen, tommy_compare_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashopen_pos* i = tommy_hashopen_bucket(hashopen, hash);
	tommy_hashopen_node* j;

	/* if empty bucket, or different hash, it's missing */
	if (i->ptr == TOMMY_HASHOPEN_EMPTY || i->hash != hash)
		return 0;

	/* for sure we have at least one object */
	j = i->ptr;
	do {
		if (cmp(cmp_arg, j->data) == 0) {
			tommy_list_remove_existing(&i->ptr, j);

			/* if it's empty */
			if (!i->ptr) {
				/* set it as deleted */
				i->ptr = TOMMY_HASHOPEN_DELETED;
				--hashopen->filled_count;
				++hashopen->deleted_count;
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

