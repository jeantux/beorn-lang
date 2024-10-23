/* drax Lang - 2022
 * Jean Carlos (jeantux)
 */
 
#ifndef __DDEFS
#define __DDEFS

#include <stdint.h>

#define UNUSED(x) (void)(x)

#define MAX_INSTRUCTIONS 80

#define STACK_OP_SIZE   256

#define MAX_STACK_SIZE (MAX_INSTRUCTIONS * STACK_OP_SIZE)

#define MAX_LOCAL_INSTRUCTIONS (MAX_INSTRUCTIONS * STACK_OP_SIZE)

#define CALL_STACK_SIZE (MAX_INSTRUCTIONS * STACK_OP_SIZE)

#define MAX_SIZE_TABLE 0.85

#define MODE_T_MAX ((mode_t)-1)

typedef uint8_t d_byte_def;

/**
 * Global Macros
 */

#define args_fail_required_size(a, b, msg) \
   if (a != b) { { raise_drax_error(vm, msg); return 0; } };

#define args_fail_required_bigger_than(a, b) if (a < b) { return 0; };

#define args_fail_required_less_than(a, b) if (a > b) { return 0; };

#define match_dfunction(n, v, f, vm, a, s) if ((strcmp(n, v) == 0)) { return f(vm, a, s); }

/* Make String as Return */
#define MSR(g, c)  \
    return DS_VAL(copy_dstring(g, c, strlen(c)));

#define DX_SUCESS_FN(v) *v = 1;

#define DX_ERROR_FN(v) *v = 0;

#define return_if_is_not_function(v, s) \
  if (!IS_FUNCTION(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected function/1 as argument")); \
  }

#define return_if_is_not_string(v, s) \
  if (!IS_STRING(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected string as argument")); \
  }

#define return_if_is_not_number(v, s) \
  if (!IS_NUMBER(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected number as argument")); \
  }

#define return_if_is_not_frame(v, s) \
  if (!IS_FRAME(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected frame as argument")); \
  }

#define return_if_is_not_list(v, s) \
  if (!IS_LIST(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected list as argument")); \
  }

#define return_if_is_not_tensor(v, s) \
  if (!IS_TENSOR(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected tensor as argument")); \
  }

#define return_if_is_not_module(v, s) \
  if (!IS_MODULE(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected module as argument")); \
  }

#define return_if_is_not_tid(v, s) \
  if (!IS_TID(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected tid as argument")); \
  }

#define return_if_is_not_time(v, s) \
  if (!IS_TIME(v)) { \
    DX_ERROR_FN(s); \
    return DS_VAL(new_derror(vm, (char *) "Expected time as argument")); \
  }  

#endif