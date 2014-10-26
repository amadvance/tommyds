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

/** \file
 * Open addressing hashtable.
 *
 * This hashtable resizes dynamically. It starts with the minimal size of 16 buckets, it doubles
 * the size then it reaches a load factor greater than 0.5 and it halves the size with a load
 * factor lower than 0.125.
 *
 * All the elements are reallocated in a single resize operation done inside 
 * tommy_hashopen_insert() or tommy_hashopen_remove().
 *
 * Note that the resize operation takes approximatively 100 [ms] with 1 million of elements, 
 * and 1 [second] with 10 millions. This could be a problem in real-time applications.
 *
 * The resize also fragment the heap, as it involves allocating a double-sized table, copy elements, 
 * and deallocating the older table. Leaving a big hole in the heap.
 * 
 * The ::tommy_hashlin hashtable fixes both problems.
 *
 * To initialize the hashtable you have to call tommy_hashopen_init().
 *
 * \code
 * tommy_hashslin hashopen;
 *
 * tommy_hashopen_init(&hashopen);
 * \endcode
 *
 * To insert elements in the hashtable you have to call tommy_hashopen_insert() for
 * each element.
 * In the insertion call you have to specify the address of the node, the
 * address of the object, and the hash value of the key to use.
 * The address of the object is used to initialize the tommy_node::data field
 * of the node, and the hash to initialize the tommy_node::key field.
 *
 * \code
 * struct object {
 *     tommy_node node;
 *     // other fields
 *     int value;
 * };
 *
 * struct object* obj = malloc(sizeof(struct object)); // creates the object
 *
 * obj->value = ...; // initializes the object
 *
 * tommy_hashopen_insert(&hashopen, &obj->node, obj, tommy_inthash_u32(obj->value)); // inserts the object
 * \endcode
 *
 * To find and element in the hashtable you have to call tommy_hashtable_search()
 * providing a comparison function, its argument, and the hash of the key to search.
 *
 * \code
 * int compare(const void* arg, const void* obj)
 * {
 *     return (*(const unsigned*)arg != ((const struct object*)obj)->value;
 * }
 *
 * struct object* obj = tommy_hashopen_search(&hashopen, compare, &value_to_find, tommy_inthash_u32(value_to_find));
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the hashtable with the same key, you have to
 * use tommy_hashopen_bucket() and follow the tommy_node::next pointer until NULL.
 * You have also to check explicitely for the key, as the bucket may contains
 * different keys.
 *
 * \code
 * tommy_node* i = tommy_hashopen_bucket(&hashopen, tommy_inthash_u32(value_to_find));
 * while (i) {
 *     struct object* obj = i->data; // gets the object pointer
 *
 *     if (obj->value == value_to_find) {
 *         printf("%d\n", obj->value); // process the object
 *     }
 *
 *     i = i->next; // goes to the next element
 * }
 * \endcode
 *
 * To remove an element from the hashtable you have to call tommy_hashopen_remove()
 * providing a comparison function, its argument, and the hash of the key to search
 * and remove.
 *
 * \code
 * struct object* obj = tommy_trie_remove(&hashtable, compare, &value_to_remove, tommy_inthash_u32(value_to_remove));
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To destroy the hashtable you have to remove all the elements, and deinitialize
 * the hashtable calling tommy_hashopen_done().
 *
 * \code
 * tommy_hashopen_done(&hashopen);
 * \endcode
 *
 * Note that you cannot iterates over all the elements in the hashtable using the
 * hashtable itself. You have to insert all the elements also in a ::tommy_list,
 * and use the list to iterate. See the \ref multiindex example for more detail.   
 */

#ifndef __TOMMYHASHOPEN_H
#define __TOMMYHASHOPEN_H

#include "tommyhash.h"

/******************************************************************************/
/* hashopen */

/** \internal
 * Initial and minimal size of the hashtable expressed as a power of 2.
 * The initial size is 2^TOMMY_HASHOPEN_BIT.
 */
#define TOMMY_HASHOPEN_BIT 4

/**
 * Open addressing hashtable node.
 * This is the node that you have to include inside your objects.
 */
typedef tommy_node tommy_hashopen_node;

/** \internal
 * Identifier for empty bucket.
 */
#define TOMMY_HASHOPEN_EMPTY ((tommy_hashopen_node*)0)

/** \internal
 * Identifier for deleted bucket.
 */
#define TOMMY_HASHOPEN_DELETED ((tommy_hashopen_node*)1)

/** \internal
 * Defines probing strategy. Linear or Quadratic.
 */
//#define HASHOPEN_USE_QUADRATIC 1
#if HASHOPEN_USE_QUADRATIC
#define HASHOPEN_NEXT(delta) delta++
#else
#define HASHOPEN_NEXT(delta) delta
#endif

/** \internal
 * Open addressing hashtable bucket.
 */
typedef struct tommy_hashopen_pos_struct {
	tommy_hashopen_node* ptr; /**< Pointer at the first element. */
	tommy_hash_t hash; /**< Hash of the elements in the bucket. */
} tommy_hashopen_pos;

/**
 * Open addressing hashtable.
 */
typedef struct tommy_hashopen_struct {
	tommy_hashopen_pos* bucket; /**< Hash buckets. */
	tommy_uint_t bucket_bit; /**< Bits used in the bit mask. */
	tommy_count_t bucket_max; /**< Number of buckets. */
	tommy_count_t bucket_mask; /**< Bit mask to access the buckets. */
	tommy_count_t count; /**< Number of elements. */
	tommy_count_t filled_count; /**< Number of filled buckets. */
	tommy_count_t deleted_count; /**< Number of deleted buckets. */
} tommy_hashopen;

/**
 * Initializes the hashtable.
 */
void tommy_hashopen_init(tommy_hashopen* hashopen);

/**
 * Deinitializes the hashtable.
 */
void tommy_hashopen_done(tommy_hashopen* hashopen);

/**
 * Inserts an element in the hashtable.
 */
void tommy_hashopen_insert(tommy_hashopen* hashopen, tommy_hashopen_node* node, void* data, tommy_hash_t hash);

/**
 * Searches and removes an element from the hashtable.
 * You have to provide a compare function and the hash of the element you want to remove.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * This operation is faster than calling tommy_hashopen_bucket() and tommy_hashopen_remove_existing() separately.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
void* tommy_hashopen_remove(tommy_hashopen* hashopen, tommy_compare_func* cmp, const void* cmp_arg, tommy_hash_t hash);

/**
 * Gets the bucket of the specified hash.
 * The bucket is guaranteed to contain ALL the elements with the specified hash,
 * but it can contain also others.
 * You can access elements in the bucket following the ::next pointer until 0.
 * \param hash Hash of the element to find. 
 * \return The bucket for the specified hash. It could be an empty one, or the one with the hash already set.
 */
tommy_inline tommy_hashopen_pos* tommy_hashopen_bucket(tommy_hashopen* hashopen, tommy_hash_t hash)
{
	tommy_count_t i = hash & hashopen->bucket_mask;
	tommy_count_t delta = 1;
	tommy_hashopen_pos* empty = 0;

	while (1) {
		tommy_hashopen_pos* p = &hashopen->bucket[i];

		/* if the bucket is empty, the element is missing */
		if (p->ptr == TOMMY_HASHOPEN_EMPTY) {
			if (!empty)
				empty = p;
			return empty;
		} else if (p->ptr == TOMMY_HASHOPEN_DELETED) {
			if (!empty)
				empty = p;
		} else if (p->hash == hash) {
			/* if the hash match, it's the right one */
			return p;
		}

		/* go to the next bucket */
		i = (i + HASHOPEN_NEXT(delta)) & hashopen->bucket_mask;
	}
}

/**
 * Searches an element in the hashtable.
 * You have to provide a compare function and the hash of the element you want to find.
 * If more equal elements are present, the first one is returned.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find.
 * \return The first element found, or 0 if none.
 */
tommy_inline void* tommy_hashopen_search(tommy_hashopen* hashopen, tommy_compare_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashopen_pos* i = tommy_hashopen_bucket(hashopen, hash);
	tommy_hashopen_node* j;

	/* if empty bucket, or different hash, it's missing */
	if (i->ptr == TOMMY_HASHOPEN_EMPTY
		|| i->ptr == TOMMY_HASHOPEN_DELETED
		|| i->hash != hash)
		return 0;

	j = i->ptr;
	while (j) {
		if (cmp(cmp_arg, j->data) == 0)
			return j->data;
		j = j->next;
	}

	return 0;
}

/**
 * Removes an element from the hashtable.
 * You must already have the address of the element to remove.
 * \return The tommy_node::data field of the node removed.
 */
void* tommy_hashopen_remove_existing(tommy_hashopen* hashopen, tommy_hashopen_node* node);

/**
 * Gets the number of elements.
 */
tommy_inline tommy_count_t tommy_hashopen_count(tommy_hashopen* hashopen)
{
	return hashopen->count;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::tommy_hashopen_node of the stored elements.
 */
tommy_size_t tommy_hashopen_memory_usage(tommy_hashopen* hashopen);

#endif

