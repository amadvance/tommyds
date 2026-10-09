// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

#include "tommytrie.h"
#include "tommylist.h"

#include <assert.h> /* for assert */

/******************************************************************************/
/* trie */

/**
 * Mask for the inner branches.
 */
#define TOMMY_TRIE_TREE_MASK (TOMMY_TRIE_TREE_MAX - 1)

/**
 * Shift for the first level of branches.
 */
#define TOMMY_TRIE_BUCKET_SHIFT (TOMMY_TRIE_BIT - TOMMY_TRIE_BUCKET_BIT)

/**
 * Shift for the first internal level, skipping bits already used by the bucket.
 * Traversal shifts are signed because the last descent consumes all key bits;
 * the resulting negative shift is never used on a leaf or empty child.
 */
#define TOMMY_TRIE_TREE_SHIFT (TOMMY_TRIE_BUCKET_SHIFT - TOMMY_TRIE_TREE_BIT)

/**
 * Max number of internal levels below the initial bucket.
 */
#define TOMMY_TRIE_LEVEL_MAX ((TOMMY_TRIE_BIT - TOMMY_TRIE_BUCKET_BIT) / TOMMY_TRIE_TREE_BIT)

/**
 * Hashtrie tree.
 * A tree contains TOMMY_TRIE_TREE_MAX ordered pointers to <null/node/tree>.
 *
 * Each tree level uses exactly TOMMY_TRIE_TREE_BIT bits from the key.
 */
struct tommy_trie_tree_struct {
	tommy_trie_node* map[TOMMY_TRIE_TREE_MAX];
};
typedef struct tommy_trie_tree_struct tommy_trie_tree;

/**
 * Kinds of an trie node.
 */
#define TOMMY_TRIE_TYPE_NODE 0 /**< The node is of type ::tommy_trie_node. */
#define TOMMY_TRIE_TYPE_TREE 1 /**< The node is of type ::tommy_trie_tree. */

/**
 * Get and set pointer of trie nodes.
 *
 * The pointer type is stored in the lower bit.
 */
#define trie_get_type(ptr) (((tommy_uintptr_t)(ptr)) & 1)
#define trie_get_tree(ptr) ((tommy_trie_tree*)(((tommy_uintptr_t)(ptr)) - TOMMY_TRIE_TYPE_TREE))
#define trie_set_tree(ptr) (void*)(((tommy_uintptr_t)(ptr)) + TOMMY_TRIE_TYPE_TREE)

TOMMY_API void tommy_trie_init(tommy_trie* trie, tommy_allocator* alloc)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i)
		trie->bucket[i] = 0;

	trie->count = 0;
	trie->node_count = 0;

	trie->alloc = alloc;
}

static void trie_clear_node(tommy_trie* trie, tommy_trie_node* node)
{
	if (!node)
		return;

	if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_TREE_MAX; ++i)
			trie_clear_node(trie, tree->map[i]);
		/* freeing the parent overwrites its branches, so transfer all children first */
		tommy_allocator_free(trie->alloc, tree);
	}
}

TOMMY_API void tommy_trie_clear(tommy_trie* trie)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_clear_node(trie, trie->bucket[i]);
		trie->bucket[i] = 0;
	}
	trie->count = 0;
	trie->node_count = 0;
}

static void trie_bucket_insert(tommy_trie* trie, int shift, tommy_trie_node** let_ptr, tommy_trie_node* insert, tommy_key_t key)
{
	tommy_trie_tree* tree;
	tommy_trie_node* node;
	void* ptr;
	tommy_uint_t i;
	tommy_uint_t j;

recurse:
	ptr = *let_ptr;

	/* if null, just insert the node */
	if (!ptr) {
		/* setup the node as a list */
		tommy_list_insert_first(let_ptr, insert);
		return;
	}

	if (trie_get_type(ptr) == TOMMY_TRIE_TYPE_TREE) {
		/* repeat the process one level down */
		let_ptr = &trie_get_tree(ptr)->map[(key >> shift) & TOMMY_TRIE_TREE_MASK];
		shift -= TOMMY_TRIE_TREE_BIT;
		goto recurse;
	}

	node = tommy_cast(tommy_trie_node*, ptr);

	/* if it's the same key, insert in the list */
	if (node->index == key) {
		tommy_list_insert_tail_not_empty(node, insert);
		return;
	}

expand:
	/* convert to a tree */
	tree = tommy_cast(tommy_trie_tree*, tommy_allocator_alloc(trie->alloc));
	++trie->node_count;
	*let_ptr = tommy_cast(tommy_trie_node*, trie_set_tree(tree));

	/* initialize it */
	for (i = 0; i < TOMMY_TRIE_TREE_MAX; ++i)
		tree->map[i] = 0;

	/* get the position of the two elements */
	i = (node->index >> shift) & TOMMY_TRIE_TREE_MASK;
	j = (key >> shift) & TOMMY_TRIE_TREE_MASK;

	/* if they don't collide */
	if (i != j) {
		/* insert the already existing element */
		tree->map[i] = node;

		/* insert the new node */
		tommy_list_insert_first(&tree->map[j], insert);
		return;
	}

	/* expand one more level */
	let_ptr = &tree->map[i];
	shift -= TOMMY_TRIE_TREE_BIT;
	goto expand;
}

TOMMY_API void tommy_trie_insert(tommy_trie* trie, tommy_trie_node* node, void* data, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	node->data = data;
	node->index = key;

	tommy_trie_node** let_ptr = &trie->bucket[key >> TOMMY_TRIE_BUCKET_SHIFT];

	trie_bucket_insert(trie, TOMMY_TRIE_TREE_SHIFT, let_ptr, node, key);

	++trie->count;
}

TOMMY_API void* tommy_trie_insert_unique(tommy_trie* trie, tommy_trie_node* node, void* data, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	tommy_trie_node** let_ptr = &trie->bucket[key >> TOMMY_TRIE_BUCKET_SHIFT];
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (*let_ptr && trie_get_type(*let_ptr) == TOMMY_TRIE_TYPE_TREE) {
		let_ptr = &trie_get_tree(*let_ptr)->map[(key >> shift) & TOMMY_TRIE_TREE_MASK];
		shift -= TOMMY_TRIE_TREE_BIT;
	}

	/* delay candidate initialization and allocation until duplicates have been excluded */
	tommy_trie_node* existing = *let_ptr;
	if (existing && existing->index == key)
		return existing->data;

	node->data = data;
	node->index = key;

	/* continue insertion at the position and key bits already reached by the search */
	trie_bucket_insert(trie, shift, let_ptr, node, key);
	++trie->count;

	return data;
}

static tommy_trie_node* trie_bucket_remove_existing(tommy_trie* trie, int shift, tommy_trie_node** let_ptr, tommy_trie_node* remove, tommy_key_t key)
{
	tommy_trie_node* node;
	tommy_trie_tree* tree;
	void* ptr;
	tommy_trie_node** let_back[TOMMY_TRIE_LEVEL_MAX + 1];
	tommy_uint_t level;
	tommy_uint_t i;
	tommy_uint_t count;
	tommy_uint_t last;

	level = 0;
recurse:
	ptr = *let_ptr;

	if (!ptr)
		return 0;

	if (trie_get_type(ptr) == TOMMY_TRIE_TYPE_TREE) {
		tree = trie_get_tree(ptr);

		/* save the path */
		let_back[level++] = let_ptr;

		/* go down one level */
		let_ptr = &tree->map[(key >> shift) & TOMMY_TRIE_TREE_MASK];
		shift -= TOMMY_TRIE_TREE_BIT;

		goto recurse;
	}

	node = tommy_cast(tommy_trie_node*, ptr);

	/* if the node to remove is not specified */
	if (!remove) {
		/* remove the first */
		remove = node;

		/* check if it's really the element to remove */
		if (remove->index != key)
			return 0;
	}

	tommy_list_remove_existing(let_ptr, remove);

	/* if the list is not empty, try to reduce */
	if (*let_ptr || !level)
		return remove;

reduce:
	/* go one level up */
	let_ptr = let_back[--level];

	tree = trie_get_tree(*let_ptr);

	/* check if there is only one child node */
	count = 0;
	last = 0;
	for (i = 0; i < TOMMY_TRIE_TREE_MAX; ++i) {
		if (tree->map[i]) {
			/* if we have a sub tree, we cannot reduce */
			if (trie_get_type(tree->map[i]) != TOMMY_TRIE_TYPE_NODE)
				return remove;
			/* if more than one node, we cannot reduce */
			if (++count > 1)
				return remove;
			last = i;
		}
	}

	/* here count is never 0, as we cannot have a tree with only one sub node */
	assert(count == 1);

	*let_ptr = tree->map[last];

	tommy_allocator_free(trie->alloc, tree);
	--trie->node_count;

	/* repeat until more level */
	if (level)
		goto reduce;

	return remove;
}

TOMMY_API void* tommy_trie_remove(tommy_trie* trie, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	tommy_trie_node** let_ptr = &trie->bucket[key >> TOMMY_TRIE_BUCKET_SHIFT];

	tommy_trie_node* ret = trie_bucket_remove_existing(trie, TOMMY_TRIE_TREE_SHIFT, let_ptr, 0, key);

	if (!ret)
		return 0;

	--trie->count;

	return ret->data;
}

TOMMY_API void* tommy_trie_remove_existing(tommy_trie* trie, tommy_trie_node* node)
{
	tommy_key_t key = node->index;

	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	tommy_trie_node** let_ptr = &trie->bucket[key >> TOMMY_TRIE_BUCKET_SHIFT];

	tommy_trie_node* ret = trie_bucket_remove_existing(trie, TOMMY_TRIE_TREE_SHIFT, let_ptr, node, key);

	/* the element removed must match the one passed */
	assert(ret == node);

	--trie->count;

	return ret->data;
}

TOMMY_API tommy_trie_node* tommy_trie_bucket(tommy_trie* trie, tommy_key_t key)
{
	/* ensure that the element is not too big */
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	void* ptr = trie->bucket[key >> TOMMY_TRIE_BUCKET_SHIFT];
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (1) {
		if (!ptr)
			return 0;

		tommy_uint_t type = trie_get_type(ptr);

		switch (type) {
		case TOMMY_TRIE_TYPE_NODE : {
			tommy_trie_node* node = tommy_cast(tommy_trie_node*, ptr);
			if (node->index != key)
				return 0;
			return node;
		}
		default :
		case TOMMY_TRIE_TYPE_TREE :
			ptr = trie_get_tree(ptr)->map[(key >> shift) & TOMMY_TRIE_TREE_MASK];
			shift -= TOMMY_TRIE_TREE_BIT;
			break;
		}
	}
}

static void trie_to_list_node(tommy_trie* trie, tommy_trie_node* node, tommy_list* list)
{
	if (!node)
		return;

	if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_TREE_MAX; ++i)
			trie_to_list_node(trie, tree->map[i], list);
		/* freeing the parent overwrites its branches, so transfer all children first */
		tommy_allocator_free(trie->alloc, tree);
	} else {
		/* each leaf is already a list of equal keys in insertion order */
		tommy_list_concat(list, &node);
	}
}

TOMMY_API void tommy_trie_to_list(tommy_trie* trie, tommy_list* list)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_to_list_node(trie, trie->bucket[i], list);
		trie->bucket[i] = 0;
	}
	trie->count = 0;
	trie->node_count = 0;
}

static void trie_foreach_node(tommy_trie_node* node, tommy_foreach_func* func)
{
	if (!node)
		return;

	if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_TREE_MAX; ++i)
			trie_foreach_node(tree->map[i], func);
	} else {
		/* list traversal saves the next node before the callback can free it */
		tommy_list_foreach(&node, func);
	}
}

TOMMY_API void tommy_trie_foreach(tommy_trie* trie, tommy_foreach_func* func)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i)
		trie_foreach_node(trie->bucket[i], func);
}

static void trie_foreach_arg_node(tommy_trie_node* node, tommy_foreach_arg_func* func, void* arg)
{
	if (!node)
		return;

	if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		for (tommy_uint_t i = 0; i < TOMMY_TRIE_TREE_MAX; ++i)
			trie_foreach_arg_node(tree->map[i], func, arg);
	} else {
		/* list traversal saves the next node before the callback can free it */
		tommy_list_foreach_arg(&node, func, arg);
	}
}

TOMMY_API void tommy_trie_foreach_arg(tommy_trie* trie, tommy_foreach_arg_func* func, void* arg)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i)
		trie_foreach_arg_node(trie->bucket[i], func, arg);
}

TOMMY_API tommy_size_t tommy_trie_memory_usage(const tommy_trie* trie)
{
	return tommy_trie_count(trie) * (tommy_size_t)sizeof(tommy_trie_node)
	       + trie->node_count * (tommy_size_t)TOMMY_TRIE_BLOCK_SIZE;
}

