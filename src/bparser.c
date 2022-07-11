#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bparser.h"
#include "blex.h"

beorn_state* bs;
b_token* gtoken;
stack_bpsm* gsb;
bg_error* gberr;

int gcurr_line_number = 1;

/* alias handler */
int create_stack_bpsm() {
  gsb = (stack_bpsm*) malloc(sizeof(stack_bpsm));
  gsb->size = 0;
  gsb->bpsm = (bpsm**) malloc(sizeof(bpsm*));
  return 0;
}

int increment_stack_bpsm() {
  if ((NULL == gsb) || (gsb->size == 0)) return 0;
  gsb->bpsm[gsb->size - 1]->count++;
  return 0;
}

int del_first_stack_bpsm() {
  if ((NULL == gsb) || (gsb->size == 0)) return 0;

  free(gsb->bpsm[gsb->size -1]);
  gsb->size--;
  gsb->bpsm = (bpsm**) realloc(gsb->bpsm, sizeof(bpsm*) * gsb->size);
  return 0;
}

int add_elem_stack_bpsm() {
  if (NULL == gsb) return 0;

  gsb->size++;
  gsb->bpsm = (bpsm**) realloc(gsb->bpsm, sizeof(bpsm*) * gsb->size);
  gsb->bpsm[gsb->size -1] = (bpsm*) malloc(sizeof(bpsm));
  gsb->bpsm[gsb->size -1]->mode = BP_NONE;
  gsb->bpsm[gsb->size -1]->count = 0;
  return 0;
}

/* helpers */

beorn_state* new_definition() {
  beorn_state* bdef = new_expression();
  return bdef;
}

beorn_state* new_call_definition() {
  beorn_state* bdef = new_expression();
  bdef->call_definition = 1;
  return bdef;
}

beorn_state* new_parser_error(const char* msg) {
  beorn_state* err = new_error(BPARSER_ERROR, msg);
  return err;
}

int initialize_new_state(stack_bpsm* gs, esm s) {
  add_elem_stack_bpsm(gs);

  gs->bpsm[gs->size -1]->mode = s;
  return 1;
}

int bauto_state_update(stack_bpsm* gs, beorn_state* b, esm tp, int lenght) {
  if (gs->size <= 0) return 0;

  bpsm* curr = gs->bpsm[gs->size -1];
  if (curr->mode == tp) {
    if (curr->count == lenght) {
      b->closed = 1;
      close_pending_structs(gs, b, BT_EXPRESSION);   
      del_first_stack_bpsm(gs);
    }
  }
  return 0; 
}

void auto_state_update(stack_bpsm* gs, beorn_state* b) {
  bauto_state_update(gs, b, BP_ONE_ARG,             2);
  bauto_state_update(gs, b, BP_TWO_ARG,             3);
  bauto_state_update(gs, b, BP_THREE_ARG,           4);
  bauto_state_update(gs, b, BP_FOUR_ARG,            5);

  bauto_state_update(gs, b, BP_SIMPLE_DEFINITIONS,  3);
  bauto_state_update(gs, b, BP_LAMBDA_DEFINITION,   3);
  bauto_state_update(gs, b, BP_FUNCTION_DEFINITION, 4);
}

beorn_state* get_last_state(beorn_state* root) {
  if (root->length <= 0) { return NULL; }
  beorn_state* curr;

  if (((root->child[root->length - 1]->type == BT_PACK) || 
       (root->child[root->length - 1]->type == BT_LIST) || 
       (root->child[root->length - 1]->type == BT_EXPRESSION)) &&
       (root->child[root->length - 1]->closed == 0))
  {
    curr = get_last_state(root->child[root->length - 1]);
  }
  
  curr = root->child[root->length - 1];
  root->length--;
  root->child = (beorn_state**) realloc(root->child, sizeof(beorn_state*) * root->length);
  return curr;
}

/**
 * Add new nodes to AST.
 * Always the last open expression.
 */
int add_child(stack_bpsm* gs, beorn_state* root, beorn_state* child) {
  if (root->length <= 0) {
    root->length++;
    root->child = (beorn_state**) malloc(sizeof(beorn_state*));
    root->child[0] = child;
    increment_stack_bpsm(gs);

    return 1;
  } else {
    beorn_state* crr;
    if (((root->child[root->length - 1]->type == BT_PACK) || 
         (root->child[root->length - 1]->type == BT_LIST) || 
         (root->child[root->length - 1]->type == BT_EXPRESSION)) &&
         (root->child[root->length - 1]->closed == 0))
    {
      crr  = root->child[root->length - 1];
      if (add_child(gs, crr, child)) return 1;
    } else {
      crr = root;
    }

    crr->length++;

    if (crr->length <= 1) {
      crr->child = (beorn_state**) malloc(sizeof(beorn_state*));
    } else {
      crr->child = (beorn_state**) realloc(crr->child, sizeof(beorn_state*) * crr->length);
    }
    crr->child[crr->length - 1] = child;
    increment_stack_bpsm(gs);
    return 1;
  }
}

/**
 * Close the last open expression/sub expression in AST.
 */
int close_pending_structs(stack_bpsm* gs, beorn_state* root, types ct) {
  if (root->length == 0) return 0;

  if (((root->child[root->length - 1]->type == BT_PACK) || 
       (root->child[root->length - 1]->type == BT_LIST) ||
       (root->child[root->length - 1]->type == BT_EXPRESSION)) &&
       (root->child[root->length - 1]->closed == 0))
  {
    if (close_pending_structs(gs, root->child[root->length - 1], ct )) {
      return 1;
    } else {
      if (root->child[root->length - 1]->type == ct) {
        root->child[root->length - 1]->closed = 1;
        del_first_stack_bpsm(gs);
        return 1;
      }
    }
  }

  return 0;
}

void next_token() {
  free(gtoken);
  gtoken = lexan();

  if (TK_BREAK_LINE == gtoken->type) {
    gcurr_line_number++;
  }
}

void set_gberror(const char *msg) {
  if (gberr->has_error) return;

  gberr->has_error = 1;
  gberr->state_error = new_error(BSYNTAX_ERROR, msg);
}

/**
 * Helpers to arith. expression tree.
 */

beorn_state* get_curr_bvalue() {  
  switch (gtoken->type)
  {
    case TK_INTEGER: return new_integer(gtoken->ival);
    
    case TK_FLOAT: return new_float(gtoken->fval);

    default:
      set_gberror("Unspected expression type!");
      break;
  }

  return NULL;
}

blex_types get_crr_type() {
  if (NULL == gtoken) return 0;
  return gtoken->type;
}

b_operator get_operator() {
  switch (gtoken->type)
  {
    case TK_ADD: return BADD;
    case TK_SUB: return BSUB;
    case TK_MUL: return BMUL;
    case TK_DIV: return BDIV;
    default: return BINVALID;
  }
}

expr_tree *new_node(blex_types type, b_operator operation, expr_tree *left, 
  expr_tree *right, beorn_state *value
) {
    expr_tree *n = (expr_tree*) malloc(sizeof(expr_tree));
    if (NULL == n) { set_gberror("Memory error, fail to process expression"); }
    n->type = type;
    n->op = operation;
    n->left = left;
    n->right = right;
    n->value = value;
    return n;
}

expr_tree *value_expr() {
  if (TK_SYMBOL == gtoken->type) {
    char* rigth_symbol = gtoken->cval;
    next_token();
    if (TK_PAR_OPEN == gtoken->type)  {
      add_child(0, bs, new_definition());
      add_child(0, bs, new_symbol(rigth_symbol));
      next_token();
      get_args_by_comma();

      if(!close_pending_structs(0, bs, BT_EXPRESSION)) 
        set_gberror("expression error, pair not found.");

      beorn_state* crr_bs = get_last_state(bs);
      expr_tree *result = new_node(TK_SYMBOL, BNONE, NULL, NULL, crr_bs);
      next_token();
      return result;
    } else {
      expr_tree *result = new_node(TK_SYMBOL, BNONE, NULL, NULL, new_symbol(rigth_symbol));
      return result;
    }
  }

  if ((TK_INTEGER == gtoken->type) || (TK_FLOAT == gtoken->type)) {
      expr_tree *result = new_node(gtoken->type, BNONE, NULL, NULL, get_curr_bvalue());
      next_token();
      return result;
  }

  set_gberror("ArithmeticError, bad argument to expression.");
  return NULL;
}

expr_tree *preference_expr() {
    if (gtoken->type == TK_PAR_OPEN) {
      next_token();
      expr_tree *expr = add_expr();
      next_token();
      return expr;
    }

    return value_expr();
}

expr_tree *mult_expr() {
    expr_tree *expr = preference_expr();
    while (gtoken->type == TK_MUL || gtoken->type == TK_DIV) {
        b_operator op = get_operator();
        next_token();
        expr_tree *expr2 = preference_expr();
        expr = new_node(gtoken->type, op, expr, expr2, NULL);
    }
    return expr;
}

expr_tree *add_expr() {
    expr_tree *expr = mult_expr();
    while (gtoken->type == TK_ADD || gtoken->type == TK_SUB) {
        b_operator op = get_operator();
        next_token();
        expr_tree *expr2 = mult_expr();
        expr = new_node(gtoken->type, op, expr, expr2, NULL);
    }
    return expr;
}

expr_tree *build_expr_tree() {
    expr_tree *expr = add_expr();
    return expr;
}

/**
 * Convert expression to beorn expression
 */

void infix_to_bexpression(beorn_state* sbs, expr_tree *expr) {
  if (NULL == expr) return;

  if ((BNONE == expr->op) &&
      ((TK_INTEGER == expr->type) || (TK_FLOAT == expr->type) || (TK_SYMBOL == expr->type))
  ) {
    add_child(0, sbs, expr->value);
    free(expr);
    return;
  }

  add_child(0, bs, new_expression());

  switch (expr->op)
  {
    case BADD: add_child(0, bs, new_symbol("+")); break;
    case BSUB: add_child(0, bs, new_symbol("-")); break;
    case BMUL: add_child(0, bs, new_symbol("*")); break;
    case BDIV: add_child(0, bs, new_symbol("/")); break;
    
    default:
      set_gberror("invalid operation.");
      break;
  }

  infix_to_bexpression(bs, expr->left);
  infix_to_bexpression(bs, expr->right);

  free(expr);

  if(!close_pending_structs(0, bs, BT_EXPRESSION))
    set_gberror("expression error, pair not found.");
}

int get_args_by_comma() {
  int processing = 1;
  int param_qtt = 0;

  while (processing)
  {
    process_token();
    processing = TK_COMMA == gtoken->type; 

    if (processing) {
      next_token();
      param_qtt++;
    }
  }
  return param_qtt;
}

/* Main language definition */

static void beorn_arith_expression() {
  expr_tree* trr = build_expr_tree();
  infix_to_bexpression(bs, trr);
}

static int beorn_call_function() {
  b_token* nxt = b_check_next(NULL);
  if (TK_PAR_OPEN != nxt->type) {
    free(nxt);
    return 0;
  }

  free(nxt);
  char* rigth_symbol = gtoken->cval;
  next_token();

  next_token();
  add_child(0, bs, new_call_definition());
  add_child(0, bs, new_symbol(rigth_symbol));
  get_args_by_comma();

  return 1;
}

static int beorn_define_var() {
  b_token* nxt = b_check_next(NULL);
  if (TK_EQ != nxt->type) {
    free(nxt);
    return 0;
  }

  free(nxt);
  char* rigth_symbol = gtoken->cval;
  next_token();

  if (initialize_new_state(gsb, BP_SIMPLE_DEFINITIONS) == 0) {
    set_gberror("Fail to initialize definition");
    return 1;
  }

  next_token();
  add_child(0,   bs, new_definition());
  add_child(gsb, bs, new_symbol("set"));
  add_child(gsb, bs, new_symbol(rigth_symbol));
  process_token();

  if(!close_pending_structs(0, bs, BT_EXPRESSION)) {
      set_gberror("function pair not found.");
  }

  return 1;
}

static int is_arith_op(blex_types type) {
  return (TK_ADD == type) || (TK_DIV == type) || 
         (TK_MUL == type) || (TK_SUB == type);
}

static int check_is_conclusion_token() {
  return 
    (TK_BREAK_LINE == gtoken->type) ||
    (TK_EOF == gtoken->type) ||
    (TK_END == gtoken->type) ||
    (TK_COMMA == gtoken->type) ||
    (TK_PAR_CLOSE == gtoken->type) ||
    (TK_BRACE_CLOSE == gtoken->type) ||
    (TK_BRACKET_CLOSE == gtoken->type);
}

static int arithm_simple_terminator(blex_types tt){//add comma
  return ((TK_BREAK_LINE == tt) || (TK_EOF == tt) || (TK_END == tt));
}

static int curr_exp_is_arithm_op() {
  int zero = 0;
  int* jump_count = &zero;
  int cnte_proc = 1;
  b_token* nxt = b_check_next(jump_count);
  size_t stack_counter = 0;

  if (is_arith_op(nxt->type))  { return 1; }
  
  /* if curr. expr. is number */
  if (arithm_simple_terminator(nxt->type)) { return 0; }

  while (cnte_proc) {
    if (TK_PAR_OPEN == nxt->type)  { stack_counter++; }
    if (TK_PAR_CLOSE == nxt->type) { stack_counter--; }
    nxt = b_check_next(jump_count);

    if ((stack_counter == 0) && (is_arith_op(nxt->type))) return 1;

    cnte_proc = (!arithm_simple_terminator(nxt->type));
  }
  free(nxt);

  return 0;
}

static int beorn_arith_op() {
  
  if (!curr_exp_is_arithm_op()) { return 0; }

  beorn_arith_expression();
  if (!check_is_conclusion_token()) {
    set_gberror("Unspected token");
  }
  return 1;
}

static int beorn_import_file() {
  next_token();
  add_child(0, bs, new_definition());
  add_child(0, bs, new_symbol("import"));

  if (TK_STRING != gtoken->type) {
    set_gberror("Cannot use import to current definition");
    return 1;
  }
  add_child(0, bs, new_string(gtoken->cval));

  if(!close_pending_structs(0, bs, BT_EXPRESSION)) {
      set_gberror("function pair not found.");
  }

  next_token();

  return 1;
}

static int beorn_end_definition() {
  if(!close_pending_structs(0, bs, BT_PACK)) {
    set_gberror("pack freeze pair not found.");
    return 1;
  }

  if(!close_pending_structs(0, bs, BT_EXPRESSION)) {
    set_gberror("function pair not found.");
    return 1;
  }

  return 0;
}

static int beorn_function_definition() {
  add_child(0, bs, new_definition());
  add_child(0, bs, new_symbol("fun"));
  next_token();

  if (TK_SYMBOL != gtoken->type) { set_gberror("Invalid function name."); }

  add_child(0, bs, new_symbol(gtoken->cval));
  next_token();

  if (TK_PAR_OPEN != gtoken->type) { set_gberror("Bad definition to function."); }

  next_token();

  int process = 1;
  
  add_child(0, bs, new_list());
  if (TK_PAR_CLOSE != gtoken->type) {
    while (process) {
      if (TK_SYMBOL != gtoken->type) {
        set_gberror("Invalid function param.");
        return 1;
      }

      add_child(0, bs, new_symbol(gtoken->cval));
      next_token();

      process = (TK_COMMA == gtoken->type);
      if (process) { next_token(); }
    }
  }
  
  if(!close_pending_structs(gsb, bs, BT_LIST)) {
    set_gberror("params pair not found.");
    return 1;
  }
  
  if (TK_PAR_CLOSE!= gtoken->type) {
    set_gberror("pair not identified to params.");
    return 1;
  }

  next_token();

  if (TK_DO != gtoken->type) { 
    set_gberror("bad definition to function.");
    return 1;
  }
  add_child(0, bs, new_pack());
  next_token();

  process = 1;
  while (process) {
    if (TK_END == gtoken->type) {
      beorn_end_definition();
      return 1;
    }
    process_token();

    if (TK_END == gtoken->type) {
      beorn_end_definition();
      return 1;
    }
    process = ((TK_EOF != gtoken->type) && (TK_END != gtoken->type));
  }

  return 1;
}

/* if definition */

static int beorn_if_definition() {
  add_child(0, bs, new_definition());
  add_child(0, bs, new_symbol("if"));

  next_token();
  process_token();

  if (TK_DO != gtoken->type) { 
    set_gberror("Fail to define 'if', unspected token.");
    return 1;
  }

  add_child(0, bs, new_pack());

  next_token();
  int process = 1;
  while (process) {
    
    process = (
      (TK_EOF != gtoken->type) &&
      (TK_END != gtoken->type) &&
      (TK_ELSE != gtoken->type)
    );

    if (process) { process_token(); }
  }

  if(!close_pending_structs(0, bs, BT_PACK)) {
    set_gberror("pack freeze pair not found.");
    return 1;
  }

  if (TK_ELSE == gtoken->type) {

    add_child(0, bs, new_pack());

    next_token();
    process = 1;
    while (process) {

      process = (
        (TK_EOF != gtoken->type) &&
        (TK_END != gtoken->type) &&
        (TK_ELSE != gtoken->type)
      );

      if (process) { process_token(); }
    }
  } else {
    add_child(0, bs, new_pack());
  }

  if (TK_END != gtoken->type) { 
    set_gberror("Fail to define 'if', unspected token.");
    return 1;
  }

  beorn_end_definition();

  next_token();
  
  return 1;
}

void process_token() {
  switch (gtoken->type) {
    case TK_BREAK_LINE:
      next_token();
      break;

    case TK_FLOAT:
      if (beorn_arith_op()) { break; }
      add_child(gsb, bs, new_float(gtoken->fval));
      next_token();
      break;

    case TK_INTEGER:
      if (beorn_arith_op()) { break; }
      add_child(gsb, bs, new_integer(gtoken->ival));
      next_token();
      break;

    case TK_PAR_OPEN:
      if (beorn_arith_op()) { break; };
      next_token();
      process_token();

      if (TK_PAR_CLOSE != gtoken->type) { set_gberror("Expression pair not found."); }
      next_token();
      break;

    case TK_SYMBOL: {
      if (beorn_define_var()) { break; };
      if (beorn_arith_op()) { break; };
      if (beorn_call_function()) { break; };

      add_child(gsb, bs, new_symbol(gtoken->cval));
      
      next_token();
      break;
    }

    case TK_IF:
      beorn_if_definition();
      break;

    case TK_IMPORT:
      beorn_import_file();
      break;

    case TK_FUN:
      beorn_function_definition();
      next_token();
      break;

    case TK_LAMBDA: break;

    case TK_STRING:
      add_child(gsb, bs, new_string(gtoken->cval));
      next_token();
      break;

    case TK_PAR_CLOSE:
      if(!close_pending_structs(gsb, bs, BT_EXPRESSION))
        set_gberror("Expression pair not found.");
      next_token();
      break;

    case TK_BRACE_OPEN: /* pending */ next_token(); break;
    case TK_BRACE_CLOSE: /* pending */ next_token(); break;
    case TK_BRACKET_OPEN: {
      add_child(gsb, bs, new_list());

      // if (initialize_new_state(gsb, BP_DINAMIC) == 0)
        // set_gberror("Fail to create list.");

      next_token();
      get_args_by_comma();
      break;
    }

    case TK_BRACKET_CLOSE: {
      if(!close_pending_structs(gsb, bs, BT_LIST))
        set_gberror("list pair not found.");
      next_token();
      break;
    }

    case TK_END: {
      beorn_end_definition();
      next_token();
      break;
    }

    default:
      set_gberror("Unspected token.");
      next_token();
      break;
  }
}

beorn_state* beorn_parser(char *input) {
  gcurr_line_number = 1;
  
  create_stack_bpsm();
  gberr = (bg_error*) malloc(sizeof(bg_error));
  gberr->line = 0;
  gberr->has_error = 0;
  gberr->state_error = NULL;

  bs = (beorn_state*) malloc(sizeof(beorn_state));
  bs->type = BT_PROGRAM;
  bs->child = (beorn_state**) malloc(sizeof(beorn_state*));
  bs->length = 0;
  bs->closed = 0;

  init_lexan(input);
  gtoken = NULL;

  next_token();
  while (TK_EOF != get_crr_type()) {
    auto_state_update(gsb, bs);
    process_token();
    
    if (gberr->has_error) {
      free(gsb);
      del_bstate(bs);
      return gberr->state_error;
    }
  }
  
  free(gsb);
  return bs;
}
