#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ddefs.h"
#include "dvm.h"
#include "dbuiltin.h"
#include "dtypes.h"
#include "dgc.h"
#include "deval.h"

#include "dstring.h"

#include "dparser.h"
#include "dio.h"

#include "doutopcode.h"

#include "mods/d_mod_tensor.h"

/* validation only number */
#define binary_op(vm, op) \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP); \
          return 1; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, AS_VALUE(a op b)); 

#define back_scope(vm) \
        vm->envs->local->count = vm->envs->local->count - vm->active_instr->local_range; \
        vm->active_instr = callstack_pop(vm); \
        vm->ip = vm->active_instr->_ip;

#define STATUS_DCALL_ERROR            0
#define STATUS_DCALL_SUCCESS          1
#define STATUS_DCALL_SUCCESS_NO_CLEAR 2

#define _L_MSG_NOT_DEF "error: '%s' is not defined\n"

/* VM stack. */

void push(d_vm* vm, drax_value v) {
  vm->stack[vm->stack_count++] = v;
}

drax_value pop(d_vm* vm) {
  if (vm->stack_count == 0) return 0;
  vm->stack_count--;
  return vm->stack[vm->stack_count];
}

static drax_value pop_times(d_vm* vm, int t) {
  vm->stack_count -= t;
  return vm->stack[vm->stack_count];
}

static drax_value peek(d_vm* vm, int distance) {
  if (vm->stack_count == 0) return 0;
  return vm->stack[vm->stack_count -1 - distance];
}

/* VM Call stack */

static void callstack_push(d_vm* vm, d_instructions* v) {
  vm->call_stack->values[vm->call_stack->count] = v;
  vm->call_stack->_ip[vm->call_stack->count] = v->_ip;
  vm->call_stack->count++;
}

static d_instructions* callstack_pop(d_vm* vm) {
  if (vm->call_stack->count == 0) return 0;
  vm->call_stack->count--;
  d_instructions* v = vm->call_stack->values[vm->call_stack->count];
  v->_ip = vm->call_stack->_ip[vm->call_stack->count];
  return v;
}

/**
 * Raise Helpers
*/

static void trace_error(d_vm* vm) {
  int last_line = -1;
  #define DO_TRACE_ERROR_ROUTINE() \
    d_instructions* instr = vm->active_instr; \
    if (instr) { \
      int idx = vm->ip - instr->values -1; \
      if (instr->lines[idx] != 0 && instr->lines[idx] != last_line) {\
        if (vm->active_instr->file != NULL) {\
          printf(TRACE_DESCRIPTION_LINE_PATH, vm->active_instr->file, instr->lines[idx]);\
          putchar('\n'); \
        } else {\
          fprintf(stderr, TRACE_DESCRIPTION_LINE, instr->lines[idx]); \
          putchar('\n');} \
      }\
      last_line = instr->lines[idx]; \
    }

  while (CURR_CALLSTACK_SIZE(vm) > 0) {
    DO_TRACE_ERROR_ROUTINE();
    back_scope(vm);
  };
  DO_TRACE_ERROR_ROUTINE();
}

/* Delegate to drax_print_error */
void raise_drax_error(d_vm* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  drax_print_error(format, args);
  putchar('\n');
  va_end(args);

  trace_error(vm);
  __reset__(vm);
}

static bool values_equal(drax_value a, drax_value b) {
  #define DV_EQS_AND_RETURN(_tp, _t1, _t2)\
    _tp* _v1##_tp = (_tp*) _t1->elems;\
    _tp* _v2##_tp = (_tp*) _t2->elems;\
    for(i = 0; i < _t1->length; i++) {\
      if (_v1##_tp[i] != _v2##_tp[i]) {  return false; }\
    }

  if(a == b) {
    return true;
  }

  if(!(IS_STRUCT(a) && IS_STRUCT(b))) {
    return false;  
  }

  if(DRAX_STYPEOF(a) != DRAX_STYPEOF(b)) {
    return false;
  }

  int i;
  switch(DRAX_STYPEOF(a)) {
    case DS_STRING:
      return CAST_STRING(a)->hash == CAST_STRING(b)->hash;
      
    case DS_LIST: {
      drax_list* l1 = CAST_LIST(a);
      drax_list* l2 = CAST_LIST(b);

      if(l1->length != l2->length) { return false; }

      for(i = 0; i < l1->length; i++) {
        if(!values_equal(l1->elems[i], l2->elems[i])) {
          return false;
        }
      }
      return true;
    }

    case DS_TENSOR: {
      drax_tensor* s1 = CAST_TENSOR(a);
      drax_tensor* s2 = CAST_TENSOR(b);

      if(s1->_stype != s2->_stype) { return false; }
      if(s1->length != s2->length) { return false; }

      switch (s1->_stype) {
        case DIT_u8: {
          DV_EQS_AND_RETURN(uint8_t, s1, s2);
          break;
        }
        case DIT_i16: {
          DV_EQS_AND_RETURN(int16_t, s1, s2);
          break;
        }
        case DIT_i32: {
          DV_EQS_AND_RETURN(int32_t, s1, s2);
          break;
        }
        case DIT_i64: {
          DV_EQS_AND_RETURN(int64_t, s1, s2);
          break;
        }
        case DIT_f32: {
          DV_EQS_AND_RETURN(float, s1, s2);
          break;
        }
        case DIT_f64: {
          DV_EQS_AND_RETURN(double, s1, s2);
          break;
        }
        
        default:
          for(i = 0; i < s1->length; i++) {
            if(!values_equal(s1->elems[i], s2->elems[i])) {
              return false;
            }
          }
          break;
      }

      return true;
    }

    case DS_FRAME: {
      drax_frame* f1 = CAST_FRAME(a);
      drax_frame* f2 = CAST_FRAME(b);

      if(f1->length != f2->length) { return false; }

      for(i = 0; i < f1->length; i++) {
        if(f1->keys[i] != f2->keys[i] || 
          !values_equal(f1->values[i], f2->values[i])) {
          return false;
        }
      }
      return true;
    }

    case DS_TIME: {
      drax_time* t1 = CAST_TIME(a);
      drax_time* t2 = CAST_TIME(b);
      struct tm* time1 = localtime(&t1->timestamp);
      struct tm* time2 = localtime(&t2->timestamp);

      if(
        time1->tm_hour == time2->tm_hour &&
        time1->tm_min == time2->tm_min   &&
        time1->tm_sec == time2->tm_sec
        ) {
        return true;
      }

      return false;
    }


    case DS_FUNCTION:
    case DS_NATIVE:
    case DS_MODULE:
      break;

    default:
      break;
  }

  return false;
}

/**
 * clear new range of local variables
 * 
 * x                => used
 * 0                => not used
 * 
 * before:
 *  local->count    =>  (6)
 *  local->array    =>  |x|x|x|x|x|x|
 *  range           =>  (4)
 * 
 * after:
 *  local->array    =>  |x|x|x|x|x|x|0|0|0|0|
 *  local->count    =>  (10)
 * 
 */
void zero_new_local_range(d_vm* vm, int range) {
  vm->envs->local->count += range;
  memset(&vm->envs->local->array[vm->envs->local->count - range], 0, sizeof(drax_value) * range);
}

static int get_definition(d_vm* vm, int is_local) { 
  char* k = (char*) GET_VALUE(vm);
  drax_value v;

  if (
    (get_mod_table(vm->envs->modules, k, &v) == 0) &&
    ((!is_local) || (get_local_table(vm->envs->local, vm->active_instr->local_range, k, &v) == 0))
  ) {
    if(get_var_table(vm->envs->global, k, &v) == 0) {
      v = get_fun_table(vm->envs->native, k, 0);
      if (!v) {
        raise_drax_error(vm, _L_MSG_NOT_DEF, k);
        return 0;
      }
    }
  }

  push(vm, v);
  return 1;
}

#define return_if_not_found_error(vm, v, n, a) \
  if (v == 0) { \
    raise_drax_error(vm, "error: function '%s/%d' is not defined\n", n, a); \
    return STATUS_DCALL_ERROR; \
  }


static void __clean_vm_tmp__(d_vm* itvm);

/**
 * import "your/path" as lib
 * 
 * this function process importations.
 */
static int import_file(d_vm* vm, drax_value v) {
  char * content = 0;

  if (!IS_STRING(v)) {
    raise_drax_error(vm, "import with unexpected type, the argument must be a string");
  }

  drax_string* p = CAST_STRING(v);
  if(get_file_content(vm->active_instr->file, p->chars, &content)) {
    printf("file '%s' not found.\n", p->chars);
    return 1;
  }

  d_vm* itvm = ligth_based_createVM(vm, -2, 1, 1);

  int stat = 0;
  char* new_p = normalize_path(vm->active_instr->file, p->chars);
  if (__build__(itvm, content, new_p)) {
    stat = __run__(itvm, 0);
    free(content);
  } else {
    free(content);
    return 1;
  }

  /**
   * Define exported value on global
   * 
   * vm->exported[0] -> is used only the 
   * first element, for now.
   */
  
    push(vm, itvm->exported[0]);
    vm->d_ls = itvm->d_ls;
    __clean_vm_tmp__(itvm);
    /*if (-1 == vm->vid) dgc_swap(vm);*/
    return stat;
}

/**
 * build self-dependent lambda
 * 
 * generates a new function, solving variables 
 * outside the scope.
 */
static int build_self_dep_fn(d_vm* vm, drax_value* v) {
  drax_function* f = CAST_FUNCTION(*v);
  drax_function* new_fn = new_function(vm);
  
  new_fn->name = f->name;
  new_fn->arity = f->arity;

  new_fn->instructions->file = f->instructions->file;
  new_fn->instructions->lines = f->instructions->lines;
  new_fn->instructions->local_range = f->instructions->local_range + f->instructions->extrn_ref_count;
  new_fn->instructions->extrn_ref_count = 0;
  new_fn->instructions->_ip = f->instructions->_ip;

  int i;
  int qtt_n_inst = f->instructions->extrn_ref_count * 4;/** we can ignore loop and alloc 4 ever */

  /**
   * For each "OP_GET_G_ID" operation, four more slots are
   * generated for commands.
   * 
   * Primarily we expect to receive "OP_GET_G_ID" 
   * and generate a local variable definition within
   * the lambda.
   * 
   * instructions before:
   * [ OP_GET_G_ID | drax_value ] 
   * [ OP_GET_G_ID | drax_value ]
   * 
   * instructions after:
   * [ OP_PUSH | drax_value | OP_SET_L_ID | drax_value | ... | OP_GET_L_ID | drax_value ]
   * [ OP_PUSH | drax_value | OP_SET_L_ID | drax_value | ... | OP_GET_L_ID | drax_value ]
   */
  new_fn->instructions->instr_count = 
    f->instructions->instr_count + qtt_n_inst;
  
  if (new_fn->instructions->instr_size < new_fn->instructions->instr_count) {
    new_fn->instructions->instr_size = new_fn->instructions->instr_count + 1;

    new_fn->instructions->values = (drax_value*) realloc(
      new_fn->instructions->values,
      sizeof(drax_value) * (new_fn->instructions->instr_size + 1)
    );
  }

  *v = DS_VAL(new_fn);

  int i_new = f->arity * 2;

  if (i_new > 0) {
    memcpy(
      new_fn->instructions->values,
      f->instructions->values,
      i_new * sizeof(drax_value)
    );
  }

  for (i = 0; i < f->instructions->extrn_ref_count; i++) {
    drax_value* ip = f->instructions->extrn_ref[i];

      drax_value gv = *(ip + 1);
      char* k = (char*) gv;
      drax_value rv;

    if (*(ip) == OP_GET_G_ID) {
      if ((NULL != new_fn->name) && (strcmp(new_fn->name, k) == 0)) {
        rv = DS_VAL(new_fn);
      } else if (
        (get_mod_table(vm->envs->modules, k, &rv) == 0) &&
        ((get_local_table(vm->envs->local, vm->active_instr->local_range, k, &rv) == 0))
      ) {
        if(get_var_table(vm->envs->global, k, &rv) == 0) {
          rv = get_fun_table(vm->envs->native, k, 0);
          if (!rv) {
            raise_drax_error(vm, _L_MSG_NOT_DEF, k);
            return 0;
          }
        }
      }
    } else {
      raise_drax_error(vm, "runtime error: unspected OP on factory");
      return 0;
    }

    /**
     * the instruction below is the new local definition.
     */
    new_fn->instructions->values[i_new++] = OP_PUSH;
    new_fn->instructions->values[i_new++] = rv;
    new_fn->instructions->values[i_new++] = OP_SET_L_ID;
    new_fn->instructions->values[i_new++] = (drax_value) k;
  }

  /**
   * Copy instructions and updated
   * OP_GET_G_ID to OP_GET_L_ID.
   */
  int j;
  int old_i = (f->arity * 2); 

  for (i = i_new; i < new_fn->instructions->instr_count; i++, old_i++) {
    new_fn->instructions->values[i] = f->instructions->values[old_i];

    for (j = 0; j < f->instructions->extrn_ref_count; j++) {
      drax_value* ip = f->instructions->extrn_ref[j];
      if (&f->instructions->values[old_i] == ip) {
        new_fn->instructions->values[i] = OP_GET_L_ID;
        break;
      }
    }
  }

  /*
  DEBUG_OP(printf("after: build self dep. *******\n"));
  DEBUG_OP(inspect_opcode(new_fn->instructions->values, 8));
  DEBUG_OP(printf("end **************************\n\n"));
  */
  return 1;
}

static void process_pipe_args(d_vm* vm, drax_value a) {
  /**
   * if the function is called via pipe to a 
   * sub-element [module.function()], we invert
   * the argument with the base element.
   * 
   * the base element must be inverted to not be 
   * considered an argument.
   */

  drax_value _t = vm->stack[vm->stack_count - a];
  vm->stack[vm->stack_count - a] = vm->stack[vm->stack_count -1 - a];
  vm->stack[vm->stack_count -1 - a] = _t;
}

static void remove_elem_on_stack(d_vm* vm, size_t distance) {
  for (; distance > 0; distance--) {
    vm->stack[(vm->stack_count - 1) - distance] = vm->stack[vm->stack_count - distance];
  }
  vm->stack_count--;
}

static int execute_d_function(d_vm* vm, drax_value a, drax_value v) {
  if (DRAX_NIL_VAL == v) {
    raise_drax_error(vm, "nil is not function!\n");
    return 1;
  }

  if (IS_FUNCTION(v)) {
    drax_function* f = CAST_FUNCTION(v);
    if (f->arity != (int) a) {
      raise_drax_error(vm, MSG_FUNCTION_IS_NOT_DEFINED, f->name ? f->name : "<function>", a);
      return 1;
    }

    vm->active_instr->_ip = vm->ip;
    callstack_push(vm, vm->active_instr);
    zero_new_local_range(vm, f->instructions->local_range);
    vm->active_instr = f->instructions;
    vm->ip = f->instructions->values;
    return 0;
  }

  if (IS_NATIVE(v)) {
    int scs = 0;
    drax_os_native* nf = CAST_NATIVE(v);
    if (nf->arity != (int) a) {
      raise_drax_error(vm, MSG_FUNCTION_IS_NOT_DEFINED, nf->name ? nf->name : "<function>", a);
      return 1;
    }

    drax_value result = nf->function(vm, &scs);

    if (!scs) {
      drax_error* e = CAST_ERROR(result);
      raise_drax_error(vm, e->chars);
      return 1;
    }
    push(vm, result);
    return 0;
  }
  
  raise_drax_error(vm, "vm_error: function is not valid!\n");
  return 1;
}

static int __start__(d_vm* vm, int inter_mode, int is_per_batch) {

  #define dbin_bool_op(op, v) \
      do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP); \
          return 1; \
        } \
        double b = CAST_NUMBER(pop(vm)); \
        double a = CAST_NUMBER(pop(vm)); \
        push(vm, v(a op b)); \
      } while (false)

  UNUSED(inter_mode);

  int _ops = 0;

  VMDispatch(is_per_batch, _ops) {
    VMcond(vm) {
      VMCase(OP_NIL) {
        push(vm, DRAX_NIL_VAL);
        break;
      }
      VMCase(OP_TRUE) {
        push(vm, DRAX_TRUE_VAL);
        break;
      }
      VMCase(OP_FALSE) {
        push(vm, DRAX_FALSE_VAL);
        break;
      }
      VMCase(OP_LIST) {
        drax_value lc = pop(vm);
        int limit = (int) CAST_NUMBER(lc);
        drax_list* l = new_dlist(vm, limit);

        int i;
        for (i = 0; i < limit; i++) {
          put_value_dlist(l, peek(vm, (limit -1) - i));
        }

        pop_times(vm, limit);
        push(vm, DS_VAL(l));
        break;
      }
      VMCase(OP_TENSOR) {
        drax_value a = GET_VALUE(vm);
        drax_value lc = pop(vm);
        int limit = (int) CAST_NUMBER(lc);
        drax_tensor* scl = new_dtensor(vm, limit, (d_internal_types) a);

        int i;
        drax_value _er;
        for (i = 0; i < limit; i++) {
          if(!put_value_dtensor(vm, scl, peek(vm, (limit -1) - i), &_er)) {
            raise_drax_error(vm, CAST_ERROR(_er)->chars);
            return 1;
          }
        }

        pop_times(vm, limit);
        push(vm, DS_VAL(scl));
        break;
      }
      VMCase(OP_FRAME) {
        drax_value lc = pop(vm);
        int limit = (int) CAST_NUMBER(lc);
        drax_frame* l = new_dframe(vm, limit);

        int i;
        for (i = limit; i > 0; i-=2) {
          char* k = (char*) peek(vm, i - 1);
          put_value_dframe(l, k, peek(vm, i - 2));
        }

        pop_times(vm, limit);
        push(vm, DS_VAL(l));
        break;
      }
      VMCase(OP_DSTR) {
        /* TODO: process db string */
        push(vm, GET_VALUE(vm));
        break;
      }
      VMCase(OP_CONST) {
        push(vm, GET_VALUE(vm));
        break;
      }
      VMCase(OP_POP) {
        pop(vm);
        break;
      }
      VMCase(OP_PUSH) {
        push(vm, GET_VALUE(vm));
        break;
      }
      VMCase(OP_SET_G_ID) {
        drax_value v = pop(vm);
        char* k = (char*) GET_VALUE(vm);
        put_var_table(vm->envs->global, k, v);
        break;
      }
      VMCase(OP_GET_G_ID) {
        if(get_definition(vm, 0) == 0) { return 1; }
        break;
      }
      VMCase(OP_SET_L_ID) {
        char* k = (char*) GET_VALUE(vm);
        put_local_table(vm->envs->local, k, pop(vm));
        break;
      }
      VMCase(OP_GET_L_ID) {
        if(get_definition(vm, 1) == 0) { return 1; }
        break;
      }
      VMCase(OP_SET_I_ID) {
        /**
         * This operation is invalid, we cannot change values 
         * of structures such as frames and lists, as this will 
         * influence old references, resulting in side effects.
         */

        raise_drax_error(vm, "error: invalid assigment\nuse helper functions to change values of structures\n");
        return 1;
      }
      VMCase(OP_GET_I_ID) {
        drax_value k = pop(vm);
        drax_value f = pop(vm);
        drax_value val;

        if (IS_FRAME(f)) {
          if(get_value_dframe(CAST_FRAME(f), (char*) k, &val) == -1) {
            push(vm, DRAX_NIL_VAL);
            break;
          }
          push(vm, val);
          break;
        }

        if (IS_MODULE(f)) {
          drax_native_module* md = CAST_MODULE(f);
          int idx = get_fun_on_module(md, (const char*) k);

          if (idx == -1) {
            raise_drax_error(vm, "error: key '%s' is not defined\n", k);
            return 1;
          }

          low_level_callback* nf = md->fun[idx];
          int a = md->arity[idx];
          push(vm, DS_VAL(new_dllcallback(vm, nf, (const char*) k, a)));
          break;
        }
        
        raise_drax_error(vm, "error: key '%s' is not defined\n", k);
        return 1;
      }
      VMCase(OP_EQUAL) {
        drax_value b = pop(vm);
        drax_value a = pop(vm);
        push(vm, BOOL_VAL(values_equal(a, b)));
        break;
      }
      VMCase(OP_GREATER) {
        dbin_bool_op(>, BOOL_VAL);
        break;
      }
      VMCase(OP_LESS) {
        dbin_bool_op(<, BOOL_VAL);
        break;
      }
      VMCase(OP_CONCAT) {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          drax_string* b = CAST_STRING(peek(vm, 0));
          drax_string* a = CAST_STRING(peek(vm, 1));

          int length = a->length + b->length;
          char* n_str = (char*) malloc(length + 1);
          memcpy(n_str, a->chars, a->length);
          memcpy(n_str + a->length, b->chars, b->length);
          n_str[length] = '\0';

          drax_string* result = new_dstring(vm, n_str, length);
          pop_times(vm, 2);
          push(vm, DS_VAL(result));
        } else if (IS_TENSOR(peek(vm, 0)) && IS_TENSOR(peek(vm, 1))) {
          int stat = 0;
          drax_value _rs = __d_tensor_concat(vm, &stat);
          if (!stat) {
            raise_drax_error(vm, CAST_ERROR(_rs)->chars );
            return 1;
          }
          push(vm, DS_VAL(_rs));
        }  else if (IS_LIST(peek(vm, 0)) && IS_LIST(peek(vm, 1))) {
          drax_list* b = CAST_LIST(peek(vm, 0));
          drax_list* a = CAST_LIST(peek(vm, 1));
          int length = a->length + b->length;
          drax_list* result = new_dlist(vm, length);
          memcpy(result->elems, a->elems, a->length * sizeof(drax_value));
          memcpy(result->elems + a->length, b->elems, b->length * sizeof(drax_value));

          result->length = length;
          pop_times(vm, 2);
          push(vm, DS_VAL(result));
        } else{
          raise_drax_error(vm, "Concat with unspected type");
          return 1;
        }
        break;
      }
      VMCase(OP_ADD) {
        if (IS_TENSOR(peek(vm, 0)) || IS_TENSOR(peek(vm, 1))) {
          int stat = 0;
          drax_value _rs = __d_tensor_add(vm, &stat);
          if (!stat) {
            raise_drax_error(vm, CAST_ERROR(_rs)->chars);
            return 1;
          }
          push(vm, DS_VAL(_rs));
          break;
        }

        binary_op(vm, +);
        break;
      }
      VMCase(OP_SUB) {
        binary_op(vm, -);
        break;
      }
      VMCase(OP_MUL) {
        binary_op(vm, *);
        break;
      }
      VMCase(OP_DIV) {
        binary_op(vm, /);
        break;
      }
      VMCase(OP_NOT) {
        drax_value v = pop(vm);
        push(vm, BOOL_VAL(IS_FALSE(v)));
        break;
      }
      VMCase(OP_NEG) {
        if (!IS_NUMBER(peek(vm, 0))) {
          raise_drax_error(vm, MSG_BAD_AGR_ARITH_OP);
          return 1;
        }
        
        push(vm, NUMBER_VAL(-CAST_NUMBER(pop(vm))));
        break;
      }
      VMCase(OP_JMP) {
        uint16_t offset = dg16_byte(vm);
        vm->ip += offset;
        break;
      }
      VMCase(OP_JMF) {
        uint16_t offset = dg16_byte(vm);
        if (IS_FALSE(peek(vm, 0))) vm->ip += offset;
        break;
      }
      VMCase(OP_LOOP) {
        break;
      }
      VMCase(OP_D_CALL_P) {
        /**
         * Direct call
         * when the function is on stack
         */
        drax_value a = GET_VALUE(vm);
        process_pipe_args(vm, a);
        drax_value v = peek(vm, a);
        remove_elem_on_stack(vm, a);

        if (execute_d_function(vm, a, v) == 1) {
          return 1;
        }
        break;
      }
      VMCase(OP_D_CALL) {
        /**
         * Direct call
         * when the function is on stack
         */
        drax_value a = GET_VALUE(vm);
        drax_value v = peek(vm, a);
        remove_elem_on_stack(vm, a);

        if (execute_d_function(vm, a, v) == 1) {
          return 1;
        }
        break;
      }
      VMCase(OP_FUN) {
        /**
         * Check if is factory
         * 
         * go through all the instructions, if any are global, 
         * try to solve it (locally and then global) by replacing 
         * the values ​​in the instruction itself.
         * 
         * but perhaps these instructions need to be cloned somehow
         */

        drax_value v = GET_VALUE(vm);
        drax_value extern_ref = GET_VALUE(vm);
        if (extern_ref == DRAX_TRUE_VAL) {
          /**
           * This routine will replace the external
           * references on lambda.
           */
          if(build_self_dep_fn(vm, &v) == 0) { return 1; }
        }
        push(vm, v);
        break;
      }
      VMCase(OP_IMPORT) {
        if (import_file(vm, pop(vm))) return 1;
        break;
      }
      VMCase(OP_EXPORT) {
        drax_value v = pop(vm);
        vm->exported[0] = v;
        break;
      }
      VMCase(OP_RETURN) {
        int is_no_instr = (vm->active_instr->instr_count <= 1);
        vm->envs->local->count -= vm->active_instr->local_range;
        vm->active_instr = callstack_pop(vm);
        if (vm->active_instr) {
          vm->ip = vm->active_instr->_ip;
        }

        if (is_no_instr) push(vm, DRAX_NIL_VAL);
        if (!vm->active_instr) return 0;
        break;
      }
      VMCase(OP_EXIT) {
        /* check if is stopVM or exit */

        if (inter_mode) {
          if (peek(vm, 0) == 0) return 0;

          print_drax(pop(vm), inter_mode);
          dbreak_line();
        }
        return 0;
      }
      default: {
        printf("runtime error.\n");
        return 1;
      }
    }
  }
  return 0;  
}

/* Constructor */

static void register_builtin(d_vm* vm, const char* n, int a, low_level_callback* f) {
  drax_value v = DS_VAL(new_dllcallback(vm, f, n, a));
  put_fun_table(vm->envs->native, v);
}

static void initialize_builtin_functions(d_vm* vm) {
  load_callback_fn(vm, &register_builtin);
}

static dt_envs* new_environment(int ignore_natives, int ignore_local, int ignore_global) {
  dt_envs* e = (dt_envs*) malloc(sizeof(dt_envs));

  if (!ignore_global) {
    e->global = new_var_table();
  }

  if (!ignore_local) {
    e->local = new_local_table();
  }

  if (ignore_natives) return e;

  e->native = new_fun_table();
  e->modules = new_mod_table();
  return e;
}


/**
 * Create a new drax vm
*/

static void __init__(d_vm* vm) {
  /**
   * vm->active_instr->values: Must always point to the first statement.
   *  - Used by stack trace.
   *  - Used to initiate a function call.
   */

  vm->ip = vm->active_instr->values;
}

void __reset__(d_vm* vm) {
  vm->active_instr = NULL;
  
  free(vm->instructions->values);
  free(vm->instructions->lines);
  free(vm->instructions);
  
  vm->instructions = new_instructions();

  vm->call_stack->count = 0;
  vm->stack_count = 0;
  vm->ip = NULL;
}

static void __clean_vm_tmp__(d_vm* itvm) { 
  itvm->active_instr = NULL;
  /*
  free(itvm->instructions->values);
  free(itvm->instructions->lines);
  free(itvm->instructions);
  free(itvm->exported);
  
  free(itvm->call_stack->values);
  free(itvm->call_stack->_ip);
  free(itvm->call_stack);
  
*/
  itvm->call_stack->count = 0;
  itvm->stack_count = 0;
  itvm->ip = NULL;
  free(itvm);
}

d_vm* createMainVM() {
  d_vm* vm = (d_vm*) malloc(sizeof(d_vm));
  vm->vid = -1;
  vm->instructions = new_instructions();
  vm->d_ls = NULL;

  /**
   * Only one item is allowed to be 
   * exported, for now.
   */
  vm->exported = (drax_value*) malloc(sizeof(drax_value) * 1);
  vm->exported[0] = DRAX_NIL_VAL;

  vm->stack = (drax_value*) malloc(sizeof(drax_value) * MAX_STACK_SIZE);
  vm->stack_size = MAX_STACK_SIZE;
  vm->stack_count = 0;
  vm->envs = new_environment(0, 0, 0);

  vm->gc_meta = (dgc_meta*) malloc(sizeof(dgc_meta));
  vm->gc_meta->n_cycles = 0;
  vm->gc_meta->n_free_structs = 0;

  /**
   * Created on builtin definitions
  */
  create_native_modules(vm);

  initialize_builtin_functions(vm);

  vm->call_stack = (dcall_stack*) malloc(sizeof(dcall_stack));
  vm->call_stack->size = CALL_STACK_SIZE;
  vm->call_stack->count = 0;
  vm->call_stack->values = (d_instructions**) malloc(sizeof(d_instructions*) * CALL_STACK_SIZE);
  vm->call_stack->_ip = (drax_value**) malloc(sizeof(drax_value*) * CALL_STACK_SIZE);
  vm->pstatus = VM_STATUS_STOPED;
  return vm;
}

/**
 * creates a new virtual machine without starting 
 * the base modules and builtin functions, these 
 * are cloned into the main VM.
 *
 * but creates the base environments
 */
d_vm* ligth_based_createVM(d_vm* vm_base, int vid, int clone_gc, int new_global_scope) {
  d_vm* vm = (d_vm*) malloc(sizeof(d_vm));
  vm->vid = vid;
  vm->instructions = new_instructions();

  /**
   * share global nested structs(d_ls)
   */
    vm->d_ls = clone_gc ? vm_base->d_ls : NULL;

  /**
   * Only one item is allowed to be 
   * exported, for now.
   */
  vm->exported = (drax_value*) malloc(sizeof(drax_value) * 1);
  vm->exported[0] = DRAX_NIL_VAL;

  vm->stack = (drax_value*) malloc(sizeof(drax_value) * MAX_STACK_SIZE);
  vm->stack_size = MAX_STACK_SIZE;
  vm->stack_count = 0;
  vm->envs = new_environment(1, 1, !new_global_scope);

  /**
   * Created on builtin definitions
  */
  if(!new_global_scope) {
    vm->envs->global = vm_base->envs->global;
  }
  vm->envs->local = vm_base->envs->local;
  vm->envs->modules = vm_base->envs->modules;
  vm->envs->native = vm_base->envs->native;

  vm->call_stack = (dcall_stack*) malloc(sizeof(dcall_stack));
  vm->call_stack->size = CALL_STACK_SIZE;
  vm->call_stack->count = 0;
  vm->call_stack->values = (d_instructions**) malloc(sizeof(d_instructions*) * CALL_STACK_SIZE);
  vm->call_stack->_ip = (drax_value**) malloc(sizeof(drax_value*) * CALL_STACK_SIZE);
  vm->pstatus = VM_STATUS_STOPED;

  return vm;
}

int __run__(d_vm* vm, int inter_mode) {
  vm->pstatus = VM_STATUS_WORKING;
  __init__(vm);

  DEBUG_OP(inspect_opcode(vm->ip, 0));

  int r = __start__(vm, inter_mode, 0);
  vm->pstatus = VM_STATUS_STOPED;

  if (-1 == vm->vid) dgc_swap(vm);

  return r;
}

int __run_per_batch__(d_vm* vm) {
  vm->pstatus = VM_STATUS_WORKING;
  int r = __start__(vm, 0, 1);

  if (!vm->active_instr) {
    vm->pstatus = VM_STATUS_FINISHED;
  }

  return r;
}
