// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommytrieinp.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* trie_inplace */

/**
 * Create a new list with a single element.
 */
tommy_inline tommy_trie_inplace_node* tommy_trie_inplace_list_insert_first(tommy_trie_inplace_node* node)
{
	/* one element "circular" prev list */
	node->prev = node;

	/* one element "0 terminated" next list */
	node->next = 0;

	return node;
}

/**
 * Add an element to an existing list.
 * \note The element is inserted at the end of the list.
 */
tommy_inline void tommy_trie_inplace_list_insert_tail_not_empty(tommy_trie_inplace_node* head, tommy_trie_inplace_node* node)
{
	/* insert in the list in the last position */

	/* insert in the "circular" prev list */
	node->prev = head->prev;
	head->prev = node;

	/* insert in the "0 terminated" next list */
	node->next = 0;
	node->prev->next = node;
}

/**
 * Remove an element from the list.
 */
tommy_inline void tommy_trie_inplace_list_remove(tommy_trie_inplace_node** let_ptr, tommy_trie_inplace_node* node)
{
	tommy_trie_inplace_node* head = *let_ptr;

	/* remove from the "circular" prev list */
	if (node->next)
		node->next->prev = node->prev;
	else
		head->prev = node->prev; /* the last */

	/* remove from the "0 terminated" next list */
	if (head == node)
		*let_ptr = node->next; /* the new first */
	else
		node->prev->next = node->next;
}

TOMMY_API void tommy_trie_inplace_init(tommy_trie_inplace* trie_inplace)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_BUCKET_MAX; ++i)
		trie_inplace->bucket[i] = 0;

	trie_inplace->count = 0;
}

tommy_inline void trie_inplace_bucket_insert(int shift, tommy_trie_inplace_node** let_ptr, tommy_trie_inplace_node* insert, tommy_key_t key)
{
	tommy_trie_inplace_node* node = *let_ptr;

	while (node && node->key != key) {
		let_ptr = &node->map[(key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK];
		node = *let_ptr;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	/* if null, just insert the node */
	if (!node) {
		/* only the head uses child pointers; promotion copies them to a duplicate */
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_TREE_MAX; ++i)
			insert->map[i] = 0;

		/* setup the node as a list */
		*let_ptr = tommy_trie_inplace_list_insert_first(insert);
	} else {
		/* if it's the same key, insert in the list */
		tommy_trie_inplace_list_insert_tail_not_empty(node, insert);
	}
}

TOMMY_API void tommy_trie_inplace_insert(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node, void* data, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	node->data = data;
	node->key = key;

	tommy_trie_inplace_node** let_ptr = &trie_inplace->bucket[key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT];

	trie_inplace_bucket_insert(TOMMY_TRIE_INPLACE_TREE_SHIFT, let_ptr, node, key);

	++trie_inplace->count;
}

TOMMY_API void* tommy_trie_inplace_insert_unique(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node, void* data, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node** let_ptr = &trie_inplace->bucket[key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT];
	tommy_trie_inplace_node* existing = *let_ptr;
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (existing && existing->key != key) {
		let_ptr = &existing->map[(key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK];
		existing = *let_ptr;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	/* delay candidate initialization so rejected child pointers remain unchanged */
	if (existing)
		return existing->data;

	node->data = data;
	node->key = key;
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_TREE_MAX; ++i)
		node->map[i] = 0;

	/* use the empty child pointer found by the search without traversing again */
	*let_ptr = tommy_trie_inplace_list_insert_first(node);
	++trie_inplace->count;

	return data;
}

tommy_inline tommy_trie_inplace_node* trie_inplace_bucket_remove(int shift, tommy_trie_inplace_node** let_ptr, tommy_trie_inplace_node* remove, tommy_key_t key)
{
	tommy_trie_inplace_node* node = *let_ptr;

	while (node && node->key != key) {
		let_ptr = &node->map[(key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK];
		node = *let_ptr;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	if (!node)
		return 0;

	/* if the node to remove is not specified */
	if (!remove)
		remove = node; /* remove the first */

	tommy_trie_inplace_list_remove(let_ptr, remove);

	/* if not change in the node, nothing more to do */
	if (*let_ptr == node)
		return remove;

	/* if we have a substitute */
	if (*let_ptr != 0) {
		/* copy the child pointers to the new one */
		node = *let_ptr;
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_TREE_MAX; ++i)
			node->map[i] = remove->map[i];

		return remove;
	}

	/* find a leaf */
	tommy_trie_inplace_node** leaf_let_ptr = 0;
	tommy_trie_inplace_node* leaf = remove;

	/* search backward, statistically we have more zeros than ones */
	int i = TOMMY_TRIE_INPLACE_TREE_MAX - 1;
	while (i >= 0) {
		if (leaf->map[i]) {
			leaf_let_ptr = &leaf->map[i];
			leaf = *leaf_let_ptr;
			i = TOMMY_TRIE_INPLACE_TREE_MAX - 1;
			continue;
		}
		--i;
	}

	/* if it's itself a leaf */
	if (!leaf_let_ptr)
		return remove;

	/* remove the leaf */
	*leaf_let_ptr = 0;

	/* copy the child pointers */
	for (tommy_uint_t j = 0; j < TOMMY_TRIE_INPLACE_TREE_MAX; ++j)
		leaf->map[j] = remove->map[j];

	/* put it in place */
	*let_ptr = leaf;

	return remove;
}

TOMMY_API void* tommy_trie_inplace_remove(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node** let_ptr = &trie_inplace->bucket[key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT];

	tommy_trie_inplace_node* ret = trie_inplace_bucket_remove(TOMMY_TRIE_INPLACE_TREE_SHIFT, let_ptr, 0, key);

	if (!ret)
		return 0;

	--trie_inplace->count;

	return ret->data;
}

TOMMY_API void* tommy_trie_inplace_remove_existing(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node)
{
	tommy_key_t key = node->key;

	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node** let_ptr = &trie_inplace->bucket[key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT];

	tommy_trie_inplace_node* ret = trie_inplace_bucket_remove(TOMMY_TRIE_INPLACE_TREE_SHIFT, let_ptr, node, key);

	/* the element removed must match the one passed */
	assert(ret == node);

	--trie_inplace->count;

	return ret->data;
}

tommy_inline tommy_trie_inplace_node* trie_inplace_head_node(tommy_trie_inplace_node* node)
{
	tommy_trie_inplace_node* candidate = node;
	while (1) {
		/* internal nodes also hold keys, which can precede all their children */
		if (node->key < candidate->key)
			candidate = node;

		tommy_uint_t i = 0;
		while (i < TOMMY_TRIE_INPLACE_TREE_MAX && !node->map[i])
			++i;
		if (i == TOMMY_TRIE_INPLACE_TREE_MAX)
			return candidate;
		node = node->map[i];
	}
}

tommy_inline tommy_trie_inplace_node* trie_inplace_tail_node(tommy_trie_inplace_node* node)
{
	tommy_trie_inplace_node* candidate = node;
	while (1) {
		/* internal nodes also hold keys, which can follow all their children */
		if (node->key > candidate->key)
			candidate = node;

		tommy_uint_t i = TOMMY_TRIE_INPLACE_TREE_MAX;
		while (i != 0 && !node->map[i - 1])
			--i;
		if (i == 0)
			return candidate->prev;
		node = node->map[i - 1];
	}
}

tommy_inline void* trie_inplace_search_less(tommy_trie_inplace* trie_inplace, tommy_key_t key, tommy_bool_t equal)
{
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node* candidate = 0;
	tommy_trie_inplace_node* branch = 0;
	tommy_trie_inplace_node** map = trie_inplace->bucket;
	tommy_uint_t pos = key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (1) {
		/* a smaller sibling deeper on the key path is closer than any earlier branch */
		for (tommy_uint_t i = pos; i != 0; ) {
			--i;
			if (map[i]) {
				branch = map[i];
				break;
			}
		}

		tommy_trie_inplace_node* node = map[pos];
		if (!node)
			break;
		if (equal && node->key == key)
			return node->prev->data;
		/* keys on the path are not ordered and compete with the saved branch */
		if (node->key < key && (!candidate || node->key > candidate->key))
			candidate = node->prev;

		/* a strict search continues below equal keys; never shift past the last key bit */
		if (shift < 0)
			break;
		map = node->map;
		pos = (key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	if (branch) {
		tommy_trie_inplace_node* node = trie_inplace_tail_node(branch);
		if (!candidate || node->key > candidate->key)
			candidate = node;
	}
	return candidate ? candidate->data : 0;
}

tommy_inline void* trie_inplace_search_greater(tommy_trie_inplace* trie_inplace, tommy_key_t key, tommy_bool_t equal)
{
	assert(key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT < TOMMY_TRIE_INPLACE_BUCKET_MAX);

	tommy_trie_inplace_node* candidate = 0;
	tommy_trie_inplace_node* branch = 0;
	tommy_trie_inplace_node** map = trie_inplace->bucket;
	tommy_uint_t size = TOMMY_TRIE_INPLACE_BUCKET_MAX;
	tommy_uint_t pos = key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (1) {
		/* a larger sibling deeper on the key path is closer than any earlier branch */
		for (tommy_uint_t i = pos + 1; i < size; ++i) {
			if (map[i]) {
				branch = map[i];
				break;
			}
		}

		tommy_trie_inplace_node* node = map[pos];
		if (!node)
			break;
		if (equal && node->key == key)
			return node->data;
		/* keys on the path are not ordered and compete with the saved branch */
		if (node->key > key && (!candidate || node->key < candidate->key))
			candidate = node;

		/* a strict search continues below equal keys; never shift past the last key bit */
		if (shift < 0)
			break;
		map = node->map;
		size = TOMMY_TRIE_INPLACE_TREE_MAX;
		pos = (key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	if (branch) {
		tommy_trie_inplace_node* node = trie_inplace_head_node(branch);
		if (!candidate || node->key < candidate->key)
			candidate = node;
	}
	return candidate ? candidate->data : 0;
}

TOMMY_API void* tommy_trie_inplace_search_less(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	return trie_inplace_search_less(trie_inplace, key, 0);
}

TOMMY_API void* tommy_trie_inplace_search_less_equal(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	return trie_inplace_search_less(trie_inplace, key, 1);
}

TOMMY_API void* tommy_trie_inplace_search_greater_equal(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	return trie_inplace_search_greater(trie_inplace, key, 1);
}

TOMMY_API void* tommy_trie_inplace_search_greater(tommy_trie_inplace* trie_inplace, tommy_key_t key)
{
	return trie_inplace_search_greater(trie_inplace, key, 0);
}

TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_head(tommy_trie_inplace* trie_inplace)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_BUCKET_MAX; ++i)
		if (trie_inplace->bucket[i])
			return trie_inplace_head_node(trie_inplace->bucket[i]);

	return 0;
}

TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_tail(tommy_trie_inplace* trie_inplace)
{
	for (tommy_uint_t i = TOMMY_TRIE_INPLACE_BUCKET_MAX; i != 0; ) {
		--i;
		if (trie_inplace->bucket[i])
			return trie_inplace_tail_node(trie_inplace->bucket[i]);
	}

	return 0;
}

TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_next(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node)
{
	if (node->next)
		return node->next;

	tommy_key_t key = node->key;
	tommy_trie_inplace_node* candidate = 0;
	tommy_trie_inplace_node* branch = 0;
	tommy_trie_inplace_node** map = trie_inplace->bucket;
	tommy_uint_t size = TOMMY_TRIE_INPLACE_BUCKET_MAX;
	tommy_uint_t pos = key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (1) {
		/* a larger sibling deeper on the key path is closer than any earlier branch */
		for (tommy_uint_t i = pos + 1; i < size; ++i) {
			if (map[i]) {
				branch = map[i];
				break;
			}
		}

		node = map[pos];
		if (!node)
			break;
		/* keys on the path are not ordered and compete with the saved branch */
		if (node->key > key && (!candidate || node->key < candidate->key))
			candidate = node;

		/* continue below the matching node, but never shift after consuming all key bits */
		if (shift < 0)
			break;
		map = node->map;
		size = TOMMY_TRIE_INPLACE_TREE_MAX;
		pos = (key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	if (branch) {
		node = trie_inplace_head_node(branch);
		if (!candidate || node->key < candidate->key)
			candidate = node;
	}
	return candidate;
}

TOMMY_API tommy_trie_inplace_node* tommy_trie_inplace_prev(tommy_trie_inplace* trie_inplace, tommy_trie_inplace_node* node)
{
	/* only the first duplicate has prev pointing to the null-terminated tail */
	if (node->prev->next)
		return node->prev;

	tommy_key_t key = node->key;
	tommy_trie_inplace_node* candidate = 0;
	tommy_trie_inplace_node* branch = 0;
	tommy_trie_inplace_node** map = trie_inplace->bucket;
	tommy_uint_t pos = key >> TOMMY_TRIE_INPLACE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_INPLACE_TREE_SHIFT;

	while (1) {
		/* a smaller sibling deeper on the key path is closer than any earlier branch */
		for (tommy_uint_t i = pos; i != 0; ) {
			--i;
			if (map[i]) {
				branch = map[i];
				break;
			}
		}

		node = map[pos];
		if (!node)
			break;
		/* select the last duplicate when a key on the path is the best predecessor */
		if (node->key < key && (!candidate || node->key > candidate->key))
			candidate = node->prev;

		/* continue below the matching node, but never shift after consuming all key bits */
		if (shift < 0)
			break;
		map = node->map;
		pos = (key >> shift) & TOMMY_TRIE_INPLACE_TREE_MASK;
		shift -= TOMMY_TRIE_INPLACE_TREE_BIT;
	}

	if (branch) {
		node = trie_inplace_tail_node(branch);
		if (!candidate || node->key > candidate->key)
			candidate = node;
	}
	return candidate;
}

/**
 * Iterator frame merging a node's bucket with its ordered child buckets.
 */
typedef struct trie_inplace_subtree_iterator_struct {
	tommy_trie_inplace_node* map[TOMMY_TRIE_INPLACE_TREE_MAX];
	tommy_trie_inplace_node* node;
	tommy_trie_inplace_node* next;
	tommy_uint_t branch;
	tommy_bool_t active;
} trie_inplace_subtree_iterator;

/**
 * Maximum number of edges below the initial bucket.
 */
#define TOMMY_TRIE_INPLACE_LEVEL_MAX ((TOMMY_TRIE_INPLACE_BIT - TOMMY_TRIE_INPLACE_BUCKET_BIT) / TOMMY_TRIE_INPLACE_TREE_BIT)

tommy_inline void trie_inplace_subtree_iterator_init(trie_inplace_subtree_iterator* iterator, tommy_trie_inplace_node* node)
{
	/* callbacks can free the node before all its children have been visited */
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_TREE_MAX; ++i)
		iterator->map[i] = node->map[i];
	iterator->node = node;
	iterator->next = 0;
	iterator->branch = 0;
	iterator->active = 0;
}

tommy_inline tommy_trie_inplace_node* trie_inplace_subtree_iterator_next(trie_inplace_subtree_iterator* stack)
{
	trie_inplace_subtree_iterator* iterator = stack;
	while (1) {
		/* child branches cover disjoint key ranges in increasing order */
		if (!iterator->next && (iterator->active || iterator->branch < TOMMY_TRIE_INPLACE_TREE_MAX)) {
			if (!iterator->active) {
				tommy_trie_inplace_node* child = iterator->map[iterator->branch];
				++iterator->branch;
				if (!child)
					continue;
				trie_inplace_subtree_iterator_init(iterator + 1, child);
				iterator->active = 1;
			}
			++iterator;
			continue;
		}

		/* retain the next child bucket while inserting the parent's bucket before it */
		tommy_trie_inplace_node* node;
		if (iterator->node && (!iterator->next || iterator->node->key < iterator->next->key)) {
			node = iterator->node;
			iterator->node = 0;
		} else {
			node = iterator->next;
			iterator->next = 0;
		}
		if (iterator == stack)
			return node;

		/* propagate the child's result to its parent without a recursive return */
		--iterator;
		iterator->next = node;
		if (!node)
			iterator->active = 0;
	}
}

TOMMY_API void tommy_trie_inplace_foreach(tommy_trie_inplace* trie_inplace, tommy_foreach_func* func)
{
	trie_inplace_subtree_iterator stack[TOMMY_TRIE_INPLACE_LEVEL_MAX + 1];
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_BUCKET_MAX; ++i) {
		if (!trie_inplace->bucket[i])
			continue;
		trie_inplace_subtree_iterator_init(stack, trie_inplace->bucket[i]);
		tommy_trie_inplace_node* node;
		while ((node = trie_inplace_subtree_iterator_next(stack)) != 0) {
			while (node) {
				void* data = node->data;
				/* save the next duplicate before the callback can free this node */
				node = node->next;
				func(data);
			}
		}
	}
}

TOMMY_API void tommy_trie_inplace_foreach_arg(tommy_trie_inplace* trie_inplace, tommy_foreach_arg_func* func, void* arg)
{
	trie_inplace_subtree_iterator stack[TOMMY_TRIE_INPLACE_LEVEL_MAX + 1];
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_INPLACE_BUCKET_MAX; ++i) {
		if (!trie_inplace->bucket[i])
			continue;
		trie_inplace_subtree_iterator_init(stack, trie_inplace->bucket[i]);
		tommy_trie_inplace_node* node;
		while ((node = trie_inplace_subtree_iterator_next(stack)) != 0) {
			while (node) {
				void* data = node->data;
				/* save the next duplicate before the callback can free this node */
				node = node->next;
				func(arg, data);
			}
		}
	}
}

TOMMY_API tommy_size_t tommy_trie_inplace_memory_usage(const tommy_trie_inplace* trie_inplace)
{
	return tommy_trie_inplace_count(trie_inplace) * (tommy_size_t)sizeof(tommy_trie_inplace_node);
}

