/* drax Lang - 2022
 * Jean Carlos (jeantux)
 */
 
#ifndef __DTYPES
#define __DTYPES

#include <stddef.h>
#include "ddefs.h"
#include <stdbool.h>

#define BASSERT(cond, t, msg, ...) if (cond) { return new_error(t, msg, ##__VA_ARGS__); }

#define UNUSED(x) (void)(x)

#define BENV_HASH_SIZE 8

#define EXPAND_CAPACITY(limit) ((limit) < 8 ? 8 : (limit) * 2)

typedef enum dstruct_type{
  DS_LIST = 3,
  DS_TENSOR,
  DS_FUNCTION,
  DS_NATIVE,
  DS_STRING,
  DS_FRAME,
  DS_MODULE,
  DS_ERROR,
  DS_TID,
  DS_TIME
} dstruct_type;

typedef struct d_struct {
  dstruct_type type;
  bool checked;
  struct d_struct* next;
} d_struct;

typedef enum drax_nanbox_def {
  DRX_STRUCT_DEF,
  DRX_NIL_DEF,
  DRX_FALSE_DEF,
  DRX_TRUE_DEF,
} drax_nanbox_def;

#define dx_gen_nbx_val(v) ((drax_value) (unsigned long) 0x7ffc000000000000 | v)

#define DRAX_NIL_VAL    dx_gen_nbx_val(DRX_NIL_DEF)
#define DRAX_FALSE_VAL  dx_gen_nbx_val(DRX_FALSE_DEF)
#define DRAX_TRUE_VAL   dx_gen_nbx_val(DRX_TRUE_DEF)

#define F_BIT ((unsigned long) 0x8004000000000000)

#define IS_BOOL(v)      ((v | 1) == DRAX_TRUE_VAL)
#define IS_FALSE(v)     (IS_NIL(v) || (IS_BOOL(v) && !CAST_BOOL(v)))
#define IS_NIL(v)       (v == DRAX_NIL_VAL)
#define IS_NUMBER(v)    ((v & 0x7ffc000000000000) != 0x7ffc000000000000)
#define IS_STRUCT(v)    ((v & (0x7ffc000000000000 | F_BIT)) == (0x7ffc000000000000 | F_BIT))

#define CAST_BOOL(v)    (v == DRAX_TRUE_VAL)
#define CAST_NUMBER(v)  draxvalue_to_num(v)
#define CAST_STRUCT(v)  ((d_struct*) (uintptr_t) ((v) & ~(F_BIT | 0x7ffc000000000000)))

#define BOOL_VAL(b)      ((b) ? DRAX_TRUE_VAL : DRAX_FALSE_VAL)
#define NUMBER_VAL(v)    num_to_draxvalue(v)
#define DS_VAL(d_struct) (drax_value)  \
          (F_BIT | 0x7ffc000000000000 | (unsigned long) (uintptr_t) (d_struct))

/**
 * For "POINTER_TO_DRAXVAL" conversion you must be sure
 * of the type you are accessing.
 */
#define POINTER_TO_DRAXVAL(v)  (drax_value) (uintptr_t) v;
#define POINTER_TO_PDRAXVAL(v)  (drax_value*) (uintptr_t) v;

/**
 * Instructions
*/
typedef enum d_op_code {
  OP_CONST = 1,
  OP_NIL, 
  OP_TRUE,
  OP_FALSE,
  OP_TENSOR,
  OP_LIST,
  OP_FRAME,
  OP_DSTR,
  OP_POP,
  OP_PUSH,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_CONCAT,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_NOT,
  OP_NEG,
  OP_JMP,
  OP_JMF,
  OP_LOOP,
  OP_D_CALL,   /* direct call */
  OP_D_CALL_P,
  OP_CALL_G,   /* global*/
  OP_CALL_L,   /* local*/
  OP_ARGS,     /* arguments */
  OP_FUN,
  OP_SET_G_ID, /* global */
  OP_GET_G_ID,
  OP_SET_L_ID, /* local */
  OP_GET_L_ID,
  OP_SET_I_ID, /* inside */
  OP_GET_I_ID,
  OP_IMPORT,   /* import file */
  OP_EXPORT,   /* export file */
  OP_RETURN,
  OP_EXIT,
} d_op_code;

/**
 * Drax Value definitions
*/

#ifndef _WIN_32
  typedef uint64_t drax_value;
#else
  typedef unsigned long drax_value;
#endif

typedef enum bimode {
  BI_NONE,
  BI_PROCESS_DEFAULT,
  BI_INTERACTIVE_DEFAULT
} bimode;

/**
 * Drax VM definitions
*/
typedef struct d_instructions {
  drax_value* values;
  int instr_count;
  int instr_size;
  char* file;
  int* lines;
  int local_range; /* used by local definitions */

  /**
   * uextrn_ref: sed by lambda to check if there are external 
   * references (local variables outside the lambda 
   * or global variables).
   */
  int extrn_ref_count;
  int extrn_ref_capacity;
  drax_value** extrn_ref;
  /**
   * _ip: Used to log current statement when 
   * added/removed from call_stack
   */
  drax_value* _ip;
} d_instructions;


/* Vars allocation */

double draxvalue_to_num(drax_value value);

drax_value num_to_draxvalue(double num);

/* Helpers */

d_instructions* new_instructions();

#endif
