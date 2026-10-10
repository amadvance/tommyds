// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2010 Andrea Mazzoleni

/** \mainpage
 * \section Introduction
 * Tommy is a C library of array, hashtable, trie, and tree data structures,
 * designed for high performance and providing an easy-to-use interface.
 *
 * It's **faster** than all the similar libraries like
 * <a href="http://www.canonware.com/rb/">rbtree</a>,
 * <a href="http://www.nedprod.com/programs/portable/nedtries/">nedtrie</a>,
 * <a href="https://github.com/attractivechaos/klib/blob/master/khash.h">khash</a>,
 * <a href="https://troydhanson.github.io/uthash/">uthash</a>,
 * <a href="http://judy.sourceforge.net/">judy</a>,
 * <a href="https://code.google.com/archive/p/judyarray/">judyarray</a>,
 * <a href="https://github.com/sparsehash/sparsehash">googledensehash</a>,
 * <a href="http://code.google.com/p/cpp-btree/">googlebtree</a>,
 * <a href="http://panthema.net/2007/stx-btree/">stxbtree</a>,
 * <a href="https://sites.google.com/site/binarysearchcube/">tesseract</a>,
 * <a href="https://github.com/fredrikwidlund/libdynamic">libdynamic</a>,
 * <a href="http://concurrencykit.org/">concurrencykit</a> and others.
 *
 * The data structures provided are:
 *
 * - ::tommy_list - A doubly linked list.
 * - ::tommy_array, ::tommy_arrayof - A linear array.
 * It doesn't fragment the heap.
 * - ::tommy_arrayblk, ::tommy_arrayblkof - A blocked linear array.
 * It doesn't fragment the heap and it minimizes space occupation.
 * - ::tommy_hashtable - A fixed-size chained hashtable.
 * - ::tommy_hashdyn - A dynamic chained hashtable.
 * - ::tommy_hashlin - A linear chained hashtable.
 * It doesn't have the problem of the delay when resizing and
 * it doesn't fragment the heap.
 * - ::tommy_trie - A trie optimized for cache utilization.
 * - ::tommy_trie_inplace - A trie completely inplace.
 * - ::tommy_tree - A tree to keep elements in order.
 *
 * The most interesting are ::tommy_array, ::tommy_hashdyn, ::tommy_hashlin, ::tommy_trie and ::tommy_trie_inplace.
 *
 * The official site of TommyDS is <a href="https://www.tommyds.it/">https://www.tommyds.it/</a>.
 *
 * \section Use
 *
 * Tommy provides two main categories of data structures:
 * - **Sequential containers and arrays**: ::tommy_array, ::tommy_arrayof, ::tommy_arrayblk,
 * ::tommy_arrayblkof and ::tommy_list.
 * - **Associative containers**: hashtables (::tommy_hashtable, ::tommy_hashdyn, ::tommy_hashlin),
 * numeric tries (::tommy_trie, ::tommy_trie_inplace), and ordered trees (::tommy_tree).
 *
 * \subsection use_selection Container Selection Guide
 *
 * | Data Structure | Best Used For | Key Characteristic |
 * |---|---|---|
 * | ::tommy_array, ::tommy_arrayof | Dynamic arrays indexed by position | No heap fragmentation; stores pointers or values directly |
 * | ::tommy_arrayblk, ::tommy_arrayblkof | Blocked dynamic arrays | Minimizes memory overhead and address-space usage |
 * | ::tommy_list | Doubly linked lists | Intrusive node; O(1) insert/remove at ends; supports sorting |
 * | ::tommy_hashdyn | Dynamic hashtable (strings, generic keys) | Fast O(1) average lookup; doubles size dynamically |
 * | ::tommy_hashlin | Real-time / low-latency hashtable | Incremental resizing avoids latency spikes and heap fragmentation |
 * | ::tommy_hashtable | Fixed-capacity hashtable | Constant bucket allocation without resizing overhead |
 * | ::tommy_trie | Integer keys: 32-bit by default, configurable to 64-bit | Cache-optimized trie; uses an external block allocator |
 * | ::tommy_trie_inplace | Integer keys: 32-bit by default, configurable to 64-bit | In-place node; requires no external allocator |
 * | ::tommy_tree | Ordered collections, range queries | AVL tree ordered by a comparison callback |
 *
 * Tommy containers do not manage the lifetime of stored objects. Memory allocation (e.g. via `malloc()`)
 * and deallocation (e.g. via `free()`) remain the caller's responsibility. Container deinitialization
 * functions (`*_done()`) release internal container buffers without freeing stored objects.
 *
 * \subsection use_seq Sequential Containers and Arrays
 *
 * Dynamic arrays (::tommy_array, ::tommy_arrayof, ::tommy_arrayblk, ::tommy_arrayblkof) store
 * elements indexed by position (from 0 to size - 1). They grow dynamically without heap
 * fragmentation and do **not** require any embedded node inside stored objects.
 * ::tommy_array and ::tommy_arrayblk store pointers to objects (`void*`), while ::tommy_arrayof
 * and ::tommy_arrayblkof store element values directly.
 *
 * \code
 * tommy_array array;
 *
 * tommy_array_init(&array); // initializes the array
 *
 * tommy_array_grow(&array, 10); // grows size to at least 10 elements
 *
 * struct object* obj = malloc(sizeof(struct object));
 * obj->value = 42;
 *
 * tommy_array_set(&array, 0, obj); // stores a pointer at index 0
 *
 * struct object* obj0 = tommy_array_get(&array, 0); // retrieves pointer at index 0
 *
 * tommy_array_done(&array); // deinitializes the array
 * \endcode
 *
 * \code
 * struct value_object {
 *     int value;
 * };
 *
 * tommy_arrayof arrayof;
 * tommy_arrayof_init(&arrayof, sizeof(struct value_object)); // sets element size
 *
 * tommy_arrayof_grow(&arrayof, 10); // grows size to at least 10 elements
 *
 * struct value_object* val0 = tommy_arrayof_ref(&arrayof, 0); // gets pointer to element at index 0
 * val0->value = 42;
 *
 * tommy_arrayof_done(&arrayof); // deinitializes the array
 * \endcode
 *
 * Doubly linked lists (::tommy_list) store pointers to objects sequentially (at head or tail).
 * An object inserted into a list must contain an embedded ::tommy_node, but elements are not
 * indexed by key.
 *
 * \code
 * struct list_object {
 *     int value;
 *     // other fields
 *     tommy_node node;
 * };
 *
 * tommy_list list;
 * tommy_list_init(&list);
 *
 * struct list_object* obj = malloc(sizeof(struct list_object));
 * obj->value = 42;
 *
 * tommy_list_insert_tail(&list, &obj->node, obj); // inserts at tail
 * \endcode
 *
 * \subsection use_assoc Associative Containers
 *
 * Tommy associative containers store pointers to generic objects and provide fast search by key,
 * hash value, or comparison callback. They support multiple elements with the same key, preserving
 * the insertion order of duplicates (semantically equivalent to C++ `multimap` and `unordered_multimap`).
 *
 * Tommy relies on two different callback signatures depending on the container:
 * - **Equality callback (::tommy_search_func)**: used by hashtables. It takes `(const void* arg, const void* obj)`
 * and returns `0` if the element matches the search argument, or non-zero (`!= 0`) if different.
 * - **Three-way comparison callback (::tommy_compare_func)**: used by trees and list sorting. It takes
 * `(const void* obj_a, const void* obj_b)` and returns `< 0` if `a < b`, `0` if `a == b`, and `> 0` if `a > b`.
 *
 * - **Hashtables** (::tommy_hashtable, ::tommy_hashdyn, ::tommy_hashlin): objects embed a ::tommy_node.
 * Insertion takes the hash of the key (type ::tommy_hash_t, e.g. computed via ::tommy_inthash_u32(),
 * ::tommy_strhash_u32(), or ::tommy_hash_u32()). Search and remove take an equality callback function
 * (::tommy_search_func) returning 0 on match to differentiate objects with matching hash values.
 * ::tommy_hashlin shares the same API pattern as ::tommy_hashdyn while resizing incrementally.
 *
 * \code
 * struct hash_object {
 *     int value;
 *     // other fields
 *     tommy_node node;
 * };
 *
 * int hash_compare(const void* arg, const void* obj)
 * {
 *     return *(const int*)arg != ((const struct hash_object*)obj)->value;
 * }
 *
 * tommy_hashdyn hashdyn;
 * tommy_hashdyn_init(&hashdyn);
 *
 * struct hash_object* obj = malloc(sizeof(struct hash_object));
 * obj->value = 1;
 *
 * tommy_hashdyn_insert(&hashdyn, &obj->node, obj, tommy_inthash_u32(obj->value));
 *
 * int value_to_find = 1;
 * struct hash_object* found = tommy_hashdyn_search(&hashdyn, hash_compare, &value_to_find, tommy_inthash_u32(value_to_find));
 * if (found) {
 *     // found
 * }
 *
 * tommy_hashdyn_done(&hashdyn);
 * \endcode
 *
 * - **Numeric Tries** (::tommy_trie, ::tommy_trie_inplace): index objects directly by an integer key
 * (::tommy_key_t). ::tommy_trie uses an embedded ::tommy_node and requires an external ::tommy_allocator.
 * ::tommy_trie_inplace uses an embedded ::tommy_trie_inplace_node directly without an external allocator.
 * Both support 32-bit keys by default, configurable to 64-bit by increasing ::TOMMY_TRIE_BIT
 * or ::TOMMY_TRIE_INPLACE_BIT, respectively, provided ::tommy_key_t is wide enough.
 * All keys must fit within the configured number of bits, even if ::tommy_key_t is wider.
 * This requirement is checked with assert() when assertions are enabled.
 *
 * \code
 * struct trie_object {
 *     tommy_key_t key;
 *     // other fields
 *     tommy_node node;
 * };
 *
 * tommy_allocator alloc;
 * tommy_trie trie;
 *
 * tommy_allocator_init(&alloc, TOMMY_TRIE_BLOCK_SIZE, TOMMY_TRIE_BLOCK_SIZE);
 * tommy_trie_init(&trie, &alloc);
 *
 * struct trie_object* obj = malloc(sizeof(struct trie_object));
 * obj->key = 1;
 *
 * tommy_trie_insert(&trie, &obj->node, obj, obj->key);
 *
 * struct trie_object* found = tommy_trie_search(&trie, 1);
 *
 * tommy_trie_done(&trie);
 * tommy_allocator_done(&alloc);
 * \endcode
 *
 * \code
 * struct trie_inplace_object {
 *     tommy_key_t key;
 *     // other fields
 *     tommy_trie_inplace_node node;
 * };
 *
 * tommy_trie_inplace trie_inplace;
 * tommy_trie_inplace_init(&trie_inplace);
 *
 * struct trie_inplace_object* obj = malloc(sizeof(struct trie_inplace_object));
 * obj->key = 1;
 *
 * tommy_trie_inplace_insert(&trie_inplace, &obj->node, obj, obj->key);
 *
 * struct trie_inplace_object* found = tommy_trie_inplace_search(&trie_inplace, 1);
 * \endcode
 *
 * Both numeric tries provide inclusive and exclusive key bound searches:
 * tommy_trie_search_less(), tommy_trie_search_less_equal(),
 * tommy_trie_search_greater_equal(), and tommy_trie_search_greater(), with
 * corresponding tommy_trie_inplace functions. The less searches select the greatest
 * qualifying key and its last duplicate in insertion order; the greater searches
 * select the smallest qualifying key and its first duplicate. They return the
 * object's data pointer, or 0 if none, without allocating memory or scanning elements.
 * Search keys must fit within the configured number of bits.
 *
 * Use tommy_trie_remove_head() or tommy_trie_remove_tail(), and the corresponding
 * tommy_trie_inplace functions, to remove the smallest or greatest key.
 * They select the first or last duplicate in insertion order, respectively,
 * and return the removed object's data pointer, or 0 if the trie is empty.
 *
 * - **Ordered Trees** (::tommy_tree): AVL tree keeping elements in the order defined by a 3-way comparison
 * callback (::tommy_compare_func). Objects embed a ::tommy_tree_node.
 *
 * \code
 * struct tree_object {
 *     int value;
 *     // other fields
 *     tommy_tree_node node;
 * };
 *
 * int tree_compare(const void* obj_a, const void* obj_b)
 * {
 *     const struct tree_object* a = obj_a;
 *     const struct tree_object* b = obj_b;
 *
 *     if (a->value < b->value)
 *         return -1;
 *     if (a->value > b->value)
 *         return 1;
 *     return 0;
 * }
 *
 * tommy_tree tree;
 * tommy_tree_init(&tree, tree_compare);
 *
 * struct tree_object* obj = malloc(sizeof(struct tree_object));
 * obj->value = 1;
 *
 * tommy_tree_insert(&tree, &obj->node, obj);
 *
 * // search using a reference object
 * struct tree_object value_to_find = { 1 };
 * struct tree_object* found = tommy_tree_search(&tree, &value_to_find);
 * \endcode
 *
 * To search with an arbitrary search key type rather than a full object, use tommy_tree_search_compare().
 *
 * \subsection use_traversal Traversal and Iteration
 *
 * Tommy provides two mechanisms to traverse container elements:
 *
 * - **Callback traversal (foreach)**: almost all containers (lists, hashtables, tries, and trees)
 * provide `foreach()` and `foreach_arg()` functions (such as tommy_hashdyn_foreach(), tommy_list_foreach(),
 * tommy_trie_foreach(), and tommy_tree_foreach()) to iterate over all stored elements using a callback function.
 * The callback may safely deallocate the current element (e.g. calling free()), but adding or removing
 * elements from inside the callback is not allowed.
 *
 * \code
 * void print_object(void* arg, void* obj)
 * {
 *     struct hash_object* o = obj;
 *     printf("%s: %d\n", (const char*)arg, o->value);
 * }
 *
 * tommy_hashdyn_foreach_arg(&hashdyn, print_object, "Item");
 * \endcode
 *
 * - **Explicit iteration**:
 *   - Lists: traverse forward using tommy_list_head() and following `tommy_node::next`, or backward using
 * tommy_list_tail() and tommy_list_prev().
 *   - Trees: traverse in order using tommy_tree_head() and tommy_tree_next(), or in reverse using
 * tommy_tree_tail() and tommy_tree_prev().
 *   - Arrays: access elements in an explicit `for` loop by index from 0 to tommy_array_size(&array) - 1.
 *   - Hashtables and tries: iterate over elements with the same key/bucket using `tommy_*_bucket()`
 * and following `->next`.
 *   - To iterate explicitly over all elements across hashtables or tries in insertion order, you can
 * insert objects into both an associative container and a ::tommy_list (see \ref multiindex).
 *
 * \section Features
 *
 * Tommy is fast and easy to use.
 *
 * Tommy is portable to all platforms and operating systems.
 *
 * Tommy associative containers support multiple elements with the same key.
 *
 * Tommy associative containers keep the original insertion order of elements with equal keys.
 *
 * Tommy is released with the \ref license "2-clause BSD license".
 *
 * See the \ref design page for more details and limitations.
 *
 * \section Performance
 * Here you can see some timings comparing with other notable implementations.
 * The *Hit* graph shows the time required for searching random objects
 * with a key.
 * The *Change* graph shows the time required for searching, removing and
 * reinserting random objects with a different key value.
 *
 * Times are expressed in nanoseconds for each element, and **lower is better**.
 *
 * To have some reference numbers, you can check <a href="https://gist.github.com/jboner/2841832">Latency numbers every programmer should know</a>.
 *
 * A complete analysis is available in the \ref benchmark page.
 *
 * <img src="def/img_random_hit.png"/>
 *
 * <img src="def/img_random_change.png"/>
 *
 * \page benchmark Tommy Benchmarks
 *
 * To help you understand TommyDS's performance, we conducted a thorough
 * benchmark, comparing it against some of the best and most popular existing C
 * and C++ data structure libraries in the conditions of a **real-world** application.
 *
 * Here are the data structures included in the comparison:
 * - ::tommy_hashtable - Fixed-size chained hashtable.
 * - ::tommy_hashdyn - Dynamic chained hashtable.
 * - ::tommy_hashlin - Linear chained hashtable.
 * - ::tommy_trie - Trie optimized for cache usage.
 * - ::tommy_trie_inplace - Trie completely inplace.
 * - <a href="http://www.canonware.com/rb/">rbtree</a> - Red-black tree by Jason Evans.
 * - <a href="http://www.nedprod.com/programs/portable/nedtries/">nedtrie</a> - Binary trie inplace by Niall Douglas.
 * - <a href="https://github.com/attractivechaos/klib/blob/master/khash.h">khash</a> - Dynamic open addressing hashtable by Attractive Chaos.
 * - <a href="https://troydhanson.github.io/uthash/">uthash</a> - Dynamic chaining hashtable by Troy D. Hanson.
 * - <a href="http://judy.sourceforge.net/">judy</a> - Burst trie (JudyL) by Doug Baskins.
 * - <a href="https://code.google.com/archive/p/judyarray/">judyarray</a> - Burst trie by Karl Malbrain.
 * - <a href="https://github.com/sparsehash/sparsehash">googledensehash</a> - Dynamic open addressing hashtable by Craig Silverstein at Google (2.0.4).
 * - <a href="http://code.google.com/p/cpp-btree/">googlebtree</a> - B-tree by Google.
 * - <a href="http://panthema.net/2007/stx-btree/">stxbtree</a> - STX B-tree by Timo Bingmann.
 * - <a href="https://cplusplus.com/reference/unordered_map/unordered_map/">c++unordered_map</a> - C++ STL unordered_map<> template.
 * - <a href="https://cplusplus.com/reference/map/map/">c++map</a> - C++ STL map<> template.
 * - <a href="https://sites.google.com/site/binarysearchcube/">tesseract</a> - Binary Search Tesseract by Gregorius van den Hoven.
 * - <a href="https://github.com/sparsehash/sparsehash/tree/master/experimental">googlelibchash</a> - LibCHash by Craig Silverstein at Google.
 * - <a href="https://github.com/fredrikwidlund/libdynamic">libdynamic</a> - Hash set by Fredrik Widlund.
 * - <a href="http://concurrencykit.org/">concurrencykit</a> - Non-blocking hash set by Samy Al Bahra.
 *
 * Note that *googlelibchash*, *googledensehash* and *concurrencykit* are generally
 * **not shown in the primary performance graphs** because they exhibit numerous
 * performance spikes across different data sizes. You can find specific details
 * and relevant graphs about these in the \ref notes section.
 *
 * \section thebenchmark Understanding the Benchmark Methodology
 *
 * The primary purpose of this benchmark is to measure the performance of
 * storing and searching a collection of N pointers to distinct objects,
 * indexed by an associated integer key.
 *
 * This test methodology deliberately **deviates from typical hash table
 * comparisons** where the entire object's data is copied and stored
 * directly within the container.
 *
 * Storing pointers is a **more common requirement** in real-world
 * applications where the same object must be referenced by or included in
 * multiple data structures (see \ref multiindex).
 * Duplicating the object in such cases would be inefficient or incorrect.
 * This difference is critical and explains why these performance metrics may
 * differ from other general-purpose hash table comparisons.
 *
 * To accurately simulate real-world usage where the application must retrieve
 * and access the object's data, the search operation in this benchmark
 * dereferences the stored pointers. This step typically necessitates
 * an additional memory load, often resulting in a cache miss.
 *
 * This overhead provides a relative performance advantage to **intrusive containers**
 * (like the Tommy structures) where the required indexing metadata is stored
 * directly within the user's object structure. For these designs, the cache
 * miss incurred to access the object's data is the same one that retrieves the
 * necessary indexing information, minimizing the "additional" cost.
 *
 * The tests performed are:
 * * **Insert:** Measures the time taken to add all N objects, starting from an
 * empty container.
 * * **Change:** Measures the time taken to **Find**, **Remove**, and immediately
 * **Reinsert** an object with a *new* key, repeated for all N objects.
 * * **Hit:** Measures the time taken to successfully **Find** all N objects
 * and then **Dereference** (access) them.
 * * **Miss:** Measures the time taken to attempt to find N objects that are
 * **not present** in the container.
 * * **Remove:** Measures the time taken to **Find** and **Remove** all N
 * objects.
 *
 * Note that *Change*, *Hit*, and *Miss* tests are always performed with N
 * objects already in the container, while *Insert* builds the container up, and
 * *Remove* empties it. The objects are always dereferenced upon successful search
 * and removal.
 *
 * All objects are preallocated on the heap, and the time for memory
 * allocation/deallocation itself is not included in the performance results.
 *
 * Each object contains a unique integer key, a value field for consistency checks,
 * and a 16-byte unused *payload* field, plus any required internal data structure
 * links.
 *
 * The keys used are unique and form a dense domain starting at
 * 0x80000000 and consisting of N even numbers (e.g., 0x80000000,
 * 0x80000002, ...).
 * * Using even numbers ensures there are missing keys (the odd numbers)
 * *within* the domain range, which is crucial for realistic *Miss* and *Change*
 * tests.
 * * Starting the key domain at 0x80000000 (instead of 0) prevents unfair
 * advantages for data structures that might have special optimizations for
 * keys near zero.
 * * For hashtables, keys are processed using the ::tommy_inthash_u32()
 * function, which is designed to guarantee a uniform distribution and no
 * hash collisions, providing an optimal scenario.
 * * For tries and trees, the keys are used directly (not hashed).
 *
 * Tests are repeated using two key access modes:
 *
 * * **Random Mode:** Keys are accessed in a completely random order. This
 * represents the real-world worst case and generally favors hashtables
 * because the key-hashing process already randomizes the access pattern.
 * * **Forward Mode:** Keys are accessed in order from the lowest to the
 * highest. This naturally advantages tries and trees as they exploit the
 * consecutive nature of the keys for cache locality.
 *
 * \section result Results Summary
 *
 * The most relevant tests depend on the intended data usage model. If you are
 * unsure which ones to consider, focus on **Random Change**, since it covers
 * searches, insertions, and removals and gives a representative view of a
 * real-world access pattern.
 *
 * ### Random Change Analysis
 * <img src="core_i7_10700_2G9_linux/img_random_change.png"/>
 *
 * Observe the vertical split around the 100,000 element limit.
 * * **Below 100k:** The modern processor's cache can contain most of the data,
 * resulting in extremely fast access for almost all structures.
 * * **Above 100k:** The number of **cache misses** becomes the dominant factor.
 * * **Hashtables** are nearly constant-time, showing almost no growth (O(1)).
 * * **Trees (rbtree, nedtrie)** grow logarithmically as log_2(N).
 * * **Tommy Tries** show better logarithmic growth: ::tommy_trie_inplace
 * grows as log_4(N), and ::tommy_trie as log_8(N). The growth curve for Tommy
 * Tries can be adjusted by configuring the number of branches per node.
 *
 * \section random Full Random Order Results
 *
 * In the **Random** access tests, **hashtables are the clear winners**, followed
 * by tries, with traditional trees generally being the slowest.
 *
 * The best choices in TommyDS for this access pattern are ::tommy_hashdyn and
 * ::tommy_hashlin. ::tommy_hashlin is often preferred for being
 * **real-time friendly** as it minimizes heap fragmentation.
 *
 * <table border="0">
 * <tr><td>
 * <img src="core_i7_10700_2G9_linux/img_random_insert.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_random_hit.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_random_miss.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_random_change.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_random_remove.png"/>
 * </td></tr>
 * </table>
 *
 * \section forward Full Forward Order Access
 *
 * In the **Forward** (sequential) access tests, **tries are the fastest**,
 * followed by hashtables. Trees remain the slowest option.
 *
 * The best choices in TommyDS here are ::tommy_trie and ::tommy_trie_inplace.
 * ::tommy_trie_inplace is often preferred as it **does not require a
 * custom allocator**.
 *
 * Note that hashtables are also faster in Forward mode than in Random mode. This
 * happens because the objects are **allocated sequentially in memory**, and
 * accessing them in key order still results in better cache utilization overall.
 *
 * <table border="0">
 * <tr><td>
 * <img src="core_i7_10700_2G9_linux/img_forward_insert.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_forward_hit.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_forward_miss.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_forward_change.png"/>
 * </td></tr><tr><td>
 * <img src="core_i7_10700_2G9_linux/img_forward_remove.png"/>
 * </td></tr>
 * </table>
 *
 * \section size Memory Usage
 *
 * Here you can see how memory usage scales for the different data structures.
 *
 * <table border="0">
 * <tr><td><img src="core_i7_10700_2G9_linux/img_random_size.png"/></td></tr>
 * </table>
 *
 * \section code Technical Details and Code Snippets
 *
 * The benchmark was performed on a Core i7 10700 2.9 GHz running Linux. The
 * compiler used was gcc 15.2.0 with the aggressive optimization flags
 * `"-O3 -march=native -flto -fpermissive"`.
 *
 * Below is the pseudo-code for the benchmark setup, written here using the C++
 * `unordered_map` as an example implementation:
 *
 * \code
 * #define N 10000000 // Number of elements
 * #define PAYLOAD 16 // Size of the object's payload data
 *
 * // Basic object inserted in the collection
 * struct obj {
 *     unsigned value; // Key used for searching
 *     char payload[PAYLOAD];
 * };
 *
 * // Custom hash function to avoid using the STL one
 * class custom_hash {
 * public:
 *     unsigned operator()(unsigned key) const { return tommy_inthash_u32(key); }
 * };
 *
 * // Map collection from "unsigned" to "pointer to object"
 * typedef std::unordered_map<unsigned, obj*, custom_hash> bag_t;
 * bag_t bag;
 *
 * // Preallocate objects
 * obj* OBJ = new obj[N];
 *
 * // Keys used for inserting and searching elements
 * unsigned INSERT[N];
 * unsigned SEARCH[N];
 *
 * // Initialize the keys
 * for(i=0;i<N;++i) {
 *     INSERT[i] = 0x80000000 + i * 2;
 *     SEARCH[i] = 0x80000000 + i * 2;
 * }
 *
 * // If random order is required, shuffle the keys with Fisher-Yates
 * // The two key orders are not correlated
 * if (test_random) {
 *     std::random_shuffle(INSERT, INSERT + N);
 *     std::random_shuffle(SEARCH, SEARCH + N);
 * }
 * \endcode
 *
 * \subsection insertion Insert benchmark
 * \code
 * for(i=0;i<N;++i) {
 *     // Setup the element to insert
 *     unsigned key = INSERT[i];
 *     obj* element = &OBJ[i];
 *     element->value = key;
 *
 *     // Insert it
 *     bag[key] = element;
 * }
 * \endcode
 *
 * \subsection change Change benchmark
 * \code
 * for(i=0;i<N;++i) {
 *     // Search for the element
 *     unsigned key = SEARCH[i];
 *     bag_t::iterator j = bag.find(key);
 *     if (j == bag.end())
 *         abort();
 *
 *     // Remove it
 *     obj* element = j->second;
 *     bag.erase(j);
 *
 *     // Reinsert the element with a new key
 *     // Use +1 in the key to ensure that the new key is unique
 *     key = INSERT[i] + 1;
 *     element->value = key;
 *     bag[key] = element;
 * }
 * \endcode
 *
 * \subsection hit Hit benchmark
 * \code
 * for(i=0;i<N;++i) {
 *     // Search for the element
 *     // Use a different key order than insertion
 *     // Use +1 in the key because we run after the "Change" test
 *     unsigned key = SEARCH[i] + 1;
 *     bag_t::const_iterator j = bag.find(key);
 *     if (j == bag.end())
 *         abort();
 *
 *     // Ensure that it's the correct element.
 *     // This operation is like using the object after finding it,
 *     // and likely involves a cache-miss operation.
 *     obj* element = j->second;
 *     if (element->value != key)
 *         abort();
 * }
 * \endcode
 *
 * \subsection miss Miss benchmark
 * \code
 * for(i=0;i<N;++i) {
 *     // Search for the element
 *     // All the keys are now shifted by +1 by the "Change" test, and we'll find nothing
 *     unsigned key = SEARCH[i];
 *     bag_t::const_iterator j = bag.find(key);
 *     if (j != bag.end())
 *         abort();
 * }
 * \endcode
 *
 * \subsection remove Remove benchmark
 * \code
 * for(i=0;i<N;++i) {
 *     // Search for the element
 *     // Use +1 in the key because we run after the "Change" test
 *     unsigned key = SEARCH[i] + 1;
 *     bag_t::iterator j = bag.find(key);
 *     if (j == bag.end())
 *         abort();
 *
 *     // Remove it
 *     bag.erase(j);
 *
 *     // Ensure that it's the correct element.
 *     obj* element = j->second;
 *     if (element->value != key)
 *         abort();
 * }
 * \endcode
 *
 * ---
 *
 * \section notes Notes on Other Libraries
 *
 * This section provides additional context on the tested libraries that are not
 * part of TommyDS.
 *
 * \subsection googlelibchash Google C libchash
 * This C implementation was excluded from the main graphs because it exhibits
 * poor performance and significant spikes in the *Change* test for certain N
 * values.
 * See this <a href="other/googlelibchash_problem.png">performance graph</a>
 * for a visual illustration of the issue.
 *
 * \subsection googledensehash Google C++ densehash
 * This C++ implementation exhibits erratic performance with significant spikes
 * during the *Change* benchmark test, particularly in version 2.0.4. The older
 * version 2.0.3 does not exhibit this behavior.
 *
 * The performance degradation is likely caused by the revised reallocation
 * strategy in 2.0.4, which enters a pathological case under the specific access
 * patterns of this test.
 *
 * This type of degeneration is characteristic of hash tables that use
 * tombstone entries for deletion handling, where the accumulation of
 * tombstones can lead to increased probe lengths and degraded performance.
 *
 * Note that downgrading to version 2.0.3 avoids this specific issue but does not
 * guarantee immunity from similar pathological cases under different workloads or
 * access patterns.
 *
 * See this <a href="other/googledensehash_problem.png">performance graph</a>
 * for a visual illustration of the issue.
 *
 * Additionally, it does not automatically release memory upon deletion. To
 * prevent an unfair advantage in the *Remove* test, we forced a periodic
 * memory release by calling `resize(0)`.
 *
 * \subsection khash khash
 * This library does not release memory when elements are deleted, which can lead
 * to an unfair performance advantage in the *Remove* test. It also does not
 * provide a way to shrink its internal storage, so this advantage remains in
 * the benchmark.
 *
 * \subsection nedtrie nedtrie
 * A crash bug was found when inserting a key with the value 0. The issue was
 * reported to the author and the necessary
 * <a href="https://github.com/ned14/nedtries/commit/21039696f27db4ffac70a82f89dc5d00ae74b332">fix</a>
 * has been implemented.
 *
 * \subsection judy Judy
 * The Judy library (specifically JudyL) can sometimes exhibit unpredictable
 * performance depending on the specific platform and input data size.
 * See for instance this <a href="other/judy_problem.png">graph</a> showing a big,
 * reproducible performance spike at 50,000 elements.
 *
 * \subsection ck Concurrency Kit
 * The non-blocking hash set displays severe performance degradation and numerous
 * spikes in the *Change* test for some data sizes.
 *
 * This type of degeneration is characteristic of hash tables that use
 * tombstone entries for deletion handling, where the accumulation of
 * tombstones can lead to increased probe lengths and degraded performance.
 *
 * See this <a href="other/ck_problem.png">performance graph</a>
 * for a visual illustration of the issue.
 *
 * \page multiindex Multi-Indexing: Searching Objects in Multiple Ways
 *
 * In any real-world application where you use objects to represent information,
 * you'll often need to search for those objects using **different keys** or
 * criteria. This is where the concept of **multi-indexing** becomes essential.
 *
 * Consider a common example: files in a file system. A single file object
 * might need to be found in several distinct ways:
 * * By its **name** (e.g., 'document.pdf').
 * * By its **residing directory** (e.g., '/home/user/documents/').
 * * By its **full path** (e.g., '/home/user/documents/document.pdf').
 * * By its unique **inode** number.
 *
 * With multi-indexing, each search key requires the file object to be
 * inserted into a separate, dedicated data structure (like a hash table or
 * a tree) to allow for a fast search based on that specific key. This is
 * exactly what TommyDS is designed to **facilitate**.
 *
 * You can compare this concept to a SQL database. In a database, a single
 * table can have multiple indexes. Each index allows for a fast lookup
 * of rows based on the value in a specific column (the search key), without
 * having to scan the entire table. TommyDS helps you achieve this same
 * performance benefit in your C application for in-memory data structures.
 *
 * TommyDS simplifies the management of these multiple indexes, allowing you to
 * have a single object, but link it into several different data structures
 * simultaneously, each using a different field of the object as the search key.
 *
 * Note that TommyDS does not provide a uniform iterator interface across all
 * containers. While containers like ::tommy_list and ::tommy_tree allow explicit
 * traversal using dedicated node functions, and other containers provide
 * callback traversal through `"foreach"` functions, if your application needs
 * an explicit external iterator across hashtables (meaning the ability to walk
 * through all objects in the collection in an explicit loop) or if you need to
 * preserve the original insertion order of the objects across multiple hash
 * tables, you can also insert all the objects into a separate ::tommy_list.
 *
 * You can then use the ::tommy_list structure as your primary iterator. This
 * gives you the best of both worlds: fast search via the indexed structures,
 * and ordered traversal via the list.
 *
 * The next example demonstrates using multiple data structures (a list and several
 * hash tables) to store a 'file' object, allowing access and searching based
 * on different fields.
 *
 * First, we declare the file object structure, including the required intrusive
 * nodes for the various data structures that will store it.
 *
 * \code
 * struct file {
 *     // data fields
 *     char dir[MAXDIR];
 *     char name[MAXNAME];
 *     inode_t inode;
 *
 *     // for containers
 *     tommy_node node; // node for the file list
 *     tommy_node node_by_dir; // node for the file dir
 *     tommy_node node_by_name; // node for the file name
 *     tommy_node node_by_path; // node for the file path
 *     tommy_node node_by_inode; // node for the file inode
 * };
 * \endcode
 *
 * Next, we define helper functions to compute the hash for each field used
 * as a key and comparison functions to search for an object based on a
 * specified key.
 *
 * \code
 * // search function by inode
 * int search_by_inode(const void* arg, const void* obj)
 * {
 *     const inode_t* inode = arg;
 *     const struct file* f = obj;
 *     return *inode != f->inode;
 * }
 *
 * // compute the hash of a inode
 * tommy_hash_t hash_by_inode(inode_t inode)
 * {
 *     return tommy_inthash_u64(inode);
 * }
 *
 * // compute the hash of a name
 * tommy_hash_t hash_by_name(const char* name)
 * {
 *     return tommy_strhash_u32(0, name);
 * }
 *
 * // compute the hash of a dir
 * tommy_hash_t hash_by_dir(const char* dir)
 * {
 *     return tommy_strhash_u32(0, dir);
 * }
 *
 * // search function by path
 * struct path {
 *     char* dir;
 *     char* name;
 * };
 *
 * int search_by_path(const void* arg, const void* obj)
 * {
 *     const struct path* p = arg;
 *     const struct file* f = obj;
 *     return strcmp(p->dir, f->dir) != 0 || strcmp(p->name, f->name) != 0;
 * }
 *
 * // compute the hash of a path as combination of a dir and a name
 * tommy_hash_t hash_by_path(const char* dir, const char* name)
 * {
 *     return tommy_strhash_u32(tommy_strhash_u32(0, dir), name);
 * }
 * \endcode
 *
 * Now we declare and initialize the data structures.
 *
 * \code
 *     tommy_list list;
 *     tommy_hashdyn hashtable_by_dir;
 *     tommy_hashdyn hashtable_by_name;
 *     tommy_hashdyn hashtable_by_path;
 *     tommy_hashdyn hashtable_by_inode;
 *
 *     // initializes the list and the hash tables
 *     tommy_list_init(&list);
 *     tommy_hashdyn_init(&hashtable_by_dir);
 *     tommy_hashdyn_init(&hashtable_by_name);
 *     tommy_hashdyn_init(&hashtable_by_path);
 *     tommy_hashdyn_init(&hashtable_by_inode);
 * \endcode
 *
 * We create a file object and insert it into all the data structures.
 *
 * \code
 *     // creates an object
 *     struct file* f = malloc(sizeof(struct file));
 *     strcpy(f->dir, ...);
 *     strcpy(f->name, ...);
 *     f->inode = ...;
 *
 *     // inserts into the list and hash tables
 *     tommy_list_insert_tail(&list, &f->node, f);
 *     tommy_hashdyn_insert(&hashtable_by_dir, &f->node_by_dir, f, hash_by_dir(f->dir));
 *     tommy_hashdyn_insert(&hashtable_by_name, &f->node_by_name, f, hash_by_name(f->name));
 *     tommy_hashdyn_insert(&hashtable_by_path, &f->node_by_path, f, hash_by_path(f->dir, f->name));
 *     tommy_hashdyn_insert(&hashtable_by_inode, &f->node_by_inode, f, hash_by_inode(f->inode));
 * \endcode
 *
 * After all files are inserted, we can now search them by inode, remove a
 * file with a specific path, and list all the files with a specific name,
 * regardless of the directory they reside in.
 *
 * \code
 *     // searches a file by inode
 *     inode_t inode_to_find = ...;
 *     struct file* found = tommy_hashdyn_search(&hashtable_by_inode, search_by_inode, &inode_to_find, hash_by_inode(inode_to_find));
 *     if (found) {
 *         printf("%s/%s\n", found->dir, found->name);
 *     }
 *
 *     // searches a file by full path and deletes it
 *     struct path path_to_find;
 *     path_to_find.dir = ...;
 *     path_to_find.name = ...;
 *     found = tommy_hashdyn_search(&hashtable_by_path, search_by_path, &path_to_find, hash_by_path(path_to_find.dir, path_to_find.name));
 *     if (found) {
 *         printf("%s/%s\n", found->dir, found->name);
 *
 *         // if found removes all the references
 *         tommy_list_remove_existing(&list, &found->node);
 *         tommy_hashdyn_remove_existing(&hashtable_by_dir, &found->node_by_dir);
 *         tommy_hashdyn_remove_existing(&hashtable_by_name, &found->node_by_name);
 *         tommy_hashdyn_remove_existing(&hashtable_by_path, &found->node_by_path);
 *         tommy_hashdyn_remove_existing(&hashtable_by_inode, &found->node_by_inode);
 *     }
 *
 *     // iterates over all files with a specific name, even in different directories
 *     const char* name_to_find = ...;
 *     tommy_node* i = tommy_hashdyn_bucket(&hashtable_by_name, hash_by_name(name_to_find));
 *     while (i) {
 *         struct file* f = i->data; // gets the file pointer
 *
 *         if (strcmp(f->name, name_to_find) == 0) { // the bucket may also contain other names
 *             printf("%s/%s\n", f->dir, f->name);
 *         }
 *
 *         i = i->next; // goes to the next file
 *     }
 *
 *     // iterates over all files
 *     i = tommy_list_head(&list);
 *     while (i) {
 *         struct file* found = i->data; // gets the file pointer
 *
 *         printf("%s/%s %lu\n", found->dir, found->name, found->inode);
 *
 *         i = i->next; // goes to the next file
 *     }
 * \endcode
 *
 * Finally, we deallocate all the file objects and deinitialize the data
 * structures.
 *
 * \code
 *     // deallocates the files
 *     tommy_list_foreach(&list, free);
 *
 *     // deallocates the hash tables
 *     tommy_hashdyn_done(&hashtable_by_dir);
 *     tommy_hashdyn_done(&hashtable_by_name);
 *     tommy_hashdyn_done(&hashtable_by_path);
 *     tommy_hashdyn_done(&hashtable_by_inode);
 * \endcode
 *
 * \page design Tommy Design
 *
 * Tommy is designed to fulfill the need for generic data structures for the
 * C language, providing at the same time high performance and a clean
 * and easy-to-use interface.
 *
 * \section testing Testing
 *
 * Extensive and automated tests with the runtime checker <a href="https://valgrind.org/">valgrind</a>
 * and the static analyzer <a href="https://clang-analyzer.llvm.org/">clang</a>
 * are done to ensure the correctness of the library.
 *
 * The test has a <a href="https://www.tommyds.it/cov/tommyds/tommyds">code coverage of 100%</a>,
 * measured with <a href="http://ltp.sourceforge.net/coverage/lcov.php">lcov</a>.
 *
 * \section Limitations
 *
 * Tommy is not thread-safe. You must always provide thread safety using
 * locks before calling any Tommy functions.
 *
 * Tommy does not provide a uniform iterator interface across all containers.
 * While individual containers provide callback-based traversal (`foreach`) and
 * explicit iteration (such as node-based traversal in ::tommy_list and ::tommy_tree,
 * or index-based access in arrays), Tommy does not provide standalone bidirectional
 * or complex cursor iterators for hashtables. To iterate over elements with an
 * explicit loop while preserving insertion order across hashtables, you can
 * insert objects also into a ::tommy_list and use the list as an iterator.
 * See the \ref multiindex example for more details.
 *
 * Tommy doesn't provide an error reporting mechanism for a malloc() failure.
 * You have to provide it by redefining malloc() if you expect it to fail.
 *
 * \section compromise Compromises
 *
 * Finding the right balance between efficiency and ease of use required some
 * compromises. Most of them are on memory efficiency, and were done to avoid
 * crippling the interface.
 *
 * The following is a list of such decisions.
 *
 * \subsection multi_key Multi key
 * All the Tommy associative containers (hashtables, tries and trees) support the
 * insertion of multiple elements with the same key, adding in each node a list
 * of equal elements.
 *
 * They are the equivalent of the C++ associative containers <a href="https://cplusplus.com/reference/map/multimap/">multimap\<unsigned,void*\></a>
 * and <a href="https://cplusplus.com/reference/unordered_map/unordered_multimap/">unordered_multimap\<unsigned,void*\></a>
 * that allow duplicates of the same key.
 *
 * A more memory-conservative approach is to not allow duplicated elements,
 * removing the need for this list.
 *
 * \subsection data_pointer Data pointer
 * The node data pointer field (tommy_node::data, tommy_trie_inplace_node::data,
 * and tommy_tree_node::data) is present to allow search and remove functions to return
 * directly a pointer to the element stored in the container.
 *
 * A more memory-conservative approach is to require the user to compute
 * the element pointer from the embedded node with a fixed displacement.
 * For an example, see the Linux Kernel declaration of
 * <a href="https://elixir.bootlin.com/linux/latest/ident/container_of">container_of()</a>.
 *
 * \subsection insertion_order Insertion order
 * In associative containers, the list used for collisions is doubly-linked to allow
 * insertion of elements at the end of the list to keep the
 * insertion order of equal elements.
 *
 * A more memory-conservative approach is to use a singly-linked list,
 * inserting elements only at the start of the list, losing the
 * original insertion order.
 *
 * \subsection zero_list Zero terminated list
 * The 0-terminated format of tommy_node::next is present to provide a forward
 * iterator terminating in 0. This allows the user to write a simple iteration
 * loop over a list or over elements in the same collision bucket.
 *
 * A more efficient approach is to use a circular list, because operating on nodes
 * in a circular list doesn't require managing the special terminating case when
 * adding or removing elements.
 *
 * \page license Tommy License
 * Tommy is released with a *2-clause BSD license*.
 *
 * \code
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
 * \endcode
 */

/** \file
 * All-in-one include for Tommy.
 *
 * This header can also be included from C++ code, providing C linkage
 * for all the functions declared by the library.
 *
 * When using C++, include this header instead of the individual headers.
 */

/* include MSVC intrinsics before extern "C" to preserve C++ linkage */
#if defined(_MSC_VER) && defined(__cplusplus)
#include <intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "tommytypes.h"
#include "tommyhash.h"
#include "tommyalloc.h"
#include "tommyarray.h"
#include "tommyarrayof.h"
#include "tommyarrayblk.h"
#include "tommyarrayblkof.h"
#include "tommylist.h"
#include "tommytree.h"
#include "tommytrie.h"
#include "tommytrieinp.h"
#include "tommyhashtbl.h"
#include "tommyhashdyn.h"
#include "tommyhashlin.h"

#ifdef __cplusplus
}
#endif

