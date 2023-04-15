#ifndef __DHHANDLER
#define __DHHANDLER

#include "dstructs.h"
#include "dtypes.h"

typedef struct drax_string drax_string;

typedef struct d_vm d_vm;

/* Runtime hash function (Match definition) */

typedef struct drax_pairs {
  size_t key;
  drax_value value;
} drax_pairs;

typedef struct drax_fun_store {
  size_t key;
  size_t args;
  drax_value value;
} drax_fun_store;

/* Global envinronment */
typedef struct d_var_table {
  int count;
  int limit;
  drax_pairs* pairs;
} d_var_table;

typedef struct d_fun_table {
  int count;
  int limit;
  drax_fun_store* pairs;
} d_fun_table;

/* impl. */

d_var_table* new_var_table();

void free_table(d_vm* vm, d_var_table* t);

void put_var_table(d_var_table* t, char* key, drax_value value);

drax_value get_var_table(d_var_table* t, char* key);

/**
 * function definitions
*/

d_fun_table* new_fun_table();

void free_fun_table(d_vm* vm, d_fun_table* t);

void put_fun_table(d_fun_table* t, drax_value value);

drax_value get_fun_table(d_fun_table* t, char* key, uint8_t arity);

#endif
