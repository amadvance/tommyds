// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \file
 * Inplace trie.
 *
 * This trie is an inplace implementation not needing any external allocation.
 *
 * Unlike ::tommy_trie, elements also represent the inner nodes and are not
 * stored in key order. Ordered traversal merges each node's elements with
 * its child subtrees.
 *
 * You can control the number of branches of each node using the ::TOMMY_TRIE_INPLACE_TREE_MAX define.
 * More branches imply more speed, but a bigger memory occupation.
 *
 * Compared to ::tommy_trie you should use a lower number of branches to limit the unused memory
 * occupation of the leaf nodes. This implies a lower speed, but without the need of an external allocator.
 *
 * To initialize the trie you have to call tommy_trie_inplace_init().
 *
 * \code
 * tommy_trie_inplace trie_inplace;
 *
 * tommy_trie_inplace_init(&trie_inplace);
 * \endcode
 *
 * To insert elements in the trie you have to call tommy_trie_inplace_insert() for
 * each element.
 * In the insertion call you have to specify the address of the node, the
 * address of the object, and the key value to use.
 * The address of the object is used to initialize the tommy_trie_inplace_node::data field
 * of the node, and the key to initialize the tommy_trie_inplace_node::key field.
 *
 * \code
 * struct object {
 *     int value;
 *     // other fields
 *     tommy_trie_inplace_node node;
 * };
 *
 * struct object* obj = malloc(sizeof(struct object)); // creates the object
 *
 * obj->value = ...; // initializes the object
 *
 * tommy_trie_inplace_insert(&trie_inplace, &obj->node, obj, obj->value); // inserts the object
 * \endcode
 *
 * To find an element in the trie you have to call tommy_trie_inplace_search() providing
 * the key to search.
 *
 * \code
 * int value_to_find = 1;
 * struct object* obj = tommy_trie_inplace_search(&trie_inplace, value_to_find);
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the trie with the same key, you have to
 * use tommy_trie_inplace_bucket() and follow the tommy_trie_inplace_node::next pointer until 0.
 *
 * \code
 * int value_to_find = 1;
 * tommy_trie_inplace_node* i = tommy_trie_inplace_bucket(&trie_inplace, value_to_find);
 * while (i) {
 *     struct object* obj = i->data; // gets the object pointer
 *
 *     printf("%d\n", obj->value); // process the object
 *
 *     i = i->next; // goes to the next element
 * }
 * \endcode
 *
 * To iterate over all elements by increasing key, start with tommy_trie_inplace_head()
 * and advance with tommy_trie_inplace_next(). Equal keys are visited in insertion order.
 * Use tommy_trie_inplace_tail() and tommy_trie_inplace_prev() for the reverse order.
 *
 * \code
 * tommy_trie_inplace_node* i = tommy_trie_inplace_head(&trie_inplace);
 * while (i) {
 *     struct object* obj = i->data;
 *     printf("%d\n", obj->value);
 *     i = tommy_trie_inplace_next(&trie_inplace, i);
 * }
 * \endcode
 *
 * \anchor tommy_trie_inplace_iterator_validity
 * Iterators are pointers to nodes contained in the trie.
 * Inserting elements or removing other nodes between iterator calls does not
 * invalidate pointers to nodes that remain in the trie.
 * Subsequent next/prev calls use the current contents and ordering;
 * inserted elements may become visible and removed elements are skipped.
 *
 * Removing the pointed node invalidates its use as an iterator, even if
 * its storage has not been freed. To continue traversal after removing
 * the current node, obtain the next or previous node before removal.
 *
 * Keys and fields used for ordering must not be modified while contained.
 * Concurrent traversal and modification require external synchronization.
 *
 * The container passed to next/prev must currently contain the node.
 * After swapping containers, use the container that now owns the node.
 *
 * To remove an element from the trie you have to call tommy_trie_inplace_remove()
 * providing the key to search and remove.
 *
 * \code
 * struct object* obj = tommy_trie_inplace_remove(&trie_inplace, value_to_remove);
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To visit all the elements use tommy_trie_inplace_foreach() or
 * tommy_trie_inplace_foreach_arg(). Elements are visited by increasing key,
 * with equal keys in insertion order.
 *
 * To destroy the trie you can deallocate all the objects with
 * tommy_trie_inplace_foreach(), as the trie doesn't allocate memory.
 *
 * \code
 * tommy_trie_inplace_foreach(&trie_inplace, free);
 * \endcode
 */

#ifndef __TOMMYTRIEINP_H
#define __TOMMYTRIEINP_H

#include "tommytypes.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* trie_inplace */

/**
 * Number of bits supported for inplace trie keys.
 *
 * All keys must fit in TOMMY_TRIE_INPLACE_BIT bits, even if tommy_key_t is wider.
 * This requirement is checked with assert(), and is not enforced
 * when assertions are disabled.
 *
 * Increase this value to support larger keys.
 * Keeping it small improves trie performance.
 */
#define TOMMY_TRIE_INPLACE_BIT 32

/**
 * Number of branches on each node. It must be a power of 2.
 * Suggested values are 2, 4 and 8.
 * Any node, including leafs, contains a pointer to each branch.
 */
#define TOMMY_TRIE_INPLACE_TREE_MAX 4

/** \internal
 * Number of bits for each branch.
 */
#define TOMMY_TRIE_INPLACE_TREE_BIT TOMMY_ILOG2(TOMMY_TRIE_INPLACE_TREE_MAX)

/** \internal
 * Number of bits of the first level.
 */
#define TOMMY_TRIE_INPLACE_BUCKET_BIT ((TOMMY_TRIE_INPLACE_BIT % TOMMY_TRIE_INPLACE_TREE_BIT) + 3 * TOMMY_TRIE_INPLACE_TREE_BIT)

/** \internal
 * Number of branches of the first level.
 * It's like an inner branch, but bigger to get any remainder bits.
 */
#define TOMMY_TRIE_INPLACE_BUCKET_MAX (1 << TOMMY_TRIE_INPLACE_BUCKET_BIT)

/** \internal
 * Mask for the inner branches.
 */
#define TOMMY_TRIE_INPLACE_TREE_MASK (TOMMY_TRIE_INPLACE_TREE_MAX - 1)

/** \internal
 * Shift for the first level of branches.
 */
#define TOMMY_TRIE_INPLACE_BUCKET_SHIFT (TOMMY_TRIE_INPLACE_BIT - TOMMY_TRIE_INPLACE_BUCKET_BIT)

/** \internal
 * Shift for the first internal level, skipping bits already used by the bucket.
 * Traversal shifts are signed because the last descent consumes all key bits;
 * the resulting negative shift is never used on a leaf or empty child.
 */
#define TOMMY_TRIE_INPLACE_TREE_SHIFT (TOMMY_TRIE_INPLACE_BUCKET_SHIFT - TOMMY_TRIE_INPLACE_TREE_BIT)

/**
 * Trie node.
 * This is the node that you have to include inside your objects.
 */
typedef struct tommy_trie_inplace_node_struct {
	struct tommy_trie_inplace_node_struct* next; /**< Next element. 0 if it's the last. */
	struct tommy_trie_inplace_node_struct* prev; /**< Circular previous element. */
	void* data; /**< Pointer to the data. */
	struct tommy_trie_inplace_node_struct* map[TOMMY_TRIE_INPLACE_TREE_MAX]; /**< Branches of the node. */
	tommy_key_t key; /**< Used to store the key or the hash. */
} tommy_trie_inplace_node;

/**
 * Trie container type.
 * \note Don't use internal fields directly, but access the container only using functions.
 */
typedef struct tommy_trie_inplace_struct {
	tommy_trie_inplace_node* bucket[TOMMY_TRIE_INPLACE_BUCKET_MAX]; /**< First tree level. */
	tommy_size_t count; /**< Number of elements. */
} tommy_trie_inplace;

/**
 * Initializes the trie.
 *
 * The trie is completely inplace, and it doesn't need to be deinitialized.
 * \param trie_inplace The trie to initialize.
 */
TOMMY_API void tommy_trie_inplace_init(tommy_trie_inplace* trie_inplace);

/**
 * Exchanges the complete state of two initialized tries.
 * The tries must not share nodes. Nodes and objects are not accessed or modified.
 * Existing node pointers remain valid and belong to the other trie.
 * Passing the same trie twice has no effect. Both tries remain usable and require no deinitialization.
 * \param first The first trie.
 * \param second The second trie.
 * \note This operation is O(1) with respect to the number of elements.
 */
tommy_inline void tommy_trie_inplace_swap(tommy_trie_inplace* first, tommy_trie_inplace* second)
{
	tommy_trie_inplace tmp = *first;
	*first = *second;
	*second = tmp;
}

/**
 * Inserts an element in the trie.
 * \param trie_inplace The trie.
 * \param node Pointer to the node embedded into the object to insert.
 * \param data Pointer to the object to insert.
 * \param key Key to use to insert the object.
 */
TOMMY_API void tommy_trie_inplace_insert(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node, void* data, tommy_key_t key);

/**
 * Inserts an element only if no element with the same numeric key is already contained.
 * If found, the first element with that key in insertion order is returned.
 * The candidate node and trie are left unchanged, and no memory is allocated.
 * Otherwise, the candidate is inserted using the normal insertion policy and its data field is returned.
 * \param trie_inplace The trie.
 * \param node The candidate node. It must not belong to any container.
 * \param data Pointer to the object to insert.
 * \param key Numeric key, which must fit within ::TOMMY_TRIE_INPLACE_BIT bits.
 * \return The first matching element's data field, or data if the candidate was inserted.
 */
TOMMY_API void* tommy_trie_inplace_insert_unique(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node, void* data, tommy_key_t key);

/**
 * Searches and removes the first element with the specified key.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * This operation is faster than calling tommy_trie_inplace_bucket() and tommy_trie_inplace_remove_existing() separately.
 * \param trie_inplace The trie.
 * \param key Key of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
TOMMY_API void* tommy_trie_inplace_remove(tommy_trie_inplace* trie_inplace, tommy_key_t key);

/**
 * Gets the bucket of the specified key.
 * The bucket is guaranteed to contain ALL and ONLY the elements with the specified key.
 * You can access elements in the bucket following the tommy_trie_inplace_node::next pointer until 0.
 * \param trie_inplace The trie.
 * \param key Key of the element to find.
 * \return The head of the bucket, or 0 if empty.
 */
tommy_inline tommy_trie_inplace_node* tommy_trie_inplace_bucket(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node* node = trie_inplace->bucket[key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT];
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (node && node->key != key) {
		node = node->map[(key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK];
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	return node;
}

/**
 * Searches an element in the trie.
 * You have to provide the key of the element you want to find.
 * If more elements with the same key are present, the first one is returned.
 * \param trie_inplace The trie.
 * \param key Key of the element to find.
 * \return The first element found, or 0 if none.
 */
tommy_inline void* tommy_trie_inplace_search(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	tommy_trie_inplace_node* i = tommy_trie_inplace_bucket(trie_inplace, key);

	if (!i)
		return 0;

	return i->data;
}

/**
 * Removes an element from the trie.
 * You must already have the address of the element to remove.
 * \param trie_inplace The trie.
 * \param node The node to remove.
 * \return The tommy_trie_inplace_node::data field of the node removed.
 */
TOMMY_API void* tommy_trie_inplace_remove_existing(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node);

/**
 * Gets the head (smallest key) node in the trie.
 * If multiple elements have the smallest key, the first in insertion order is returned.
 * No memory is allocated or modified; auxiliary space is O(1).
 * \param trie_inplace The trie.
 * \return The head node, or 0 if the trie is empty.
 * \note Traversal is bounded by the number of key bits and branches per level.
 */
TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_head(tommy_trie_inplace* trie_inplace);

/**
 * Gets the tail (greatest key) node in the trie.
 * If multiple elements have the greatest key, the last in insertion order is returned.
 * No memory is allocated or modified; auxiliary space is O(1).
 * \param trie_inplace The trie.
 * \return The tail node, or 0 if the trie is empty.
 * \note Traversal is bounded by the number of key bits and branches per level.
 */
TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_tail(tommy_trie_inplace* trie_inplace);

/**
 * Gets the node following the specified one by increasing numeric key.
 * Elements with equal keys are visited in insertion order.
 * No memory is allocated or modified; auxiliary space is O(1).
 * See \ref tommy_trie_inplace_iterator_validity for iterator validity across insertions and removals.
 * \param trie_inplace The trie containing the node.
 * \param node Node currently contained in the trie. It must not be 0.
 * \return The following node, or 0 for the tail node.
 * \note Moving within duplicates is O(1). Moving to another key traverses
 * the trie from the root, bounded by the number of key bits and branches per level.
 */
TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_next(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node);

/**
 * Gets the node preceding the specified one by increasing numeric key.
 * Elements with equal keys are visited in reverse insertion order.
 * No memory is allocated or modified; auxiliary space is O(1).
 * See \ref tommy_trie_inplace_iterator_validity for iterator validity across insertions and removals.
 * \param trie_inplace The trie containing the node.
 * \param node Node currently contained in the trie. It must not be 0.
 * \return The preceding node, or 0 for the head node.
 * \note Moving within duplicates is O(1). Moving to another key traverses
 * the trie from the root, bounded by the number of key bits and branches per level.
 */
TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_prev(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node);

/**
 * Calls the specified function for each element in the trie.
 * Elements are visited by increasing key, with equal keys in insertion order.
 * The callback receives the data field of each node.
 * An empty trie does not invoke the callback.
 *
 * The callback may deallocate the current object, including its embedded node.
 * It must not add or remove elements, modify keys or node links, or deallocate
 * other elements or the trie.
 * This operation does not remove elements or update the count.
 * After deallocating objects, discard or reinitialize the trie before using it again.
 *
 * No memory is allocated. Stack space and recursion depth are bounded by the
 * number of key bits.
 */
TOMMY_API void tommy_trie_inplace_foreach(tommy_trie_inplace* trie_inplace, tommy_foreach_func* func);

/**
 * Calls the specified function with an argument for each element in the trie.
 * The iteration order and callback rules are the same as tommy_trie_inplace_foreach().
 * The callback receives arg followed by the data field of each node.
 */
TOMMY_API void tommy_trie_inplace_foreach_arg(tommy_trie_inplace* trie_inplace, tommy_foreach_arg_func* func, void* arg);

/**
 * Gets the number of elements.
 */
tommy_inline tommy_size_t tommy_trie_inplace_count(const tommy_trie_inplace* trie_inplace)
{
	return trie_inplace->count;
}

/**
 * Checks if empty in O(1) time.
 * \return If the trie contains no elements.
 */
tommy_inline tommy_bool_t tommy_trie_inplace_empty(const tommy_trie_inplace* trie_inplace)
{
	return trie_inplace->count == 0;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::tommy_trie_inplace_node of the stored elements.
 */
TOMMY_API tommy_size_t tommy_trie_inplace_memory_usage(const tommy_trie_inplace* trie_inplace);

#endif

