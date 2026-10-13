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
tommy_inline void tommy_imp_hashlin_stable(tommy_hashlin* hashlin)
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
	tommy_imp_hashlin_stable(hashlin);

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
		tommy_free(&segment[(tommy_ptrdiff_t)1 << i]);
	}

	hashlin->bucket_bit = TOMMY_HASHLIN_BIT;
	hashlin->bucket_max = (tommy_size_t)1 << hashlin->bucket_bit;
	hashlin->bucket_mask = hashlin->bucket_max - 1;
	hashlin->count = 0;
	tommy_imp_hashlin_stable(hashlin);
}

/**
 * Grow one step.
 */
tommy_inline void tommy_imp_hashlin_grow_step(tommy_hashlin* hashlin)
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

		/* high buckets share the last segment and low_max offset, both fixed between iterations */
		tommy_hashlin_node** high = &hashlin->bucket[hashlin->bucket_bit - 1][hashlin->low_max];

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max < split_target) {
			/* get the low and high buckets */
			tommy_hashlin_node** split[2];
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);
			split[1] = &high[hashlin->split];

			/* save the low bucket */
			tommy_hashlin_node* j = *split[0];

			/* reinitialize the buckets */
			*split[0] = 0;
			*split[1] = 0;

			/* the bit used to identify the bucket */
			tommy_size_t mask = hashlin->low_max;

			/* flush the bucket */
			if (j && !j->next) {
				/* both destinations are empty, and singleton links are already valid */
				*split[(j->index & mask) != 0] = j;
			} else {
				while (j) {
					tommy_hashlin_node* j_next = j->next;
					tommy_size_t pos = (j->index & mask) != 0;
					if (*split[pos])
						tommy_list_insert_tail_not_empty(*split[pos], j);
					else
						tommy_list_insert_first(split[pos], j);
					j = j_next;
				}
			}

			/* go forward */
			++hashlin->split;

			/* if we have finished, change the state */
			if (hashlin->split == hashlin->low_max) {
				/* go in stable mode */
				tommy_imp_hashlin_stable(hashlin);
				break;
			}
		}
	}
}

/**
 * Shrink one step.
 */
tommy_inline void tommy_imp_hashlin_shrink_step(tommy_hashlin* hashlin)
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

		/* high buckets share the last segment and low_max offset, both fixed between iterations */
		tommy_hashlin_node** high = &hashlin->bucket[hashlin->bucket_bit - 1][hashlin->low_max];

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max > split_target) {
			/* go backward position */
			--hashlin->split;

			/* get the low and high buckets */
			tommy_hashlin_node** split[2];
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);
			split[1] = &high[hashlin->split];

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
				tommy_imp_hashlin_stable(hashlin);
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

	tommy_imp_hashlin_grow_step(hashlin);
}

TOMMY_API void* tommy_hashlin_insert_unique(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashlin_node** bucket = tommy_hashlin_bucket_ref(hashlin, hash);
	tommy_hashlin_node* i = *bucket;

	while (i) {
		/* compare keys only for matching full hashes, as the bucket may contain different hashes */
		if (i->index == hash && cmp(cmp_arg, i->data) == 0)
			return i->data;
		i = i->next;
	}

	/* insert before resizing, while the bucket selected for the search is still valid */
	tommy_list_insert_tail(bucket, node, data);

	node->index = hash;

	++hashlin->count;

	tommy_imp_hashlin_grow_step(hashlin);

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

	tommy_imp_hashlin_shrink_step(hashlin);

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

			tommy_imp_hashlin_shrink_step(hashlin);

			return node->data;
		}
		node = node->next;
	}

	return 0;
}

/**
 * Compares hashes in least-significant-bit-first order, with 0 before 1.
 *
 * The first differing bit decides the order: first precedes second when
 * second has a 1 there. Equal hashes compare false.
 */
tommy_inline tommy_bool_t tommy_imp_hashlin_before(tommy_hash_t first, tommy_hash_t second)
{
	/* mark the positions where the hashes differ using XOR */
	tommy_hash_t diff = first ^ second;

	/* isolate the lowest differing bit using two's complement */
	tommy_hash_t bit = diff & -diff;

	/* a 1 in second means first has 0 there; equal hashes yield bit == 0 */
	return (second & bit) != 0;
}

/**
 * Gets the adjacent active bucket in least-significant-bit-first order.
 * Returns bucket_max when there is no adjacent bucket.
 *
 * Traverses the virtual binary tree of buckets in bit-reversed order.
 *
 * For example, with 8 buckets the forward traversal order is:
 *   0, 4, 2, 6, 1, 5, 3, 7
 *
 * During a progressive resize to 12 buckets (split = 4, low_max = 8):
 *   0, 8, 4, 2, 10, 6, 1, 9, 5, 3, 11, 7
 *
 * Split child buckets (8, 10, 9, 11) immediately follow their parents (0, 2, 1, 3),
 * keeping the relative order of all other buckets unchanged.
 *
 * When resizing completes at 16 buckets:
 *   0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
 */
tommy_inline tommy_size_t tommy_imp_hashlin_step(tommy_hashlin* hashlin, tommy_size_t pos, tommy_bool_t reverse)
{
	/* a split bucket uses one more bit, placing its two children consecutively */
	tommy_size_t mask = (pos & hashlin->low_mask) < hashlin->split ? hashlin->bucket_mask : hashlin->low_mask;
	tommy_size_t bit = (mask + 1) / 2;

	/* propagate binary carry in bit-reversed order from MSB downwards */
	while (bit && ((pos & bit) != 0) != reverse) {
		pos ^= bit;
		bit >>= 1;
	}

	/* carry wrapped around the root of the tree: end of traversal */
	if (!bit)
		return hashlin->bucket_max;
	pos ^= bit;

	/* enter a deeper preceding bucket at its last child, or a following one at its first */
	if (reverse)
		pos |= ~mask;

	/* route to the split child bucket if active, otherwise use the parent */
	if ((pos & hashlin->low_mask) < hashlin->split)
		return pos & hashlin->bucket_mask;
	return pos & hashlin->low_mask;
}

/**
 * Finds the first or last node starting at an active bucket.
 *
 * Scans buckets in least-significant-bit-first order until a non-empty bucket
 * is found, then returns its extreme element (minimum for forward, maximum for reverse).
 */
tommy_inline tommy_hashlin_node* tommy_imp_hashlin_limit(tommy_hashlin* hashlin, tommy_size_t pos, tommy_bool_t reverse)
{
	while (pos != hashlin->bucket_max) {
		tommy_hashlin_node* best = 0;
		for (tommy_hashlin_node* i = *tommy_hashlin_pos(hashlin, pos); i; i = i->next) {
			/* equal hashes retain insertion order, so the reverse limit selects the last */
			if (reverse) {
				if (!best || !tommy_imp_hashlin_before(i->index, best->index))
					best = i;
			} else {
				if (!best || tommy_imp_hashlin_before(i->index, best->index))
					best = i;
			}
		}
		if (best)
			return best;
		pos = tommy_imp_hashlin_step(hashlin, pos, reverse);
	}
	return 0;
}

/**
 * Finds a neighbor using the current bucket layout and a resize-independent order.
 *
 * First searches for the closest neighbor within the current bucket.
 * If none exists, advances to the next non-empty active bucket in LSB order.
 */
tommy_inline tommy_hashlin_node* tommy_imp_hashlin_neighbor(tommy_hashlin* hashlin, tommy_hashlin_node* node, tommy_bool_t reverse)
{
	tommy_size_t pos = node->index & hashlin->low_mask;
	if (pos < hashlin->split)
		pos = node->index & hashlin->bucket_mask;
	tommy_hashlin_node* best = 0;
	tommy_bool_t passed = 0;

	/*
	 * Bucket links can change during split and merge. Compare full hashes instead
	 * of following those links as iteration order, so no remaining node moves
	 * behind the iterator. Equal hashes always stay in one bucket in insertion order.
	 */
	for (tommy_hashlin_node* i = *tommy_hashlin_pos(hashlin, pos); i; i = i->next) {
		if (i == node) {
			passed = 1;
		} else if (i->index == node->index) {
			/* for identical hashes, follow insertion order relative to the current node */
			if (reverse && !passed)
				best = i;
			else if (!reverse && passed)
				return i;
		} else if (reverse) {
			if (tommy_imp_hashlin_before(i->index, node->index)) {
				/* find the closest neighbor within the bucket in LSB order */
				if (!best || !tommy_imp_hashlin_before(i->index, best->index))
					best = i;
			}
		} else {
			if (tommy_imp_hashlin_before(node->index, i->index)) {
				/* find the closest neighbor within the bucket in LSB order */
				if (!best || tommy_imp_hashlin_before(i->index, best->index))
					best = i;
			}
		}
	}
	if (best)
		return best;

	/* bucket exhausted: advance to the next non-empty bucket in LSB order */
	return tommy_imp_hashlin_limit(hashlin, tommy_imp_hashlin_step(hashlin, pos, reverse), reverse);
}

TOMMY_API tommy_hashlin_node* tommy_hashlin_head(tommy_hashlin* hashlin)
{
	/* start forward traversal at bucket 0, which has all LSB bits at zero */
	return tommy_imp_hashlin_limit(hashlin, 0, 0);
}

TOMMY_API tommy_hashlin_node* tommy_hashlin_tail(tommy_hashlin* hashlin)
{
	/* split is always below low_max between API calls, so the all-ones low bucket is unsplit */
	return tommy_imp_hashlin_limit(hashlin, hashlin->low_mask, 1);
}

TOMMY_API tommy_hashlin_node* tommy_hashlin_next(tommy_hashlin* hashlin, tommy_hashlin_node* node)
{
	return tommy_imp_hashlin_neighbor(hashlin, node, 0);
}

TOMMY_API tommy_hashlin_node* tommy_hashlin_prev(tommy_hashlin* hashlin, tommy_hashlin_node* node)
{
	return tommy_imp_hashlin_neighbor(hashlin, node, 1);
}

TOMMY_API void tommy_hashlin_foreach(tommy_hashlin* hashlin, tommy_foreach_func* func)
{
	/* number of valid buckets */
	tommy_size_t bucket_max = hashlin->low_max + hashlin->split;
	tommy_size_t pos = 0;

	/* start at the last alias of the initial segment to visit it only once */
	for (tommy_uint_t i = TOMMY_HASHLIN_BIT - 1; i < hashlin->bucket_bit; ++i) {
		tommy_hashlin_node** bucket = hashlin->bucket[i];
		tommy_size_t end = (tommy_size_t)1 << (i + 1);

		/* the last segment can contain inactive or uninitialized slots */
		if (end > bucket_max)
			end = bucket_max;

		for (; pos < end; ++pos) {
			tommy_hashlin_node* node = bucket[pos];

			while (node) {
				void* data = node->data;
				node = node->next;
				func(data);
			}
		}
	}
}

TOMMY_API void tommy_hashlin_foreach_arg(tommy_hashlin* hashlin, tommy_foreach_arg_func* func, void* arg)
{
	/* number of valid buckets */
	tommy_size_t bucket_max = hashlin->low_max + hashlin->split;
	tommy_size_t pos = 0;

	/* start at the last alias of the initial segment to visit it only once */
	for (tommy_uint_t i = TOMMY_HASHLIN_BIT - 1; i < hashlin->bucket_bit; ++i) {
		tommy_hashlin_node** bucket = hashlin->bucket[i];
		tommy_size_t end = (tommy_size_t)1 << (i + 1);

		/* the last segment can contain inactive or uninitialized slots */
		if (end > bucket_max)
			end = bucket_max;

		for (; pos < end; ++pos) {
			tommy_hashlin_node* node = bucket[pos];

			while (node) {
				void* data = node->data;
				node = node->next;
				func(arg, data);
			}
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
	tommy_builder builder;
	tommy_node* builder_tail = tommy_builder_init(&builder);

	tommy_size_t active_max = hashlin->low_max + hashlin->split;
	tommy_size_t initial_max = (tommy_size_t)1 << TOMMY_HASHLIN_BIT;
	tommy_hashlin_node** bucket = hashlin->bucket[0];

	/*
	 * Visit the initial segment once, despite its multiple aliases.
	 *
	 * This segment starts at zero and is always fully active. Processing it
	 * separately lets the following loop use active_max - size, since each
	 * later segment starts at an offset equal to its size.
	 */
	for (tommy_size_t pos = 0; pos < initial_max; ++pos) {
		tommy_node* head = bucket[pos];
		if (head) {
			tommy_node* tail = head->prev;
			builder_tail = tommy_builder_concat(builder_tail, head, tail);
		}
		bucket[pos] = 0;
	}

	for (tommy_uint_t i = TOMMY_HASHLIN_BIT; i < hashlin->bucket_bit; ++i) {
		tommy_size_t size = (tommy_size_t)1 << i;
		bucket = &hashlin->bucket[i][size];

		/* only the last allocated segment can be partially active */
		tommy_size_t active = active_max - size;
		if (active > size)
			active = size;

		for (tommy_size_t pos = 0; pos < active; ++pos) {
			tommy_node* head = bucket[pos];
			if (head) {
				tommy_node* tail = head->prev;
				builder_tail = tommy_builder_concat(builder_tail, head, tail);
			}
		}

		/* free only after transferring active buckets; inactive slots may be uninitialized or stale */
		tommy_free(bucket);
	}

	tommy_list_concat_builder(list, &builder, builder_tail);
	hashlin->bucket_bit = TOMMY_HASHLIN_BIT;
	hashlin->bucket_max = initial_max;
	hashlin->bucket_mask = initial_max - 1;
	hashlin->count = 0;
	tommy_imp_hashlin_stable(hashlin);
}

