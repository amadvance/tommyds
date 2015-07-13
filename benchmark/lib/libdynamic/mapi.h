#ifndef MAPI_H_INCLUDED
#define MAPI_H_INCLUDED

#ifdef DONT_HAVE_BUILTIN_EXPECT
#define __unlikely(cond)        __builtin_expect(!!(cond), 0)
#define __likely(cond)          __builtin_expect(!!(cond), 1)
#else
#define __unlikely(cond)        (cond)
#define __likely(cond)          (cond)
#endif /* DONT_HAVE_BUILTIN_EXPECT */

#define MAPI_ROUNDUP(v, m)      (((v) + (m) - 1) & ~((m) - 1))
#define MAPI_EMPTY_KEY          ((uint32_t) -1)
#define MAPI_MIN_CAPACITY       16

typedef struct mapi_object mapi_object;
typedef struct mapi mapi;

struct mapi_object
{
  uint32_t    key;
};

struct mapi
{
  char       *objects;
  size_t      object_size;
  size_t      size;
  size_t      capacity;
  uint32_t    empty_key;
  void      (*release)(void *);  
};

/* allocators */
mapi         *mapi_new(size_t);
void          mapi_init(mapi *, size_t);
void          mapi_empty_key(mapi *, uint32_t);
void          mapi_release(mapi *, void (*)(void *));
void          mapi_free(mapi *);

/* capacity */
size_t        mapi_size(mapi *);

/* element access */
void         *mapi_get(mapi *, size_t);
void         *mapi_at(mapi *, uint32_t);
mapi_object  *mapi_super(void *);

/* element lookup */
void         *mapi_find(mapi *, uint32_t);

/* modifiers */
void          mapi_insert(mapi *, void *);
void         *mapi_erase(mapi *, uint32_t); // XXX MODIFIED
//void          mapi_erase(mapi *, uint32_t);
void          mapi_clear(mapi *);

/* buckets */
size_t        mapi_bucket_count(mapi *);

/* hash policy */
void          mapi_rehash(mapi *, size_t);
void          mapi_reserve(mapi *, size_t);

/* iterators */
void         *mapi_begin(mapi *);
void         *mapi_next(mapi *, void *);
void         *mapi_end(mapi *);
void         *mapi_skip(mapi *, void *);
void         *mapi_inc(mapi *, void *);

#endif /* MAPI_H_INCLUDED */
