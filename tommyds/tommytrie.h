// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Trie optimized for cache utilization.
 *
 * This trie is a standard implementation that stores elements in the order defined
 * by the key.
 *
 * It needs an external allocator for the inner nodes in the trie.
 *
 * You can control the number of branches of each node using the ::TOMMY_TRIE_TREE_MAX
 * define. More branches imply more speed, but a bigger memory occupation.
 *
 * Compared to ::tommy_trie_inplace you have to provide a ::tommy_allocator allocator.
 * Note that the C malloc() is too slow to fulfill this role.
 *
 * To initialize the trie you have to call tommy_allocator_init() to initialize
 * the allocator, and tommy_trie_init() for the trie.
 *
 * \code
 * tommy_allocator alloc;
 * tommy_trie trie;
 *
 * tommy_allocator_init(&alloc, TOMMY_TRIE_BLOCK_SIZE, TOMMY_TRIE_BLOCK_SIZE);
 *
 * tommy_trie_init(&trie, &alloc);
 * \endcode
 *
 * To insert elements in the trie you have to call tommy_trie_insert() for
 * each element.
 * In the insertion call you have to specify the address of the node, the
 * address of the object, and the key value to use.
 * The address of the object is used to initialize the tommy_node::data field
 * of the node, and the key to initialize the tommy_node::index field.
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
 * tommy_trie_insert(&trie, &obj->node, obj, obj->value); // inserts the object
 * \endcode
 *
 * To find an element in the trie you have to call tommy_trie_search() providing
 * the key to search.
 *
 * \code
 * int value_to_find = 1;
 * struct object* obj = tommy_trie_search(&trie, value_to_find);
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the trie with the same key, you have to
 * use tommy_trie_bucket() and follow the tommy_node::next pointer until 0.
 *
 * \code
 * int value_to_find = 1;
 * tommy_node* i = tommy_trie_bucket(&trie, value_to_find);
 * while (i) {
 *     struct object* obj = i->data; // gets the object pointer
 *
 *     printf("%d\n", obj->value); // process the object
 *
 *     i = i->next; // goes to the next element
 * }
 * \endcode
 *
 * To remove an element from the trie you have to call tommy_trie_remove()
 * providing the key to search and remove.
 *
 * \code
 * struct object* obj = tommy_trie_remove(&trie, value_to_remove);
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To visit all the elements use tommy_trie_foreach() or tommy_trie_foreach_arg().
 * Elements are visited by increasing key, with equal keys in insertion order.
 *
 * To destroy the trie you can deallocate all the objects with
 * tommy_trie_foreach(), and deinitialize the allocator using tommy_allocator_done().
 * The allocator must no longer be used by other tries.
 *
 * \code
 * tommy_trie_foreach(&trie, free);
 * tommy_allocator_done(&alloc);
 * \endcode
 */

#ifndef __TOMMYTRIE_H
#define __TOMMYTRIE_H

#include "tommytypes.h"
#include "tommyalloc.h"
#include "tommylist.h"

/******************************************************************************/
/* trie */

/**
 * Number of bits supported for trie keys.
 *
 * All keys must fit in TOMMY_TRIE_BIT bits, even if tommy_key_t is wider.
 * This requirement is checked with assert(), and is not enforced
 * when assertions are disabled.
 *
 * Increase this value to support larger keys.
 * Keeping it small improves trie performance.
 */
#define TOMMY_TRIE_BIT 32

/**
 * Number of branches on each inner node. It must be a power of 2.
 * Suggested values are 8, 16 and 32.
 * Any inner node, excluding leafs, contains a pointer to each branch.
 *
 * The default size is chosen to exactly fit a typical cache line of 64 bytes.
 */
#define TOMMY_TRIE_TREE_MAX (64 / sizeof(void*))

/**
 * Trie block size.
 * You must use this value to initialize the allocator.
 */
#define TOMMY_TRIE_BLOCK_SIZE (TOMMY_TRIE_TREE_MAX * sizeof(void*))

/** \internal
 * Number of bits for each branch.
 */
#define TOMMY_TRIE_TREE_BIT TOMMY_ILOG2(TOMMY_TRIE_TREE_MAX)

/** \internal
 * Number of bits of the first level.
 */
#define TOMMY_TRIE_BUCKET_BIT ((TOMMY_TRIE_BIT % TOMMY_TRIE_TREE_BIT) + TOMMY_TRIE_TREE_BIT)

/** \internal
 * Number of branches of the first level.
 * It's like an inner branch, but bigger to get any remainder bits.
 */
#define TOMMY_TRIE_BUCKET_MAX (1 << TOMMY_TRIE_BUCKET_BIT)

/**
 * Trie node.
 * This is the node that you have to include inside your objects.
 */
typedef tommy_node tommy_trie_node;

/**
 * Trie container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_trie_struct {
	tommy_trie_node* bucket[TOMMY_TRIE_BUCKET_MAX]; /**< First tree level. */
	tommy_size_t count; /**< Number of elements. */
	tommy_size_t node_count; /**< Number of nodes. */
	tommy_allocator* alloc; /**< Allocator for internal nodes. */
} tommy_trie;

/**
 * Initializes the trie.
 * You have to provide an allocator initialized with *both* the size and align with TOMMY_TRIE_BLOCK_SIZE.
 * You can share this allocator with other tries.
 *
 * The trie is completely allocated through the allocator, and it doesn't need to be deinitialized.
 * \param trie The trie to initialize.
 * \param alloc Allocator initialized with *both* the size and align with TOMMY_TRIE_BLOCK_SIZE.
 */
TOMMY_API void tommy_trie_init(tommy_trie* trie, tommy_allocator* alloc);

/**
 * Exchanges the complete state of two initialized tries, including their allocator references.
 * Each allocator reference moves with its internal nodes so subsequent operations use the associated allocator.
 * The tries must not share nodes. Nodes and objects are not accessed or modified.
 * Existing node pointers remain valid and belong to the other trie. Referenced allocators must remain initialized.
 * Passing the same trie twice has no effect. Both tries remain usable; their memory is released through their allocators.
 * \param first The first trie.
 * \param second The second trie.
 * \note This operation is O(1) with respect to the number of elements.
 */
tommy_inline void tommy_trie_swap(tommy_trie* first, tommy_trie* second)
{
	tommy_trie tmp = *first;
	*first = *second;
	*second = tmp;
}

/**
 * Removes all elements, preserving the allocator reference.
 * The trie remains initialized and can be reused immediately with the same allocator.
 * Objects are not freed and nodes are not accessed or modified.
 * Their links must not be used to traverse the previous contents.
 * You can call this function after tommy_trie_foreach() has freed the objects.
 * Internal nodes are returned to the allocator, leaving the trie empty.
 * Other tries sharing the allocator are unaffected.
 * No memory is allocated; free blocks remain available for reuse in the allocator.
 * \param trie The trie to clear.
 * \note This operation is O(n), with recursion depth bounded by the number of key bits.
 */
TOMMY_API void tommy_trie_clear(tommy_trie* trie);

/**
 * Inserts an element in the trie.
 * You have to provide the pointer of the node embedded into the object,
 * the pointer to the object and the key to use.
 * \param trie The trie.
 * \param node Pointer to the node embedded into the object to insert.
 * \param data Pointer to the object to insert.
 * \param key Key to use to insert the object.
 */
TOMMY_API void tommy_trie_insert(tommy_trie* trie, tommy_trie_node* node, void* data, tommy_key_t key);

/**
 * Inserts an element only if no element with the same numeric key is already contained.
 * If found, the first element with that key in insertion order is returned.
 * The candidate node and trie, including its allocator, are left unchanged, and no memory is allocated.
 * Otherwise, the candidate is inserted using the normal insertion policy and its data field is returned.
 * \param trie The trie.
 * \param node The candidate node. It must not belong to any container.
 * \param data Pointer to the object to insert.
 * \param key Numeric key, which must fit within ::TOMMY_TRIE_BIT bits.
 * \return The first matching element's data field, or data if the candidate was inserted.
 */
TOMMY_API void* tommy_trie_insert_unique(tommy_trie* trie, tommy_trie_node* node, void* data, tommy_key_t key);

/**
 * Searches and removes the first element with the specified key.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * This operation is faster than calling tommy_trie_bucket() and tommy_trie_remove_existing() separately.
 * \param trie The trie.
 * \param key Key of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
TOMMY_API void* tommy_trie_remove(tommy_trie* trie, tommy_key_t key);

/**
 * Gets the bucket of the specified key.
 * The bucket is guaranteed to contain ALL and ONLY the elements with the specified key.
 * You can access elements in the bucket following the tommy_node::next pointer until 0.
 * \param trie The trie.
 * \param key Key of the element to find.
 * \return The head of the bucket, or 0 if empty.
 */
TOMMY_API tommy_trie_node* tommy_trie_bucket(tommy_trie* trie, tommy_key_t key);

/**
 * Searches an element in the trie.
 * You have to provide the key of the element you want to find.
 * If more elements with the same key are present, the first one is returned.
 * \param trie The trie.
 * \param key Key of the element to find.
 * \return The first element found, or 0 if none.
 */
tommy_inline void* tommy_trie_search(tommy_trie* trie, tommy_key_t key)
{
	tommy_trie_node* i = tommy_trie_bucket(trie, key);

	if (!i)
		return 0;

	return i->data;
}

/**
 * Removes an element from the trie.
 * You must already have the address of the element to remove.
 * \param trie The trie.
 * \param node The node to remove.
 * \return The tommy_node::data field of the node removed.
 */
TOMMY_API void* tommy_trie_remove_existing(tommy_trie* trie, tommy_trie_node* node);

/**
 * Transfers all elements from the trie to the tail of a list by increasing numeric key.
 * Elements with equal keys retain their insertion order. Existing list elements
 * remain before the transferred elements; an initially empty list is therefore sorted.
 * The list must be initialized and must not share nodes with the trie.
 * Objects are not freed, and the node data and index fields are left unchanged.
 * Internal nodes are returned to the allocator, leaving the trie empty and
 * reusable with the same allocator. Other tries sharing the allocator are unaffected.
 * No memory is allocated; free blocks remain available for reuse in the allocator.
 * \param trie The trie to drain.
 * \param list The destination list.
 * \note This operation is O(n), with recursion depth bounded by the number of key bits.
 */
TOMMY_API void tommy_trie_to_list(tommy_trie* trie, tommy_list* list);

/**
 * Calls the specified function for each element in the trie.
 * Elements are visited by increasing key, with equal keys in insertion order.
 * The callback receives the data field of each node.
 * An empty trie does not invoke the callback.
 *
 * The callback may deallocate the current object, including its embedded node.
 * It must not add or remove elements, modify keys or node links, or deallocate
 * other elements, the trie, or its allocator.
 * This operation does not remove elements or update the count.
 * After deallocating objects, discard or reinitialize the trie before using it again.
 * Internal nodes remain allocated until released through the allocator.
 *
 * No memory is allocated. Recursion depth is bounded by the number of key bits.
 */
TOMMY_API void tommy_trie_foreach(tommy_trie* trie, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the trie.
 * The iteration order and callback rules are the same as tommy_trie_foreach().
 * The callback receives arg followed by the data field of each node.
 */
TOMMY_API void tommy_trie_foreach_arg(tommy_trie* trie, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the number of elements.
 */
tommy_inline tommy_size_t tommy_trie_count(const tommy_trie* trie)
{
	return trie->count;
}

/**
 * Checks if empty in O(1) time.
 * \return If the trie contains no elements.
 */
tommy_inline tommy_bool_t tommy_trie_empty(const tommy_trie* trie)
{
	return trie->count == 0;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::tommy_trie_node of the stored elements.
 */
TOMMY_API tommy_size_t tommy_trie_memory_usage(const tommy_trie* trie);

#endif

