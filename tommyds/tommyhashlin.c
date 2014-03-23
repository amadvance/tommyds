/*
 * Copyright 2010 Andrea Mazzoleni. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY ANDREA MAZZOLENI AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANDREA MAZZOLENI OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "tommyhashlin.h"
#include "tommylist.h"

#include <string.h> /* for memset */
#include <assert.h> /* for assert */

/******************************************************************************/
/* hashlin */

/**
 * Reallocation states.
 */
#define TOMMY_HASHLIN_STATE_STABLE 0
#define TOMMY_HASHLIN_STATE_GROW 1
#define TOMMY_HASHLIN_STATE_SHRINK 2

void tommy_hashlin_init(tommy_hashlin* hashlin)
{
	/* fixed initial size */
	hashlin->bucket_bit = TOMMY_HASHLIN_BIT;
	hashlin->bucket_max = 1 << hashlin->bucket_bit;
	hashlin->bucket_mask = hashlin->bucket_max - 1;
	hashlin->bucket[0] = tommy_cast(tommy_hashlin_node**, tommy_malloc(hashlin->bucket_max * sizeof(tommy_hashlin_node*)));
	memset(hashlin->bucket[0], 0, hashlin->bucket_max * sizeof(tommy_hashlin_node*));
	hashlin->bucket_mac = 1;

	/* stable state */
	hashlin->state = TOMMY_HASHLIN_STATE_STABLE;

	hashlin->count = 0;
}

void tommy_hashlin_done(tommy_hashlin* hashlin)
{
	unsigned i;
	for(i=0;i<hashlin->bucket_mac;++i)
		tommy_free(hashlin->bucket[i]);
}

/**
 * Return the bucket at the specified pos.
 */
tommy_inline tommy_hashlin_node** tommy_hashlin_pos(tommy_hashlin* hashlin, tommy_hash_t pos)
{  
	unsigned bsr;  
 
	/* special case for the first bucket */
	if (pos < (1 << TOMMY_HASHLIN_BIT)) {
		return &hashlin->bucket[0][pos];
	}

	/* get the highest bit set */
	bsr = tommy_ilog2_u32(pos);

	/* clear the highest bit */
	pos -= 1 << bsr;

	return &hashlin->bucket[bsr - TOMMY_HASHLIN_BIT + 1][pos];
}

/**
 * Return the bucket to use.
 */
tommy_inline tommy_hashlin_node** tommy_hashlin_bucket_ptr(tommy_hashlin* hashlin, tommy_hash_t hash)
{  
	unsigned pos;

	/* if we are reallocating */
	if (hashlin->state != TOMMY_HASHLIN_STATE_STABLE) {
		/* compute the old position */
		pos = hash & hashlin->low_mask;

		/* if we have not reallocated this position yet */ 
		if (pos >= hashlin->split) {

			/* use it as it was before */
			return tommy_hashlin_pos(hashlin, pos);
		}
	}

	/* otherwise operates normally */
	pos = hash & hashlin->bucket_mask;

	return tommy_hashlin_pos(hashlin, pos);
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

			/* grow the hash size and allocate */
			++hashlin->bucket_bit;
			hashlin->bucket_max = 1 << hashlin->bucket_bit;
			hashlin->bucket_mask = hashlin->bucket_max - 1;
			hashlin->bucket[hashlin->bucket_mac] = tommy_cast(tommy_hashlin_node**, tommy_malloc(hashlin->low_max * sizeof(tommy_hashlin_node*)));
			++hashlin->bucket_mac;

			/* start from the beginning going forward */
			hashlin->split = 0;
		}

		/* grow state */
		hashlin->state = TOMMY_HASHLIN_STATE_GROW;
	}

	/* if we are growing */
	if (hashlin->state == TOMMY_HASHLIN_STATE_GROW) {
		/* compute the split target required to finish the reallocation before the next resize */
		unsigned split_target = 2 * hashlin->count;

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max < split_target) {
			tommy_hashlin_node** split[2];
			tommy_hashlin_node* j;
			unsigned mask;

			/* get the low bucket */
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);

			/* get the high bucket */
			/* it's always in the second half, so we can index it directly */
			/* without calling tommy_hashlin_pos() */
			split[1] = &hashlin->bucket[hashlin->bucket_mac-1][hashlin->split];

			/* save the low bucket */
			j = *split[0];

			/* reinitialize the buckets */
			*split[0] = 0;
			*split[1] = 0;

			/* compute the bit to identify the bucket */
			mask = hashlin->bucket_mask & ~hashlin->low_mask;

			/* flush the bucket */
			while (j) {
				tommy_hashlin_node* j_next = j->next;
				unsigned index = (j->key & mask) != 0;
				if (*split[index])
					tommy_list_insert_tail_not_empty(*split[index], j);
				else
					tommy_list_insert_first(split[index], j);
				j = j_next;
			}

			/* go forward */
			++hashlin->split;

			/* if we have finished, change the state */
			if (hashlin->split == hashlin->low_max) {
				hashlin->state = TOMMY_HASHLIN_STATE_STABLE;
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
		unsigned split_target = 8 * hashlin->count;

		/* reallocate buckets until the split target */
		while (hashlin->split + hashlin->low_max > split_target) {
			tommy_hashlin_node** split[2];

			/* go backward position */
			--hashlin->split;
   
			/* get the low bucket */
			split[0] = tommy_hashlin_pos(hashlin, hashlin->split);

			/* get the high bucket */
			/* it's always in the second half, so we can index it directly */
			/* without calling tommy_hashlin_pos() */
			split[1] = &hashlin->bucket[hashlin->bucket_mac-1][hashlin->split];

			/* concat the high bucket into the low one */
			tommy_list_concat(split[0], split[1]);

			/* if we have finished, clean up and change the state */
			if (hashlin->split == 0) {
				hashlin->state = TOMMY_HASHLIN_STATE_STABLE;

				/* shrink the hash size */
				--hashlin->bucket_bit;
				hashlin->bucket_max = 1 << hashlin->bucket_bit;
				hashlin->bucket_mask = hashlin->bucket_max - 1;

				/* free the last segment */
				--hashlin->bucket_mac;
				tommy_free(hashlin->bucket[hashlin->bucket_mac]);
				break;
			}
		}
	}
}

void tommy_hashlin_insert(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_hash_t hash)
{
	tommy_list_insert_tail(tommy_hashlin_bucket_ptr(hashlin, hash), node, data);

	node->key = hash;

	++hashlin->count;

	hashlin_grow_step(hashlin);
}

void* tommy_hashlin_remove_existing(tommy_hashlin* hashlin, tommy_hashlin_node* node)
{
	tommy_list_remove_existing(tommy_hashlin_bucket_ptr(hashlin, node->key), node);

	--hashlin->count;

	hashlin_shrink_step(hashlin);

	return node->data;
}

tommy_hashlin_node* tommy_hashlin_bucket(tommy_hashlin* hashlin, tommy_hash_t hash)
{
	return *tommy_hashlin_bucket_ptr(hashlin, hash);
}

void* tommy_hashlin_remove(tommy_hashlin* hashlin, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashlin_node** let_ptr = tommy_hashlin_bucket_ptr(hashlin, hash);
	tommy_hashlin_node* i = *let_ptr;

	while (i) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (i->key == hash && cmp(cmp_arg, i->data) == 0) {
			tommy_list_remove_existing(let_ptr, i);

			--hashlin->count;

			hashlin_shrink_step(hashlin);

			return i->data;
		}
		i = i->next;
	}

	return 0;
}

tommy_size_t tommy_hashlin_memory_usage(tommy_hashlin* hashlin)
{
	return hashlin->bucket_max * (tommy_size_t)sizeof(hashlin->bucket[0][0])
		+ hashlin->count * (tommy_size_t)sizeof(tommy_hashlin_node);
}

