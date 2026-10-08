// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Fixed size chained hashtable.
 *
 * This hashtable is a standard implementation of a chained hashtable with a fixed size.
 *
 * Note that performance starts to degenerate after reaching a load factor greater than 0.75.
 * The ::tommy_hashdyn and ::tommy_hashlin hashtables fix this problem growing dynamically.
 *
 * To initialize the hashtable you have to call tommy_hashtable_init() specifying
 * the fixed bucket size.
 *
 * \code
 * tommy_hashtable hashtable;
 *
 * tommy_hashtable_init(&hashtable, 1024);
 * \endcode
 *
 * To insert elements in the hashtable you have to call tommy_hashtable_insert() for
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
 * tommy_hashtable_insert(&hashtable, &obj->node, obj, tommy_inthash_u32(obj->value)); // inserts the object
 * \endcode
 *
 * To find an element in the hashtable you have to call tommy_hashtable_search()
 * providing a comparison function, its argument, and the hash of the key to search.
 *
 * \code
 * int compare(const void* arg, const void* obj)
 * {
 *     return *(const int*)arg != ((const struct object*)obj)->value;
 * }
 *
 * int value_to_find = 1;
 * struct object* obj = tommy_hashtable_search(&hashtable, compare, &value_to_find, tommy_inthash_u32(value_to_find));
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the hashtable with the same key, you have to
 * use tommy_hashtable_bucket() and follow the tommy_node::next pointer until NULL.
 * You have also to check explicitly for the key, as the bucket may contain
 * different keys.
 *
 * \code
 * tommy_node* i = tommy_hashtable_bucket(&hashtable, tommy_inthash_u32(value_to_find));
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
 * To remove an element from the hashtable you have to call tommy_hashtable_remove()
 * providing a comparison function, its argument, and the hash of the key to search
 * and remove.
 *
 * \code
 * struct object* obj = tommy_hashtable_remove(&hashtable, compare, &value_to_remove, tommy_inthash_u32(value_to_remove));
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To destroy the hashtable you have to remove all the elements, and deinitialize
 * the hashtable calling tommy_hashtable_done().
 *
 * \code
 * tommy_hashtable_done(&hashtable);
 * \endcode
 *
 * If you need to iterate over all the elements in the hashtable, you can use
 * tommy_hashtable_foreach() or tommy_hashtable_foreach_arg().
 * If you need a more precise control with a real iteration, you have to insert
 * all the elements also in a ::tommy_list, and use the list to iterate.
 * See the \ref multiindex example for more detail.
 */

#ifndef __TOMMYHASHTBL_H
#define __TOMMYHASHTBL_H

#include "tommyhash.h"
#include "tommylist.h"

/******************************************************************************/
/* hashtable */

/**
 * Hashtable node.
 * This is the node that you have to include inside your objects.
 */
typedef tommy_node tommy_hashtable_node;

/**
 * Hashtable container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_hashtable_struct {
	tommy_hashtable_node** bucket; /**< Hash buckets. One list for each hash modulus. */
	tommy_size_t bucket_max; /**< Number of buckets. */
	tommy_size_t bucket_mask; /**< Bit mask to access the buckets. */
	tommy_size_t count; /**< Number of elements. */
} tommy_hashtable;

/**
 * Initializes the hashtable.
 * \param hashtable Hashtable to initialize.
 * \param bucket_max Minimum number of buckets to allocate. The effective number
 * is rounded up to a power of 2, with a minimum of 16 buckets.
 */
TOMMY_API void tommy_hashtable_init(tommy_hashtable* hashtable, tommy_size_t bucket_max);

/**
 * Deinitializes the hashtable.
 *
 * You can call this function with elements still contained,
 * but such elements are not going to be freed by this call.
 */
TOMMY_API void tommy_hashtable_done(tommy_hashtable* hashtable);

/**
 * Exchanges the complete state of two initialized hashtables, including bucket allocations.
 * The hashtables must not share nodes. Nodes and objects are not accessed or modified.
 * Existing node pointers remain valid and belong to the other hashtable.
 * Passing the same hashtable twice has no effect. Both hashtables remain usable and deinitializable.
 * \param first The first hashtable.
 * \param second The second hashtable.
 * \note This operation is O(1) with respect to the number of elements.
 */
tommy_inline void tommy_hashtable_swap(tommy_hashtable* first, tommy_hashtable* second)
{
	tommy_hashtable tmp = *first;
	*first = *second;
	*second = tmp;
}

/**
 * Removes all elements, preserving the allocated buckets.
 * The hashtable remains initialized and can be reused immediately.
 * Objects are not freed and nodes are not accessed or modified.
 * Their links must not be used to traverse the previous contents.
 * You can call this function after tommy_hashtable_foreach() has freed the objects.
 * \note This operation is O(b), where b is the number of buckets.
 */
TOMMY_API void tommy_hashtable_clear(tommy_hashtable* hashtable);

/**
 * Inserts an element in the hashtable.
 */
TOMMY_API void tommy_hashtable_insert(tommy_hashtable* hashtable, tommy_hashtable_node* node, void* data, tommy_hash_t hash);

/**
 * Inserts an element only if no equal element is already contained.
 * If found, the first equal element's tommy_node::data field is returned,
 * and the hashtable and candidate node are left unchanged.
 * Otherwise, the candidate is inserted using the normal insertion policy,
 * and its data field is returned.
 * Objects are not freed by this call.
 * \param hashtable Hashtable to insert into.
 * \param node The candidate node. It must not belong to any container.
 * \param data The object to insert.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument describing the candidate key.
 * \param hash Hash of the candidate key, consistent with the comparison function.
 * \return The first equal element's data field, or data if the candidate was inserted.
 */
TOMMY_API void* tommy_hashtable_insert_unique(tommy_hashtable* hashtable, tommy_hashtable_node* node, void* data, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash);

/**
 * Searches and removes an element from the hashtable.
 * You have to provide a compare function and the hash of the element you want to remove.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * \param hashtable Hashtable to remove from.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
TOMMY_API void* tommy_hashtable_remove(tommy_hashtable* hashtable, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash);

/**
 * Gets the bucket of the specified hash.
 * The bucket is guaranteed to contain ALL the elements with the specified hash,
 * but it can contain also others.
 * You can access elements in the bucket following the tommy_node::next pointer until 0.
 * \param hashtable Hashtable to query.
 * \param hash Hash of the element to find.
 * \return The head of the bucket, or 0 if empty.
 */
tommy_inline tommy_hashtable_node* tommy_hashtable_bucket(tommy_hashtable* hashtable, tommy_hash_t hash)
{
	return hashtable->bucket[hash & hashtable->bucket_mask];
}

/**
 * Searches an element in the hashtable.
 * You have to provide a compare function and the hash of the element you want to find.
 * If more equal elements are present, the first one is returned.
 * \param hashtable Hashtable to search.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find.
 * \return The first element found, or 0 if none.
 */
tommy_inline void* tommy_hashtable_search(tommy_hashtable* hashtable, tommy_search_func* cmp, const void* cmp_arg, tommy_hash_t hash)
{
	tommy_hashtable_node* i = tommy_hashtable_bucket(hashtable, hash);

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
TOMMY_API void* tommy_hashtable_remove_existing(tommy_hashtable* hashtable, tommy_hashtable_node* node);

/**
 * Updates the hash of an element already contained in the hashtable.
 * The node must belong to this hashtable. The caller updates the object key
 * and provides its new hash, without modifying tommy_node::index directly.
 * If the hash is unchanged, the node and its position are left unchanged.
 * Otherwise, the node is moved to the tail of the destination bucket,
 * even if the old and new hashes identify the same bucket.
 * The tommy_node::data field and the number of elements are left unchanged.
 * No memory allocation, deallocation or resize is performed.
 * Equal keys are allowed; no uniqueness check is performed.
 * \param hashtable Hashtable containing the node.
 * \param node The node whose hash is updated.
 * \param hash The new hash of the element.
 * \note This operation is O(1).
 */
TOMMY_API void tommy_hashtable_rehash_existing(tommy_hashtable* hashtable, tommy_hashtable_node* node, tommy_hash_t hash);

/**
 * Calls the specified function for each element in the hashtable.
 *
 * You cannot add or remove elements from the inside of the callback,
 * but can use it to deallocate them.
 *
 * \code
 * tommy_hashtable hashtable;
 *
 * // initializes the hashtable
 * tommy_hashtable_init(&hashtable, ...);
 *
 * ...
 *
 * // creates an object
 * struct object* obj = malloc(sizeof(struct object));
 *
 * ...
 *
 * // insert it in the hashtable
 * tommy_hashtable_insert(&hashtable, &obj->node, obj, tommy_inthash_u32(obj->value));
 *
 * ...
 *
 * // deallocates all the objects iterating the hashtable
 * tommy_hashtable_foreach(&hashtable, free);
 *
 * // deallocates the hashtable
 * tommy_hashtable_done(&hashtable);
 * \endcode
 */
TOMMY_API void tommy_hashtable_foreach(tommy_hashtable* hashtable, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the hashtable.
 * The iteration order and callback rules are the same as tommy_hashtable_foreach().
 * The callback may deallocate the current element.
 * Adding or removing elements from inside the callback is not allowed.
 */
TOMMY_API void tommy_hashtable_foreach_arg(tommy_hashtable* hashtable, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the number of elements.
 */
tommy_inline tommy_size_t tommy_hashtable_count(const tommy_hashtable* hashtable)
{
	return hashtable->count;
}

/**
 * Checks if empty.
 * \return If the hashtable is empty.
 */
tommy_inline tommy_bool_t tommy_hashtable_empty(const tommy_hashtable* hashtable)
{
	return hashtable->count == 0;
}

/**
 * Gets the number of buckets.
 */
tommy_inline tommy_size_t tommy_hashtable_bucket_count(const tommy_hashtable* hashtable)
{
	return hashtable->bucket_max;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::tommy_hashtable_node of the stored elements.
 */
TOMMY_API tommy_size_t tommy_hashtable_memory_usage(const tommy_hashtable* hashtable);

/**
 * Transfers all elements from the hashtable into a tommy_list.
 *
 * Removes every element from the \p hashtable and inserts them
 * into the provided \p list (at the tail), preserving the per-bucket order
 * but not guaranteeing any particular global order.
 *
 * After the call:
 * - the hashtable is left empty and initialized, preserving its allocated buckets
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
 * \note The operation is O(b) where b is the number of buckets.
 * \note No memory allocation or deallocation is performed.
 * \note The relative order of elements that were in the same bucket is preserved,
 *       but the order among different buckets is bucket-order dependent.
 *
 * Typical usage pattern:
 * \code
 * tommy_list all_elements;
 * tommy_list_init(&all_elements);
 *
 * // move everything out of the hashtable into the list
 * tommy_hashtable_to_list(&hashtable, &all_elements);
 *
 * // now you can sort, filter, process sequentially, etc.
 * tommy_list_sort(&all_elements, compare_by_value);
 * \endcode
 *
 * \param hashtable The hashtable to drain
 * \param list The destination list. It must be initialized and must not share
 * nodes with the hashtable. Existing elements remain at the head of the list.
 */
TOMMY_API void tommy_hashtable_to_list(tommy_hashtable* hashtable, tommy_list* list);

#endif

