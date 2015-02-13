#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include "map_int.h"

/* allocators */

map_int *map_int_new(size_t value_size, uint32_t reserved_empty, uint32_t reserved_deleted)
{
  map_int *map;
  
  map = (map_int *) malloc(sizeof *map);
  map_int_init(map, value_size, reserved_empty, reserved_deleted);
  
  return map;
}

void map_int_init(map_int *map, size_t value_size, uint32_t reserved_empty, 
		   uint32_t reserved_deleted)
{
  memset(map, 0, sizeof *map);
  map->value_size = value_size;
  map->reserved_empty = reserved_empty;
  map->reserved_deleted = reserved_deleted;
  map_int_max_load_factor(map, MAP_INT_MAX_LOAD_FACTOR);
}

void map_int_release(map_int *map, void (*release)(void *))
{
  map->release = release;
}

void map_int_free(map_int *map)
{
  map_int_clear(map);
  free(map);
}

/* capacity */

size_t map_int_size(map_int *map)
{
  return map->size;
}

/* Element access */

void *map_int_get(map_int *map, size_t position)
{
  return map_int_data_offset(map->values, position * map->value_size);
}

void map_int_put(map_int *map, size_t position, uint32_t key, void *value)
{
  if (map->keys[position] == map->reserved_deleted)
    map->deleted --;
  map->keys[position] = key;
  memcpy(map_int_get(map, position), value, map->value_size);
  map->size ++;
}

void *map_int_at(map_int *map, uint32_t key)
{
  size_t position = map_int_find(map, key);
  
  return position == map_int_end(map) ? NULL : map_int_get(map, position);
}

/* element lookup */

size_t map_int_find(map_int *map, uint32_t key)
{
  size_t mask = map->capacity - 1, i = key & mask, step = 0;
  
  while (1)
    {
      if (__likely(map->keys[i] == key))
	return i;
      if (__likely(map->keys[i] == map->reserved_empty))
	return map_int_end(map);
      step ++;
      i = (i + step) & mask;
    }
}

/* modifiers */

void map_int_insert(map_int *map, uint32_t key, void *value)
{
  size_t position;

  map_int_reserve(map, map->size + 1);
  position = map_int_find_free(map, key);
  if (position == map_int_end(map))
    {
      if (map->release)
	map->release(value);
    }
  else
    map_int_put(map, position, key, value);
}

void map_int_erase(map_int *map, uint32_t key, void *value)
{
  size_t position;
  
  position = map_int_find(map, key);
  if (position != map_int_end(map))
    {
      memcpy(value, map_int_get(map, position), map->value_size);
      if (map->release)
	map->release(map_int_get(map, position));
      map->keys[position] = map->reserved_deleted;
      map->deleted ++;
      map->size --;
    }
}

void map_int_clear(map_int *map)
{
  size_t position;

  if (map->release)
    for (position = map_int_begin(map); 
	 position != map_int_end(map); 
	 position = map_int_next(map, position))
      map->release(map_int_get(map, position));

  if (map->keys)
    {
      free(map->keys);
      map->keys = NULL;
    }

  if (map->values)
    {
      free(map->values);
      map->values = NULL;
    }
  
  map->size = 0;
  map->deleted = 0;
  map->capacity = 0;
  map->watermark = 0;
}

/* buckets */

size_t map_int_bucket_count(map_int *map)
{
  return map->capacity;
}

/* hash policy */

void map_int_max_load_factor(map_int *map, double max_load_factor)
{
  map->max_load_factor = max_load_factor;
}

void map_int_rehash(map_int *map, size_t capacity_requested)
{
  map_int map_old;
  size_t capacity, i, position;

  if (map_int_roundup_size(capacity_requested + map->deleted) > map->capacity)
    {
      map_old = *map;
      capacity = map_int_roundup_size(capacity_requested + map->deleted);
      map->keys = (uint32_t *) malloc(capacity * sizeof(*map->keys));
      map->values = malloc(capacity * map->value_size);
      for (i = 0; i < capacity; i ++)
	map->keys[i] =  map->reserved_empty;
      
      map->size = 0;
      map->deleted = 0;
      map->capacity = capacity;
      map->watermark = capacity * map->max_load_factor; 
      
      if (map_old.size)
	for (position = map_int_begin(&map_old); 
	     position != map_int_end(&map_old); 
	     position = map_int_next(&map_old, position))
	  map_int_insert(map, map_old.keys[position], map_int_get(&map_old, position));
      
      if (map_old.keys)
	free(map_old.keys);

      if (map_old.values)
	free(map_old.values);
    }
}

void map_int_reserve(map_int *map, size_t size)
{
  if (size + map->deleted > map->watermark)
    map_int_rehash(map, size / map->max_load_factor);
}

/* iterators */

size_t map_int_begin(map_int *map)
{
  return map_int_next_inclusive(map, 0);
}

size_t map_int_next(map_int *map, size_t position)
{
  return map_int_next_inclusive(map, position + 1);
}

size_t map_int_end(map_int *map)
{
  return map->capacity;
}

/* internals */

size_t map_int_find_free(map_int *map, uint32_t key)
{
  size_t mask = map->capacity - 1, i = key & mask, step = 0;
  uint32_t test;

  while (1)
    {
      test = map->keys[i];
      if (test == map->reserved_deleted || test == map->reserved_empty)
	return i;
      if (test == key)
	return map_int_end(map);
      step ++;
      i = (i + step) & mask;
    }
}

size_t map_int_next_inclusive(map_int *map, size_t position)
{
  uint32_t test;
  
  while (position < map->capacity)
    {
      test = map->keys[position];
      if (test != map->reserved_empty && test != map->reserved_deleted)
	break;
      position ++;
    }
  
  return position;
}

size_t map_int_roundup_size(size_t size)
{
  size --;
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
  size |= size >> 32;
  size ++;

  return size;
}

void *map_int_data_offset(void *data, size_t offset)
{
  return (char *) data + offset;
}
