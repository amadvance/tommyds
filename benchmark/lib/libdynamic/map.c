#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"
#include "vector.h"
#include "map.h"

static size_t map_roundup(size_t s)
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

static inline void map_assert(int value)
{
  if (!value)
    abort();
}

static void *map_element(map *m, size_t position)
{
  return (char *) m->elements + (position * m->element_size);
}

static void map_release_all(map *m, map_equal_callback equal, map_release_callback release)
{
  size_t i;

  if (release)
    for (i = 0; i < m->elements_capacity; i ++)
      if (!equal(m, map_element(m, i), m->element_empty))
        release(m, map_element(m, i));
}

static void map_rehash(map *m, size_t size, map_hash_callback hash, map_equal_callback equal, map_set_callback set)
{
  map new;
  size_t i;

  size = map_roundup(size);
  new = *m;
  new.elements_count = 0;
  new.elements_capacity = size;
  new.elements = malloc(new.elements_capacity * new.element_size);
  map_assert(new.elements != NULL);

  for (i = 0; i < new.elements_capacity; i ++)
    set(m, map_element(&new, i), new.element_empty);

  if (m->elements)
    {
      for (i = 0; i < m->elements_capacity; i ++)
        if (!equal(m, map_element(m, i), m->element_empty))
          map_insert(&new, map_element(m, i), hash, equal, set, NULL);
      free(m->elements);
    }

  *m = new;
}

/* constructor/destructor */

void map_construct(map *m, size_t element_size, void *element_empty, map_set_callback set)
{
  m->elements = NULL;
  m->elements_count = 0;
  m->elements_capacity = 0;
  m->element_size = element_size;
  m->element_empty = element_empty;
  map_rehash(m, MAP_ELEMENTS_CAPACITY_MIN, NULL, NULL, set);
}

void map_destruct(map *m, map_equal_callback equal, map_release_callback release)
{
  map_release_all(m, equal, release);
  free(m->elements);
}

/* capacity */

size_t map_size(map *m)
{
  return m->elements_count;
}

void map_reserve(map *m, size_t size, map_hash_callback hash, map_equal_callback equal, map_set_callback set)
{
  size *= 2;
  if (size > m->elements_capacity)
    map_rehash(m, size, hash, equal, set);
}

/* element access */

void *map_element_empty(map *m)
{
  return m->element_empty;
}

void *map_at(map *m, void *element, map_hash_callback hash, map_equal_callback equal)
{
  size_t i;
  void *test;

  i = hash(m, element);
  while (1)
    {
      i &= m->elements_capacity - 1;
      test = map_element(m, i);
      if (equal(m, test, element) || equal(m, test, m->element_empty))
        return test;
      i ++;
    }
}

/* modifiers */

void map_insert(map *m, void *element, map_hash_callback hash, map_equal_callback equal, map_set_callback set, map_release_callback release)
{
  void *test;

  map_reserve(m, m->elements_count + 1, hash, equal, set);
  test = map_at(m, element, hash, equal);
  if (equal(m, test, m->element_empty))
    {
      set(m, test, element);
      m->elements_count ++;
    }
  else if (release)
    release(m, element);
}

void map_erase(map *m, void *element, map_hash_callback hash, map_equal_callback equal, map_set_callback set, map_release_callback release)
{
  void *test;
  size_t i, j, k;

  i = hash(m, element);
  while (1)
    {
      i &= m->elements_capacity - 1;
      test = map_element(m, i);
      if (equal(m, test, m->element_empty))
        return;
      if (equal(m, test, element))
        break;
      i ++;
    }

  if (release)
    release(m, test);
  m->elements_count --;

  j = i;
  while (1)
    {
      j = (j + 1) & (m->elements_capacity - 1);
      if (equal(m, map_element(m, j), m->element_empty))
        break;

      k = hash(m, map_element(m, j)) & (m->elements_capacity - 1);
      if ((i < j && (k <= i || k > j)) ||
          (i > j && (k <= i && k > j)))
        {
          set(m, map_element(m, i), map_element(m, j));
          i = j;
        }
    }

  set(m, map_element(m, i), m->element_empty);
}

void map_clear(map *m, map_equal_callback equal, map_set_callback set, map_release_callback release)
{
  map_release_all(m, equal, release);
  free(m->elements);
  m->elements = NULL;
  m->elements_count = 0;
  m->elements_capacity = 0;
  map_rehash(m, MAP_ELEMENTS_CAPACITY_MIN, NULL, NULL, set);
}

