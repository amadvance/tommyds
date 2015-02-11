#ifndef MAP_INT_H_INCLUDED
#define MAP_INT_H_INCLUDED

#define MAP_INT_MAX_LOAD_FACTOR        0.5

#define HAVE_BUILTIN_EXPECT 1

#ifdef HAVE_BUILTIN_EXPECT
#define __unlikely(cond) __builtin_expect(!!(cond), 0)
#define __likely(cond)   __builtin_expect(!!(cond), 1)
#else
#define __unlikely(cond) (cond)
#define __likely(cond)   (cond)
#endif /* HAVE_BUILTIN_EXPECT */

//#define map_int_insert_rvalue(m, k, v) do {__typeof__(v) __value[] = {(v)}; map_int_insert((m), (k), __value);} while (0)

typedef struct map_int map_int;

struct map_int
{
  uint32_t  *keys;
  void      *values;
  size_t     value_size;
  size_t     size;
  size_t     deleted;
  size_t     capacity;
  size_t     watermark;
  double     max_load_factor;
  uint32_t   reserved_empty;
  uint32_t   reserved_deleted;
  void     (*release)(void *);  
};

/* allocators */
map_int *map_int_new(size_t, uint32_t, uint32_t);
void        map_int_init(map_int *, size_t, uint32_t, uint32_t);
void        map_int_release(map_int *, void (*)(void *));
void        map_int_free(map_int *);

/* capacity */
size_t      map_int_size(map_int *);

/* element access */
void       *map_int_get(map_int *, size_t);
void        map_int_put(map_int *, size_t, uint32_t, void *);
void       *map_int_at(map_int *, uint32_t);

/* element lookup */
size_t      map_int_find(map_int *, uint32_t);

/* modifiers */
void        map_int_insert(map_int *, uint32_t, void *);
void        map_int_erase(map_int *, uint32_t, void*);
void        map_int_clear(map_int *);

/* buckets */
size_t      map_int_bucket_count(map_int *);

/* hash policy */
void        map_int_max_load_factor(map_int *, double);
void        map_int_rehash(map_int *, size_t);
void        map_int_reserve(map_int *, size_t);

/* iterators */
size_t      map_int_begin(map_int *);
size_t      map_int_next(map_int *, size_t);
size_t      map_int_end(map_int *);

/* internals */
size_t      map_int_find_free(map_int *, uint32_t);
size_t      map_int_next_inclusive(map_int *, size_t);
size_t      map_int_roundup_size(size_t);
void       *map_int_data_offset(void *, size_t);

#endif /* MAP_INT_H_INCLUDED */
