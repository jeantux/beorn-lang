#include <stdlib.h>
#include <stdio.h>
#include "dtypes.h"
#include "dprint.h"

void bshow_error(drax_state* curr) {
  const char * et = berrors_to_str(curr->et);
  if (NULL != curr->trace) {
    printf("%sline: %ld, %s: %s%s", BRED, curr->trace->line, et, (char*) curr->val, BDEF);    
    return;
  }
  printf("%s%s: %s%s", BRED, et, (char*) curr->val, BDEF);
}

void bprint_pack(drax_state* curr) {
  putchar('{');
  for (int i = 0; i < curr->length; i++) {
    bprint(curr->child[i]);
    if (i + 1 < curr->length) putchar(' ');
  }
  putchar('}');
}

void bprint_list(drax_state* curr) {
  putchar('[');
  for (int i = 0; i < curr->length; i++) {
    bprint(curr->child[i]);
    if (i + 1 < curr->length) {
      putchar(',');
      putchar(' ');
    }
  }
  putchar(']');
}

void bprint_expression(drax_state* curr) {
    putchar('(');
    for (int i = 0; i < curr->length; i++) {
      bprint(curr->child[i]);
      if (i + 1 < curr->length) putchar(' ');
    }
    putchar(')');
}

void bprint(drax_state* curr) {
  bprint_default(curr, 1);
}

static int prtstr(int sstr, char* cval) {
  if (sstr) {
    printf("\"%s\"", cval);
  } else {
    printf("%s", cval);
  }
  return 1;
}

void bprint_default(drax_state* curr, int sstr) {
  switch (curr->type) {
    case BT_INTEGER:
    case BT_FLOAT:        printf("%g", draxvalue_to_num(curr->val));     
                          break;
    case BT_STRING:       prtstr(sstr,  (char*) curr->val); break;
    case BT_SYMBOL:       printf("%s",  (char*) curr->val); break;
    case BT_FUNCTION:     printf("#function<>");            break;
    case BT_LAMBDA:       printf("#lambda<>");              break;
    case BT_PACK:         bprint_pack(curr);                break;
    case BT_LIST:         bprint_list(curr);                break;
    case BT_EXPRESSION:   bprint_expression(curr);          break;
    case BT_NIL:          printf("nil");                    break;
    case BT_ERROR:        bshow_error(curr);                break;
    
    default:
      bshow_error(new_error(BRUNTIME_ERROR, "unspected token."));
      break;
  }

  free(curr);
}

void bbreak_line() { putchar('\n'); }
void bspace_line() { putchar(' '); }
