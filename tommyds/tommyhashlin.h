// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Linear chained hashtable.
 *
 * This hashtable resizes dynamically and progressively using a variation of the
 * linear hashing algorithm described in http://en.wikipedia.org/wiki/Linear_hashing
 *
 * It starts with the minimal size of 64 buckets, it doubles the size when it
 * reaches a load factor greater than 0.5, and it halves the size when the load
 * factor is lower than 0.125.
 *
 * The progressive resize spreads resizing work across insert and delete
 * operations, reducing pauses caused by resizing the entire table at once.
 * Resizing may traverse a whole bucket, so many collisions can still make
 * an individual operation expensive.
 *
 * For resizing, a dynamic array that supports access to non-contiguous
 * segments is used.
 * In this way, we only allocate additional table segments on the heap, without
 * freeing the previous table, and thus not increasing the heap fragmentation.
 *
 * The resize takes place inside tommy_hashlin_insert() and tommy_hashlin_remove().
 * No resize is done in the tommy_hashlin_search() operation.
 *
 * To initialize the hashtable you have to call tommy_hashlin_init().
 *
 * \code
 * tommy_hashlin hashlin;
 *
 * tommy_hashlin_init(&hashlin);
 * \endcode
 *
 * To insert elements in the hashtable you have to call tommy_hashlin_insert() for
 * each element.
 * In the insertion call you have to specify the address of the node, the
 * address of the object, and the hash value of the key to use.
 * The address of the object is used to initialize the tommy_node::data field
 * of the node, and the hash to initialize the tommy_node::index field.
 *
 * \code
 * struct object {
 *     int value;
 *     // other fields
 *     tommy_node node;
 * };
 *
 * struct object* obj = malloc(sizeof(struct object)); // creates the object
 *
 * obj->value = ...; // initializes the object
 *
 * tommy_hashlin_insert(&hashlin, &obj->node, obj, tommy_inthash_u32(obj->value)); // inserts the object
 * \endcode
 *
 * To find an element in the hashtable you have to call tommy_hashlin_search()
 * providing a comparison function, its argument, and the hash of the key to search.
 *
 * \code
 * int compare(const void* arg, const void* obj)
 * {
 *     return *(const int*)arg != ((const struct object*)obj)->value;
 * }
 *
 * int value_to_find = 1;
 * struct object* obj = tommy_hashlin_search(&hashlin, compare, &value_to_find, tommy_inthash_u32(value_to_find));
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the hashtable with the same key, you have to
 * use tommy_hashlin_bucket() and follow the tommy_node::next pointer until NULL.
 * You have also to check explicitly for the key, as the bucket may contain
 * different keys.
 *
 * \code
 * int value_to_find = 1;
 * tommy_node* i = tommy_hashlin_bucket(&hashlin, tommy_inthash_u32(value_to_find));
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
 * To remove an element from the hashtable you have to call tommy_hashlin_remove()
 * providing a comparison function, its argument, and the hash of the key to search
 * and remove.
 *
 * \code
 * struct object* obj = tommy_hashlin_remove(&hashlin, compare, &value_to_remove, tommy_inthash_u32(value_to_remove));
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To destroy the hashtable you have to deinitialize it calling tommy_hashlin_done().
 * Elements can still be contained when calling this function, but their memory
 * is not freed by it and must be managed separately (for example, freed with
 * tommy_hashlin_foreach()).
 *
 * \code
 * tommy_hashlin_done(&hashlin);
 * \endcode
 *
 * If you need to iterate over all the elements in the hashtable, you can use
 * tommy_hashlin_foreach() or tommy_hashlin_foreach_arg().
 * If you need a more precise control with a real iteration, you have to insert
 * all the elements also in a ::tommy_list, and use the list to iterate.
 * See the \ref multiindex example for more detail.
 */

#ifndef __TOMMYHASHLIN_H
#define __TOMMYHASHLIN_H

#include "tommyhash.h"
#include "tommylist.h"

/******************************************************************************/
/* hashlin */

/** \internal
 * Initial and minimal size of the hashtable expressed as a power of 2.
 * The initial size is 2^TOMMY_HASHLIN_BIT.
 */
#define TOMMY_HASHLIN_BIT 6

/**
 * Hashtable node.
 * This is the node that you have to include inside your objects.
 */
typedef tommy_node tommy_hashlin_node;

/**
 * Hashtable container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_hashlin_struct {
	tommy_hashlin_node** bucket[TOMMY_SIZE_BIT]; /**< Dynamic array of hash buckets. One list for each hash modulus. */
	tommy_size_t bucket_max; /**< Number of buckets. */
	tommy_size_t bucket_mask; /**< Bit mask to access the buckets. */
	tommy_size_t low_max; /**< Low order max value. */
	tommy_size_t low_mask; /**< Low order mask value. */
	tommy_size_t split; /**< Split position. */
	tommy_size_t count; /**< Number of elements. */
	tommy_uint_t bucket_bit; /**< Bits used in the bit mask. */
	tommy_uint_t state; /**< Reallocation state. */
} tommy_hashlin;

/**
 * Initializes the hashtable.
 */
TOMMY_API void tommy_hashlin_init(tommy_hashlin* hashlin);

/**
 * Deinitializes the hashtable.
 *
 * You can call this function with elements still contained,
 * but such elements are not going to be freed by this call.
 */
TOMMY_API void tommy_hashlin_done(tommy_hashlin* hashlin);

/**
 * Exchanges the complete state of two initialized hashtables, including bucket allocations and progressive resize state.
 * The hashtables must not share nodes. Nodes and objects are not accessed or modified.
 * Existing node pointers remain valid and belong to the other hashtable.
 * Passing the same hashtable twice has no effect. Both hashtables remain usable and deinitializable.
 * \param first The first hashtable.
 * \param second The second hashtable.
 * \note This operation is O(1) with respect to the number of elements.
 */
tommy_inline void tommy_hashlin_swap(tommy_hashlin* first, tommy_hashlin* second)
{
	tommy_hashlin tmp = *first;
	*first = *second;
	*second = tmp;
}

/**
 * Removes all elements and resets the hashtable to its initial state,
 * as after tommy_hashlin_init(). It can be reused immediately.
 * Resetting the hashtable is intentional. Keeping a large empty table
 * would make subsequent removals expensive, as progressive shrinking could
 * require scanning many empty buckets.
 * Any pending resize is canceled. No new memory is allocated.
 * Objects are not freed and nodes are not accessed or modified.
 * Their links must not be used to traverse the previous contents.
 * You can call this function after tommy_hashlin_foreach() has freed the objects.
 * Subsequent insertions and removals retain the normal resizing policy.
 * \note This operation is O(m + log b), where m is the initial number of buckets
 * and b is the number of allocated buckets before the call. Nodes are not traversed.
 */
TOMMY_API void tommy_hashlin_clear(tommy_hashlin* hashlin);

/**
 * Inserts an element in the hashtable.
 */
TOMMY_API void tommy_hashlin_insert(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_hash_t hash);

/**
 * Inserts an element only if no equal element is already contained.
 * If found, the first equal element's tommy_node::data field is returned,
 * and the hashtable and candidate node are left unchanged.
 * Otherwise, the candidate is inserted using the normal insertion policy,
 * and its data field is returned.
 * Objects are not freed by this call.
 * \param hashlin Hashtable to insert into.
 * \param node The candidate node. It must not belong to any container.
 * \param data The object to insert.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument describing the candidate key.
 * \param hash Hash of the candidate key, consistent with the comparison function.
 * \return The first equal element's data field, or data if the candidate was inserted.
 */
TOMMY_API void* tommy_hashlin_insert_unique(tommy_hashlin* hashlin, tommy_hashlin_node* node, void* data, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash);

/**
 * Searches and removes an element from the hashtable.
 * You have to provide a compare function and the hash of the element you want to remove.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * \param hashlin Hashtable to remove from.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
TOMMY_API void* tommy_hashlin_remove(tommy_hashlin* hashlin, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash);

/** \internal
 * Returns the bucket at the specified position.
 */
tommy_inline tommy_hashlin_node** tommy_hashlin_pos(tommy_hashlin* hashlin, tommy_hash_t pos)
{
	tommy_uint_t bsr;

	/* get the highest bit set, in case of all 0, return 0 */
	bsr = tommy_ilog2(pos | 1);

	return &hashlin->bucket[bsr][pos];
}

/** \internal
 * Returns a pointer to the bucket of the specified hash.
 */
tommy_inline tommy_hashlin_node** tommy_hashlin_bucket_ref(tommy_hashlin* hashlin, tommy_hash_t hash)
{
	tommy_size_t pos = hash & hashlin->low_mask;
	tommy_size_t high_pos = hash & hashlin->bucket_mask;

	/* if this position is already allocated in the high half */
	if (pos < hashlin->split) {
		/* the following assignment is expected to be implemented */
		/* with a conditional move instruction */
		/* that results in a little better and constant performance */
		/* regardless of the split position. */
		/* this affects mostly the worst case, when the split value */
		/* is near at its half, resulting in a totally unpredictable */
		/* condition by the CPU. */
		/* in such case, the use of the conditional move is generally faster. */

		/* use also the high bit */
		pos = high_pos;
	}

	return tommy_hashlin_pos(hashlin, pos);
}

/**
 * Gets the bucket of the specified hash.
 * The bucket is guaranteed to contain ALL the elements with the specified hash,
 * but it can contain also others.
 * You can access elements in the bucket following the tommy_node::next pointer until 0.
 * \param hashlin Hashtable to query.
 * \param hash Hash of the element to find.
 * \return The head of the bucket, or 0 if empty.
 */
tommy_inline tommy_hashlin_node* tommy_hashlin_bucket(tommy_hashlin* hashlin, tommy_hash_t hash)
{
	return *tommy_hashlin_bucket_ref(hashlin, hash);
}

/**
 * Searches an element in the hashtable.
 * You have to provide a compare function and the hash of the element you want to find.
 * If more equal elements are present, the first one is returned.
 * \param hashlin Hashtable to search.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find.
 * \return The first element found, or 0 if none.
 */
tommy_inline void* tommy_hashlin_search(tommy_hashlin* hashlin, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashlin_node* i = tommy_hashlin_bucket(hashlin, hash);

	while (i) {
		/* we first check if the hash matches, as in the same bucket we may have multiple hash values */
		if (i->index == hash && cmp(cmp_arg, i->data) == 0)
			return i->data;
		i = i->next;
	}
	return 0;
}

/**
 * Removes an element from the hashtable.
 * You must already have the address of the element to remove.
 * \return The tommy_node::data field of the node removed.
 */
TOMMY_API void* tommy_hashlin_remove_existing(tommy_hashlin* hashlin, tommy_hashlin_node* node);

/**
 * Updates the hash of an element already contained in the hashtable.
 * The node must belong to this hashtable. The caller updates the object key
 * and provides its new hash, without modifying tommy_node::index directly.
 * If the hash is unchanged, the node and its position are left unchanged.
 * Otherwise, the node is moved to the tail of the destination bucket,
 * even if the old and new hashes identify the same bucket.
 * The tommy_node::data field and the number of elements are left unchanged.
 * No memory allocation, deallocation or resize is performed.
 * The progressive resize state is left unchanged.
 * Equal keys are allowed; no uniqueness check is performed.
 * \param hashlin Hashtable containing the node.
 * \param node The node whose hash is updated.
 * \param hash The new hash of the element.
 * \note This operation is O(1).
 */
TOMMY_API void tommy_hashlin_rehash_existing(tommy_hashlin* hashlin, tommy_hashlin_node* node, tommy_hash_t hash);

/**
 * Calls the specified function for each element in the hashtable.
 *
 * You cannot add or remove elements from the inside of the callback,
 * but can use it to deallocate them.
 *
 * \code
 * tommy_hashlin hashlin;
 *
 * // initializes the hashtable
 * tommy_hashlin_init(&hashlin);
 *
 * ...
 *
 * // creates an object
 * struct object* obj = malloc(sizeof(struct object));
 *
 * ...
 *
 * // insert it in the hashtable
 * tommy_hashlin_insert(&hashlin, &obj->node, obj, tommy_inthash_u32(obj->value));
 *
 * ...
 *
 * // deallocates all the objects iterating the hashtable
 * tommy_hashlin_foreach(&hashlin, free);
 *
 * // deallocates the hashtable
 * tommy_hashlin_done(&hashlin);
 * \endcode
 */
TOMMY_API void tommy_hashlin_foreach(tommy_hashlin* hashlin, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the hashtable.
 * The iteration order and callback rules are the same as tommy_hashlin_foreach().
 * The callback may deallocate the current element.
 * Adding or removing elements from inside the callback is not allowed.
 */
TOMMY_API void tommy_hashlin_foreach_arg(tommy_hashlin* hashlin, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the number of elements.
 */
tommy_inline tommy_size_t tommy_hashlin_count(const tommy_hashlin* hashlin)
{
	return hashlin->count;
}

/**
 * Checks if empty.
 * \return If the hashtable is empty.
 */
tommy_inline tommy_bool_t tommy_hashlin_empty(const tommy_hashlin* hashlin)
{
	return hashlin->count == 0;
}

/**
 * Gets the number of active buckets.
 * During a progressive resize, this can be less than the number of allocated buckets.
 */
tommy_inline tommy_size_t tommy_hashlin_bucket_count(const tommy_hashlin* hashlin)
{
	return hashlin->low_max + hashlin->split;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::tommy_hashlin_node of the stored elements.
 */
TOMMY_API tommy_size_t tommy_hashlin_memory_usage(const tommy_hashlin* hashlin);

/**
 * Transfers all elements from the hashtable into a tommy_list.
 *
 * Removes every element from the \p hashlin hashtable and inserts them
 * into the provided \p list (at the tail), preserving the per-bucket order
 * but not guaranteeing any particular global order.
 * Buckets are visited by increasing active bucket index.
 *
 * After the call:
 * - the hashtable is left in the same state as after tommy_hashlin_init()
 * - the target list contains all the elements that were previously in the hashtable
 *
 * The tommy_node::data and tommy_node::index fields are left unchanged.
 *
 * This function is useful when you need to:
 * - extract all elements to process/sort them outside the hash table
 * - convert the hashtable into a list for sequential iteration
 * - prepare for a full clear + re-insertion with different hash/ordering
 * - move ownership of the nodes to a list-based container
 *
 * \note The operation is O(b) where b is the number of active buckets before the call.
 * \note No new memory is allocated.
 * \note Any pending resize is canceled. The hashtable and list can be reused immediately.
 * \note The relative order of elements that were in the same bucket is preserved,
 *       but the order among different buckets is bucket-order dependent.
 *
 * Typical usage pattern:
 * \code
 * tommy_list all_elements;
 * tommy_list_init(&all_elements);
 *
 * // move everything out of the hashtable into the list
 * tommy_hashlin_to_list(&hashlin, &all_elements);
 *
 * // now you can sort, filter, process sequentially, etc.
 * tommy_list_sort(&all_elements, compare_by_value);
 * \endcode
 *
 * \param hashlin The hashtable to drain
 * \param list The destination list. It must be initialized and must not share
 * nodes with the hashtable. Existing elements remain at the head of the list.
 */
TOMMY_API void tommy_hashlin_to_list(tommy_hashlin* hashlin, tommy_list* list);

#endif

