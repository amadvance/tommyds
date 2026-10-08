// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommyhashlin.h"
#include "tommylist.h"

#include <assert.h> /* for assert */
#include <string.h> /* for memset */

/******************************************************************************/
/* hashlin */

/**
 * Reallocation states.
 */
#define TOMMY_HASHLIN_STATE_STABLE 0
#define TOMMY_HASHLIN_STATE_GROW 1
#define TOMMY_HASHLIN_STATE_SHRINK 2

/**
 * Set the hashtable in stable state.
 */
tommy_inline void tommy_hashlin_stable(tommy_hashlin* hashlin)
{
	hashlin->state = TOMMY_HASHLIN_STATE_STABLE;

	/* setup low_mask/max/split to allow tommy_hashlin_bucket_ref() */
	/* and tommy_hashlin_foreach() to work regardless we are in stable state */
	hashlin->low_max = hashlin->bucket_max;
	hashlin->low_mask = hashlin->bucket_mask;
	hashlin->split = 0;
}

TOMMY_API void tommy_hashlin_init(tommy_hashlin* hashlin)
{
	/* fixed initial size */
	hashlin->bucket_bit = TOMMY_HASHLIN_BIT;
	hashlin->bucket_max = (tommy_size_t)1 << hashlin->bucket_bit;
	hashlin->bucket_mask = hashlin->bucket_max - 1;
	hashlin->bucket[0] = tommy_cast(tommy_hashlin_node**, tommy_calloc(hashlin->bucket_max, sizeof(tommy_hashlin_node*)));
	for (tommy_uint_t i = 1; i < TOMMY_HASHLIN_BIT; ++i)
		hashlin->bucket[i] = hashlin->bucket[0];

	/* stable state */
	tommy_hashlin_stable(hashlin);

	hashlin->count = 0;
}

TOMMY_API void tommy_hashlin_done(tommy_hashlin* hashlin)
{
	tommy_free(hashlin->bucket[0]);
	for (tommy_uint_t i = TOMMY_HASHLIN_BIT; i < hashlin->bucket_bit; ++i) {
		tommy_hashlin_node** segment = hashlin->bucket[i];
		tommy_free(&segment[(tommy_ptrdiff_t)1 << i]);
	}
}

TOMMY_API void tommy_hashlin_clear(tommy_hashlin* hashlin)
{
	/* clear the initial segment once, despite its multiple bucket aliases. */
	memset(hashlin->bucket[0], 0, ((tommy_size_t)1 << TOMMY_HASHLIN_BIT) * sizeof(tommy_hashlin_node*));
	for (tommy_uint_t i = TOMMY_HASHLIN_BIT; i < hashlin->bucket_bit; ++i) {
		tommy_hashlin_node** segment = hashlin->bucket[i];

		/* clear also the slots not yet initialized by a progressive grow. */
		memset(&segment[(tommy_ptrdiff_t)1 << i], 0, ((tommy_size_t)1 << i) * sizeof(tommy_hashlin_node*));
	}

	hashlin->count = 0;
	tommy_hashlin_stable(hashlin);
}

/**
 * Grow one step.
 */
tommy_inline void hashlin_grow_step(tommy_hashlin* hashlin)
{
	/* grow if more than 50% full */
	if (hashlin->state != TOMMY_HASHLIN_STATE_GROW
		&& hashlin->count > hashlin->bucket_max / 2
	) {
		/* if we are stable, setup a new grow state */
		/* otherwise continue with the already setup shrink one */
		/* but in backward direction */
		if (hashlin->state == TOMMY_HASHLIN_STATE_STABLE) {
			/* set the lower size */
			hashlin->low_max = hashlin->bucket_max;
			hashlin->low_mask = hashlin->bucket_mask;

			/* allocate the new vector using malloc() and not calloc() */
			/* because data is fully initialized in the split process */
			tommy_hashlin_node** segment = tommy_cast(tommy_hashlin_node**, tommy_malloc(hashlin->low_max * sizeof(tommy_hashlin_node*)));

			/* store it adjusting the offset */
			/* cast to ptrdiff_t to ensure to get a negative value */
			hashlin->bucket[hashlin->bucket_bit] = &segment[-(tommy_ptrdiff_t)hashlin->low_max];

			/* grow the hash size */
			++hashlin->bucket_bit;
			hashlin->bucket_max = (tommy_size_t)1 << hashlin->bucket_bit;
			hashlin->bucket_mask = hashlin->bucket_max - 1;

			/* start from the beginning going forward */
			hashlin->split = 0;
		}

		/* grow state */
		hashlin->state = TOMMY_HASHLIN_STATE_GROW;
	}

	/* if we are growing */
	if (hashlin->state == TOMMY_HASHLIN_STATE_GROW) {
		/* compute the split target required to finish the reallocation before the next resize */
		tommy_size_t split_target = 2 * hashlin->count;

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max < split_target) {
			/* get the low and high buckets */
			tommy_hashlin_node** split[2];
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);
			split[1] = tommy_hashlin_pos(hashlin, hashlin->split + hashlin->low_max);

			/* save the low bucket */
			tommy_hashlin_node* j = *split[0];

			/* reinitialize the buckets */
			*split[0] = 0;
			*split[1] = 0;

			/* the bit used to identify the bucket */
			tommy_size_t mask = hashlin->low_max;

			/* flush the bucket */
			while (j) {
				tommy_hashlin_node* j_next = j->next;
				tommy_size_t pos = (j->index & mask) != 0;
				if (*split[pos])
					tommy_list_insert_tail_not_empty(*split[pos], j);
				else
					tommy_list_insert_first(split[pos], j);
				j = j_next;
			}

			/* go forward */
			++hashlin->split;

			/* if we have finished, change the state */
			if (hashlin->split == hashlin->low_max) {
				/* go in stable mode */
				tommy_hashlin_stable(hashlin);
				break;
			}
		}
	}
}

/**
 * Shrink one step.
 */
tommy_inline void hashlin_shrink_step(tommy_hashlin* hashlin)
{
	/* shrink if less than 12.5% full */
	if (hashlin->state != TOMMY_HASHLIN_STATE_SHRINK
		&& hashlin->count < hashlin->bucket_max / 8
	) {
		/* avoid to shrink the first bucket */
		if (hashlin->bucket_bit > TOMMY_HASHLIN_BIT) {
			/* if we are stable, setup a new shrink state */
			/* otherwise continue with the already setup grow one */
			/* but in backward direction */
			if (hashlin->state == TOMMY_HASHLIN_STATE_STABLE) {
				/* set the lower size */
				hashlin->low_max = hashlin->bucket_max / 2;
				hashlin->low_mask = hashlin->bucket_mask / 2;

				/* start from the half going backward */
				hashlin->split = hashlin->low_max;
			}

			/* start reallocation */
			hashlin->state = TOMMY_HASHLIN_STATE_SHRINK;
		}
	}

	/* if we are shrinking */
	if (hashlin->state == TOMMY_HASHLIN_STATE_SHRINK) {
		/* compute the split target required to finish the reallocation before the next resize */
		tommy_size_t split_target = 8 * hashlin->count;

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max > split_target) {
			/* go backward position */
			--hashlin->split;

			/* get the low and high buckets */
			tommy_hashlin_node** split[2];
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);
			split[1] = tommy_hashlin_pos(hashlin, hashlin->split + hashlin->low_max);

			/* concat the high bucket into the low one */
			tommy_list_concat(split[0], split[1]);

			/* if we have finished, clean up and change the state */
			if (hashlin->split == 0) {
				/* shrink the hash size */
				--hashlin->bucket_bit;
				hashlin->bucket_max = (tommy_size_t)1 << hashlin->bucket_bit;
				hashlin->bucket_mask = hashlin->bucket_max - 1;

				/* free the last segment */
				tommy_hashlin_node** segment = hashlin->bucket[hashlin->bucket_bit];
				tommy_free(&segment[(tommy_ptrdiff_t)1 << hashlin->bucket_bit]);

				/* go in stable mode */
				tommy_hashlin_stable(hashlin);
				break;
			}
		}
	}
}

TOMMY_API void tommy_hashlin_insert(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_hash_t hash)
{
	tommy_list_insert_tail(tommy_hashlin_bucket_ref(hashlin, hash), node, data);

	node->index = hash;

	++hashlin->count;

	hashlin_grow_step(hashlin);
}

TOMMY_API void* tommy_hashlin_insert_unique(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	void* existing = tommy_hashlin_search(hashlin, cmp, cmp_arg, hash);
	if (existing)
		return existing;

	tommy_hashlin_insert(hashlin, node, data, hash);
	return data;
}

TOMMY_API void tommy_hashlin_rehash_existing(tommy_hashlin* hashlin, tommy_hashlin_node* node, tommy_hash_t hash)
{
	if (node->index == hash)
		return;

	/* unlink using the stored hash, without invoking the resize policy. */
	tommy_list_remove_existing(tommy_hashlin_bucket_ref(hashlin, node->index), node);
	node->index = hash;
	tommy_list_insert_tail(tommy_hashlin_bucket_ref(hashlin, hash), node, node->data);
}

TOMMY_API void* tommy_hashlin_remove_existing(tommy_hashlin* hashlin, tommy_hashlin_node* node)
{
	tommy_list_remove_existing(tommy_hashlin_bucket_ref(hashlin, node->index), node);

	--hashlin->count;

	hashlin_shrink_step(hashlin);

	return node->data;
}

TOMMY_API void* tommy_hashlin_remove(tommy_hashlin* hashlin, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashlin_node** let_ptr = tommy_hashlin_bucket_ref(hashlin, hash);
	tommy_hashlin_node* node = *let_ptr;

	while (node) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (node->index == hash && cmp(cmp_arg, node->data) == 0) {
			tommy_list_remove_existing(let_ptr, node);

			--hashlin->count;

			hashlin_shrink_step(hashlin);

			return node->data;
		}
		node = node->next;
	}

	return 0;
}

TOMMY_API void tommy_hashlin_foreach(tommy_hashlin* hashlin, tommy_foreach_func* func)
{
	/* number of valid buckets */
	tommy_size_t bucket_max = hashlin->low_max + hashlin->split;

	for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
		tommy_hashlin_node* node = *tommy_hashlin_pos(hashlin, pos);

		while (node) {
			void* data = node->data;
			node = node->next;
			func(data);
		}
	}
}

TOMMY_API void tommy_hashlin_foreach_arg(tommy_hashlin* hashlin, tommy_foreach_arg_func* func, void* arg)
{
	/* number of valid buckets */
	tommy_size_t bucket_max = hashlin->low_max + hashlin->split;

	for (tommy_size_t pos = 0; pos < bucket_max; ++pos) {
		tommy_hashlin_node* node = *tommy_hashlin_pos(hashlin, pos);

		while (node) {
			void* data = node->data;
			node = node->next;
			func(arg, data);
		}
	}
}

TOMMY_API tommy_size_t tommy_hashlin_memory_usage(const tommy_hashlin* hashlin)
{
	return hashlin->bucket_max * (tommy_size_t)sizeof(hashlin->bucket[0][0])
	       + hashlin->count * (tommy_size_t)sizeof(tommy_hashlin_node);
}

TOMMY_API void tommy_hashlin_to_list(tommy_hashlin* hashlin, tommy_list* list)
{
	tommy_size_t bucket_max = hashlin->low_max + hashlin->split;

	/* inactive slots may be uninitialized or refer to nodes already merged. */
	for (tommy_size_t pos = 0; pos < bucket_max; ++pos)
		tommy_list_concat(list, tommy_hashlin_pos(hashlin, pos));

	/* clear all allocated slots before making the table stable and reusable. */
	tommy_hashlin_clear(hashlin);
}

