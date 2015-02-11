#ifndef VECTOR_H_INCLUDED
#define VECTOR_H_INCLUDED

typedef struct vector vector;
struct vector
{
  buffer   buffer;
  size_t   object_size;
  void   (*object_release)(void *);
};

/* constructor/destructor */
void    vector_construct(vector *, size_t);
void    vector_object_release(vector *, void (*)(void *));
void    vector_destruct(vector *);

/* capacity */
size_t  vector_size(vector *);
size_t  vector_capacity(vector *);
int     vector_empty(vector *);
void    vector_reserve(vector *, size_t);
void    vector_shrink_to_fit(vector *);

/* element access */
void   *vector_at(vector *, size_t);
void   *vector_front(vector *);
void   *vector_back(vector *);
void   *vector_data(vector *);

/* modifiers */

void    vector_insert(vector *, size_t, void *);
void    vector_insert_range(vector *, size_t, void *, void *);
void    vector_insert_fill(vector *, size_t, size_t, void *);
void    vector_erase(vector *, size_t);
void    vector_erase_range(vector *, size_t, size_t);
void    vector_push_back(vector *, void *);
void    vector_pop_back(vector *);
void    vector_clear(vector *);

#endif /* VECTOR_H_INCLUDED */
