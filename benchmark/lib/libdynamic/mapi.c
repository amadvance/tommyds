#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <err.h>

#include "mapi.h"

/* internals */

static size_t mapi_roundup_size(size_t s)
{
  s --;
  s |= s >> 1;
  s |= s >> 2;
  s |= s >> 4;
  s |= s >> 8;
  s |= s >> 16;
  s |= s >> 32;
  s ++;

  return s;
}

/* allocators */

mapi *mapi_new(size_t s)
{
  mapi *m;

  m = (mapi *) malloc(sizeof *m);
  if (!m)
    return NULL;
  mapi_init(m, s);
  
  return m;
}

void mapi_init(mapi *m, size_t s)
{
  *m = {0};

  m->object_size = s;
  m->empty_key = MAPI_EMPTY_KEY;
}

void mapi_empty_key(mapi *m, uint32_t k)
{
  m->empty_key = k;
}

void mapi_release(mapi *m, void (*release)(void *))
{
  m->release = release;
}
      
void mapi_free(mapi *m)
{
  mapi_clear(m);
  free(m);
}

/* capacity */

size_t mapi_size(mapi *m)
{
  return m->size;
}

/* Element access */

void *mapi_get(mapi *m, size_t p)
{
  return (void *) (m->objects + (p * m->object_size));
}

void *mapi_at(mapi *m, uint32_t k)
{
  size_t p = k;
  mapi_object *o;
  
  while (1)
    {
      p &= m->capacity - 1;
      o = mapi_super(mapi_get(m, p));
      if (__likely(o->key == k) || __likely(o->key == m->empty_key))
	return o;
      p ++;
    }
}

mapi_object *mapi_super(void *o)
{
  return (mapi_object *) o;
}

/* element lookup */

void *mapi_find(mapi *m, uint32_t k)
{
  return mapi_at(m, k);
}

/* modifiers */

void mapi_insert(mapi *m, void *o)
{
  mapi_object *mo;

  mapi_reserve(m, m->size + 1);
  mo = mapi_super(mapi_at(m, mapi_super(o)->key));
  if (mo->key == m->empty_key)
    {
      memcpy(mo, o, m->object_size);
      m->size ++;
    }
  else
    {
      if (m->release)
	m->release(mo);
    }
}

void *mapi_erase(mapi *m, uint32_t k) // MOD
{
  mapi_object *o, *i, *w;
  struct {unsigned key; void *value;} *pair; // MOD
  void *value;

  // o = mapi_super(mapi_at(m, k));
  pair = mapi_at(m, k); // MOD
  o = mapi_super(pair); // MOD
  if (o->key == m->empty_key)
    return NULL; // MOD

  value = pair->value; // MOD
  
  if (m->release)
    m->release(o);
  m->size --;
  i = o;
  
  while (1)
    {
      i = mapi_super(mapi_inc(m, i));
      if (i == mapi_super(mapi_end(m)))
	i = mapi_super(m->objects);

      if (i->key == m->empty_key)
	break;

      w = mapi_super(mapi_get(m, i->key & (m->capacity - 1)));
      if ((i > o && (w <= o || w > i)) ||
	  (i < o && (w <= o && w > i)))
	{
	  memcpy(o, i, m->object_size);
	  o = i;
	}
    }

  o->key = m->empty_key;

  return value; // MOD
}

void mapi_clear(mapi *m)
{
  void *o;
  
  if (m->release)
    for (o = mapi_begin(m); o < mapi_end(m); o = mapi_next(m, o))
      m->release(o);
  
  free(m->objects);
  m->size = 0;
  m->capacity = 0;
}

/* buckets */

size_t mapi_bucket_count(mapi *m)
{
  return m->capacity;
}

/* hash policy */

void mapi_rehash(mapi *m, size_t c)
{
  mapi nm;
  void *o;
  
  c = mapi_roundup_size(c);
  if (c > m->capacity)
    {
      if (m->capacity == 0)
	c = c < MAPI_MIN_CAPACITY ? MAPI_MIN_CAPACITY : c;

      mapi_init(&nm, m->object_size);
      mapi_empty_key(&nm, m->empty_key);
      mapi_release(&nm, m->release);
      
      nm.capacity = c;
      nm.objects = (char *) aligned_alloc(64, MAPI_ROUNDUP(c * nm.object_size, 64));
      if (!nm.objects)
	err(1, "alloc failed");
      
      for (o = mapi_begin(&nm); o < mapi_end(&nm); o = mapi_inc(&nm, o))
	mapi_super(o)->key = nm.empty_key;
      
      for (o = mapi_begin(m); o < mapi_end(m); o = mapi_next(m, o))
	mapi_insert(&nm, o);

      free(m->objects);

      *m = nm;
    }
}

void mapi_reserve(mapi *m, size_t s)
{
  s <<= 1;
  if (s > m->capacity)
    mapi_rehash(m, s);
}

/* iterators */

void *mapi_begin(mapi *m)
{
  return mapi_skip(m, m->objects);
}

void *mapi_next(mapi *m, void *o)
{
  return mapi_skip(m, mapi_inc(m, o));
}

void *mapi_end(mapi *m)
{
  return mapi_get(m, m->capacity);
}

void *mapi_skip(mapi *m, void *o)
{
  while (o < mapi_end(m) && mapi_super(o)->key == m->empty_key)
    o = mapi_inc(m, o);
  
  return o;
}

void *mapi_inc(mapi *m, void *o)
{
  return (void *) ((char *) o + m->object_size);
}
