// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommyhashtbl.h"
#include "tommylist.h"

#include <assert.h> /* for assert */
#include <string.h> /* for memset */

/******************************************************************************/
/* hashtable */

TOMMY_API void tommy_hashtable_init(tommy_hashtable* hashtable, tommy_size_t bucket_max)
{
	if (bucket_max < 16)
		bucket_max = 16;
	else
		bucket_max = tommy_roundup_pow2(bucket_max);

	/* ensure bucket rounding and allocation size cannot overflow */
	assert(bucket_max != 0 && bucket_max <= TOMMY_SIZE_MAX / sizeof(tommy_hashtable_node*));

	hashtable->bucket_max = bucket_max;
	hashtable->bucket_mask = hashtable->bucket_max - 1;

	/* let calloc defer memory initialization when supported by the allocator */
	hashtable->bucket = tommy_cast(tommy_hashtable_node**, tommy_calloc(hashtable->bucket_max, sizeof(tommy_hashtable_node*)));

	hashtable->count = 0;
}

TOMMY_API void tommy_hashtable_done(tommy_hashtable* hashtable)
{
	tommy_free(hashtable->bucket);
}

TOMMY_API void tommy_hashtable_clear(tommy_hashtable* hashtable)
{
	memset(hashtable->bucket, 0, hashtable->bucket_max * sizeof(tommy_hashtable_node*));
	hashtable->count = 0;
}

TOMMY_API void tommy_hashtable_insert(tommy_hashtable* hashtable, tommy_hashtable_node* node, void* data, tommy_hash_t hash)
{
	tommy_size_t pos = hash & hashtable->bucket_mask;

	tommy_list_insert_tail(&hashtable->bucket[pos], node, data);

	node->index = hash;

	++hashtable->count;
}

TOMMY_API void* tommy_hashtable_insert_unique(tommy_hashtable* hashtable, tommy_hashtable_node* node, void* data, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashtable_node** bucket = &hashtable->bucket[hash & hashtable->bucket_mask];
	tommy_hashtable_node* i = *bucket;

	while (i) {
		/* compare keys only for matching full hashes, as the bucket may contain different hashes */
		if (i->index == hash && cmp(cmp_arg, i->data) == 0)
			return i->data;
		i = i->next;
	}

	tommy_list_insert_tail(bucket, node, data);

	node->index = hash;

	++hashtable->count;

	return data;
}

TOMMY_API void tommy_hashtable_rehash_existing(tommy_hashtable* hashtable, tommy_hashtable_node* node, tommy_hash_t hash)
{
	if (node->index == hash)
		return;

	/* unlink using the stored hash, without invoking the resize policy. */
	tommy_list_remove_existing(&hashtable->bucket[node->index & hashtable->bucket_mask], node);
	node->index = hash;
	tommy_list_insert_tail(&hashtable->bucket[hash & hashtable->bucket_mask], node, node->data);
}

TOMMY_API void* tommy_hashtable_remove_existing(tommy_hashtable* hashtable, tommy_hashtable_node* node)
{
	tommy_size_t pos = node->index & hashtable->bucket_mask;

	tommy_list_remove_existing(&hashtable->bucket[pos], node);

	--hashtable->count;

	return node->data;
}

TOMMY_API void* tommy_hashtable_remove(tommy_hashtable* hashtable, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_size_t pos = hash & hashtable->bucket_mask;
	tommy_hashtable_node* node = hashtable->bucket[pos];

	while (node) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (node->index == hash && cmp(cmp_arg, node->data) == 0) {
			tommy_list_remove_existing(&hashtable->bucket[pos], node);

			--hashtable->count;

			return node->data;
		}
		node = node->next;
	}

	return 0;
}

TOMMY_API void tommy_hashtable_foreach(tommy_hashtable* hashtable, tommy_foreach_func* func)
{
	tommy_size_t bucket_max = hashtable->bucket_max;
	tommy_hashtable_node** bucket = hashtable->bucket;

	for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
		tommy_hashtable_node* node = bucket[pos];

		while (node) {
			void* data = node->data;
			node = node->next;
			func(data);
		}
	}
}

TOMMY_API void tommy_hashtable_foreach_arg(tommy_hashtable* hashtable, tommy_foreach_arg_func* func, void* arg)
{
	tommy_size_t bucket_max = hashtable->bucket_max;
	tommy_hashtable_node** bucket = hashtable->bucket;

	for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
		tommy_hashtable_node* node = bucket[pos];

		while (node) {
			void* data = node->data;
			node = node->next;
			func(arg, data);
		}
	}
}

TOMMY_API tommy_size_t tommy_hashtable_memory_usage(const tommy_hashtable* hashtable)
{
	return hashtable->bucket_max * (tommy_size_t)sizeof(hashtable->bucket[0])
	       + tommy_hashtable_count(hashtable) * (tommy_size_t)sizeof(tommy_hashtable_node);
}

TOMMY_API void tommy_hashtable_to_list(tommy_hashtable* hashtable, tommy_list* list)
{
	tommy_builder builder;
	tommy_node* builder_tail = tommy_builder_init(&builder);

	/* clear buckets as they are transferred to avoid a second scan */
	for (tommy_size_t pos = 0; pos < hashtable->bucket_max; ++pos) {
		tommy_node* head = hashtable->bucket[pos];
		if (head) {
			tommy_node* tail = head->prev;
			builder_tail = tommy_builder_concat(builder_tail, head, tail);
		}
		hashtable->bucket[pos] = 0;
	}

	tommy_list_concat_builder(list, &builder, builder_tail);
	hashtable->count = 0;
}

/**
 * Finds the first or last node starting at an active bucket.
 *
 * Scans buckets in sequential order until a non-empty bucket is found,
 * then returns its head (for forward) or tail (for reverse).
 */
tommy_inline tommy_hashtable_node* tommy_imp_hashtable_limit(tommy_hashtable* hashtable, tommy_size_t pos, tommy_bool_t reverse)
{
	if (reverse) {
		while (pos < hashtable->bucket_max) {
			if (hashtable->bucket[pos])
				return hashtable->bucket[pos]->prev;
			if (!pos)
				break;
			--pos;
		}
	} else {
		while (pos < hashtable->bucket_max) {
			if (hashtable->bucket[pos])
				return hashtable->bucket[pos];
			++pos;
		}
	}
	return 0;
}

TOMMY_API tommy_hashtable_node* tommy_hashtable_head(tommy_hashtable* hashtable)
{
	/* start forward traversal at the first bucket */
	return tommy_imp_hashtable_limit(hashtable, 0, 0);
}

TOMMY_API tommy_hashtable_node* tommy_hashtable_tail(tommy_hashtable* hashtable)
{
	/* start reverse traversal at the last bucket */
	return tommy_imp_hashtable_limit(hashtable, hashtable->bucket_max - 1, 1);
}

TOMMY_API tommy_hashtable_node* tommy_hashtable_next(tommy_hashtable* hashtable, tommy_hashtable_node* node)
{
	if (node->next)
		return node->next;

	/* bucket exhausted: advance to the next non-empty bucket */
	tommy_size_t pos = node->index & hashtable->bucket_mask;
	return tommy_imp_hashtable_limit(hashtable, pos + 1, 0);
}

TOMMY_API tommy_hashtable_node* tommy_hashtable_prev(tommy_hashtable* hashtable, tommy_hashtable_node* node)
{
	tommy_size_t pos = node->index & hashtable->bucket_mask;

	/* if not at the head of the current bucket, use the previous node */
	if (node != hashtable->bucket[pos])
		return node->prev;

	if (!pos)
		return 0;

	/* bucket exhausted: advance to the preceding non-empty bucket */
	return tommy_imp_hashtable_limit(hashtable, pos - 1, 1);
}

