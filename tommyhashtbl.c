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

#include "tommyhashtbl.h"
#include "tommylist.h"

#include <string.h> /* for memset */

/******************************************************************************/
/* hashtable */

void tommy_hashtable_init(tommy_hashtable* hashtable, unsigned bucket_max)
{
	if (bucket_max < 16) {
		bucket_max = 16;
	} else {
		bucket_max = tommy_roundup_pow2_u32(bucket_max);
	}

	hashtable->bucket_max = bucket_max;
	hashtable->bucket_mask = hashtable->bucket_max - 1;
	hashtable->bucket = tommy_cast(tommy_hashtable_node**, tommy_malloc(hashtable->bucket_max * sizeof(tommy_hashtable_node*)));
	memset(hashtable->bucket, 0, hashtable->bucket_max * sizeof(tommy_hashtable_node*));

	hashtable->count = 0;
}

void tommy_hashtable_done(tommy_hashtable* hashtable)
{
	tommy_free(hashtable->bucket);
}

void tommy_hashtable_insert(tommy_hashtable* hashtable, tommy_hashtable_node* node, void* data, tommy_hash_t hash)
{
	unsigned pos = hash & hashtable->bucket_mask;

	tommy_list_insert_tail(&hashtable->bucket[pos], node, data);

	node->key = hash;

	++hashtable->count;
}

void* tommy_hashtable_remove_existing(tommy_hashtable* hashtable, tommy_hashtable_node* node)
{
	unsigned pos = node->key & hashtable->bucket_mask;

	tommy_list_remove_existing(&hashtable->bucket[pos], node);

	--hashtable->count;

	return node->data;
}

void* tommy_hashtable_remove(tommy_hashtable* hashtable, tommy_compare_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	unsigned pos = hash & hashtable->bucket_mask;
	tommy_hashtable_node* i = hashtable->bucket[pos];

	while (i) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (i->key == hash && cmp(cmp_arg, i->data) == 0) {
			tommy_list_remove_existing(&hashtable->bucket[pos], i);

			--hashtable->count;

			return i->data;
		}
		/* we assume that i->next is still valid also after removing */
		i = i->next;
	}

	return 0;
}

tommy_size_t tommy_hashtable_memory_usage(tommy_hashtable* hashtable)
{
	return hashtable->bucket_max * (tommy_size_t)sizeof(hashtable->bucket[0])
		+ tommy_hashtable_count(hashtable) * (tommy_size_t)sizeof(tommy_hashtable_node);
}

