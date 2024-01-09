#pragma once

#define TABLE_HASH_FUNC(name) unsigned long name(void* key)
typedef TABLE_HASH_FUNC(table_hash_func);

#define TABLE_COMPARE_FUNC(name) int name(void* key_a, void* key_b)
typedef TABLE_COMPARE_FUNC(table_compare_func);

struct bucket
{
  void* key;
  void* value;
  struct bucket* next;
};
struct table
{
  int count;
  int capacity;
  float max_load;
  table_hash_func* hash;
  table_compare_func* cmp;
  struct bucket** buckets;
};

void table_init(struct table* table, int capacity, table_hash_func hash, table_compare_func cmp);
void table_free(struct table* table);
void table_clear(struct table* table);

#define table_add(table, key, value) _table_add(table, key, sizeof(*key), value)
void _table_add(struct table* table, void* key, int key_size, void* value);
void table_remove(struct table* table, void* key);
void* table_find(struct table* table, void* key);
