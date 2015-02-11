#ifndef MAP_H_INCLUDED
#define MAP_H_INCLUDED

#ifndef MAP_ELEMENTS_CAPACITY_MIN
#define MAP_ELEMENTS_CAPACITY_MIN 16
#endif /* MAP_ELEMENTS_CAPACITY_MIN */

typedef struct map map;
struct map
{
  void   *elements;
  void   *element_empty;
  size_t  elements_count;
  size_t  elements_capacity;
  size_t  element_size;
};

typedef size_t (*map_hash_callback)(map *, void *);
typedef int    (*map_equal_callback)(map *, void *, void *);
typedef void   (*map_set_callback)(map *, void *, void *);
typedef void   (*map_release_callback)(map *, void *);

/* constructor/destructor */

void    map_construct(map *, size_t, void *, map_set_callback);
void    map_destruct(map *, map_equal_callback, map_release_callback);

/* capacity */

size_t  map_size(map *);
void    map_reserve(map *, size_t, map_hash_callback, map_equal_callback, map_set_callback);

/* element access */

void   *map_element_empty(map *);
void   *map_at(map *, void *, map_hash_callback, map_equal_callback);

/* modifiers */
void    map_insert(map *, void *, map_hash_callback, map_equal_callback, map_set_callback, map_release_callback);
void    map_erase(map *, void *, map_hash_callback, map_equal_callback, map_set_callback, map_release_callback);
void    map_clear(map *, map_equal_callback, map_set_callback, map_release_callback);

#endif /* MAP_H_INCLUDED */
