#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "dstructs.h"
#include "dhandler.h"
#include "dtypes.h"
#include "ddefs.h"
#include "dvm.h"

#define ALLOCATE_DSTRUCT(b, type, d_struct_type) \
    (type*) allocate_struct(b, sizeof(type), d_struct_type, 0)

#define ALLOCATE_DSTRUCT_ORPHAN(b, type, d_struct_type) \
    (type*) allocate_struct(b, sizeof(type), d_struct_type, 1)

static d_struct* allocate_struct(d_vm* vm, size_t size, dstruct_type type, int is_orphan) {
  d_struct* d = (d_struct*) malloc(sizeof(d_struct) * size);
  d->type = type;
  d->checked = 0;
  if (!is_orphan) {
    d->next = vm->d_ls;
    vm->d_ls = d;
  }

  return d;
}

/* 
 * Drax list helpers
 */

#define LIST_PRE_SIZE 10

drax_list* new_dlist(d_vm* vm, int cap) {
  drax_list* l = ALLOCATE_DSTRUCT(vm, drax_list, DS_LIST);
  l->length = 0;
  l->cap = cap == 0 ? LIST_PRE_SIZE : cap;
  l->elems = malloc(sizeof(drax_value) * l->cap);
  return l;
}

drax_time* new_dtime(d_vm* vm) {
  drax_time* l = ALLOCATE_DSTRUCT(vm, drax_time, DS_TIME);
  l->timestamp = 0;
  return l;
}

void put_value_dlist(drax_list* l, drax_value v) {
  if (l->cap <= l->length) {
    l->cap = (l->cap + LIST_PRE_SIZE);
    l->elems = realloc(l->elems, sizeof(drax_value) * l->cap);
  }

  l->elems[l->length] = v;
  l->length++;
}

/**
 * Tensor 
 */
#define TENSOR_PRE_SIZE 20

static d_internal_types get_tensor_type(drax_value v) {
  if (IS_STRING(v)) return DIT_STRING;

  if (IS_STRUCT(v)) {
    return (d_internal_types) DRAX_STYPEOF(v);
  }

  return DIT_UNDEFINED;
}

int tensor_type_is_number(d_internal_types t) {
  return (
    t == DIT_i16 ||
    t == DIT_u8 ||
    t == DIT_i32 ||
    t == DIT_i64 ||
    t == DIT_f32 ||
    t == DIT_f64);
}

static int is_tensor_tp_valid(drax_value v, d_internal_types _stype) {
  if (IS_NUMBER(v) && tensor_type_is_number(_stype)) {
    return 1;
  }
  
  return get_tensor_type(v) == _stype;
}

drax_tensor* new_dtensor(d_vm* vm, int cap, d_internal_types type) {
  drax_tensor* l = ALLOCATE_DSTRUCT(vm, drax_tensor, DS_TENSOR);
  l->_stype = type;
  l->length = 0;
  l->cap = cap == 0 ? TENSOR_PRE_SIZE : cap;

  switch (l->_stype) {
    case DIT_i16: {
      double* _dvint16_t = malloc(sizeof(int16_t) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvint16_t);
      break;
    }
    case DIT_u8: {
      double* _dvu8_t = malloc(sizeof(uint8_t) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvu8_t);
      break;
    }
    case DIT_i32: {
      double* _dvint32_t = malloc(sizeof(int32_t) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvint32_t);
      break;
    }
    case DIT_i64: {
      double* _dvint64_t = malloc(sizeof(int64_t) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvint64_t);
      break;
    }
    case DIT_f32: {
      double* _dvfloat = malloc(sizeof(float) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvfloat);
      break;
    }
    case DIT_f64: {
      double* _dvdouble = malloc(sizeof(double) * l->cap);
      l->elems = POINTER_TO_PDRAXVAL(_dvdouble);
      break;
    }
    
    default:
      l->elems = malloc(sizeof(drax_value) * l->cap);
      break;
  }

  return l;
}

int put_value_dtensor(d_vm* vm, drax_tensor* l, drax_value v, drax_value* r) {
  #define REALLOC_FOR_TYPE(_dv, _l, _tp)\
    _tp* _dv = (_tp*) _l->elems;\
    _dv = realloc(_dv, sizeof(_tp) * _l->cap);

  #define APPEND_VAL_FOR_TYPE(_dv, _l, _tp, _val) {\
        _tp* _dv = (_tp*) _l->elems;\
        _dv[_l->length] = (_tp) CAST_NUMBER(_val);\
    }


  if (DIT_UNDEFINED == l->_stype) {
      l->_stype = IS_NUMBER(v) ? DIT_f64 : get_tensor_type(v);
  }

  if (DIT_TENSOR != l->_stype && !tensor_type_is_number(l->_stype)) {
    *r = DS_VAL(new_derror(vm, (char*) "type not supported by tensor."));
    return 0;
  }

  if (!is_tensor_tp_valid(v, l->_stype)) {
    *r = DS_VAL(new_derror(vm, (char*) "Insertion of elements with different types in tensor."));
    return 0;
  }

  if (l->cap <= l->length) {
    l->cap = (l->cap + TENSOR_PRE_SIZE);

    switch (l->_stype) {
      case DIT_i16: {
        REALLOC_FOR_TYPE(_i16, l, int16_t);
        break;
      }
      case DIT_u8: {
        REALLOC_FOR_TYPE(_u8, l, uint8_t);
        break;
      }
      case DIT_i32: {
        REALLOC_FOR_TYPE(_i32, l, int32_t);
        break;
      }
      case DIT_i64: {
        REALLOC_FOR_TYPE(_i64, l, int64_t);
        break;
      }
      case DIT_f32: {
        REALLOC_FOR_TYPE(_f32, l, float);
        break;
      }
      case DIT_f64: {
        REALLOC_FOR_TYPE(_f64, l, double);
        break;
      }
      
      default:
        l->elems = realloc(l->elems, sizeof(drax_value) * l->cap);
        break;
    }
  }

  switch (l->_stype) {
    case DIT_i16:
      APPEND_VAL_FOR_TYPE(_i16, l, int16_t, v)
      break;
    case DIT_u8:
      APPEND_VAL_FOR_TYPE(_u8, l, uint8_t, v)
      break;

    case DIT_i32:
      APPEND_VAL_FOR_TYPE(_i32, l, int32_t, v)
      break;

    case DIT_i64:
      APPEND_VAL_FOR_TYPE(_i64, l, int64_t, v)
      break;

    case DIT_f32:
      APPEND_VAL_FOR_TYPE(_f32, l, float, v)
      break;

    case DIT_f64:
      APPEND_VAL_FOR_TYPE(_f64, l, double, v)
      break;
    
    default:
      l->elems[l->length] = v;
      break;
  }

  l->length++;
  return 1;
}

drax_error* new_derror(d_vm* vm, char* msg) {
  drax_error* v = ALLOCATE_DSTRUCT(vm, drax_error, DS_ERROR);
  int sz = strlen(msg);
  char* m = (char*) malloc(sz * sizeof(char));
  strcpy((char*)m, msg);

  v->chars = m;
  v->length = sz;
  return v;
}

drax_os_native* new_dllcallback(d_vm* vm, low_level_callback* f, const char* name, int arity) {
  drax_os_native* native = ALLOCATE_DSTRUCT(vm, drax_os_native, DS_NATIVE);
  native->name = name;
  native->function = f;
  native->arity = arity;
  return native;
}

drax_function* new_function(d_vm* vm) {
  drax_function* f = ALLOCATE_DSTRUCT(vm, drax_function, DS_FUNCTION);
  f->arity = 0;
  f->name = NULL;
  f->instructions = new_instructions();

  return f;
}

static drax_string* allocate_string(d_vm* vm, char* chars, int length, uint32_t hash) {
  drax_string* string = ALLOCATE_DSTRUCT(vm, drax_string, DS_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  return string;
}

drax_string* new_dstring(d_vm* vm, char* chars, int length) {
  uint32_t hash = fnv1a_hash(chars, length);

  return allocate_string(vm, chars, length, hash);
}

drax_string* copy_dstring(d_vm* vm, const char* chars, int length) {
  char* new_chars = malloc(sizeof(char) * length + 1);
  int new_length = 0;
  int i;
  for (i = 0; i < length; i++) {
    if (chars[i] == '\\') {
      if(i + 1 < length && chars[i+1] == '\"') {
        new_chars[new_length++] = '\"';
        i++;
        continue;
      } 
      
      if(i + 1 < length && chars[i+1] == '\\') {
        new_chars[new_length++] = '\\';
        i++;
        continue;
      }
    }
    new_chars[new_length++] = chars[i];
  }
  new_chars[new_length] = '\0';

  uint32_t hash = fnv1a_hash(new_chars, new_length);

  return allocate_string(vm, new_chars, new_length, hash);
}

drax_frame* new_dframe(d_vm* vm, int cap) {
  drax_frame* l = ALLOCATE_DSTRUCT(vm, drax_frame, DS_FRAME);
  l->length = 0;
  l->cap = cap == 0 ? 8 : cap;
  l->values = (drax_value*) malloc(sizeof(drax_value) * l->cap);
  l->literals = (char**) malloc(sizeof(char*) * l->cap);
  l->keys = (int*) malloc(sizeof(int) * l->cap);
  return l;
}

/**
 * return the index of the value in the frame
 * -1 if not found
 */

int get_value_dframe(drax_frame* l, char* name, drax_value* value) {
  int i;
  int k = fnv1a_hash(name, strlen(name));
  for (i = 0; i < l->length; i++) {
    if (l->keys[i] == k) {
      *value = l->values[i];
      return i;
    }
  }
  return -1;
}

void put_value_dframe(drax_frame* l, char* k, drax_value v) {
  if (l->cap <= l->length) {
    l->cap = (l->cap + 8);
    l->keys = realloc(l->keys, sizeof(int) * l->cap);
    l->values = realloc(l->values, sizeof(drax_value) * l->cap);
    l->literals = realloc(l->literals, sizeof(char*) * l->cap);
  }

  drax_value tv;
  int idx = get_value_dframe(l, k, &tv);

  if (idx != -1) {
    l->values[idx] = v;
    return;
  }

  l->keys[l->length] = fnv1a_hash(k, strlen(k));
  l->literals[l->length] = k;
  l->values[l->length] = v;
  l->length++;
}

drax_tid* new_dtid(d_vm* vm, drax_value value) {
  drax_tid* t = ALLOCATE_DSTRUCT(vm, drax_tid, DS_TID);
  t->value = value;
  return t;
}

/**
 * Module helpers
 */

drax_native_module* new_native_module(d_vm* vm, const char* name, int cap) {
  drax_native_module* m = ALLOCATE_DSTRUCT(vm, drax_native_module, DS_MODULE);
  m->name = name;
  m->count = 0;
  m->arity = (int*) malloc(sizeof(int) * cap);
  m->fun = (low_level_callback**) malloc(sizeof(low_level_callback*) * cap);
  m->fn_names = (const char**) malloc(sizeof(const char*) * cap);
  return m;
}

void put_fun_on_module(drax_native_module* m, const drax_native_module_helper helper[], int size) {
  int i;
  for (i = 0; i < size; i++) {
    m->fn_names[i] = helper[i].name;
    m->fun[i] = helper[i].fun;
    m->arity[i] = helper[i].arity;
  }
  m->count = size;
}

int get_fun_on_module(drax_native_module* m, const char* n) {
  int i;
  for (i = 0; i < m->count; i++) {
    if (strcmp(m->fn_names[i], n) == 0) {
      return i;
    }
  }
  return -1;
}

void print_funcs_on_module(drax_native_module* m) {
  #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    } else {
        columns = 80;
    }

    int max_distance = 0;
    int i, j;

    for (i = 0; i < m->count; i++) {
      int str_len = strlen(m->fn_names[i]);
      max_distance = str_len > max_distance ? str_len : max_distance;
    }
    max_distance += 5;

    int num_cols = (int) floor(columns / max_distance);
    int curr_col = 0;

    for (i = 0; i < m->count; i++) {
      curr_col++;

      printf("%s/%i", m->fn_names[i], m->arity[i]);

      if (curr_col == num_cols) {
        curr_col = 0;
        putchar('\n');
        continue;
      }

        int spaces = (max_distance -1) - strlen(m->fn_names[i]);
        for(j = 0; j < spaces; j++) {
          putchar(' ');
        }

    }
  #else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int max_distance = 0;
    int i, j;

    for (i = 0; i < m->count; i++) {
      int str_len = strlen(m->fn_names[i]);
      max_distance = str_len > max_distance ? str_len : max_distance;
    }
    max_distance += 5;

    int num_cols = (int) floor(w.ws_col / max_distance);
    int curr_col = 0;

    for (i = 0; i < m->count; i++) {
      curr_col++;

      printf("%s/%i", m->fn_names[i], m->arity[i]);

      if (curr_col == num_cols) {
        curr_col = 0;
        putchar('\n');
        continue;
      }

        int spaces = (max_distance -1) - strlen(m->fn_names[i]);
        for(j = 0; j < spaces; j++) {
          putchar(' ');
        }

    }
  #endif

  putchar('\n');
}