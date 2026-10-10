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

/**
 * Iterator frame visiting leaves by increasing key and internal nodes
 * after their children, allowing completed internal nodes to be freed.
 * The stack holds at most TOMMY_TRIE_LEVEL_MAX internal nodes and one leaf.
 */
typedef struct trie_subtree_iterator_struct {
	tommy_trie_node* node;
	tommy_uint_t branch;
} trie_subtree_iterator;

tommy_inline void trie_subtree_iterator_init(trie_subtree_iterator* iterator, tommy_trie_node* node)
{
	iterator->node = node;
	iterator->branch = 0;
}

tommy_inline tommy_trie_node* trie_subtree_iterator_next(trie_subtree_iterator* stack, tommy_uint_t* depth)
{
	while (*depth) {
		trie_subtree_iterator* iterator = &stack[*depth - 1];
		tommy_trie_node* node = iterator->node;

		if (node && trie_get_type(node) == TOMMY_TRIE_TYPE_TREE
			&& iterator->branch < TOMMY_TRIE_TREE_MAX) {
			tommy_trie_node* child = trie_get_tree(node)->map[iterator->branch];
			++iterator->branch;
			if (child) {
				trie_subtree_iterator_init(&stack[*depth], child);
				++*depth;
			}
			continue;
		}

		/* pop before returning: callers may free leaves and completed internal nodes */
		--*depth;
		if (node)
			return node;
	}
	return 0;
}

TOMMY_API void tommy_trie_clear(tommy_trie* trie)
{
	trie_subtree_iterator stack[TOMMY_TRIE_LEVEL_MAX + 1];
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_subtree_iterator_init(stack, trie->bucket[i]);
		tommy_uint_t depth = 1;
		tommy_trie_node* node;
		while ((node = trie_subtree_iterator_next(stack, &depth)) != 0) {
			/* postorder releases parents only after their child maps are no longer needed */
			if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE)
				tommy_allocator_free(trie->alloc, trie_get_tree(node));
		}
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

tommy_inline tommy_trie_node* trie_head_node(tommy_trie_node* node)
{
	while (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		tommy_uint_t i = 0;
		while (!tree->map[i])
			++i;
		node = tree->map[i];
	}

	return node;
}

tommy_inline tommy_trie_node* trie_tail_node(tommy_trie_node* node)
{
	while (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
		tommy_trie_tree* tree = trie_get_tree(node);
		tommy_uint_t i = TOMMY_TRIE_TREE_MAX - 1;
		while (!tree->map[i])
			--i;
		node = tree->map[i];
	}

	/* each leaf points to the first duplicate, whose prev is the last duplicate */
	return node->prev;
}

tommy_inline void* trie_search_less(tommy_trie* trie, tommy_key_t key, tommy_bool_t equal)
{
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	tommy_trie_node* candidate = 0;
	tommy_trie_node** map = trie->bucket;
	tommy_uint_t pos = key >> TOMMY_TRIE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (1) {
		/* a smaller sibling deeper on the key path is closer than any earlier candidate */
		for (tommy_uint_t i = pos; i != 0; ) {
			--i;
			if (map[i]) {
				candidate = map[i];
				break;
			}
		}

		tommy_trie_node* node = map[pos];
		if (!node)
			break;
		if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE) {
			/* compressed leaves must be compared using the complete key */
			if (node->index < key || (equal && node->index == key))
				return node->prev->data;
			break;
		}

		map = trie_get_tree(node)->map;
		pos = (key >> shift) & TOMMY_TRIE_TREE_MASK;
		shift -= TOMMY_TRIE_TREE_BIT;
	}

	return candidate ? trie_tail_node(candidate)->data : 0;
}

tommy_inline void* trie_search_greater(tommy_trie* trie, tommy_key_t key, tommy_bool_t equal)
{
	assert(key >> TOMMY_TRIE_BUCKET_SHIFT < TOMMY_TRIE_BUCKET_MAX);

	tommy_trie_node* candidate = 0;
	tommy_trie_node** map = trie->bucket;
	tommy_uint_t size = TOMMY_TRIE_BUCKET_MAX;
	tommy_uint_t pos = key >> TOMMY_TRIE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (1) {
		/* a larger sibling deeper on the key path is closer than any earlier candidate */
		for (tommy_uint_t i = pos + 1; i < size; ++i) {
			if (map[i]) {
				candidate = map[i];
				break;
			}
		}

		tommy_trie_node* node = map[pos];
		if (!node)
			break;
		if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE) {
			/* compressed leaves must be compared using the complete key */
			if (node->index > key || (equal && node->index == key))
				return node->data;
			break;
		}

		map = trie_get_tree(node)->map;
		size = TOMMY_TRIE_TREE_MAX;
		pos = (key >> shift) & TOMMY_TRIE_TREE_MASK;
		shift -= TOMMY_TRIE_TREE_BIT;
	}

	return candidate ? trie_head_node(candidate)->data : 0;
}

TOMMY_API void* tommy_trie_search_less(tommy_trie* trie, tommy_key_t key)
{
	return trie_search_less(trie, key, 0);
}

TOMMY_API void* tommy_trie_search_less_equal(tommy_trie* trie, tommy_key_t key)
{
	return trie_search_less(trie, key, 1);
}

TOMMY_API void* tommy_trie_search_greater_equal(tommy_trie* trie, tommy_key_t key)
{
	return trie_search_greater(trie, key, 1);
}

TOMMY_API void* tommy_trie_search_greater(tommy_trie* trie, tommy_key_t key)
{
	return trie_search_greater(trie, key, 0);
}

TOMMY_API tommy_trie_node* tommy_trie_head(tommy_trie* trie)
{
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i)
		if (trie->bucket[i])
			return trie_head_node(trie->bucket[i]);

	return 0;
}

TOMMY_API tommy_trie_node* tommy_trie_tail(tommy_trie* trie)
{
	for (tommy_uint_t i = TOMMY_TRIE_BUCKET_MAX; i != 0; ) {
		--i;
		if (trie->bucket[i])
			return trie_tail_node(trie->bucket[i]);
	}

	return 0;
}

TOMMY_API tommy_trie_node* tommy_trie_next(tommy_trie* trie, tommy_trie_node* node)
{
	if (node->next)
		return node->next;

	tommy_key_t key = node->index;
	tommy_trie_node* candidate = 0;
	tommy_trie_node** map = trie->bucket;
	tommy_uint_t size = TOMMY_TRIE_BUCKET_MAX;
	tommy_uint_t pos = key >> TOMMY_TRIE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (1) {
		/* a larger sibling deeper on the key path is closer than any earlier candidate */
		for (tommy_uint_t i = pos + 1; i < size; ++i) {
			if (map[i]) {
				candidate = map[i];
				break;
			}
		}

		node = map[pos];
		if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE)
			break;

		map = trie_get_tree(node)->map;
		size = TOMMY_TRIE_TREE_MAX;
		pos = (key >> shift) & TOMMY_TRIE_TREE_MASK;
		shift -= TOMMY_TRIE_TREE_BIT;
	}

	return candidate ? trie_head_node(candidate) : 0;
}

TOMMY_API tommy_trie_node* tommy_trie_prev(tommy_trie* trie, tommy_trie_node* node)
{
	/* only the first duplicate has prev pointing to the null-terminated tail */
	if (node->prev->next)
		return node->prev;

	tommy_key_t key = node->index;
	tommy_trie_node* candidate = 0;
	tommy_trie_node** map = trie->bucket;
	tommy_uint_t pos = key >> TOMMY_TRIE_BUCKET_SHIFT;
	int shift = TOMMY_TRIE_TREE_SHIFT;

	while (1) {
		/* a smaller sibling deeper on the key path is closer than any earlier candidate */
		for (tommy_uint_t i = pos; i != 0; ) {
			--i;
			if (map[i]) {
				candidate = map[i];
				break;
			}
		}

		node = map[pos];
		if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE)
			break;

		map = trie_get_tree(node)->map;
		pos = (key >> shift) & TOMMY_TRIE_TREE_MASK;
		shift -= TOMMY_TRIE_TREE_BIT;
	}

	return candidate ? trie_tail_node(candidate) : 0;
}

TOMMY_API void tommy_trie_to_list(tommy_trie* trie, tommy_list* list)
{
	trie_subtree_iterator stack[TOMMY_TRIE_LEVEL_MAX + 1];
	tommy_builder builder;
	tommy_node* builder_tail = tommy_builder_init(&builder);

	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_subtree_iterator_init(stack, trie->bucket[i]);
		tommy_uint_t depth = 1;
		tommy_trie_node* node;
		while ((node = trie_subtree_iterator_next(stack, &depth)) != 0) {
			if (trie_get_type(node) == TOMMY_TRIE_TYPE_TREE) {
				/* all children have been transferred before the allocator overwrites the map */
				tommy_allocator_free(trie->alloc, trie_get_tree(node));
			} else {
				/* each leaf is already a list of equal keys in insertion order */
				tommy_node* last = node->prev;
				builder_tail = tommy_builder_concat(builder_tail, node, last);
			}
		}
		trie->bucket[i] = 0;
	}
	tommy_list_concat_builder(list, &builder, builder_tail);
	trie->count = 0;
	trie->node_count = 0;
}

TOMMY_API void tommy_trie_foreach(tommy_trie* trie, tommy_foreach_func* func)
{
	trie_subtree_iterator stack[TOMMY_TRIE_LEVEL_MAX + 1];
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_subtree_iterator_init(stack, trie->bucket[i]);
		tommy_uint_t depth = 1;
		tommy_trie_node* node;
		while ((node = trie_subtree_iterator_next(stack, &depth)) != 0) {
			/* list traversal saves the next node before the callback can free it */
			if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE)
				tommy_list_foreach(&node, func);
		}
	}
}

TOMMY_API void tommy_trie_foreach_arg(tommy_trie* trie, tommy_foreach_arg_func* func, void* arg)
{
	trie_subtree_iterator stack[TOMMY_TRIE_LEVEL_MAX + 1];
	for (tommy_uint_t i = 0; i < TOMMY_TRIE_BUCKET_MAX; ++i) {
		trie_subtree_iterator_init(stack, trie->bucket[i]);
		tommy_uint_t depth = 1;
		tommy_trie_node* node;
		while ((node = trie_subtree_iterator_next(stack, &depth)) != 0) {
			/* list traversal saves the next node before the callback can free it */
			if (trie_get_type(node) == TOMMY_TRIE_TYPE_NODE)
				tommy_list_foreach_arg(&node, func, arg);
		}
	}
}

TOMMY_API tommy_size_t tommy_trie_memory_usage(const tommy_trie* trie)
{
	return tommy_trie_count(trie) * (tommy_size_t)sizeof(tommy_trie_node)
	       + trie->node_count * (tommy_size_t)TOMMY_TRIE_BLOCK_SIZE;
}

