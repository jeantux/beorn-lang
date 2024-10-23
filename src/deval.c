#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "deval.h"
#include "dstring.h"
#include "drax.h"

#define D_NUMBER_STR_PRINT "%.17g"

extern int is_teractive_mode;

#define DPRINT_COLOR1(_color, str, arg1) \
  if (is_teractive_mode) {printf(_color str D_COLOR_RESET, arg1);} else {printf(str, arg1);}

static void print_list(drax_list* l) {
  putchar('[');
  int i;
  for (i = 0; i < l->length; i++) {
    print_drax(l->elems[i], 1);

    if ((i+1) < l->length) printf(", ");
  }
  putchar(']');
}

static void print_tensor_type(d_internal_types v) {
  if (is_teractive_mode) printf(D_COLOR_PURPLE);
  switch (v) {
    case DIT_UNDEFINED: {
      printf("undefined");
      break;
    }
    case DIT_f32: {
      printf("f32");
      break;
    }
    case DIT_u8: {
      printf("u8");
      break;
    }
    case DIT_f64: {
      printf("f64");
      break;
    }
    case DIT_i16: {
      printf("i16");
      break;
    }
    case DIT_i32: {
      printf("i32");
      break;
    }
    case DIT_i64: {
      printf("i64");
      break;
    }
    case DIT_LIST: {
      printf("list");
      break;
    }
    case DIT_TENSOR: {
      printf("tensor");
      break;
    }
    case DIT_FUNCTION:
    case DIT_NATIVE: {
      printf("function");
      break;
    }
    case DIT_FRAME: {
      printf("frame");
      break;
    }
    case DIT_MODULE: {
      printf("module");
      break;
    }
    case DIT_TID: {
      printf("tid");
      break;
    }
    case DIT_STRING: {
      printf("string");
      break;
    }
    case DIT_TIME: {
      printf("time");
      break;
    }
    default: {
      printf("undefined");
      break;
    }
  }
  if (is_teractive_mode) printf(D_COLOR_RESET);
}

static void print_tensor(drax_tensor* l, int formated, int level) {
  int _i;
  #define PRINT_LEVEL() if(is_teractive_mode) {for (_i = 0; _i < level; _i++) { printf("  "); }}
  
  int lb = l->_stype == DIT_TENSOR;

  PRINT_LEVEL();
  printf("<<");
  print_tensor_type(l->_stype);
  printf(" :: ");
  if (lb && is_teractive_mode) {
    printf("\n");
    PRINT_LEVEL();
  }

  int i;
  if (DIT_i16 == l->_stype) {
    int16_t* _i16 = (int16_t*) l->elems;
    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%i", (int16_t) _i16[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_u8 == l->_stype) {
    uint8_t* _u8 = (uint8_t*) l->elems;
    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%d", (uint8_t) _u8[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_i32 == l->_stype) {
    int32_t* _i32 = (int32_t*) l->elems;
    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%i", (int32_t) _i32[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_i64 == l->_stype) {
    int64_t* _i64 = (int64_t*) l->elems;
    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%li", (int64_t) _i64[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_f32 == l->_stype) {
    float* _f32 = (float*) l->elems;
    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%f", (double) _f32[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_f64 == l->_stype) {
    double* _f64 = (double*) l->elems;

    for (i = 0; i < l->length; i++) {
      DPRINT_COLOR1(D_COLOR_BLUE, "%.15f", _f64[i]);
      if ((i+1) < l->length) printf(", ");
    }
  } else if (DIT_TENSOR == l->_stype) {
    for (i = 0; i < l->length; i++) {
      print_tensor(CAST_TENSOR(l->elems[i]), formated, level + 1);
      if ((i+1) < l->length) printf(", ");
      if ((i+1) < l->length && is_teractive_mode) printf("\n");
    }
  } else {
    for (i = 0; i < l->length; i++) {
      print_drax(l->elems[i], 1);
      if ((i+1) < l->length) printf(", ");
      if ((i+1) < l->length && formated) printf("\n");
    }
  }

  if (lb && formated) putchar('\n');
  printf(">>");
}

static void print_frame(drax_frame* f) {
  putchar('{');
  int i;
  for (i = 0; i < f->length; i++) {
    printf("'%s': ", f->literals[i]);
    print_drax(f->values[i], 1);

    if ((i+1) < f->length) printf(", ");
  }
  putchar('}');
}

static void print_string(const char* str, int formated) {
  if (!formated) {
    char* tmpstr = str_format_output(str);
    if (tmpstr == NULL) {
      fprintf(stderr, D_COLOR_RED"runtime error: cannot format string"D_COLOR_RESET);
      return;
    }
    printf(is_teractive_mode ? D_COLOR_GREEN "\"%s\"" D_COLOR_RESET : "%s", tmpstr);
    free(tmpstr);
  } else {
    printf(is_teractive_mode ? D_COLOR_GREEN "\"%s\"" D_COLOR_RESET : "%s", str);
  }
}

static void print_d_struct(drax_value value, int formated) {
  switch (DRAX_STYPEOF(value)) {
    case DS_LIST:
      print_list(CAST_LIST(value));
      break;

    case DS_TENSOR:
      print_tensor(CAST_TENSOR(value), formated, 0);
      break;

    case DS_FRAME:
      print_frame(CAST_FRAME(value));
      break;

    case DS_FUNCTION:
      printf("<function/%d>", CAST_FUNCTION(value)->arity);
      break;

    case DS_NATIVE:
      printf("<function:native>");
      break;

    case DS_MODULE:
      printf("<module>");
      break;

    case DS_STRING:
      print_string(CAST_STRING(value)->chars, formated);
      break;

    case DS_ERROR:
      printf("error");
      break;

    case DS_TID:
      printf("<tid::%ld>", CAST_TID(value)->value);
      break;

    case DS_TIME:
      struct tm* time = localtime(&CAST_TIME(value)->timestamp);
      printf("Time<%02d:%02d:%02d>", time->tm_hour, time->tm_min, time->tm_sec);
      break; 
  }
}

void drax_print_error(const char* format, va_list args) {
  vfprintf(stderr, format, args);
}

void print_drax(drax_value value, int formated) {
  if (IS_BOOL(value)) {
    printf(CAST_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    printf("nil");
  } else if (IS_NUMBER(value)) {
    DPRINT_COLOR1(
      D_COLOR_BLUE,
      D_NUMBER_STR_PRINT,
      CAST_NUMBER(value)
    );
  } else if(IS_MODULE(value)) {
    printf("<module:%s>", CAST_MODULE(value)->name);
  } else if (IS_STRUCT(value)) {
    print_d_struct(value, formated);
  }
}

void dbreak_line() { putchar('\n'); }