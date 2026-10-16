/* The type checker (see front/check.h). SHARED by the tcc kits.

   Checking is bidirectional. A term is checked against an expected type (the
   hint) or its type is inferred. Two types are equal when their normal forms
   are equal (front/eval.c). The checker writes core terms (front/core.h). */
#include "front/check.h"

#include <string.h>

#include "front/face.h"
#include "front/fr.h"

#define CHECK_DEPTH_LIMIT 2000u
#define SPINE_MAX 64u
#define SHOW_MAX 512u
#define NAMES_MAX 64u
#define UNIVERSE_MAX 2u
#define PRINT_MAX 131072u /* M3 s5-i (USER):  thu prints 106,675 bytes */
#define NO_FAMILY UINT32_MAX
#define NO_DEF UINT32_MAX

#define FAIL(c, code, ...) diag_fail((c)->m->diag, (code), (c)->m->def, __VA_ARGS__)

typedef struct Scope Scope;
struct Scope {
  const char *name; /* NULL for the binder of `A -> B` */
  const Value *type; /* NULL for an interval variable */
  int interval;      /* 1 for an interval variable (M2) */
  uint32_t level;
  const Scope *next;
};

typedef struct {
  Machine *m;
  const DeclList *decls;
  size_t decl_index;
  const Scope *scope; /* the innermost binder first */
  const Env *env;     /* a variable for each binder */
  uint32_t level;     /* the number of binders */
  unsigned depth;
  uint32_t eta_count;
  uint32_t self;      /* the family whose fields are checked, or NO_FAMILY */
  uint32_t self_uses; /* the uses of that family in the current field */
} Checker;

typedef enum {
  RULE_TYPE0,
  RULE_TYPE1,
  RULE_TYPE2,
  RULE_VALUE0,
  RULE_PAIR,
  RULE_INJ,
  RULE_EMPTY,
  RULE_SOME,
  RULE_CONS,
  RULE_PACK,
  RULE_REFL,
  RULE_PART,
  RULE_SIGMA_PART,
  RULE_EITHER,
  RULE_OPTION,
  RULE_PURE,
  RULE_MAP,
  RULE_BIND,
  RULE_FILTER,
  RULE_FOLD,
  RULE_UNFOLD,
  RULE_CONG,
  RULE_NAT2,
  RULE_FLAG_IF,
  RULE_INTERVAL,
  RULE_PATH,
  RULE_PATHP,
  RULE_SYM,
  RULE_FUNEXT,
  RULE_HAPPLY,
  RULE_PARTIAL,
  RULE_SSET,
  RULE_TRANSP,
  RULE_HCOMP,
  RULE_COMP,
  RULE_HFILL,
  RULE_TRANS,
  RULE_TRANSPORT,
  RULE_SUBST,
  RULE_TRANSPORT_REFL,
  RULE_J,
  RULE_J_REFL,
  RULE_IS_CONTR,
  RULE_IS_PROP,
  RULE_FIBER,
  RULE_IS_EQUIV,
  RULE_EQUIV,
  RULE_ID_EQUIV,
  RULE_GLUE,
  RULE_GLUE_ELEM,
  RULE_UNGLUE,
  RULE_UA,
  RULE_UA_BETA
} Rule;

typedef struct {
  const char *name;
  uint32_t arity; /* fold: the least number of arguments */
  Rule rule;
  Op op;
  Op result;     /* VALUE0, NAT2: the result type. EMPTY, INJ: the type former. */
  uint32_t part; /* INJ, PART, SIGMA_PART: 0 or 1 */
} Builtin;

static const Builtin BUILTINS[] = {
  {"Nat", 0, RULE_TYPE0, OP_NAT, OP_NAT, 0},
  {"Flag", 0, RULE_TYPE0, OP_FLAG, OP_NAT, 0},
  {"Field", 0, RULE_TYPE0, OP_FIELD, OP_NAT, 0},
  {"Unit", 0, RULE_TYPE0, OP_UNIT, OP_NAT, 0},
  {"Prod", 2, RULE_TYPE2, OP_PROD, OP_NAT, 0},
  {"Sum", 2, RULE_TYPE2, OP_SUM, OP_NAT, 0},
  {"Option", 1, RULE_TYPE1, OP_OPTION, OP_NAT, 0},
  {"List", 1, RULE_TYPE1, OP_LIST, OP_NAT, 0},
  {"unit", 0, RULE_VALUE0, OP_UNIT_VAL, OP_UNIT, 0},
  {"flagYes", 0, RULE_VALUE0, OP_FLAG_YES, OP_FLAG, 0},
  {"flagNo", 0, RULE_VALUE0, OP_FLAG_NO, OP_FLAG, 0},
  {"pair", 2, RULE_PAIR, OP_PAIR, OP_PROD, 0},
  {"inl", 1, RULE_INJ, OP_INL, OP_SUM, 0},
  {"inr", 1, RULE_INJ, OP_INR, OP_SUM, 1},
  {"none", 0, RULE_EMPTY, OP_NONE, OP_OPTION, 0},
  {"nil", 0, RULE_EMPTY, OP_NIL, OP_LIST, 0},
  {"some", 1, RULE_SOME, OP_SOME, OP_OPTION, 0},
  {"cons", 2, RULE_CONS, OP_CONS, OP_LIST, 0},
  {"pack", 2, RULE_PACK, OP_PACK, OP_NAT, 0},
  {"refl", 0, RULE_REFL, OP_PATHP, OP_NAT, 0},
  {"first", 1, RULE_PART, OP_FIRST, OP_PROD, 0},
  {"second", 1, RULE_PART, OP_SECOND, OP_PROD, 1},
  {"witness", 1, RULE_SIGMA_PART, OP_WITNESS, OP_NAT, 0},
  {"payload", 1, RULE_SIGMA_PART, OP_PAYLOAD, OP_NAT, 1},
  {"either", 3, RULE_EITHER, OP_EITHER, OP_SUM, 0},
  {"option", 3, RULE_OPTION, OP_OPTION_ELIM, OP_OPTION, 0},
  {"pure", 1, RULE_PURE, OP_PURE, OP_NAT, 0},
  {"map", 2, RULE_MAP, OP_MAP, OP_NAT, 0},
  {"bind", 2, RULE_BIND, OP_BIND, OP_NAT, 0},
  {"filter", 2, RULE_FILTER, OP_FILTER, OP_NAT, 0},
  {"fold", 2, RULE_FOLD, OP_FOLD_NAT, OP_NAT, 0},
  {"unfold", 3, RULE_UNFOLD, OP_UNFOLD, OP_LIST, 0},
  {"cong", 2, RULE_CONG, OP_PATHP, OP_NAT, 0},
  {"natAdd", 2, RULE_NAT2, OP_NAT_ADD, OP_NAT, 0},
  {"natSub", 2, RULE_NAT2, OP_NAT_SUB, OP_NAT, 0},
  {"natMul", 2, RULE_NAT2, OP_NAT_MUL, OP_NAT, 0},
  {"natEq", 2, RULE_NAT2, OP_NAT_EQ, OP_FLAG, 0},
  {"natLe", 2, RULE_NAT2, OP_NAT_LE, OP_FLAG, 0},
  {"flagIf", 3, RULE_FLAG_IF, OP_FLAG_IF, OP_FLAG, 0},
  {"I", 0, RULE_INTERVAL, OP_NAT, OP_NAT, 0},
  {"i0", 0, RULE_INTERVAL, OP_NAT, OP_NAT, 0},
  {"i1", 0, RULE_INTERVAL, OP_NAT, OP_NAT, 0},
  {"Path", 3, RULE_PATH, OP_PATHP, OP_NAT, 0},
  {"PathP", 3, RULE_PATHP, OP_PATHP, OP_NAT, 0},
  {"sym", 1, RULE_SYM, OP_PATHP, OP_NAT, 0},
  {"funExt", 1, RULE_FUNEXT, OP_PATHP, OP_NAT, 0},
  {"happly", 2, RULE_HAPPLY, OP_PATHP, OP_NAT, 0},
  {"Partial", 2, RULE_PARTIAL, OP_PARTIAL, OP_NAT, 0},
  {"SSet", 0, RULE_SSET, OP_SSET, OP_NAT, 0},
  {"transp", 3, RULE_TRANSP, OP_TRANSP, OP_NAT, 0},
  {"hcomp", 4, RULE_HCOMP, OP_HCOMP, OP_NAT, 0},
  {"comp", 4, RULE_COMP, OP_HCOMP, OP_NAT, 0},
  {"hfill", 5, RULE_HFILL, OP_HCOMP, OP_NAT, 0},
  {"trans", 2, RULE_TRANS, OP_HCOMP, OP_NAT, 0},
  {"transport", 2, RULE_TRANSPORT, OP_TRANSP, OP_NAT, 0},
  {"subst", 3, RULE_SUBST, OP_TRANSP, OP_NAT, 0},
  {"transportRefl", 1, RULE_TRANSPORT_REFL, OP_TRANSP, OP_NAT, 0},
  {"J", 3, RULE_J, OP_TRANSP, OP_NAT, 0},
  {"JRefl", 2, RULE_J_REFL, OP_TRANSP, OP_NAT, 0},
  {"isContr", 1, RULE_IS_CONTR, OP_PATHP, OP_NAT, 0},
  {"isProp", 1, RULE_IS_PROP, OP_PATHP, OP_NAT, 0},
  {"fiber", 2, RULE_FIBER, OP_PATHP, OP_NAT, 0},
  {"isEquiv", 1, RULE_IS_EQUIV, OP_PATHP, OP_NAT, 0},
  {"Equiv", 2, RULE_EQUIV, OP_PATHP, OP_NAT, 0},
  {"idEquiv", 1, RULE_ID_EQUIV, OP_PACK, OP_NAT, 0},
  {"Glue", 2, RULE_GLUE, OP_GLUE, OP_NAT, 0},
  {"glue", 2, RULE_GLUE_ELEM, OP_GLUE_ELEM, OP_NAT, 0},
  {"unglue", 1, RULE_UNGLUE, OP_UNGLUE, OP_NAT, 0},
  {"ua", 1, RULE_UA, OP_GLUE, OP_NAT, 0},
  {"uaBeta", 2, RULE_UA_BETA, OP_TRANSP, OP_NAT, 0}
};

#define BUILTIN_COUNT ((uint32_t)(sizeof BUILTINS / sizeof BUILTINS[0]))

/* Names that no declaration can take (rule R4). M3 D10: each Kan, Glue and
   univalence name is reserved from s1, and its slice moves it to BUILTINS. */
static const char *const RESERVED[] = {
  "Bool", "Text"
};

typedef enum {
  HEAD_TERM, /* a local, a definition or another term: plain application */
  HEAD_FAMILY,
  HEAD_CTOR,
  HEAD_PROJ,
  HEAD_BUILTIN
} HeadKind;

typedef struct {
  HeadKind kind;
  uint32_t index; /* FAMILY: the family. CTOR, PROJ: the constructor. */
  uint32_t field; /* PROJ: the field number */
  const Builtin *builtin;
} Head;

/* One application of a core name to the arguments of a spine. */
typedef struct {
  Checker *c;
  const Builtin *b;
  const Term *const *args;
  uint32_t argc;      /* the arguments in the spine */
  const Value *hint;  /* the expected type of the result, or NULL */
  const Value *outer; /* the expected type of the whole spine, or NULL */
  uint32_t used;      /* the arguments that the core name takes */
  const Value *type;  /* the type of the result */
} Call;

static const Core *nul(int failed) {
  (void)failed;
  return NULL;
}

static Core *new_core(Checker *c, CoreKind kind) {
  Core *k = arena_alloc(c->m->arena, sizeof *k);
  if (k == NULL) {
    FAIL(c, "OOM", "out of memory");
    return NULL;
  }
  k->kind = kind;
  return k;
}

static const Core *core_index(Checker *c, CoreKind kind, uint32_t index) {
  Core *k = new_core(c, kind);
  if (k != NULL)
    k->index = index;
  return k;
}

static const Core *core_leaf(Checker *c, CoreKind kind, uint64_t nat) {
  Core *k = new_core(c, kind);
  if (k != NULL)
    k->nat = nat;
  return k;
}

static const Core *core_bind(Checker *c, CoreKind kind, const char *name, const Core *a, const Core *b) {
  Core *k;
  if (b == NULL || (a == NULL && kind != CORE_LAM && kind != CORE_PLAM))
    return NULL;
  k = new_core(c, kind);
  if (k == NULL)
    return NULL;
  k->name = name;
  k->a = a;
  k->b = b;
  return k;
}

static const Core *core_app(Checker *c, const Core *f, const Core *x) {
  Core *k;
  if (f == NULL || x == NULL)
    return NULL;
  k = new_core(c, CORE_APP);
  if (k == NULL)
    return NULL;
  k->a = f;
  k->b = x;
  return k;
}

static const Core *core_op(Checker *c, Op op, uint32_t inst, uint32_t field, const Core *const *args, uint32_t n) {
  const Core **copy = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *copy);
  Core *k;
  uint32_t i;
  if (copy == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (i = 0; i < n; i++) {
    if (args[i] == NULL)
      return NULL;
    copy[i] = args[i];
  }
  k = new_core(c, CORE_OP);
  if (k == NULL)
    return NULL;
  k->op = op;
  k->inst = inst;
  k->field = field;
  k->args = copy;
  k->argc = n;
  return k;
}

static const Core *op3(Checker *c, Op op, uint32_t inst, uint32_t n, const Core *a, const Core *b, const Core *x) {
  const Core *args[3];
  args[0] = a;
  args[1] = b;
  args[2] = x;
  return core_op(c, op, inst, 0, args, n);
}

static const Value *tyop(Checker *c, Op op, uint32_t n, const Value *a, const Value *b, const Value *x) {
  return val_make(c->m, op, 0, n, a, b, x);
}

static const Value *nat_type(Checker *c) {
  return tyop(c, OP_NAT, 0, NULL, NULL, NULL);
}

static const Value *flag_type(Checker *c) {
  return tyop(c, OP_FLAG, 0, NULL, NULL, NULL);
}

/* The value of the core term K in the current context. */
static const Value *here(Checker *c, const Core *k) {
  return k == NULL ? NULL : eval_core(c->m, c->env, k);
}

/* V in the surface syntax, for a diagnostic. */
static const char *show(Checker *c, const Value *v) {
  const char *names[NAMES_MAX];
  const Scope *s;
  char *buf = arena_alloc(c->m->arena, SHOW_MAX);
  uint32_t count = c->level < NAMES_MAX ? c->level : NAMES_MAX;
  for (s = c->scope; s != NULL; s = s->next) {
    if (s->level < NAMES_MAX)
      names[s->level] = s->name != NULL ? s->name : "_";
  }
  if (buf == NULL || v == NULL)
    return "?";
  value_print(c->m, names, count, v, buf, SHOW_MAX);
  return buf;
}

/* Binds NAME to the variable at the next level: a term variable of TYPE, or
   an interval variable (INTERVAL 1, TYPE NULL). */
static int push_var(Checker *c, const char *name, const Value *type, int interval) {
  Scope *s = arena_alloc(c->m->arena, sizeof *s);
  const Env *e = env_push(c->m, c->env, interval ? val_ivl_var(c->m, c->level) : val_var(c->m, c->level));
  if ((type == NULL && !interval) || s == NULL || e == NULL)
    return c->m->diag->set ? 0 : FAIL(c, "OOM", "out of memory");
  s->name = name;
  s->type = type;
  s->interval = interval;
  s->level = c->level;
  s->next = c->scope;
  c->scope = s;
  c->env = e;
  c->level++;
  return 1;
}

static int push(Checker *c, const char *name, const Value *type) {
  return push_var(c, name, type, 0);
}

static int push_ivl(Checker *c, const char *name) {
  return push_var(c, name, NULL, 1);
}

static void pop(Checker *c) {
  c->scope = c->scope->next;
  c->env = c->env->next;
  c->level--;
}

/* 1 when a term of type FOUND can stand where WANT is expected.
   Universes are not cumulative (former F14). */
static int expect(Checker *c, const Value *found, const Value *want) {
  if (found == NULL || want == NULL)
    return 0;
  if (conv_values(c->m, c->level, found, want))
    return 1;
  return FAIL(c, "TYPE_MISMATCH", "expected a term of type %s, found a term of type %s", show(c, want), show(c, found));
}

static const Scope *find_local(const Checker *c, const char *name) {
  const Scope *s;
  for (s = c->scope; s != NULL; s = s->next) {
    if (s->name != NULL && strcmp(s->name, name) == 0)
      return s;
  }
  return NULL;
}

static uint32_t find_def(const Machine *m, const char *name) {
  uint32_t i;
  for (i = 0; i < m->def_count; i++) {
    if (strcmp(m->defs[i].name, name) == 0)
      return i;
  }
  return NO_DEF;
}

/* An environment that binds VALS in order: the last one is index 0. */
static const Env *env_of(Checker *c, const Value *const *vals, uint32_t n) {
  const Env *e = NULL;
  uint32_t i;
  for (i = 0; i < n; i++)
    e = env_push(c->m, e, vals[i]);
  return e;
}

static const Core *elab(Checker *c, const Term *t, const Value *hint, const Value **type);
static const Core *check_lam(Checker *c, const Term *t, const Value *hint, const Value **type);

static const Core *check(Checker *c, const Term *t, const Value *want) {
  const Value *type = NULL;
  return want == NULL ? NULL : elab(c, t, want, &type);
}

static const Core *infer(Checker *c, const Term *t, const Value **type) {
  *type = NULL;
  return elab(c, t, NULL, type);
}

/* infer, but a failure leaves no diagnostic. */
static const Core *try_infer(Checker *c, const Term *t, const Value **type) {
  Diag saved = *c->m->diag;
  const Core *k = infer(c, t, type);
  if (k == NULL)
    *c->m->diag = saved;
  return k;
}

/* M2 s4: a type in Type n (*LEVEL is n) or in SSet (*SSET is 1). */
static const Core *check_sort(Checker *c, const Term *t, uint64_t *level, int *sset) {
  const Value *type = NULL;
  const Core *k = infer(c, t, &type);
  *sset = 0;
  if (k == NULL)
    return NULL;
  if (val_is(type, OP_SSET)) {
    *sset = 1;
    return k;
  }
  if (type == NULL || type->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "expected a type, found a term of type %s", show(c, type)));
  *level = type->nat;
  return k;
}

/* A type in Type n. A type in SSet is refused (SORT_FIBRANT). */
static const Core *check_type(Checker *c, const Term *t, uint64_t *level) {
  int sset = 0;
  const Core *k = check_sort(c, t, level, &sset);
  if (k != NULL && sset)
    return nul(FAIL(c, "SORT_FIBRANT", "%s is in SSet, not a type in Type n", show(c, here(c, k))));
  return k;
}

/* Compares the inferred *TYPE with HINT. */
static const Core *finish(Checker *c, const Core *k, const Value *hint, const Value **type) {
  if (k == NULL || *type == NULL)
    return NULL;
  if (hint != NULL && !expect(c, *type, hint))
    return NULL;
  *type = hint != NULL ? hint : *type;
  return k;
}

/* 1 when T is Option A, List A or Sum E A. *ELEM is A. */
static int carrier_of(const Value *t, Carrier *carrier, const Value **elem) {
  if (val_is(t, OP_OPTION) || val_is(t, OP_LIST)) {
    *carrier = val_is(t, OP_OPTION) ? CARRIER_OPTION : CARRIER_LIST;
    *elem = t->args[0];
    return 1;
  }
  if (!val_is(t, OP_SUM))
    return 0;
  *carrier = CARRIER_SUM;
  *elem = t->args[1];
  return 1;
}

/* SHAPE (Option, List or Sum E) with the element ELEM. */
static const Value *carried(Checker *c, const Value *shape, const Value *elem) {
  if (shape == NULL)
    return NULL;
  return val_is(shape, OP_SUM) ? tyop(c, OP_SUM, 2, shape->args[0], elem, NULL) : tyop(c, shape->op, 1, elem, NULL, NULL);
}

/* Checks T as a function from DOM. *RESULT is WANT, or the result type that
   T infers when WANT is NULL. */
static const Core *fn_into(Checker *c, const Term *t, const Value *dom, const Value *want, const Value **result) {
  const Value *ft = NULL;
  const Core *f;
  if (want != NULL) {
    *result = want;
    return check(c, t, val_arrow(c->m, dom, want));
  }
  f = infer(c, t, &ft);
  if (f == NULL)
    return NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "expected a function, found a term of type %s", show(c, ft)));
  *result = closure_apply(c->m, ft, val_var(c->m, c->level));
  /* Compare at two fresh arguments before the result leaves the binder.
     Reusing its level in expect would capture a dependent result. */
  if (!conv_values(c->m, c->level + 2u, *result,
                   closure_apply(c->m, ft, val_var(c->m, c->level + 1u))))
    return nul(FAIL(c, "TYPE_MISMATCH", "this operation needs a function with a non-dependent result type"));
  return expect(c, ft, val_arrow(c->m, dom, *result)) ? f : NULL;
}

/* The argument type of the function T, from its binder or its type. */
static const Value *lam_domain(Checker *c, const Term *t) {
  uint64_t level = 0;
  const Value *ft = NULL;
  if (t->kind == TERM_LAM && t->left != NULL)
    return here(c, check_type(c, t->left, &level));
  return try_infer(c, t, &ft) != NULL && ft != NULL && ft->kind == VAL_PI ? ft->dom : NULL;
}

/* The carried argument ARG of map, bind or filter. When ARG does not infer
   (`inr 3`), its type comes from the expected type and from the binder of
   the function FN. */
static const Core *carried_arg(Call *k, const Term *arg, const Term *fn, const Value **type) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  const Value *elem = NULL;
  const Value *dom;
  const Core *x = try_infer(c, arg, type);
  if (x != NULL)
    return x;
  if (!carrier_of(k->hint, &carrier, &elem))
    return infer(c, arg, type);
  dom = lam_domain(c, fn);
  if (dom == NULL)
    return infer(c, arg, type);
  *type = carried(c, k->hint, dom);
  return check(c, arg, *type);
}

static const Core *need_type(Call *k, const char *what) {
  return nul(FAIL(k->c, "TYPE_INFER", "%s needs a declared type %s here", k->b->name, what));
}

static const Core *not_carrier(Call *k, const Value *type) {
  return nul(FAIL(k->c, "TYPE_MISMATCH", "%s needs a value of type Option A, List A or Sum E A, found a term of type %s", k->b->name, show(k->c, type)));
}

static const Core *rule_type1(Call *k) {
  uint64_t level = 0;
  const Core *a = check_type(k->c, k->args[0], &level);
  k->type = val_univ(k->c->m, level);
  return op3(k->c, k->b->op, 0, 1, a, NULL, NULL);
}

static const Core *rule_type2(Call *k) {
  uint64_t la = 0;
  uint64_t lb = 0;
  const Core *a = check_type(k->c, k->args[0], &la);
  const Core *b = a == NULL ? NULL : check_type(k->c, k->args[1], &lb);
  k->type = val_univ(k->c->m, la > lb ? la : lb);
  return op3(k->c, k->b->op, 0, 2, a, b, NULL);
}

static const Core *rule_pair(Call *k) {
  Checker *c = k->c;
  const Value *ta = val_is(k->hint, OP_PROD) ? k->hint->args[0] : NULL;
  const Value *tb = val_is(k->hint, OP_PROD) ? k->hint->args[1] : NULL;
  const Core *a = ta != NULL ? check(c, k->args[0], ta) : infer(c, k->args[0], &ta);
  const Core *b = a == NULL ? NULL : (tb != NULL ? check(c, k->args[1], tb) : infer(c, k->args[1], &tb));
  k->type = tyop(c, OP_PROD, 2, ta, tb, NULL);
  return op3(c, OP_PAIR, 0, 2, a, b, NULL);
}

static const Core *rule_inj(Call *k) {
  if (!val_is(k->hint, OP_SUM))
    return need_type(k, "Sum A B");
  k->type = k->hint;
  return op3(k->c, k->b->op, 0, 1, check(k->c, k->args[0], k->hint->args[k->b->part]), NULL, NULL);
}

static const Core *rule_empty(Call *k) {
  if (!val_is(k->hint, k->b->result))
    return need_type(k, k->b->result == OP_OPTION ? "Option A" : "List A");
  k->type = k->hint;
  return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
}

static const Core *rule_some(Call *k) {
  Checker *c = k->c;
  const Value *a = val_is(k->hint, OP_OPTION) ? k->hint->args[0] : NULL;
  const Core *x = a != NULL ? check(c, k->args[0], a) : infer(c, k->args[0], &a);
  k->type = tyop(c, OP_OPTION, 1, a, NULL, NULL);
  return op3(c, OP_SOME, 0, 1, x, NULL, NULL);
}

static const Core *rule_cons(Call *k) {
  Checker *c = k->c;
  const Value *a = val_is(k->hint, OP_LIST) ? k->hint->args[0] : NULL;
  const Core *x = a != NULL ? check(c, k->args[0], a) : infer(c, k->args[0], &a);
  const Value *list = tyop(c, OP_LIST, 1, a, NULL, NULL);
  const Core *xs = x == NULL ? NULL : check(c, k->args[1], list);
  k->type = list;
  return op3(c, OP_CONS, 0, 2, x, xs, NULL);
}

static const Core *rule_pack(Call *k) {
  Checker *c = k->c;
  const Value *s = k->hint;
  const Core *w;
  const Core *p;
  if (s == NULL || s->kind != VAL_SIGMA)
    return need_type(k, "Sigma (x : A) B");
  w = check(c, k->args[0], s->dom);
  p = w == NULL ? NULL : check(c, k->args[1], closure_apply(c->m, s, here(c, w)));
  k->type = s;
  return op3(c, OP_PACK, 0, 2, w, p, NULL);
}

static const Core *core_ivl(Checker *c, IvlOp op, const Core *a, const Core *b);
static const Core *elab_ivl(Checker *c, const Term *t);
static const Core *rule_transp(Call *k);
static const Core *rule_hcomp(Call *k);
static const Core *rule_comp(Call *k);
static const Core *rule_hfill(Call *k);
static const Core *rule_trans(Call *k);
static const Core *rule_transport(Call *k);
static const Core *rule_subst(Call *k);
static const Core *rule_transport_refl(Call *k);
static const Core *rule_j(Call *k);
static const Core *rule_j_refl(Call *k);
static const Core *rule_is_contr(Call *k);
static const Core *rule_is_prop(Call *k);
static const Core *hit_case_eta(Checker *c, const CtorInfo *ci, const Core *term, const Value *type, uint32_t field, uint32_t ivl, uint32_t level);
static const Core *rule_fiber(Call *k);
static const Core *rule_is_equiv(Call *k);
static const Core *rule_equiv(Call *k);
static const Core *rule_id_equiv(Call *k);
static const Core *rule_glue(Call *k);
static const Core *rule_glue_elem(Call *k);
static const Core *rule_unglue(Call *k);
static const Core *rule_ua(Call *k);
static const Core *rule_ua_beta(Call *k);
static const Core *refl_path(Call *k);
static const Core *cong_path(Call *k, const Core *p, const Value *pt);

static const Core *rule_refl(Call *k) {
  return val_is(k->hint, OP_PATHP) ? refl_path(k) : need_type(k, "Path A a b");
}

static const Core *sigma_part(Call *k, const Core *s, const Value *st);

/* M3 s4: first and second of a Sigma value (for example an Equiv) are its
   witness and its payload. */
static const Core *rule_part(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  if (pt != NULL && pt->kind == VAL_SIGMA)
    return sigma_part(k, p, pt);
  if (!val_is(pt, OP_PROD))
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a pair, found a term of type %s", k->b->name, show(c, pt)));
  k->type = pt->args[k->b->part];
  return op3(c, k->b->op, 0, 1, p, NULL, NULL);
}

static const Core *rule_sigma_part(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = infer(c, k->args[0], &st);
  if (s == NULL)
    return NULL;
  if (st == NULL || st->kind != VAL_SIGMA)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a Sigma value, found a term of type %s", k->b->name, show(c, st)));
  return sigma_part(k, s, st);
}

/* S : ST, a Sigma type. PART 0 gives the witness, PART 1 the payload. */
static const Core *sigma_part(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  const Core *w = op3(c, OP_WITNESS, 0, 1, s, NULL, NULL);
  k->type = k->b->part == 0u ? st->dom : closure_apply(c->m, st, here(c, w));
  return k->b->part == 0u ? w : op3(c, OP_PAYLOAD, 0, 1, s, NULL, NULL);
}

/* either f g s */
static const Core *rule_either(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Value *res = NULL;
  const Core *s = infer(c, k->args[2], &st);
  const Core *f;
  const Core *g;
  if (s == NULL)
    return NULL;
  if (!val_is(st, OP_SUM))
    return nul(FAIL(c, "TYPE_MISMATCH", "either needs a value of type Sum A B, found a term of type %s", show(c, st)));
  f = fn_into(c, k->args[0], st->args[0], k->hint, &res);
  g = f == NULL ? NULL : check(c, k->args[1], val_arrow(c->m, st->args[1], res));
  k->type = res;
  return op3(c, OP_EITHER, 0, 3, f, g, s);
}

/* option z f o */
static const Core *rule_option(Call *k) {
  Checker *c = k->c;
  const Value *ot = NULL;
  const Value *res = k->hint;
  const Core *o = infer(c, k->args[2], &ot);
  const Core *z;
  const Core *f;
  if (o == NULL)
    return NULL;
  if (!val_is(ot, OP_OPTION))
    return nul(FAIL(c, "TYPE_MISMATCH", "option needs a value of type Option A, found a term of type %s", show(c, ot)));
  z = res != NULL ? check(c, k->args[0], res) : infer(c, k->args[0], &res);
  f = z == NULL ? NULL : check(c, k->args[1], val_arrow(c->m, ot->args[0], res));
  k->type = res;
  return op3(c, OP_OPTION_ELIM, 0, 3, z, f, o);
}

static const Core *rule_pure(Call *k) {
  Carrier carrier = CARRIER_OPTION;
  const Value *a = NULL;
  if (!carrier_of(k->hint, &carrier, &a))
    return need_type(k, "Option A, List A or Sum E A");
  k->type = k->hint;
  return op3(k->c, OP_PURE, carrier, 1, check(k->c, k->args[0], a), NULL, NULL);
}

/* map f m */
static const Core *rule_map(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  Carrier hint_carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Value *b = NULL;
  const Value *hint_elem = NULL;
  const Core *x = carried_arg(k, k->args[1], k->args[0], &mt);
  const Core *f;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a))
    return not_carrier(k, mt);
  f = fn_into(c, k->args[0], a, carrier_of(k->hint, &hint_carrier, &hint_elem) ? hint_elem : NULL, &b);
  k->type = carried(c, mt, b);
  return op3(c, OP_MAP, carrier, 2, f, x, NULL);
}

/* bind m f */
static const Core *rule_bind(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  Carrier result_carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Value *b = NULL;
  const Value *res = NULL;
  const Core *x = carried_arg(k, k->args[0], k->args[1], &mt);
  const Core *f;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a))
    return not_carrier(k, mt);
  f = fn_into(c, k->args[1], a, k->hint, &res);
  if (f == NULL)
    return NULL;
  if (!carrier_of(res, &result_carrier, &b) || result_carrier != carrier)
    return nul(FAIL(c, "TYPE_MISMATCH", "bind needs a function into the carrier of %s, found one into %s", show(c, mt), show(c, res)));
  if (carrier == CARRIER_SUM && !conv_values(c->m, c->level, res->args[0], mt->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "bind must preserve the Sum error type %s, found %s", show(c, mt->args[0]), show(c, res->args[0])));
  k->type = res;
  return op3(c, OP_BIND, carrier, 2, x, f, NULL);
}

/* filter p m */
static const Core *rule_filter(Call *k) {
  Checker *c = k->c;
  Carrier carrier = CARRIER_OPTION;
  const Value *mt = NULL;
  const Value *a = NULL;
  const Core *x = carried_arg(k, k->args[1], k->args[0], &mt);
  const Core *p;
  if (x == NULL)
    return NULL;
  if (!carrier_of(mt, &carrier, &a) || carrier == CARRIER_SUM)
    return nul(FAIL(c, "TYPE_MISMATCH", "filter needs a value of type Option A or List A, found a term of type %s", show(c, mt)));
  p = check(c, k->args[0], val_arrow(c->m, a, flag_type(c)));
  k->type = mt;
  return op3(c, OP_FILTER, carrier, 2, p, x, NULL);
}

/* fold step z n (Nat) or fold f z xs (List A). */
static const Core *fold_simple(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  const Value *res = k->hint;
  const Core *z = res != NULL ? check(c, k->args[1], res) : infer(c, k->args[1], &res);
  int list = val_is(st, OP_LIST);
  const Value *step = list ? val_arrow(c->m, st->args[0], val_arrow(c->m, res, res)) : val_arrow(c->m, res, res);
  const Core *f = z == NULL ? NULL : check(c, k->args[0], step);
  k->used = 3u;
  k->type = res;
  return op3(c, list ? OP_FOLD_LIST : OP_FOLD_NAT, 0, 3, f, z, s);
}

/* The type of the fold case of CTOR: one argument for each field (a
   recursive field arrives folded, so its type is RES), then RES. */
static const Value *case_type(Checker *c, const CtorInfo *ctor, const Env *env, const Value *res) {
  const Value *t = res;
  uint32_t i = ctor->field_count;
  while (i > 0u && t != NULL) {
    const FieldInfo *field = &ctor->fields[--i];
    t = val_arrow(c->m, field->recursive ? res : eval_core(c->m, env, field->type), t);
  }
  return t;
}

/* fold case... x over a domain family: one case for each constructor. */
static const Core *fold_family(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  Machine *m = c->m;
  const FamilyInfo *f = &m->families[st->inst];
  const Env *env = env_of(c, st->args, st->argc);
  const Value *res = k->hint;
  const Core **cores = arena_alloc(m->arena, ((size_t)f->ctor_count + 2u) * sizeof *cores);
  uint32_t j;
  if (cores == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (res == NULL && m->ctors[f->first_ctor].field_count == 0u)
    cores[0] = infer(c, k->args[0], &res);
  if (res == NULL)
    return nul(FAIL(c, "TYPE_INFER", "fold over %s needs a declared result type here", f->name));
  for (j = 0; j < f->ctor_count; j++) {
    cores[j] = cores[j] != NULL ? cores[j] : check(c, k->args[j], case_type(c, &m->ctors[f->first_ctor + j], env, res));
    if (cores[j] == NULL)
      return NULL;
  }
  cores[f->ctor_count] = s;
  k->used = f->ctor_count + 1u;
  k->type = res;
  return core_op(c, OP_FOLD_FAMILY, st->inst, 0, cores, f->ctor_count + 1u);
}

/* M4 s3 (D6): the end E of binder J of the type of the path case of CI,
   as pcon_end builds the end of `ci fields`, with the fields as the
   variables FVALS (levels L .. L+n-1) and the boundary body folded. A body
   that is a recursive field variable folds to that variable (its binder
   has the result type). Another body folds by the evaluator with the
   cases checked so far; a case not yet checked (a constructor declared
   later) is a variable above the context, so a body that needs it is
   refused. Quoted at L + n + k - 1 under the binders after J. */
static const Core *hit_case_end(Checker *c, const CtorInfo *ci, const Value *const *pvals, const Value *res, const Value *const *cases, const Value *const *fvals, uint32_t L, uint32_t j, int e) {
  Machine *m = c->m;
  const FamilyInfo *f = &m->families[ci->family];
  uint32_t n = ci->field_count;
  uint32_t kb = ci->binder_count;
  const Env *env = env_of(c, pvals, f->param_count);
  const Value *bv;
  const Value *fv;
  const Core *core;
  uint32_t bound = 0;
  uint32_t i;
  for (i = 0; i < n; i++)
    env = env_push(m, env, fvals[i]);
  for (i = 0; i < kb; i++)
    env = env_push(m, env, i == j ? val_ivl_end(m, e) : val_ivl_var(m, L + n + (i < j ? i : i - 1u)));
  bv = env == NULL ? NULL : pcon_boundary(m, ci, env);
  if (bv == NULL)
    return m->diag->set ? NULL : nul(FAIL(c, "INTERNAL", "the boundary of %s has no face i1 at the end %d of %s", ci->name, e, ci->binders[j]));
  fv = eval_fold_hit_boundary(m, ci, res, cases, f->ctor_count, fvals, bv);
  if (fv == NULL)
    return NULL;
  if (!value_bound(m, fv, &bound) || bound > L + n + kb - 1u)
    return nul(FAIL(c, "INTERNAL", "the boundary of %s at the end %d of %s folds a case of a constructor declared after it; not supported", ci->name, e, ci->binders[j]));
  core = quote_value(m, L + n + kb - 1u, fv);
  for (i = kb; i > j + 1u; i--)
    core = core_bind(c, CORE_PLAM, ci->binders[i - 1u], NULL, core);
  return core;
}

/* M4 s3 (D6): the type of the path case of CI at the result type RES.
   The fields are Pi binders (a recursive field has the type RES); the
   codomain is the path type over the k binders whose ends are the folded
   boundary bodies (hit_case_end). CVALS: the values of the DONE cases
   checked so far, in declaration order. */
static const Value *hit_case_type(Checker *c, const CtorInfo *ci, const Value *const *pvals, const Value *res, const Value *const *cvals, uint32_t done) {
  Machine *m = c->m;
  const FamilyInfo *f = &m->families[ci->family];
  uint32_t n = ci->field_count;
  uint32_t kb = ci->binder_count;
  uint32_t L = c->level;
  const Env *penv = env_of(c, pvals, f->param_count);
  const Value **fvals = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *fvals);
  const Value **ftypes = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *ftypes);
  const Value **cases = arena_alloc(m->arena, ((size_t)f->ctor_count + 1u) * sizeof *cases);
  const Core *type;
  uint32_t i;
  if (fvals == NULL || ftypes == NULL || cases == NULL) {
    FAIL(c, "OOM", "out of memory");
    return NULL;
  }
  for (i = 0; i < f->ctor_count; i++)
    cases[i] = i < done ? cvals[i] : val_var(m, L + n + kb + i);
  for (i = 0; i < n; i++) {
    fvals[i] = val_var(m, L + i);
    ftypes[i] = ci->fields[i].recursive ? res : eval_core(m, penv, ci->fields[i].type);
    if (fvals[i] == NULL || ftypes[i] == NULL)
      return NULL;
  }
  type = quote_value(m, L + n + kb, res);
  for (i = kb; i > 0; i--) {
    const Core *e0 = hit_case_end(c, ci, pvals, res, cases, fvals, L, i - 1u, 0);
    const Core *e1 = hit_case_end(c, ci, pvals, res, cases, fvals, L, i - 1u, 1);
    type = op3(c, OP_PATHP, 0, 3, core_bind(c, CORE_PLAM, ci->binders[i - 1u], NULL, type), e0, e1);
  }
  for (i = n; i > 0; i--)
    type = core_bind(c, CORE_PI, ci->fields[i - 1u].name, quote_value(m, L + i - 1u, ftypes[i - 1u]), type);
  return here(c, type);
}

/* M4 s3 (D-s3-3): the failed check of the case of CI. An inner type,
   boundary or refl mismatch becomes HIT_CASES (a point case) or HIT_CASE
   (a path case) with the constructor and the expected type; another inner
   code stays. */
static const Core *recode_case(Checker *c, const CtorInfo *ci, const Value *want) {
  Diag *d = c->m->diag;
  char inner[sizeof d->msg];
  int mismatch = d->set && d->code != NULL
    && (strcmp(d->code, "TYPE_MISMATCH") == 0 || strcmp(d->code, "PATH_BOUNDARY") == 0 || strcmp(d->code, "TYPE_REFL") == 0);
  if (!mismatch || want == NULL)
    return NULL;
  memcpy(inner, d->msg, sizeof inner);
  d->set = 0;
  if (ci->binder_count == 0u)
    return nul(FAIL(c, "HIT_CASES", "the case for %s must have the type %s: %s", ci->name, show(c, want), inner));
  return nul(FAIL(c, "HIT_CASE", "the case for %s must match its boundary and have the type %s: %s", ci->name, show(c, want), inner));
}

/* M4 s3 (D6): fold B case... x at a HIT. B is explicit; one case for each
   constructor in declaration order; a point case has the family case type,
   a path case the type hit_case_type; the scrutinee is last. */
static const Core *fold_hit_rule(Call *k, const Core *s, const Value *st) {
  Checker *c = k->c;
  Machine *m = c->m;
  const FamilyInfo *f = &m->families[st->inst];
  const Env *env = env_of(c, st->args, st->argc);
  uint64_t level = 0;
  const Core **cores = arena_alloc(m->arena, ((size_t)f->ctor_count + 3u) * sizeof *cores);
  const Value **cvals = arena_alloc(m->arena, ((size_t)f->ctor_count + 1u) * sizeof *cvals);
  const Value *res;
  uint32_t j;
  if (cores == NULL || cvals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (k->argc != f->ctor_count + 2u)
    return nul(FAIL(c, "HIT_CASES", "fold at the higher inductive family %s needs the result type, then %u cases, then the scrutinee; found %u arguments", f->name, f->ctor_count, k->argc));
  cores[0] = check_type(c, k->args[0], &level);
  res = cores[0] == NULL ? NULL : here(c, cores[0]);
  if (res == NULL)
    return NULL;
  for (j = 0; j < f->ctor_count; j++) {
    const CtorInfo *ci = &m->ctors[f->first_ctor + j];
    const Value *want = ci->binder_count == 0u ? case_type(c, ci, env, res) : hit_case_type(c, ci, st->args, res, cvals, j);
    cores[1u + j] = want == NULL ? NULL : check(c, k->args[1u + j], want);
    if (cores[1u + j] == NULL)
      return recode_case(c, ci, want);
    if (ci->binder_count > 0u)
      cores[1u + j] = hit_case_eta(c, ci, cores[1u + j], want, 0u, 0u, c->level);
    cvals[j] = eval_core(m, c->env, cores[1u + j]);
    if (cvals[j] == NULL)
      return NULL;
  }
  cores[f->ctor_count + 1u] = s;
  k->used = f->ctor_count + 2u;
  k->type = res;
  return core_op(c, OP_FOLD_HIT, st->inst, 0, cores, f->ctor_count + 2u);
}

static int simple_scrutinee(const Value *t) {
  return val_is(t, OP_NAT) || val_is(t, OP_LIST);
}

/* fold takes its scrutinee last, so the scrutinee type sets the arity. */
static const Core *rule_fold(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = try_infer(c, k->args[k->argc - 1u], &st);
  uint32_t fam = val_is(st, OP_FAMILY) ? st->inst : NO_FAMILY;
  if (s != NULL && fam != NO_FAMILY && c->m->families[fam].path_ctors > 0u)
    return fold_hit_rule(k, s, st);
  if (s != NULL && fam != NO_FAMILY && c->m->families[fam].ctor_count + 1u == k->argc) {
    k->hint = k->outer;
    return fold_family(k, s, st);
  }
  if (s != NULL && k->argc == 3u && simple_scrutinee(st)) {
    k->hint = k->outer;
    return fold_simple(k, s, st);
  }
  s = k->argc > 3u ? try_infer(c, k->args[2], &st) : NULL;
  if (s != NULL && simple_scrutinee(st)) {
    k->hint = NULL;
    return fold_simple(k, s, st);
  }
  s = infer(c, k->args[k->argc - 1u], &st);
  return s == NULL ? NULL : nul(FAIL(c, "TYPE_MISMATCH", "fold needs a last argument of type Nat, List A or a domain family, and one case for each constructor; found %u arguments and a last argument of type %s", k->argc, show(c, st)));
}

/* unfold g limit seed, with g : S -> Option (Prod A S). */
static const Core *rule_unfold(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Value *a;
  const Core *seed;
  const Core *limit;
  const Core *g;
  if (!val_is(k->hint, OP_LIST))
    return need_type(k, "List A");
  a = k->hint->args[0];
  seed = infer(c, k->args[2], &st);
  limit = seed == NULL ? NULL : check(c, k->args[1], nat_type(c));
  g = limit == NULL ? NULL : check(c, k->args[0], val_arrow(c->m, st, tyop(c, OP_OPTION, 1, tyop(c, OP_PROD, 2, a, st, NULL), NULL, NULL)));
  k->type = k->hint;
  return op3(c, OP_UNFOLD, 0, 3, g, limit, seed);
}

/* cong f p */
static const Core *rule_cong(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[1], &pt);
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PATHP))
    return nul(FAIL(c, "PATH_EXPECTED", "cong needs a path of type Path A a b, found a term of type %s", show(c, pt)));
  return cong_path(k, p, pt);
}

static const Core *rule_nat2(Call *k) {
  Checker *c = k->c;
  const Core *x = check(c, k->args[0], nat_type(c));
  const Core *y = x == NULL ? NULL : check(c, k->args[1], nat_type(c));
  k->type = tyop(c, k->b->result, 0, NULL, NULL, NULL);
  return op3(c, k->b->op, 0, 2, x, y, NULL);
}

/* flagIf b yes no */
static const Core *rule_flag_if(Call *k) {
  Checker *c = k->c;
  const Value *res = k->hint;
  const Core *b = check(c, k->args[0], flag_type(c));
  const Core *yes = b == NULL ? NULL : (res != NULL ? check(c, k->args[1], res) : infer(c, k->args[1], &res));
  const Core *no = yes == NULL ? NULL : check(c, k->args[2], res);
  k->type = res;
  return op3(c, OP_FLAG_IF, 0, 3, b, yes, no);
}

/* I, i0 and i1 where a term of a type is necessary. */
static const Core *rule_interval(Call *k) {
  return nul(FAIL(k->c, "INTERVAL_TERM", "%s is %s, not a term of a type", k->b->name, k->b->name[1] == '\0' ? "the interval" : "an interval end point"));
}

/* 1 when T is the name I, and no local binder takes that name. */
static int is_interval_type(const Checker *c, const Term *t) {
  return t->kind == TERM_VAR && strcmp(t->name, "I") == 0 && find_local(c, "I") == NULL;
}

/* The line <NAME> A of Path and PathP. A must be in Type n under the fresh
   interval variable NAME. *LEVEL is n. */
static const Core *path_line(Checker *c, const char *name, const Term *a, uint64_t *level) {
  const Value *ty = NULL;
  const Core *body;
  if (!push_ivl(c, name))
    return NULL;
  body = is_interval_type(c, a) ? nul(FAIL(c, "PATH_TYPE", "the interval I is not a type in Type n, so it cannot be the type of a path")) : infer(c, a, &ty);
  if (body != NULL && val_is(ty, OP_SSET))
    body = nul(FAIL(c, "SORT_FIBRANT", "the type of a path must be in Type n, found %s in SSet", show(c, here(c, body))));
  if (body != NULL && (ty == NULL || ty->kind != VAL_UNIV))
    body = nul(FAIL(c, "PATH_TYPE", "the type of a path must be in Type n, found a term of type %s", show(c, ty)));
  pop(c);
  *level = body == NULL ? 0u : ty->nat;
  return core_bind(c, CORE_PLAM, name, NULL, body);
}

/* PathP (<NAME> A) a b, and Path A a b with a line that the lexer cannot
   name. a : A[i := i0], b : A[i := i1]. */
static const Core *path_former(Call *k, const char *name, const Term *a) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *line = path_line(c, name, a, &level);
  const Value *lv = here(c, line);
  const Core *x = lv == NULL ? NULL : check(c, k->args[1], closure_apply(c->m, lv, val_ivl_end(c->m, 0)));
  const Core *y = x == NULL ? NULL : check(c, k->args[2], closure_apply(c->m, lv, val_ivl_end(c->m, 1)));
  k->type = val_univ(c->m, level);
  return op3(c, OP_PATHP, 0, 3, line, x, y);
}

/* M2 s4: Partial phi A, a type in SSet. phi is a face, A is in Type n. */
static const Core *rule_partial(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *face = elab_ivl(c, k->args[0]);
  const Core *a = face == NULL ? NULL : check_type(c, k->args[1], &level);
  k->type = tyop(c, OP_SSET, 0, NULL, NULL, NULL);
  return a == NULL ? NULL : op3(c, OP_PARTIAL, 0, 2, face, a, NULL);
}

static const Core *rule_pathp(Call *k) {
  const Term *line = k->args[0];
  if (line->kind != TERM_PLAM)
    return nul(FAIL(k->c, "PATH_TYPE", "PathP needs a line <i> A as its first argument"));
  return path_former(k, line->name, line->right);
}

/* M2 s2: refl, sym, cong, funExt and happly write ordinary core path terms
   (M2-PLAN D4 and section 4). The evaluator has no rule for them. */

/* K with each free variable (index >= CUT) moved BY binders out. */
static const Core *core_shift(Checker *c, const Core *k, uint32_t by, uint32_t cut) {
  Core *n;
  const Core **args;
  uint32_t i;
  int binds;
  if (k == NULL)
    return NULL;
  switch (k->kind) {
  case CORE_VAR:
    if (k->index < cut)
      return k;
    n = new_core(c, CORE_VAR);
    if (n != NULL) {
      *n = *k;
      n->index += by;
    }
    return n;
  case CORE_GLOBAL:
  case CORE_NAT:
  case CORE_TRAP:
  case CORE_UNIV:
    return k;
  case CORE_LAM:
  case CORE_PI:
  case CORE_SIGMA:
  case CORE_APP:
  case CORE_OP:
  case CORE_IVL:
  case CORE_PLAM:
  case CORE_PAPP:
    break;
  }
  n = new_core(c, k->kind);
  if (n == NULL)
    return NULL;
  *n = *k;
  binds = k->kind == CORE_LAM || k->kind == CORE_PI || k->kind == CORE_SIGMA || k->kind == CORE_PLAM;
  if (k->kind != CORE_OP) {
    n->a = core_shift(c, k->a, by, cut);
    n->b = core_shift(c, k->b, by, binds ? cut + 1u : cut);
  }
  if (k->kind == CORE_OP || k->kind == CORE_PAPP) {
    args = arena_alloc(c->m->arena, ((size_t)k->argc + 1u) * sizeof *args);
    if (args == NULL)
      return nul(FAIL(c, "OOM", "out of memory"));
    for (i = 0; i < k->argc; i++)
      args[i] = core_shift(c, k->args[i], by, cut);
    n->args = args;
  }
  return c->m->diag->set ? NULL : n;
}

/* The core term P @ R with the ends X and Y of the type of P (D5). */
static const Core *papp_ends(Checker *c, const Core *p, const Core *r, const Core *x, const Core *y) {
  const Core **ends;
  Core *k;
  if (p == NULL || r == NULL || x == NULL || y == NULL)
    return NULL;
  ends = arena_alloc(c->m->arena, 2u * sizeof *ends);
  k = ends == NULL ? NULL : new_core(c, CORE_PAPP);
  if (k == NULL)
    return c->m->diag->set ? NULL : nul(FAIL(c, "OOM", "out of memory"));
  ends[0] = x;
  ends[1] = y;
  k->a = p;
  k->b = r;
  k->args = ends;
  k->argc = 2u;
  return k;
}

/* The core term P @ R, P : PT = PathP L a b, in a context of LEVEL binders.
   The core keeps a and b, so the evaluator can reduce a neutral P at i0 and
   i1 (D5). */
static const Core *core_papp(Checker *c, uint32_t level, const Core *p, const Value *pt, const Core *r) {
  if (p == NULL || r == NULL)
    return NULL;
  return papp_ends(c, p, r, quote_value(c->m, level, pt->args[1]), quote_value(c->m, level, pt->args[2]));
}

/* Keep the checked ends on every path application performed by fold_hit.
   A neutral case is expanded through its fields and interval binders. */
static const Core *hit_case_eta(Checker *c, const CtorInfo *ci, const Core *term, const Value *type, uint32_t field, uint32_t ivl, uint32_t level) {
  const Core *body;
  const Core *arg;
  const Value *next;
  if (term == NULL || type == NULL)
    return NULL;
  if (field == ci->field_count && ivl == ci->binder_count)
    return term;
  arg = core_index(c, CORE_VAR, 0u);
  if (field < ci->field_count) {
    body = core_app(c, core_shift(c, term, 1u, 0u), arg);
    next = closure_apply(c->m, type, val_var(c->m, level));
    body = hit_case_eta(c, ci, body, next, field + 1u, ivl, level + 1u);
    return core_bind(c, CORE_LAM, ci->fields[field].name, quote_value(c->m, level, type->dom), body);
  }
  body = core_papp(c, level + 1u, core_shift(c, term, 1u, 0u), type, arg);
  next = closure_apply(c->m, type->args[0], val_ivl_var(c->m, level));
  body = hit_case_eta(c, ci, body, next, field, ivl + 1u, level + 1u);
  return core_bind(c, CORE_PLAM, ci->binders[ivl], NULL, body);
}

/* The closure of KIND (VAL_LAM, VAL_PI or VAL_PLAM) with the body BODY, a
   value in which the bound variable is the level c->level. */
static const Value *close_over(Checker *c, ValKind kind, const char *name, const Value *dom, const Value *body) {
  const Core *k = body == NULL ? NULL : quote_value(c->m, c->level + 1u, body);
  return k == NULL ? NULL : val_closure(c->m, kind, name, dom, c->env, k);
}

/* The value of the interval core K under one more binder: the interval
   variable at the level c->level. */
static const Value *under_ivl(Checker *c, const Core *k) {
  return k == NULL ? NULL : eval_core(c->m, env_push(c->m, c->env, val_ivl_var(c->m, c->level)), k);
}

/* A when the line L of a path type is constant (L @ i = A for each i), else
   NULL. */
static const Value *line_type(Checker *c, const Value *line) {
  const Value *a = closure_apply(c->m, line, val_ivl_end(c->m, 0));
  const Value *ai = closure_apply(c->m, line, val_ivl_var(c->m, c->level));
  return a != NULL && ai != NULL && conv_values(c->m, c->level + 1u, a, ai) ? a : NULL;
}

/* refl against PathP L a b: L is constant and a = b. The core is <i> a. */
static const Core *refl_path(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  if (line_type(c, e->args[0]) == NULL)
    return nul(FAIL(c, "TYPE_REFL", "refl needs a path type with a constant line, found %s", show(c, e)));
  if (!conv_values(c->m, c->level, e->args[1], e->args[2]))
    return nul(FAIL(c, "TYPE_REFL", "refl cannot prove the path: the start normalizes to %s, the end normalizes to %s", show(c, e->args[1]), show(c, e->args[2])));
  k->type = e;
  return core_bind(c, CORE_PLAM, "i", NULL, quote_value(c->m, c->level + 1u, e->args[1]));
}

/* sym p, p : PathP L a b, gives PathP (<i> L @ ~i) b a. The core is
   <i> p @ ~i. */
static const Core *rule_sym(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  const Core *rev;
  const Value *line;
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PATHP))
    return nul(FAIL(c, "PATH_EXPECTED", "sym needs a path of type PathP L a b, found a term of type %s", show(c, pt)));
  rev = core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 0u), NULL);
  line = close_over(c, VAL_PLAM, "i", NULL, closure_apply(c->m, pt->args[0], under_ivl(c, rev)));
  if (line == NULL)
    return NULL;
  k->type = tyop(c, OP_PATHP, 3, line, pt->args[2], pt->args[1]);
  return core_bind(c, CORE_PLAM, "i", NULL, core_papp(c, c->level + 1u, core_shift(c, p, 1u, 0u), pt, rev));
}

/* cong f p, p : Path A a b, f : (x : A) -> B x, gives
   PathP (<i> B (p @ i)) (f a) (f b). The core is <i> f (p @ i). */
static const Core *cong_path(Call *k, const Core *p, const Value *pt) {
  Checker *c = k->c;
  const Value *a = line_type(c, pt->args[0]);
  const Value *b = val_is(k->hint, OP_PATHP) ? line_type(c, k->hint->args[0]) : NULL;
  const Value *ft = NULL;
  const Value *fv;
  const Value *line;
  const Core *f;
  const Core *at;
  if (a == NULL)
    return nul(FAIL(c, "TYPE_MISMATCH", "cong needs a path of type Path A a b with a constant line, found a term of type %s", show(c, pt)));
  /* A constant result path can come from a dependent function at refl.
     Preserve its inferred type before using the hint to check a lambda. */
  f = b == NULL ? infer(c, k->args[0], &ft) : try_infer(c, k->args[0], &ft);
  if (f == NULL && b != NULL) {
    ft = val_arrow(c->m, a, b);
    f = check(c, k->args[0], ft);
  }
  if (f == NULL)
    return NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "cong needs a function, found a term of type %s", show(c, ft)));
  if (!expect(c, a, ft->dom))
    return NULL;
  at = core_papp(c, c->level + 1u, core_shift(c, p, 1u, 0u), pt, core_index(c, CORE_VAR, 0u));
  line = close_over(c, VAL_PLAM, "i", NULL, closure_apply(c->m, ft, under_ivl(c, at)));
  fv = here(c, f);
  if (line == NULL || fv == NULL)
    return NULL;
  k->type = tyop(c, OP_PATHP, 3, line, apply_value(c->m, fv, pt->args[1]), apply_value(c->m, fv, pt->args[2]));
  return core_bind(c, CORE_PLAM, "i", NULL, core_app(c, core_shift(c, f, 1u, 0u), at));
}

/* The type (x : A) -> Path (B x) (f x) (g x) that funExt h needs of h
   against Path FN F G, FN = (x : A) -> B x. */
static const Value *funext_arg(Checker *c, const Value *fn, const Value *f, const Value *g) {
  const Value *x;
  const Value *line;
  const Value *path = NULL;
  if (!push(c, NULL, fn->dom))
    return NULL;
  x = c->env->value;
  line = close_over(c, VAL_PLAM, "i", NULL, closure_apply(c->m, fn, x));
  if (line != NULL)
    path = tyop(c, OP_PATHP, 3, line, apply_value(c->m, f, x), apply_value(c->m, g, x));
  pop(c);
  return close_over(c, VAL_PI, "x", fn->dom, path);
}

/* The type Path ((x : A) -> B x) f g of funExt h, from the type HT of h,
   (x : A) -> Path (B x) (f x) (g x). NULL when HT has another form. */
static const Value *funext_type(Checker *c, const Value *ht) {
  const Value *cod;
  const Value *bx;
  const Value *fn;
  const Value *f;
  const Value *g;
  const Value *line;
  if (ht == NULL || ht->kind != VAL_PI || !push(c, NULL, ht->dom))
    return NULL;
  cod = closure_apply(c->m, ht, c->env->value);
  bx = val_is(cod, OP_PATHP) ? line_type(c, cod->args[0]) : NULL;
  pop(c);
  if (bx == NULL)
    return NULL;
  fn = close_over(c, VAL_PI, "x", ht->dom, bx);
  f = close_over(c, VAL_LAM, "x", NULL, cod->args[1]);
  g = close_over(c, VAL_LAM, "x", NULL, cod->args[2]);
  line = fn == NULL ? NULL : close_over(c, VAL_PLAM, "i", NULL, fn);
  return line == NULL || f == NULL || g == NULL ? NULL : tyop(c, OP_PATHP, 3, line, f, g);
}

/* <i> fun x => h x @ i, h : HT. Under <i> and fun x, h is two binders out
   and x is the level c->level + 1. */
static const Core *funext_core(Checker *c, const Core *h, const Value *ht) {
  const Value *cod = closure_apply(c->m, ht, val_var(c->m, c->level + 1u));
  const Core *hx = core_app(c, core_shift(c, h, 2u, 0u), core_index(c, CORE_VAR, 0u));
  if (!val_is(cod, OP_PATHP))
    return nul(FAIL(c, "TYPE_MISMATCH", "funExt needs a function of type (x : A) -> Path (B x) (f x) (g x)"));
  return core_bind(c, CORE_PLAM, "i", NULL, core_bind(c, CORE_LAM, "x", NULL, core_papp(c, c->level + 2u, hx, cod, core_index(c, CORE_VAR, 1u))));
}

/* funExt h, h : (x : A) -> Path (B x) (f x) (g x), gives
   Path ((x : A) -> B x) f g. With no expected type, f and g are the ends of
   the codomain of h. funExt is a theorem with a body, not an axiom. */
static const Core *rule_funext(Call *k) {
  Checker *c = k->c;
  const Value *want = k->hint;
  const Value *fn = val_is(want, OP_PATHP) ? line_type(c, want->args[0]) : NULL;
  const Value *ht = NULL;
  const Core *h;
  if (want != NULL && (fn == NULL || fn->kind != VAL_PI))
    return nul(FAIL(c, "TYPE_MISMATCH", "funExt needs a type Path ((x : A) -> B x) f g, found %s", show(c, want)));
  if (want != NULL) {
    ht = funext_arg(c, fn, want->args[1], want->args[2]);
    h = ht == NULL ? NULL : check(c, k->args[0], ht);
    k->type = want;
  } else {
    h = infer(c, k->args[0], &ht);
    k->type = h == NULL ? NULL : funext_type(c, ht);
    if (h != NULL && k->type == NULL)
      return nul(FAIL(c, "TYPE_MISMATCH", "funExt needs a function of type (x : A) -> Path (B x) (f x) (g x), found a term of type %s", show(c, ht)));
  }
  return h == NULL ? NULL : funext_core(c, h, ht);
}

/* happly p x, p : Path ((x : A) -> B x) f g, gives Path (B x) (f x) (g x).
   The core is <i> (p @ i) x. */
static const Core *rule_happly(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  const Value *fn;
  const Value *xv;
  const Value *line;
  const Core *x;
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PATHP))
    return nul(FAIL(c, "PATH_EXPECTED", "happly needs a path of type Path ((x : A) -> B x) f g, found a term of type %s", show(c, pt)));
  fn = line_type(c, pt->args[0]);
  if (fn == NULL || fn->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_MISMATCH", "happly needs a path of type Path ((x : A) -> B x) f g, found a term of type %s", show(c, pt)));
  x = check(c, k->args[1], fn->dom);
  xv = here(c, x);
  line = xv == NULL ? NULL : close_over(c, VAL_PLAM, "i", NULL, closure_apply(c->m, fn, xv));
  if (line == NULL)
    return NULL;
  k->type = tyop(c, OP_PATHP, 3, line, apply_value(c->m, pt->args[1], xv), apply_value(c->m, pt->args[2], xv));
  return core_bind(c, CORE_PLAM, "i", NULL, core_app(c, core_papp(c, c->level + 1u, core_shift(c, p, 1u, 0u), pt, core_index(c, CORE_VAR, 0u)), core_shift(c, x, 1u, 0u)));
}

static const Core *rule(Call *k) {
  switch (k->b->rule) {
  case RULE_TYPE0:
    k->type = val_univ(k->c->m, 0);
    return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
  case RULE_VALUE0:
    k->type = tyop(k->c, k->b->result, 0, NULL, NULL, NULL);
    return op3(k->c, k->b->op, 0, 0, NULL, NULL, NULL);
  case RULE_TYPE1:
    return rule_type1(k);
  case RULE_TYPE2:
    return rule_type2(k);
  case RULE_PAIR:
    return rule_pair(k);
  case RULE_INJ:
    return rule_inj(k);
  case RULE_EMPTY:
    return rule_empty(k);
  case RULE_SOME:
    return rule_some(k);
  case RULE_CONS:
    return rule_cons(k);
  case RULE_PACK:
    return rule_pack(k);
  case RULE_REFL:
    return rule_refl(k);
  case RULE_PART:
    return rule_part(k);
  case RULE_SIGMA_PART:
    return rule_sigma_part(k);
  case RULE_EITHER:
    return rule_either(k);
  case RULE_OPTION:
    return rule_option(k);
  case RULE_PURE:
    return rule_pure(k);
  case RULE_MAP:
    return rule_map(k);
  case RULE_BIND:
    return rule_bind(k);
  case RULE_FILTER:
    return rule_filter(k);
  case RULE_FOLD:
    return rule_fold(k);
  case RULE_UNFOLD:
    return rule_unfold(k);
  case RULE_CONG:
    return rule_cong(k);
  case RULE_NAT2:
    return rule_nat2(k);
  case RULE_FLAG_IF:
    return rule_flag_if(k);
  case RULE_INTERVAL:
    return rule_interval(k);
  case RULE_PATH:
    return path_former(k, "%i", k->args[0]);
  case RULE_PATHP:
    return rule_pathp(k);
  case RULE_SYM:
    return rule_sym(k);
  case RULE_FUNEXT:
    return rule_funext(k);
  case RULE_HAPPLY:
    return rule_happly(k);
  case RULE_PARTIAL:
    return rule_partial(k);
  case RULE_SSET:
    return nul(FAIL(k->c, "SORT_FIBRANT", "SSet is a sort, not a type in Type n: it cannot be a binder type"));
  case RULE_TRANSP:
    return rule_transp(k);
  case RULE_HCOMP:
    return rule_hcomp(k);
  case RULE_COMP:
    return rule_comp(k);
  case RULE_HFILL:
    return rule_hfill(k);
  case RULE_TRANS:
    return rule_trans(k);
  case RULE_TRANSPORT:
    return rule_transport(k);
  case RULE_SUBST:
    return rule_subst(k);
  case RULE_TRANSPORT_REFL:
    return rule_transport_refl(k);
  case RULE_J:
    return rule_j(k);
  case RULE_J_REFL:
    return rule_j_refl(k);
  case RULE_IS_CONTR:
    return rule_is_contr(k);
  case RULE_IS_PROP:
    return rule_is_prop(k);
  case RULE_FIBER:
    return rule_fiber(k);
  case RULE_IS_EQUIV:
    return rule_is_equiv(k);
  case RULE_EQUIV:
    return rule_equiv(k);
  case RULE_ID_EQUIV:
    return rule_id_equiv(k);
  case RULE_GLUE:
    return rule_glue(k);
  case RULE_GLUE_ELEM:
    return rule_glue_elem(k);
  case RULE_UNGLUE:
    return rule_unglue(k);
  case RULE_UA:
    return rule_ua(k);
  case RULE_UA_BETA:
    return rule_ua_beta(k);
  }
  return NULL;
}

/* Checks the family parameters ARGS. VALS gets their values. */
static int check_params(Checker *c, const FamilyInfo *f, const Term *const *args, const Core **cores, const Value **vals) {
  uint32_t i;
  for (i = 0; i < f->param_count; i++) {
    cores[i] = check(c, args[i], eval_core(c->m, env_of(c, vals, i), f->param_types[i]));
    vals[i] = here(c, cores[i]);
    if (vals[i] == NULL)
      return 0;
  }
  return 1;
}

static const Core *family_app(Call *k, uint32_t fam) {
  Checker *c = k->c;
  const FamilyInfo *f = &c->m->families[fam];
  const Core **cores = arena_alloc(c->m->arena, ((size_t)f->param_count + 1u) * sizeof *cores);
  const Value **vals = arena_alloc(c->m->arena, ((size_t)f->param_count + 1u) * sizeof *vals);
  if (cores == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (!check_params(c, f, k->args, cores, vals))
    return NULL;
  k->type = val_univ(c->m, f->universe);
  return core_op(c, OP_FAMILY, fam, 0, cores, f->param_count);
}

/* A constructor takes the family parameters (the indices), then its fields. */
/* M4 s1 (D5): the end E of binder J of the path constructor CI, as a core
   in the context L + J (the binders before J are at the levels L .. L+J-1).
   The boundary runs in the env params, fields, then the interval values:
   binder J is the end, the binders after J are the levels L+J .. L+k-2
   under the PLAMs that wrap the result. */
static const Core *pcon_end(Checker *c, const CtorInfo *ci, const Value *const *pvals, const Value *const *fvals, uint32_t L, uint32_t j, int e) {
  Machine *m = c->m;
  uint32_t kb = ci->binder_count;
  const Env *env = env_of(c, pvals, m->families[ci->family].param_count);
  const Value *bv;
  const Core *core;
  uint32_t i;
  for (i = 0; i < ci->field_count; i++)
    env = env_push(m, env, fvals[i]);
  for (i = 0; i < kb; i++)
    env = env_push(m, env, i == j ? val_ivl_end(m, e) : val_ivl_var(m, L + (i < j ? i : i - 1u)));
  bv = env == NULL ? NULL : pcon_boundary(m, ci, env);
  if (bv == NULL)
    return m->diag->set ? NULL : nul(FAIL(c, "INTERNAL", "the boundary of %s has no face i1 at the end %d of %s", ci->name, e, ci->binders[j]));
  core = quote_value(m, L + kb - 1u, bv);
  for (i = kb; i > j + 1u; i--)
    core = core_bind(c, CORE_PLAM, ci->binders[i - 1u], NULL, core);
  return core;
}

/* M4 s1: a path constructor use is <i ...> pcon params fields i ... with
   the type Path ... Self (E^0) (E^1) at its boundary ends (D5). The checked
   argument cores are quoted at L + k: that is the de Bruijn shift under the
   k binders (the check_sys face trick). */
static const Core *pcon_app(Call *k, uint32_t ctor, const Core *const *cores, const Value *const *vals) {
  Checker *c = k->c;
  Machine *m = c->m;
  const CtorInfo *ci = &m->ctors[ctor];
  uint32_t p = m->families[ci->family].param_count;
  uint32_t n = p + ci->field_count;
  uint32_t kb = ci->binder_count;
  uint32_t L = c->level;
  const Core **args = arena_alloc(m->arena, ((size_t)n + kb + 1u) * sizeof *args);
  const Value **fvals = arena_alloc(m->arena, ((size_t)ci->field_count + 1u) * sizeof *fvals);
  const Value *selfv = val_op(m, VAL_OP, OP_FAMILY, ci->family, 0, vals, p);
  const Core *body;
  const Core *type;
  uint32_t i;
  if (args == NULL || fvals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (i = 0; i < n; i++) {
    args[i] = quote_value(m, L + kb, eval_core(m, c->env, cores[i]));
    if (args[i] == NULL)
      return NULL;
  }
  for (i = 0; i < ci->field_count; i++)
    fvals[i] = eval_core(m, c->env, cores[p + i]);
  for (i = 0; i < kb; i++)
    args[n + i] = core_index(c, CORE_VAR, kb - 1u - i);
  body = core_op(c, OP_PCON, ctor, 0, args, n + kb);
  type = quote_value(m, L + kb, selfv);
  for (i = kb; i > 0; i--) {
    const Core *e0 = pcon_end(c, ci, vals, fvals, L, i - 1u, 0);
    const Core *e1 = pcon_end(c, ci, vals, fvals, L, i - 1u, 1);
    body = core_bind(c, CORE_PLAM, ci->binders[i - 1u], NULL, body);
    type = op3(c, OP_PATHP, 0, 3, core_bind(c, CORE_PLAM, ci->binders[i - 1u], NULL, type), e0, e1);
  }
  k->type = here(c, type);
  return k->type == NULL ? NULL : body;
}

static const Core *ctor_app(Call *k, uint32_t ctor) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  const FamilyInfo *f = &c->m->families[ci->family];
  uint32_t p = f->param_count;
  uint32_t n = p + ci->field_count;
  const Core **cores = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *cores);
  const Value **vals = arena_alloc(c->m->arena, ((size_t)p + 1u) * sizeof *vals);
  const Env *env;
  uint32_t j;
  if (cores == NULL || vals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  if (!check_params(c, f, k->args, cores, vals))
    return NULL;
  env = env_of(c, vals, p);
  for (j = 0; j < ci->field_count; j++) {
    cores[p + j] = check(c, k->args[p + j], eval_core(c->m, env, ci->fields[j].type));
    if (cores[p + j] == NULL)
      return NULL;
  }
  if (ci->binder_count > 0u)
    return pcon_app(k, ctor, cores, vals);
  k->type = val_op(c->m, VAL_OP, OP_FAMILY, ci->family, 0, vals, p);
  return core_op(c, OP_CTOR, ctor, 0, cores, n);
}

/* A field of a one-constructor family is a projection. */
static const Core *proj_app(Call *k, uint32_t ctor, uint32_t field) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  const Value *xt = NULL;
  if (ci->binder_count > 0u)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s is a field of the path constructor %s; it has no record projection", ci->fields[field].name, ci->name));
  const Core *x = infer(c, k->args[0], &xt);
  if (x == NULL)
    return NULL;
  if (!val_is(xt, OP_FAMILY) || xt->inst != ci->family)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a value of the family %s, found a term of type %s", ci->fields[field].name, c->m->families[ci->family].name, show(c, xt)));
  k->type = eval_core(c->m, env_of(c, xt->args, xt->argc), ci->fields[field].type);
  return core_op(c, OP_PROJ, ctor, field, &x, 1);
}

static int find_known(Checker *c, const char *name, Head *h) {
  const Machine *m = c->m;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < m->family_count; i++) {
    if (strcmp(m->families[i].name, name) == 0) {
      h->kind = HEAD_FAMILY;
      h->index = i;
      c->self_uses += i == c->self;
      return 1;
    }
  }
  for (i = 0; i < m->ctor_count; i++) {
    if (strcmp(m->ctors[i].name, name) == 0) {
      h->kind = HEAD_CTOR;
      h->index = i;
      return 1;
    }
  }
  for (i = 0; i < m->ctor_count; i++) {
    for (j = 0; m->families[m->ctors[i].family].ctor_count == 1u && j < m->ctors[i].field_count; j++) {
      if (strcmp(m->ctors[i].fields[j].name, name) == 0) {
        h->kind = HEAD_PROJ;
        h->index = i;
        h->field = j;
        return 1;
      }
    }
  }
  for (i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(BUILTINS[i].name, name) == 0) {
      h->kind = HEAD_BUILTIN;
      h->builtin = &BUILTINS[i];
      return 1;
    }
  }
  return 0;
}

/* An unknown name. When it names this declaration or a later one, the use
   is recursion (rule R3). */
static int unknown_name(Checker *c, const char *name) {
  size_t i;
  for (i = c->decl_index; i < c->decls->count; i++) {
    if (strcmp(c->decls->items[i].name, name) == 0)
      return FAIL(c, "REFUSE_REC", "%s is this definition or a later one; a definition can use only earlier ones (rule R3)", name);
  }
  return FAIL(c, "NAME_UNKNOWN", "unknown name %s", name);
}

/* Name resolution order: locals (the innermost first), definitions,
   families, constructors, projections, core names. */
static int resolve_head(Checker *c, const Term *head, Head *h) {
  memset(h, 0, sizeof *h);
  h->kind = HEAD_TERM;
  if (head->kind != TERM_VAR || find_local(c, head->name) != NULL || find_def(c->m, head->name) != NO_DEF)
    return 1;
  return find_known(c, head->name, h) ? 1 : unknown_name(c, head->name);
}

static uint32_t known_arity(const Checker *c, const Head *h) {
  const CtorInfo *ci = h->kind == HEAD_CTOR ? &c->m->ctors[h->index] : NULL;
  switch (h->kind) {
  case HEAD_TERM:
    return 0u;
  case HEAD_FAMILY:
    return c->m->families[h->index].param_count;
  case HEAD_CTOR:
    return ci == NULL ? 0u : c->m->families[ci->family].param_count + ci->field_count;
  case HEAD_PROJ:
    return 1u;
  case HEAD_BUILTIN:
    return h->builtin->arity;
  }
  return 0u;
}

static const char *known_name(const Checker *c, const Head *h) {
  switch (h->kind) {
  case HEAD_TERM:
    return "?";
  case HEAD_FAMILY:
    return c->m->families[h->index].name;
  case HEAD_CTOR:
    return c->m->ctors[h->index].name;
  case HEAD_PROJ:
    return c->m->ctors[h->index].fields[h->field].name;
  case HEAD_BUILTIN:
    return h->builtin->name;
  }
  return "?";
}

static const Core *known_core(Call *k, const Head *h) {
  switch (h->kind) {
  case HEAD_TERM:
    return nul(FAIL(k->c, "INTERNAL", "a plain term reached the core name rules"));
  case HEAD_FAMILY:
    return family_app(k, h->index);
  case HEAD_CTOR:
    return ctor_app(k, h->index);
  case HEAD_PROJ:
    return proj_app(k, h->index, h->field);
  case HEAD_BUILTIN:
    return rule(k);
  }
  return NULL;
}

/* An under-applied core name checked against a function type: WHOLE becomes
   `fun x => WHOLE x`, with a name that the lexer cannot make. */
static const Core *eta(Checker *c, const Term *whole, const Value *hint, const Value **type) {
  char *name = arena_alloc(c->m->arena, 24);
  Term *x = arena_alloc(c->m->arena, sizeof *x);
  Term *app = arena_alloc(c->m->arena, sizeof *app);
  Term *lam = arena_alloc(c->m->arena, sizeof *lam);
  if (name == NULL || x == NULL || app == NULL || lam == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  c->eta_count++;
  snprintf(name, 24, "%%eta%u", c->eta_count);
  x->kind = TERM_VAR;
  x->name = name;
  app->kind = TERM_APP;
  app->left = whole;
  app->right = x;
  lam->kind = TERM_LAM;
  lam->name = name;
  lam->right = app;
  return check_lam(c, lam, hint, type);
}

/* Applies K of type TY to ARGS, one by one. */
static const Core *apply_rest(Checker *c, const Core *k, const Value *ty, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  uint32_t i;
  for (i = 0; i < argc && k != NULL; i++) {
    const Core *a;
    if (ty == NULL || ty->kind != VAL_PI)
      return nul(FAIL(c, "TYPE_NOT_FUNCTION", "a term of type %s is applied to an argument", show(c, ty)));
    a = val_is(ty->dom, OP_INTERVAL) ? elab_ivl(c, args[i]) : check(c, args[i], ty->dom);
    ty = closure_apply(c->m, ty, here(c, a));
    k = core_app(c, k, a);
  }
  *type = ty;
  return finish(c, k, hint, type);
}

static const Core *var_core(Checker *c, const char *name, const Value **type) {
  const Scope *s = find_local(c, name);
  uint32_t d = find_def(c->m, name);
  if (s != NULL && s->interval)
    return nul(FAIL(c, "INTERVAL_TERM", "%s is an interval variable, not a term of a type", name));
  if (s != NULL) {
    *type = s->type;
    return core_index(c, CORE_VAR, c->level - 1u - s->level);
  }
  if (d != NO_DEF) {
    *type = c->m->defs[d].type;
    return core_index(c, CORE_GLOBAL, d);
  }
  return nul(FAIL(c, "NAME_UNKNOWN", "unknown name %s", name));
}

static const Core *apply_term(Checker *c, const Term *head, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  const Value *ty = NULL;
  const Core *k = head->kind == TERM_VAR ? var_core(c, head->name, &ty) : infer(c, head, &ty);
  return apply_rest(c, k, ty, args, argc, hint, type);
}

static const Core *apply_known(Checker *c, const Term *whole, const Head *h, const Term *const *args, uint32_t argc, const Value *hint, const Value **type) {
  uint32_t arity = known_arity(c, h);
  Call k;
  const Core *core;
  if (argc < arity && hint != NULL && hint->kind == VAL_PI)
    return eta(c, whole, hint, type);
  if (argc < arity)
    return nul(FAIL(c, "TYPE_ARITY", "%s needs %u arguments, found %u", known_name(c, h), arity, argc));
  memset(&k, 0, sizeof k);
  k.c = c;
  k.b = h->builtin;
  k.args = args;
  k.argc = argc;
  k.hint = argc == arity ? hint : NULL;
  k.outer = hint;
  k.used = arity;
  core = known_core(&k, h);
  return apply_rest(c, core, k.type, args + k.used, argc - k.used, hint, type);
}

static const Core *elab_spine(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Term **args;
  const Term *head = t;
  uint32_t argc = 0;
  uint32_t i;
  Head h;
  while (head->kind == TERM_APP) {
    argc++;
    head = head->left;
  }
  if (argc > SPINE_MAX)
    return nul(FAIL(c, "TYPE_ARITY", "an application has more than %u arguments", SPINE_MAX));
  args = arena_alloc(c->m->arena, ((size_t)argc + 1u) * sizeof *args);
  if (args == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (head = t, i = argc; head->kind == TERM_APP; head = head->left)
    args[--i] = head->right;
  if (!resolve_head(c, head, &h))
    return NULL;
  return h.kind == HEAD_TERM ? apply_term(c, head, args, argc, hint, type) : apply_known(c, t, &h, args, argc, hint, type);
}

static int annotation_ok(Checker *c, const Term *ann, const Value *dom) {
  uint64_t level = 0;
  int sset = 0;
  const Value *v = here(c, check_sort(c, ann, &level, &sset));
  if (v == NULL)
    return 0;
  return conv_values(c->m, c->level, v, dom) ? 1 : FAIL(c, "TYPE_MISMATCH", "the binder type %s differs from the expected type %s", show(c, v), show(c, dom));
}

static const Core *check_lam(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *body;
  int ivl;
  if (hint->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_MISMATCH", "a function is not a term of type %s", show(c, hint)));
  ivl = val_is(hint->dom, OP_INTERVAL);
  if (t->left != NULL && ivl && !is_interval_type(c, t->left))
    return nul(FAIL(c, "TYPE_MISMATCH", "the binder type must be the interval I for a term of type %s", show(c, hint)));
  if (t->left != NULL && !ivl && !annotation_ok(c, t->left, hint->dom))
    return NULL;
  if (!(ivl ? push_ivl(c, t->name) : push(c, t->name, hint->dom)))
    return NULL;
  body = check(c, t->right, closure_apply(c->m, hint, c->env->value));
  pop(c);
  *type = hint;
  return core_bind(c, CORE_LAM, t->name, ivl ? core_op(c, OP_INTERVAL, 0, 0, NULL, 0) : NULL, body);
}

static const Core *infer_lam(Checker *c, const Term *t, const Value **type) {
  uint64_t level = 0;
  const Value *dom;
  const Value *bt = NULL;
  const Core *body;
  const Core *quoted;
  int ivl;
  int sset = 0;
  if (t->left == NULL)
    return nul(FAIL(c, "TYPE_INFER", "fun %s => ... needs a binder type here", t->name));
  ivl = is_interval_type(c, t->left);
  dom = ivl ? tyop(c, OP_INTERVAL, 0, NULL, NULL, NULL) : here(c, check_sort(c, t->left, &level, &sset));
  if (dom == NULL || !(ivl ? push_ivl(c, t->name) : push(c, t->name, dom)))
    return NULL;
  body = infer(c, t->right, &bt);
  quoted = body == NULL ? NULL : quote_value(c->m, c->level, bt);
  pop(c);
  *type = val_closure(c->m, VAL_PI, t->name, dom, c->env, quoted);
  return core_bind(c, CORE_LAM, t->name, ivl ? core_op(c, OP_INTERVAL, 0, 0, NULL, 0) : NULL, body);
}

/* (x : A) -> B and Sigma (x : A) B. */
static const Core *elab_binder(Checker *c, const Term *t, CoreKind kind, const Value *hint, const Value **type) {
  uint64_t la = 0;
  uint64_t lb = 0;
  int sa = 0;
  int sb = 0;
  /* M2 s4: a Pi over I, from SSet or into SSet is in SSet. A Sigma
     component must be in Type n. */
  int ivl = kind == CORE_PI && is_interval_type(c, t->left);
  const Core *a = ivl ? core_op(c, OP_INTERVAL, 0, 0, NULL, 0) : kind == CORE_PI ? check_sort(c, t->left, &la, &sa) : check_type(c, t->left, &la);
  const Core *b;
  if (a == NULL || !(ivl ? push_ivl(c, t->name) : push(c, t->name, here(c, a))))
    return NULL;
  b = kind == CORE_PI ? check_sort(c, t->right, &lb, &sb) : check_type(c, t->right, &lb);
  pop(c);
  *type = ivl || sa || sb ? tyop(c, OP_SSET, 0, NULL, NULL, NULL) : val_univ(c->m, la > lb ? la : lb);
  return finish(c, core_bind(c, kind, t->name, a, b), hint, type);
}

static const Value *field_type(Checker *c) {
  return tyop(c, OP_FIELD, 0, NULL, NULL, NULL);
}

/* glue-lang: a Field literal, the canonical decimal of a value below r. */
static const Core *field_literal(Checker *c, const Term *t, const Value **type) {
  char text[FR_TEXT_MAX];
  Fr x;
  Core *k;
  if (!fr_parse(t->name, strlen(t->name), &x))
    return nul(FAIL(c, "FIELD_RANGE", "%d:%d: the Field literal %s is not a decimal below r (BN254 Fr)", t->line, t->col, t->name));
  fr_print(&x, text);
  k = new_core(c, CORE_OP);
  if (k == NULL)
    return NULL;
  k->op = OP_FIELD_LIT;
  k->name = arena_strndup(c->m->arena, text, strlen(text));
  *type = field_type(c);
  return k->name == NULL ? nul(FAIL(c, "OOM", "out of memory")) : k;
}

/* An operand of + - *: a literal is a Field literal; any other term must
   have type Field. */
static const Core *field_term(Checker *c, const Term *t, const char *code, const char *what) {
  const Value *type = NULL;
  const Core *k;
  if (t->kind == TERM_NAT)
    return field_literal(c, t, &type);
  k = infer(c, t, &type);
  if (k != NULL && !val_is(type, OP_FIELD))
    return nul(FAIL(c, code, "%d:%d: %s, found a term of type %s", t->line, t->col, what, show(c, type)));
  return k;
}

static const Core *field_operand(Checker *c, const Term *t) {
  return field_term(c, t, "FIELD_ARITH", "+ - * need Field operands");
}

static const Core *elab_arith(Checker *c, const Term *t, const Value *hint, const Value **type) {
  static const Op OPS[] = {OP_FIELD_ADD, OP_FIELD_SUB, OP_FIELD_MUL};
  const Core *a = field_operand(c, t->left);
  const Core *b = a == NULL ? NULL : field_operand(c, t->right);
  *type = field_type(c);
  return b == NULL ? NULL : finish(c, op3(c, OPS[t->nat < 3u ? t->nat : 0u], 0, 2, a, b, NULL), hint, type);
}

static const Core *core_ivl(Checker *c, IvlOp op, const Core *a, const Core *b) {
  Core *k = new_core(c, CORE_IVL);
  if (k == NULL)
    return NULL;
  k->nat = op;
  k->a = a;
  k->b = b;
  return k;
}

/* An interval term: an interval variable, i0, i1, ~r, r /\ s or r \/ s. */
static const Core *elab_ivl(Checker *c, const Term *t) {
  const Scope *s = t->kind == TERM_VAR ? find_local(c, t->name) : NULL;
  const Core *a;
  const Core *b;
  Head h;
  if (c->depth >= CHECK_DEPTH_LIMIT)
    return nul(FAIL(c, "CHECK_DEPTH", "the term is nested deeper than %u levels", CHECK_DEPTH_LIMIT));
  if (s != NULL && s->interval)
    return core_index(c, CORE_VAR, c->level - 1u - s->level);
  if (t->kind == TERM_VAR && s == NULL && (strcmp(t->name, "i0") == 0 || strcmp(t->name, "i1") == 0))
    return core_ivl(c, t->name[1] == '1' ? IVL_ONE : IVL_ZERO, NULL, NULL);
  if (t->kind == TERM_VAR && !resolve_head(c, t, &h))
    return NULL;
  if (t->kind != TERM_IVL)
    return nul(FAIL(c, "INTERVAL_TERM", "@ needs an interval term (an interval variable, i0, i1, ~, /\\ or \\/), found a term of a type"));
  c->depth++;
  a = elab_ivl(c, t->left);
  b = a == NULL || t->nat == IVL_REV ? NULL : elab_ivl(c, t->right);
  c->depth--;
  if (a == NULL || (b == NULL && t->nat != IVL_REV))
    return NULL;
  return core_ivl(c, (IvlOp)t->nat, a, b);
}

/* <i> t against PathP L a b: t : L @ i, t[i := i0] = a, t[i := i1] = b. */
static const Core *check_plam(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *body;
  const Core *k;
  const Value *v;
  int one;
  if (hint == NULL)
    return nul(FAIL(c, "PATH_EXPECTED", "<%s> ... needs a declared type PathP L a b here", t->name));
  if (!val_is(hint, OP_PATHP))
    return nul(FAIL(c, "PATH_EXPECTED", "a path <%s> ... is not a term of type %s", t->name, show(c, hint)));
  if (!push_ivl(c, t->name))
    return NULL;
  body = check(c, t->right, closure_apply(c->m, hint->args[0], c->env->value));
  pop(c);
  k = core_bind(c, CORE_PLAM, t->name, NULL, body);
  v = here(c, k);
  for (one = 0; v != NULL && one < 2; one++) {
    const Value *end = closure_apply(c->m, v, val_ivl_end(c->m, one));
    if (end == NULL)
      return NULL;
    if (!conv_values(c->m, c->level, end, hint->args[1 + one]))
      return nul(FAIL(c, "PATH_BOUNDARY", "the path <%s> ... does not have the end points of its type: at i%d it normalizes to %s, the type needs %s", t->name, one, show(c, end), show(c, hint->args[1 + one])));
  }
  *type = hint;
  return v == NULL ? NULL : k;
}

/* p @ r, p : PathP L a b. The type is L @ r. The core keeps a and b, so the
   evaluator can reduce a neutral p at i0 and i1. */
static const Core *infer_papp(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Value *pt = NULL;
  const Core *p = infer(c, t->left, &pt);
  const Core *r;
  const Core *k;
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PATHP))
    return nul(FAIL(c, "PATH_EXPECTED", "@ needs a path of type PathP L a b, found a term of type %s", show(c, pt)));
  r = elab_ivl(c, t->right);
  k = core_papp(c, c->level, p, pt, r);
  if (k == NULL)
    return NULL;
  *type = closure_apply(c->m, pt->args[0], here(c, r));
  return finish(c, k, hint, type);
}

/* M2 s4: the context restricted by conjunction K of FACE. An interval
   variable that the conjunction sets is a constant end point. Each local
   type is quoted, then evaluated in the restricted context below it. */
static int restrict_ctx(Checker *c, const Scope *s, const Env *e, const Ivl *face, uint32_t k, const Scope **rs, const Env **re) {
  const Scope *ts = NULL;
  const Env *te = NULL;
  const Value *v;
  Scope *ns;
  int one = 0;
  if (s == NULL || e == NULL) {
    *rs = s;
    *re = e;
    return 1;
  }
  if (!restrict_ctx(c, s->next, e->next, face, k, &ts, &te))
    return 0;
  ns = arena_alloc(c->m->arena, sizeof *ns);
  v = s->interval && face_conj_sets(face, k, s->level, &one) ? val_ivl_end(c->m, one) : e->value;
  if (ns == NULL || v == NULL)
    return c->m->diag->set ? 0 : FAIL(c, "OOM", "out of memory");
  *ns = *s;
  ns->type = s->type == NULL ? NULL : eval_core(c->m, te, quote_value(c->m, s->level, s->type));
  ns->next = ts;
  *rs = ns;
  *re = env_push(c->m, te, v);
  return (s->type == NULL || ns->type != NULL) && *re != NULL;
}

/* BODY against A in the context restricted by conjunction K of FACE. */
static const Core *check_restricted(Checker *c, const Term *body, const Value *a, const Ivl *face, uint32_t k) {
  const Scope *scope = c->scope;
  const Env *env = c->env;
  const Scope *rs = NULL;
  const Env *re = NULL;
  const Core *out;
  if (!restrict_ctx(c, scope, env, face, k, &rs, &re))
    return NULL;
  c->scope = rs;
  c->env = re;
  out = check(c, body, eval_core(c->m, re, quote_value(c->m, c->level, a)));
  c->scope = scope;
  c->env = env;
  return out;
}

/* The core K evaluated in the context restricted by conjunction J of FACE. */
static const Value *eval_restricted(Checker *c, const Core *k, const Ivl *face, uint32_t j) {
  const Scope *rs = NULL;
  const Env *re = NULL;
  return restrict_ctx(c, c->scope, c->env, face, j, &rs, &re) ? eval_core(c->m, re, k) : NULL;
}

/* FACE_AGREE: branches K and L give the same value on each conjunction of
   the meet of their faces. */
static int branches_agree(Checker *c, const Core *const *args, const Ivl *const *faces, uint32_t k, uint32_t l) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Ivl *meet = face_meet(&h, faces[k], faces[l]);
  uint32_t j;
  if (meet == NULL)
    return 0;
  for (j = 0; j < meet->count; j++) {
    const Value *x = eval_restricted(c, args[2u * k + 1u], meet, j);
    const Value *y = eval_restricted(c, args[2u * l + 1u], meet, j);
    if (!conv_values(c->m, c->level, x, y))
      return c->m->diag->set ? 0 : FAIL(c, "FACE_AGREE", "the branches on %s and %s differ on %s: %s and %s", show(c, val_ivl(c->m, faces[k])), show(c, val_ivl(c->m, faces[l])), show(c, val_ivl(c->m, meet)), show(c, x), show(c, y));
  }
  return 1;
}

/* M3 s1: transp (<i> A) phi a : A[i := i1]. The line is a PathP line;
   a : A[i := i0];  on each conjunction of phi, A at a fresh i converts to
   A[i := i0] (TRANSP_CONSTANT). The line binder is not in the context, so
   the restriction changes only the context variables. */
static const Core *rule_transp(Call *k) {
  Checker *c = k->c;
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Term *t = k->args[0];
  uint64_t level = 0;
  const Core *line;
  const Value *lv;
  const Core *face;
  const Value *fv;
  const Ivl *norm;
  const Core *a;
  uint32_t j;
  if (t->kind != TERM_PLAM)
    return nul(FAIL(c, "PATH_TYPE", "transp needs a line <i> A as its first argument"));
  line = path_line(c, t->name, t->right, &level);
  lv = here(c, line);
  face = lv == NULL ? NULL : elab_ivl(c, k->args[1]);
  fv = here(c, face);
  norm = fv == NULL || fv->kind != VAL_IVL ? NULL : face_norm(&h, fv->ivl);
  a = norm == NULL ? NULL : check(c, k->args[2], closure_apply(c->m, lv, val_ivl_end(c->m, 0)));
  if (a == NULL)
    return NULL;
  for (j = 0; j < norm->count; j++) {
    const Value *lr = eval_restricted(c, line, norm, j);
    const Value *x = closure_apply(c->m, lr, val_ivl_var(c->m, c->level));
    const Value *y = closure_apply(c->m, lr, val_ivl_end(c->m, 0));
    if (!conv_values(c->m, c->level + 1u, x, y))
      return c->m->diag->set ? NULL : nul(FAIL(c, "TRANSP_CONSTANT", "transp needs a line that is constant on %s, but the line goes from %s to %s there", show(c, val_ivl(c->m, face_conj(&h, norm, j))), show(c, y), show(c, closure_apply(c->m, lr, val_ivl_end(c->m, 1)))));
  }
  k->type = closure_apply(c->m, lv, val_ivl_end(c->m, 1));
  return op3(c, OP_TRANSP, 0, 3, line, face, a);
}

/* M3 s2: hcomp, comp and hfill (M3-PLAN 3.2 and section 4). comp and hfill
   are checker rules that write an hcomp core term (D2). */

/* The face normal form of the interval core FACE. */
static const Ivl *kan_face(Checker *c, const Core *face) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Value *fv = here(c, face);
  return fv == NULL || fv->kind != VAL_IVL ? NULL : face_norm(&h, fv->ivl);
}

/* The type (i : I) -> Partial PHI A[i] of a tube. A[i] is A when LINE is
   NULL, else LINE at i. */
static const Value *tube_type(Checker *c, const Value *phi, const Value *line, const Value *a) {
  const Value *ai = line == NULL ? a : closure_apply(c->m, line, val_ivl_var(c->m, c->level));
  const Value *p = ai == NULL ? NULL : tyop(c, OP_PARTIAL, 2, phi, ai, NULL);
  return close_over(c, VAL_PI, "i", tyop(c, OP_INTERVAL, 0, NULL, NULL, NULL), p);
}

/* HCOMP_BASE: on each conjunction of NORM, u i0 converts to u0. */
static int hcomp_base(Checker *c, const char *name, const Ivl *norm, const Core *u, const Core *u0) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  uint32_t j;
  for (j = 0; j < norm->count; j++) {
    const Value *uv = eval_restricted(c, u, norm, j);
    const Value *x = uv == NULL ? NULL : apply_value(c->m, uv, val_ivl_end(c->m, 0));
    const Value *y = x == NULL ? NULL : eval_restricted(c, u0, norm, j);
    if (y == NULL)
      return 0;
    if (!conv_values(c->m, c->level, x, y))
      return c->m->diag->set ? 0 : FAIL(c, "HCOMP_BASE", "%s needs u i0 to be u0 on %s, but u i0 is %s and u0 is %s", name, show(c, val_ivl(c->m, face_conj(&h, norm, j))), show(c, x), show(c, y));
  }
  return 1;
}

/* K under one more binder. */
static const Core *up(Checker *c, const Core *k) {
  return core_shift(c, k, 1u, 0);
}

/* fun i => [ F0 -> B0, F1 -> B1 ] with N (1 or 2) branches. The faces and
   the bodies are core terms under the binder i. */
static const Core *tube_core(Checker *c, uint32_t n, const Core *f0, const Core *b0, const Core *f1, const Core *b1) {
  const Core *args[4];
  args[0] = f0;
  args[1] = b0;
  args[2] = f1;
  args[3] = b1;
  return core_bind(c, CORE_LAM, "i", core_op(c, OP_INTERVAL, 0, 0, NULL, 0), core_op(c, OP_SYS, n, 0, args, 2u * n));
}

static const Core *hcomp_core(Checker *c, const Core *a, const Core *phi, const Core *u, const Core *u0) {
  const Core *args[4];
  args[0] = a;
  args[1] = phi;
  args[2] = u;
  args[3] = u0;
  return core_op(c, OP_HCOMP, 0, 0, args, 4u);
}

/* A[i \/ j] for the line LV, a core term under the binders i and j. */
static const Core *line_join(Checker *c, const Value *lv) {
  const Core *k = NULL;
  const Value *r;
  const Value *x;
  if (!push_ivl(c, "i"))
    return NULL;
  if (push_ivl(c, "j")) {
    r = here(c, core_ivl(c, IVL_JOIN, core_index(c, CORE_VAR, 1u), core_index(c, CORE_VAR, 0)));
    x = r == NULL ? NULL : closure_apply(c->m, lv, r);
    k = x == NULL ? NULL : quote_value(c->m, c->level, x);
    pop(c);
  }
  pop(c);
  return k;
}

/* hcomp A phi u u0 : A. A : Type n;  u : (i : I) -> Partial phi A;
   u0 : A;  HCOMP_BASE. */
static const Core *rule_hcomp(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *a = check_type(c, k->args[0], &level);
  const Value *av = here(c, a);
  const Core *face = av == NULL ? NULL : elab_ivl(c, k->args[1]);
  const Ivl *norm = face == NULL ? NULL : kan_face(c, face);
  const Core *u = norm == NULL ? NULL : check(c, k->args[2], tube_type(c, here(c, face), NULL, av));
  const Core *u0 = u == NULL ? NULL : check(c, k->args[3], av);
  if (u0 == NULL || !hcomp_base(c, "hcomp", norm, u, u0))
    return NULL;
  k->type = av;
  return hcomp_core(c, a, face, u, u0);
}

/* comp (<i> A) phi u u0 : A[i1]. u : (i : I) -> Partial phi A[i];
   u0 : A[i0];  HCOMP_BASE. The core term (section 4):
   hcomp A[i1] phi (fun i => [ phi -> transp (<j> A[i \/ j]) i (u i) ])
     (transp (<i> A) i0 u0). */
static const Core *rule_comp(Call *k) {
  Checker *c = k->c;
  const Term *t = k->args[0];
  uint64_t level = 0;
  const Core *line;
  const Value *lv;
  const Core *face;
  const Ivl *norm;
  const Core *u;
  const Core *u0;
  const Core *fill;
  const Core *out;
  if (t->kind != TERM_PLAM)
    return nul(FAIL(c, "PATH_TYPE", "comp needs a line <i> A as its first argument"));
  line = path_line(c, t->name, t->right, &level);
  lv = here(c, line);
  face = lv == NULL ? NULL : elab_ivl(c, k->args[1]);
  norm = face == NULL ? NULL : kan_face(c, face);
  u = norm == NULL ? NULL : check(c, k->args[2], tube_type(c, here(c, face), lv, NULL));
  u0 = u == NULL ? NULL : check(c, k->args[3], closure_apply(c->m, lv, val_ivl_end(c->m, 0)));
  if (u0 == NULL || !hcomp_base(c, "comp", norm, u, u0))
    return NULL;
  k->type = closure_apply(c->m, lv, val_ivl_end(c->m, 1));
  fill = op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_PLAM, "j", NULL, line_join(c, lv)), core_index(c, CORE_VAR, 0), core_app(c, up(c, u), core_index(c, CORE_VAR, 0)));
  out = hcomp_core(c, k->type == NULL ? NULL : quote_value(c->m, c->level, k->type), face,
                   tube_core(c, 1u, up(c, face), fill, NULL, NULL),
                   op3(c, OP_TRANSP, 0, 3, line, core_ivl(c, IVL_ZERO, NULL, NULL), u0));
  return c->m->diag->set ? NULL : out;
}

/* hfill A phi u u0 r : A. The premises of hcomp, and r is an interval
   term. The core term (section 4):
   hcomp A (phi \/ ~r) (fun i => [ phi -> u (i /\ r), ~r -> u0 ]) u0. */
static const Core *rule_hfill(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *a = check_type(c, k->args[0], &level);
  const Value *av = here(c, a);
  const Core *face = av == NULL ? NULL : elab_ivl(c, k->args[1]);
  const Ivl *norm = face == NULL ? NULL : kan_face(c, face);
  const Core *u = norm == NULL ? NULL : check(c, k->args[2], tube_type(c, here(c, face), NULL, av));
  const Core *u0 = u == NULL ? NULL : check(c, k->args[3], av);
  const Core *r = u0 == NULL ? NULL : elab_ivl(c, k->args[4]);
  const Core *ri;
  const Core *out;
  if (r == NULL || !hcomp_base(c, "hfill", norm, u, u0))
    return NULL;
  k->type = av;
  ri = up(c, r);
  out = ri == NULL ? NULL : hcomp_core(c, a, core_ivl(c, IVL_JOIN, face, core_ivl(c, IVL_REV, r, NULL)),
                                       tube_core(c, 2u, up(c, face), core_app(c, up(c, u), core_ivl(c, IVL_MEET, core_index(c, CORE_VAR, 0), ri)),
                                                 core_ivl(c, IVL_REV, ri, NULL), up(c, u0)),
                                       u0);
  return c->m->diag->set ? NULL : out;
}

/* M3 s3: trans, transport, subst, transportRefl, J and JRefl are checker
   rules that write transp and hcomp core terms (M3-PLAN section 4, D2). */

/* The type A of PT = Path A a b (a constant line), or NULL with
   PATH_EXPECTED for NAME. */
static const Value *path_base(Checker *c, const char *name, const Value *pt) {
  const Value *a = val_is(pt, OP_PATHP) ? line_type(c, pt->args[0]) : NULL;
  if (a == NULL && !c->m->diag->set)
    FAIL(c, "PATH_EXPECTED", "%s needs a path of type Path A a b, found a term of type %s", name, show(c, pt));
  return a;
}

/* <i> P @ i for P : PT, a line in a context of one more binder. */
static const Core *path_body(Checker *c, const Core *p, const Value *pt) {
  return core_papp(c, c->level + 1u, up(c, p), pt, core_index(c, CORE_VAR, 0));
}

/* transp LINE i0 X */
static const Core *transp_zero(Checker *c, const Core *line, const Core *x) {
  return op3(c, OP_TRANSP, 0, 3, line, core_ivl(c, IVL_ZERO, NULL, NULL), x);
}

/* trans p q, p : Path A x y, q : Path A y z, gives Path A x z. The core is
   <i> hcomp A (~i \/ i) (fun j => [ ~i -> x, i -> q @ j ]) (p @ i). */
static const Core *rule_trans(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Value *qt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  const Value *a = p == NULL ? NULL : path_base(c, "trans", pt);
  const Core *q = a == NULL ? NULL : infer(c, k->args[1], &qt);
  const Value *b = q == NULL ? NULL : path_base(c, "trans", qt);
  const Core *sys[4];
  if (b == NULL || !expect(c, b, a))
    return NULL;
  if (!conv_values(c->m, c->level, pt->args[2], qt->args[1]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs q to start where p ends, but p ends at %s and q starts at %s", show(c, pt->args[2]), show(c, qt->args[1])));
  k->type = tyop(c, OP_PATHP, 3, pt->args[0], pt->args[1], qt->args[2]);
  sys[0] = core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 1u), NULL);
  sys[1] = quote_value(c->m, c->level + 2u, pt->args[1]);
  sys[2] = core_index(c, CORE_VAR, 1u);
  sys[3] = core_papp(c, c->level + 2u, core_shift(c, q, 2u, 0), qt, core_index(c, CORE_VAR, 0));
  return core_bind(c, CORE_PLAM, "i", NULL,
                   hcomp_core(c, quote_value(c->m, c->level + 1u, a),
                              core_ivl(c, IVL_JOIN, core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 0), NULL), core_index(c, CORE_VAR, 0)),
                              core_bind(c, CORE_LAM, "j", core_op(c, OP_INTERVAL, 0, 0, NULL, 0), core_op(c, OP_SYS, 2u, 0, sys, 4u)),
                              path_body(c, p, pt)));
}

/* transport p x, p : Path (Type n) A B, x : A, gives B (D6). The core is
   transp (<i> p @ i) i0 x. */
static const Core *rule_transport(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  const Value *u = p == NULL ? NULL : path_base(c, "transport", pt);
  const Core *x;
  if (u == NULL)
    return NULL;
  if (u->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "transport needs a path of type Path (Type n) A B, found a term of type %s", show(c, pt)));
  x = check(c, k->args[1], pt->args[1]);
  k->type = pt->args[2];
  return transp_zero(c, core_bind(c, CORE_PLAM, "i", NULL, path_body(c, p, pt)), x);
}

/* subst P p x, P : A -> Type n, p : Path A a b, x : P a, gives P b. The
   core is transp (<i> P (p @ i)) i0 x. */
static const Core *rule_subst(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Value *ft = NULL;
  const Core *p = infer(c, k->args[1], &pt);
  const Value *a = p == NULL ? NULL : path_base(c, "subst", pt);
  const Core *f = a == NULL ? NULL : infer(c, k->args[0], &ft);
  const Value *fv = here(c, f);
  const Value *cod;
  const Core *x;
  if (fv == NULL)
    return NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "subst needs a family P : A -> Type n, found a term of type %s", show(c, ft)));
  if (!expect(c, a, ft->dom))
    return NULL;
  cod = closure_apply(c->m, ft, pt->args[1]);
  if (cod == NULL || cod->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "subst needs a family P : A -> Type n, found a term of type %s", show(c, ft)));
  x = check(c, k->args[2], apply_value(c->m, fv, pt->args[1]));
  k->type = apply_value(c->m, fv, pt->args[2]);
  return transp_zero(c, core_bind(c, CORE_PLAM, "i", NULL, core_app(c, up(c, f), path_body(c, p, pt))), x);
}

/* transportRefl x, x : A, gives Path A (transp (<_> A) i0 x) x. The core
   is <i> transp (<_> A) i x. */
static const Core *rule_transport_refl(Call *k) {
  Checker *c = k->c;
  const Value *at = NULL;
  const Core *x = infer(c, k->args[0], &at);
  const Core *line0 = x == NULL ? NULL : core_bind(c, CORE_PLAM, "_", NULL, quote_value(c->m, c->level + 1u, at));
  const Value *start = here(c, transp_zero(c, line0, x));
  const Value *line = start == NULL ? NULL : close_over(c, VAL_PLAM, "i", NULL, at);
  if (line == NULL)
    return NULL;
  k->type = tyop(c, OP_PATHP, 3, line, start, here(c, x));
  return core_bind(c, CORE_PLAM, "i", NULL,
                   op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_PLAM, "_", NULL, quote_value(c->m, c->level + 2u, at)),
                       core_index(c, CORE_VAR, 0), up(c, x)));
}

/* J motive: FT = (y : A) -> Path A x y -> Type n, checked at a fresh y and
   a fresh path. LINE is the constant line of A. */
static int j_motive(Checker *c, const Value *ft, const Value *a, const Value *line, const Value *x) {
  const Value *t1;
  const Value *t2 = NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return FAIL(c, "TYPE_NOT_FUNCTION", "J needs a motive P : (y : A) -> Path A x y -> Type n, found a term of type %s", show(c, ft));
  if (!expect(c, a, ft->dom) || !push(c, "y", a))
    return 0;
  t1 = closure_apply(c->m, ft, c->env->value);
  if (t1 != NULL && t1->kind == VAL_PI && expect(c, tyop(c, OP_PATHP, 3, line, x, c->env->value), t1->dom) && push(c, "p", t1->dom)) {
    t2 = closure_apply(c->m, t1, c->env->value);
    pop(c);
  }
  pop(c);
  if (t2 == NULL || t2->kind != VAL_UNIV)
    return c->m->diag->set ? 0 : FAIL(c, "TYPE_MISMATCH", "J needs a motive P : (y : A) -> Path A x y -> Type n, found a term of type %s", show(c, ft));
  return 1;
}

/* J P d p, P : (y : A) -> Path A x y -> Type n, d : P x refl,
   p : Path A x y, gives P y p. The core is
   transp (<i> P (p @ i) (<j> p @ (i /\ j))) i0 d. */
static const Core *rule_j(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Value *ft = NULL;
  const Core *p = infer(c, k->args[2], &pt);
  const Value *a = p == NULL ? NULL : path_base(c, "J", pt);
  const Core *f = a == NULL ? NULL : infer(c, k->args[0], &ft);
  const Value *fv = here(c, f);
  const Value *refl;
  const Core *d;
  const Core *pij;
  if (fv == NULL || !j_motive(c, ft, a, pt->args[0], pt->args[1]))
    return NULL;
  refl = here(c, core_bind(c, CORE_PLAM, "i", NULL, quote_value(c->m, c->level + 1u, pt->args[1])));
  d = refl == NULL ? NULL : check(c, k->args[1], apply_value(c->m, apply_value(c->m, fv, pt->args[1]), refl));
  if (d == NULL)
    return NULL;
  k->type = apply_value(c->m, apply_value(c->m, fv, pt->args[2]), here(c, p));
  pij = core_papp(c, c->level + 2u, core_shift(c, p, 2u, 0), pt, core_ivl(c, IVL_MEET, core_index(c, CORE_VAR, 1u), core_index(c, CORE_VAR, 0)));
  return transp_zero(c, core_bind(c, CORE_PLAM, "i", NULL,
                                  core_app(c, core_app(c, up(c, f), path_body(c, p, pt)), core_bind(c, CORE_PLAM, "j", NULL, pij))),
                     d);
}

/* JRefl P d, P : (y : A) -> Path A x y -> Type n, d : P x refl, gives
   Path (P x refl) (J P d refl) d. The core is <i> transp (<_> P x refl) i d.
   x comes from the type of P and cannot depend on y. */
static const Core *rule_j_refl(Call *k) {
  Checker *c = k->c;
  const Value *ft = NULL;
  const Core *f = infer(c, k->args[0], &ft);
  const Value *fv = here(c, f);
  const Value *t1 = NULL;
  const Value *x = NULL;
  const Value *line;
  const Value *refl;
  const Value *ty;
  const Value *start;
  const Core *d;
  const Core *tyk;
  uint32_t bound = 0;
  if (fv == NULL)
    return NULL;
  if (ft == NULL || ft->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "JRefl needs a motive P : (y : A) -> Path A x y -> Type n, found a term of type %s", show(c, ft)));
  if (push(c, "y", ft->dom)) {
    t1 = closure_apply(c->m, ft, c->env->value);
    x = t1 != NULL && t1->kind == VAL_PI && val_is(t1->dom, OP_PATHP) ? t1->dom->args[1] : NULL;
    pop(c);
  }
  if (x != NULL && (!value_bound(c->m, x, &bound) || bound > c->level))
    x = NULL;
  if (x == NULL)
    return c->m->diag->set ? NULL : nul(FAIL(c, "TYPE_MISMATCH", "JRefl needs a motive P : (y : A) -> Path A x y -> Type n, found a term of type %s", show(c, ft)));
  line = close_over(c, VAL_PLAM, "i", NULL, ft->dom);
  if (line == NULL || !j_motive(c, ft, ft->dom, line, x))
    return NULL;
  refl = here(c, core_bind(c, CORE_PLAM, "i", NULL, quote_value(c->m, c->level + 1u, x)));
  ty = refl == NULL ? NULL : apply_value(c->m, apply_value(c->m, fv, x), refl);
  d = ty == NULL ? NULL : check(c, k->args[1], ty);
  tyk = d == NULL ? NULL : quote_value(c->m, c->level + 2u, ty);
  line = tyk == NULL ? NULL : close_over(c, VAL_PLAM, "i", NULL, ty);
  start = line == NULL ? NULL : here(c, transp_zero(c, core_bind(c, CORE_PLAM, "_", NULL, quote_value(c->m, c->level + 1u, ty)), d));
  if (start == NULL)
    return NULL;
  k->type = tyop(c, OP_PATHP, 3, line, start, here(c, d));
  return core_bind(c, CORE_PLAM, "i", NULL, op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_PLAM, "_", NULL, tyk), core_index(c, CORE_VAR, 0), up(c, d)));
}

/* M2 s4: [ phi1 -> t1, ... ] against Partial phi A. Each ti : A under each
   conjunction of phi_i;  FACE_AGREE on each meet;  FACE_COVER: the join of
   the faces is phi. */
static const Core *check_sys(Checker *c, const Term *t, const Value *hint, const Value **type, int glue) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Ivl *cover = ivl_const(&h, 0);
  const Value *base;
  const Value *required;
  const Core **args;
  const Ivl **faces;
  const Ivl **branch_faces;
  const Term *b;
  uint32_t branches = 0;
  uint32_t n = 0;
  uint32_t at = 0;
  uint32_t k;
  uint32_t l;
  if (hint == NULL)
    return nul(FAIL(c, "PARTIAL_EXPECTED", "%d:%d: a system [ ... ] needs a declared type Partial phi A here, found %s", t->line, t->col, hint == NULL ? "no type" : show(c, hint)));
  /* Partial i1 A reduces to A before checking its system. A total system
     therefore checks against A directly, with the same agreement checks. */
  base = val_is(hint, OP_PARTIAL) ? hint->args[1] : hint;
  required = val_is(hint, OP_PARTIAL) ? hint->args[0] : val_ivl_end(c->m, 1);
  if (required == NULL || required->kind != VAL_IVL)
    return NULL;
  for (b = t; b != NULL; b = b->next)
    branches++;
  branch_faces = arena_alloc(c->m->arena, ((size_t)branches + 1u) * sizeof *branch_faces);
  if (branch_faces == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (b = t, k = 0; b != NULL && cover != NULL; b = b->next, k++) {
    const Value *fv = here(c, elab_ivl(c, b->left));
    uint32_t count;
    branch_faces[k] = fv == NULL ? NULL : face_norm(&h, fv->ivl);
    if (branch_faces[k] == NULL)
      return NULL;
    count = branch_faces[k]->count == 0u ? 1u : branch_faces[k]->count;
    if (count > UINT32_MAX / 2u - n)
      return nul(FAIL(c, "OOM", "the system has too many conjunctions"));
    n += count;
    cover = face_join(&h, cover, branch_faces[k]);
  }
  if (cover == NULL)
    return NULL;
  if (!val_is(hint, OP_PARTIAL) && !ivl_is_const(cover, 1))
    return nul(FAIL(c, "PARTIAL_EXPECTED", "%d:%d: a system [ ... ] needs a declared type Partial phi A here, found %s", t->line, t->col, show(c, hint)));
  args = arena_alloc(c->m->arena, (2u * (size_t)n + 1u) * sizeof *args);
  faces = arena_alloc(c->m->arena, ((size_t)n + 1u) * sizeof *faces);
  if (args == NULL || faces == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  /* Elaboration can specialize a body (for example refl) differently on
     each conjunction. Keep that core only on the face where it checks. */
  for (b = t, k = 0; b != NULL; b = b->next, k++) {
    const Ivl *face = branch_faces[k];
    uint32_t count = face->count == 0u ? 1u : face->count;
    for (l = 0; l < count; l++, at++) {
      faces[at] = face->count == 0u ? face : face_conj(&h, face, l);
      args[2u * at] = faces[at] == NULL ? NULL : quote_value(c->m, c->level, val_ivl(c->m, faces[at]));
      /* A false Glue branch has no selected branch type. Its body is
         unreachable under every substitution, so keep an erased unit body. */
      args[2u * at + 1u] = args[2u * at] == NULL ? NULL : glue && face->count == 0u
        ? op3(c, OP_UNIT_VAL, 0, 0, NULL, NULL, NULL) : face->count == 0u
        ? check(c, b->right, base) : check_restricted(c, b->right, base, face, l);
      if (args[2u * at + 1u] == NULL)
        return NULL;
    }
  }
  if (!face_equal(cover, required->ivl))
    return nul(FAIL(c, "FACE_COVER", "the faces of the system join to %s, but the type needs the face %s", show(c, val_ivl(c->m, cover)), show(c, required)));
  for (k = 0; k < n; k++)
    for (l = k + 1u; l < n; l++)
      if (!branches_agree(c, args, faces, k, l))
        return NULL;
  *type = hint;
  return core_op(c, OP_SYS, n, 0, args, 2u * n);
}

/* M3 s4: the equivalences of M3-PLAN section 4 as core terms (D2). The
   arguments are core terms in the current context. Each builder moves them
   under its binders. */

/* Path A x y, with the constant line <_> A. */
static const Core *path_core(Checker *c, const Core *a, const Core *x, const Core *y) {
  return op3(c, OP_PATHP, 0, 3, core_bind(c, CORE_PLAM, "_", NULL, up(c, a)), x, y);
}

/* isContr A = Sigma (c : A) ((y : A) -> Path A c y) */
static const Core *core_is_contr(Checker *c, const Core *a) {
  const Core *path = path_core(c, core_shift(c, a, 2u, 0), core_index(c, CORE_VAR, 1u), core_index(c, CORE_VAR, 0));
  return core_bind(c, CORE_SIGMA, "c", a, core_bind(c, CORE_PI, "y", up(c, a), path));
}

/* M4 s3 (D10): isProp A = (x : A) -> (y : A) -> Path A x y */
static const Core *core_is_prop(Checker *c, const Core *a) {
  const Core *path = path_core(c, core_shift(c, a, 2u, 0), core_index(c, CORE_VAR, 1u), core_index(c, CORE_VAR, 0));
  return core_bind(c, CORE_PI, "x", a, core_bind(c, CORE_PI, "y", up(c, a), path));
}

/* fiber f y = Sigma (x : A) (Path B (f x) y), f : A -> B, y : B */
static const Core *core_fiber(Checker *c, const Core *a, const Core *b, const Core *f, const Core *y) {
  const Core *fx = core_app(c, up(c, f), core_index(c, CORE_VAR, 0));
  return core_bind(c, CORE_SIGMA, "x", a, path_core(c, up(c, b), fx, up(c, y)));
}

/* isEquiv f = (y : B) -> isContr (fiber f y), f : A -> B */
static const Core *core_is_equiv(Checker *c, const Core *a, const Core *b, const Core *f) {
  const Core *fib = core_fiber(c, up(c, a), up(c, b), up(c, f), core_index(c, CORE_VAR, 0));
  return core_bind(c, CORE_PI, "y", b, core_is_contr(c, fib));
}

/* Equiv A B = Sigma (f : A -> B) (isEquiv f) */
static const Core *core_equiv(Checker *c, const Core *a, const Core *b) {
  const Core *fn = core_bind(c, CORE_PI, NULL, a, up(c, b));
  return core_bind(c, CORE_SIGMA, "f", fn, core_is_equiv(c, up(c, a), up(c, b), core_index(c, CORE_VAR, 0)));
}

/* idEquiv A = pack (fun x => x) (fun y => pack (pack y (<_> y))
   (fun z => <i> pack (second z @ ~i) (<j> second z @ (~i \/ j)))). The
   path second z has the ends witness z and y. The term does not use A. */
static const Core *core_id_equiv(Checker *c) {
  const Core *z_ij = papp_ends(c, op3(c, OP_PAYLOAD, 0, 1, core_index(c, CORE_VAR, 2u), NULL, NULL),
                               core_ivl(c, IVL_JOIN, core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 1u), NULL), core_index(c, CORE_VAR, 0)),
                               op3(c, OP_WITNESS, 0, 1, core_index(c, CORE_VAR, 2u), NULL, NULL), core_index(c, CORE_VAR, 3u));
  const Core *z_i = papp_ends(c, op3(c, OP_PAYLOAD, 0, 1, core_index(c, CORE_VAR, 1u), NULL, NULL),
                              core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 0), NULL),
                              op3(c, OP_WITNESS, 0, 1, core_index(c, CORE_VAR, 1u), NULL, NULL), core_index(c, CORE_VAR, 2u));
  const Core *contract = core_bind(c, CORE_LAM, "z", NULL,
                                   core_bind(c, CORE_PLAM, "i", NULL, op3(c, OP_PACK, 0, 2, z_i, core_bind(c, CORE_PLAM, "j", NULL, z_ij), NULL)));
  const Core *center = op3(c, OP_PACK, 0, 2, core_index(c, CORE_VAR, 0), core_bind(c, CORE_PLAM, "_", NULL, core_index(c, CORE_VAR, 1u)), NULL);
  const Core *id = core_bind(c, CORE_LAM, "x", NULL, core_index(c, CORE_VAR, 0));
  return op3(c, OP_PACK, 0, 2, id, core_bind(c, CORE_LAM, "y", NULL, op3(c, OP_PACK, 0, 2, center, contract, NULL)), NULL);
}

/* *LEVEL is n for a type T in Type n: a universe, a neutral, a core type
   former, a Pi or Sigma type, a path type or a Glue type. Else
   TYPE_INFER. */
static int type_level(Checker *c, const char *name, const Value *t, uint64_t *level) {
  const Scope *s = c->scope;
  uint64_t la = 0;
  uint64_t lb = 0;
  int ok;
  if (t != NULL && t->kind == VAL_UNIV) {
    *level = t->nat + 1u;
    return 1;
  }
  if (t != NULL && t->sort_level != 0u) {
    *level = t->sort_level - 1u;
    return 1;
  }
  while (t != NULL && t->kind == VAL_VAR && s != NULL && s->level != t->nat)
    s = s->next;
  if (t != NULL && t->kind == VAL_VAR && s != NULL && s->type != NULL && s->type->kind == VAL_UNIV) {
    *level = s->type->nat;
    return 1;
  }
  if (val_is(t, OP_FAMILY)) {
    *level = c->m->families[t->inst].universe;
    return 1;
  }
  if (val_is(t, OP_NAT) || val_is(t, OP_FLAG) || val_is(t, OP_FIELD) || val_is(t, OP_UNIT)) {
    *level = 0;
    return 1;
  }
  if (val_is(t, OP_OPTION) || val_is(t, OP_LIST) || val_is(t, OP_GLUE))
    return type_level(c, name, t->args[0], level);
  if (val_is(t, OP_PATHP))
    return type_level(c, name, closure_apply(c->m, t->args[0], val_ivl_end(c->m, 0)), level);
  if (val_is(t, OP_PROD) || val_is(t, OP_SUM)) {
    ok = type_level(c, name, t->args[0], &la) && type_level(c, name, t->args[1], &lb);
    *level = la > lb ? la : lb;
    return ok;
  }
  if (t != NULL && (t->kind == VAL_PI || t->kind == VAL_SIGMA) && type_level(c, name, t->dom, &la)) {
    if (!push(c, t->name, t->dom))
      return 0;
    ok = type_level(c, name, closure_apply(c->m, t, val_var(c->m, c->level - 1u)), &lb);
    pop(c);
    *level = la > lb ? la : lb;
    return ok;
  }
  return c->m->diag->set ? 0 : FAIL(c, "TYPE_INFER", "%s cannot find the universe of the type %s", name, show(c, t));
}

/* B when FT is a function type A -> B whose codomain does not use the
   binder, else NULL. B at two fresh variables must convert: value_bound
   counts each value of a closure environment (M3 s5), so a function type
   B such as Nat -> Nat would look dependent on the binder. */
static const Value *arrow_cod(Checker *c, const Value *ft) {
  const Value *b = ft == NULL || ft->kind != VAL_PI ? NULL : closure_apply(c->m, ft, val_var(c->m, c->level));
  const Value *b2 = b == NULL ? NULL : closure_apply(c->m, ft, val_var(c->m, c->level + 1u));
  return b2 != NULL && conv_values(c->m, c->level + 2u, b, b2) ? b : NULL;
}

/* isContr A : Type n, A : Type n. */
static const Core *rule_is_contr(Call *k) {
  uint64_t level = 0;
  const Core *a = check_type(k->c, k->args[0], &level);
  k->type = val_univ(k->c->m, level);
  return core_is_contr(k->c, a);
}

/* M4 s3 (D10): isProp A : Type n, A : Type n. */
static const Core *rule_is_prop(Call *k) {
  uint64_t level = 0;
  const Core *a = check_type(k->c, k->args[0], &level);
  k->type = val_univ(k->c->m, level);
  return core_is_prop(k->c, a);
}

/* f : A -> B of fiber and isEquiv. *A and *B get the core terms of A and
   B; the type of the result is the larger universe of A and B. */
static const Core *equiv_fn(Call *k, const Core **a, const Core **b) {
  Checker *c = k->c;
  const Value *ft = NULL;
  const Core *f = infer(c, k->args[0], &ft);
  const Value *cod = f == NULL ? NULL : arrow_cod(c, ft);
  uint64_t la = 0;
  uint64_t lb = 0;
  if (f != NULL && cod == NULL)
    return nul(FAIL(c, "TYPE_NOT_FUNCTION", "%s needs a function f : A -> B, found a term of type %s", k->b->name, show(c, ft)));
  if (cod == NULL || !type_level(c, k->b->name, ft->dom, &la) || !type_level(c, k->b->name, cod, &lb))
    return NULL;
  *a = quote_value(c->m, c->level, ft->dom);
  *b = quote_value(c->m, c->level, cod);
  k->type = val_univ(c->m, la > lb ? la : lb);
  return *a == NULL || *b == NULL ? NULL : f;
}

/* fiber f y : Type n, f : A -> B, y : B. */
static const Core *rule_fiber(Call *k) {
  const Core *a = NULL;
  const Core *b = NULL;
  const Core *f = equiv_fn(k, &a, &b);
  const Core *y = f == NULL ? NULL : check(k->c, k->args[1], here(k->c, b));
  return y == NULL ? NULL : core_fiber(k->c, a, b, f, y);
}

/* isEquiv f : Type n, f : A -> B. */
static const Core *rule_is_equiv(Call *k) {
  const Core *a = NULL;
  const Core *b = NULL;
  const Core *f = equiv_fn(k, &a, &b);
  return f == NULL ? NULL : core_is_equiv(k->c, a, b, f);
}

/* Equiv A B : Type n, A : Type n, B : Type m, n the larger level. */
static const Core *rule_equiv(Call *k) {
  uint64_t la = 0;
  uint64_t lb = 0;
  const Core *a = check_type(k->c, k->args[0], &la);
  const Core *b = a == NULL ? NULL : check_type(k->c, k->args[1], &lb);
  k->type = val_univ(k->c->m, la > lb ? la : lb);
  return b == NULL ? NULL : core_equiv(k->c, a, b);
}

/* idEquiv A : Equiv A A. */
static const Core *rule_id_equiv(Call *k) {
  uint64_t level = 0;
  const Core *a = check_type(k->c, k->args[0], &level);
  k->type = a == NULL ? NULL : here(k->c, core_equiv(k->c, a, a));
  return k->type == NULL ? NULL : core_id_equiv(k->c);
}

/* The join of the branch faces of the system T, a core interval term. */
static const Core *sys_face(Checker *c, const char *name, const Term *t) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Ivl *cover = ivl_const(&h, 0);
  const Term *b;
  if (t->kind != TERM_SYS)
    return nul(FAIL(c, "PARTIAL_EXPECTED", "%d:%d: %s needs a system [ phi -> t, ... ] here", t->line, t->col, name));
  for (b = t; b != NULL && cover != NULL; b = b->next) {
    const Value *fv = here(c, elab_ivl(c, b->left));
    const Ivl *f = fv == NULL ? NULL : face_norm(&h, fv->ivl);
    cover = f == NULL ? NULL : face_join(&h, cover, f);
  }
  return cover == NULL ? NULL : quote_value(c->m, c->level, val_ivl(c->m, cover));
}

/* Glue A [ phi -> pack T e, ... ] : Type n. A : Type n. The system checks
   against Partial psi (Sigma (T : Type n) (Equiv T A)), psi the join of its
   faces (D5). */
static const Core *rule_glue(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *a = check_type(c, k->args[0], &level);
  const Core *psi = a == NULL ? NULL : sys_face(c, "Glue", k->args[1]);
  const Core *fam = psi == NULL ? NULL : core_bind(c, CORE_SIGMA, "T", core_leaf(c, CORE_UNIV, level), core_equiv(c, core_index(c, CORE_VAR, 0), up(c, a)));
  const Value *want = fam == NULL ? NULL : tyop(c, OP_PARTIAL, 2, here(c, psi), here(c, fam), NULL);
  const Core *sys = want == NULL ? NULL : check(c, k->args[1], want);
  k->type = val_univ(c->m, level);
  return sys == NULL ? NULL : op3(c, OP_GLUE, 0, 3, a, psi, sys);
}

/* GLUE_BOUNDARY: on each conjunction of NORM, (witness e) t converts to a.
   pack T e is the branch of GSYS, the system of the Glue type; t is the
   branch of TSYS. */
static int glue_boundary(Checker *c, const Ivl *norm, const Core *gsys, const Core *tsys, const Core *a) {
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Core *f = op3(c, OP_WITNESS, 0, 1, op3(c, OP_PAYLOAD, 0, 1, gsys, NULL, NULL), NULL, NULL);
  uint32_t j;
  for (j = 0; f != NULL && j < norm->count; j++) {
    const Value *fv = eval_restricted(c, f, norm, j);
    const Value *tv = fv == NULL ? NULL : eval_restricted(c, tsys, norm, j);
    const Value *x = tv == NULL ? NULL : apply_value(c->m, fv, tv);
    const Value *y = x == NULL ? NULL : eval_restricted(c, a, norm, j);
    if (y == NULL)
      return 0;
    if (!conv_values(c->m, c->level, x, y))
      return c->m->diag->set ? 0 : FAIL(c, "GLUE_BOUNDARY", "glue needs (witness e) t to be a on %s, but (witness e) t is %s and a is %s", show(c, val_ivl(c->m, face_conj(&h, norm, j))), show(c, x), show(c, y));
  }
  return f != NULL;
}

/* glue [ phi -> t, ... ] a against Glue A [ psi -> pack T e, ... ]: the
   system checks against Partial psi T (FACE_COVER), a : A, GLUE_BOUNDARY. */
static const Core *rule_glue_elem(Call *k) {
  Checker *c = k->c;
  IvlHeap h = {c->m->arena, c->m->diag, c->m->def};
  const Value *g = k->hint;
  const Core *gsys;
  const Value *want;
  const Core *tsys;
  const Core *a;
  const Ivl *norm;
  const Value *st = NULL;
  if (!val_is(g, OP_GLUE))
    return nul(FAIL(c, "GLUE_EXPECTED", "glue needs a declared type Glue A [ ... ] here, found %s", g == NULL ? "no type" : show(c, g)));
  gsys = quote_value(c->m, c->level, g->args[2]);
  want = gsys == NULL ? NULL : tyop(c, OP_PARTIAL, 2, g->args[1], here(c, op3(c, OP_WITNESS, 0, 1, gsys, NULL, NULL)), NULL);
  tsys = want == NULL ? NULL : k->args[0]->kind == TERM_SYS
    ? check_sys(c, k->args[0], want, &st, 1) : check(c, k->args[0], want);
  a = tsys == NULL ? NULL : check(c, k->args[1], g->args[0]);
  norm = a == NULL ? NULL : face_norm(&h, g->args[1]->ivl);
  if (norm == NULL || !glue_boundary(c, norm, gsys, tsys, a))
    return NULL;
  k->type = g;
  return op3(c, OP_GLUE_ELEM, 0, 3, a, quote_value(c->m, c->level, g->args[1]), tsys);
}

/* unglue g : A, g : Glue A [ ... ]. The core copies the face and the system
   of the type of g (D5 of M2). */
static const Core *rule_unglue(Call *k) {
  Checker *c = k->c;
  const Value *gt = NULL;
  const Core *g = infer(c, k->args[0], &gt);
  if (g == NULL)
    return NULL;
  if (!val_is(gt, OP_GLUE))
    return nul(FAIL(c, "GLUE_EXPECTED", "unglue needs a term of a type Glue A [ ... ], found a term of type %s", show(c, gt)));
  k->type = gt->args[0];
  return op3(c, OP_UNGLUE, 0, 3, g, quote_value(c->m, c->level, gt->args[1]), quote_value(c->m, c->level, gt->args[2]));
}

/* ua e, e : Equiv A B, A and B in Type n, gives Path (Type n) A B. The core
   is <i> Glue B [ ~i -> pack A e, i -> pack B (idEquiv B) ]. *EP gets the
   core of e (uaBeta uses it). */
static const Core *ua_core(Call *k, const Core **ep) {
  Checker *c = k->c;
  const Value *et = NULL;
  const Core *e = *ep = infer(c, k->args[0], &et);
  const Value *fn = et == NULL || et->kind != VAL_SIGMA ? NULL : et->dom;
  const Value *b = arrow_cod(c, fn);
  const Core *ak = b == NULL ? NULL : quote_value(c->m, c->level, fn->dom);
  const Core *bk = ak == NULL ? NULL : quote_value(c->m, c->level, b);
  const Value *want = bk == NULL ? NULL : here(c, core_equiv(c, ak, bk));
  uint64_t la = 0;
  uint64_t lb = 0;
  const Core *sys[4];
  if (e == NULL || c->m->diag->set)
    return NULL;
  if (want == NULL || !conv_values(c->m, c->level, et, want))
    return nul(FAIL(c, "TYPE_MISMATCH", "ua needs an equivalence of type Equiv A B, found a term of type %s", show(c, et)));
  if (!type_level(c, "ua", fn->dom, &la) || !type_level(c, "ua", b, &lb))
    return NULL;
  if (la != lb)
    return nul(FAIL(c, "TYPE_MISMATCH", "ua needs A and B in one universe, found A in Type %llu and B in Type %llu", (unsigned long long)la, (unsigned long long)lb));
  k->type = tyop(c, OP_PATHP, 3, close_over(c, VAL_PLAM, "i", NULL, val_univ(c->m, la)), fn->dom, b);
  sys[0] = core_ivl(c, IVL_REV, core_index(c, CORE_VAR, 0), NULL);
  sys[1] = op3(c, OP_PACK, 0, 2, up(c, ak), up(c, e), NULL);
  sys[2] = core_index(c, CORE_VAR, 0);
  sys[3] = op3(c, OP_PACK, 0, 2, up(c, bk), core_id_equiv(c), NULL);
  return core_bind(c, CORE_PLAM, "i", NULL,
                   op3(c, OP_GLUE, 0, 3, up(c, bk), core_ivl(c, IVL_JOIN, sys[0], sys[2]), core_op(c, OP_SYS, 2u, 0, sys, 4u)));
}

static const Core *rule_ua(Call *k) {
  const Core *e = NULL;
  return ua_core(k, &e);
}

/* uaBeta e x, e : Equiv A B, x : A, gives Path B (transport (ua e) x)
   (first e x). The core is <i> transp (<_> B) i (first e x). The Glue Kan
   rules must make its end at i0 convert to transport (ua e) x: else
   PATH_BOUNDARY. */
static const Core *rule_ua_beta(Call *k) {
  Checker *c = k->c;
  const Core *e = NULL;
  const Core *u = ua_core(k, &e);
  const Value *ut = u == NULL ? NULL : k->type;
  const Core *x = ut == NULL ? NULL : check(c, k->args[1], ut->args[1]);
  const Core *fx = x == NULL ? NULL : core_app(c, op3(c, OP_WITNESS, 0, 1, e, NULL, NULL), x);
  const Value *start = fx == NULL ? NULL : here(c, transp_zero(c, core_bind(c, CORE_PLAM, "i", NULL, path_body(c, u, ut)), x));
  const Value *end = start == NULL ? NULL : here(c, transp_zero(c, core_bind(c, CORE_PLAM, "_", NULL, quote_value(c->m, c->level + 1u, ut->args[2])), fx));
  const Value *line = end == NULL ? NULL : close_over(c, VAL_PLAM, "i", NULL, ut->args[2]);
  if (line == NULL)
    return NULL;
  if (!conv_values(c->m, c->level, start, end))
    return nul(c->m->diag->set ? 0 : FAIL(c, "PATH_BOUNDARY", "uaBeta needs transport (ua e) x to be transp (<_> B) i0 (first e x), but it normalizes to %s and the other side is %s", show(c, start), show(c, end)));
  k->type = tyop(c, OP_PATHP, 3, line, start, here(c, fx));
  return core_bind(c, CORE_PLAM, "i", NULL,
                   op3(c, OP_TRANSP, 0, 3, core_bind(c, CORE_PLAM, "_", NULL, quote_value(c->m, c->level + 2u, ut->args[2])),
                       core_index(c, CORE_VAR, 0), up(c, fx)));
}

static const Core *elab_inner(Checker *c, const Term *t, const Value *hint, const Value **type) {
  switch (t->kind) {
  case TERM_VAR:
  case TERM_APP:
    return elab_spine(c, t, hint, type);
  case TERM_NAT:
    if (hint != NULL && val_is(hint, OP_FIELD))
      return field_literal(c, t, type);
    if (t->big)
      return nul(FAIL(c, "LEX_NAT_RANGE", "%d:%d: a Nat literal is larger than 2^64 - 1", t->line, t->col));
    *type = nat_type(c);
    return finish(c, core_leaf(c, CORE_NAT, t->nat), hint, type);
  case TERM_LAM:
    return hint != NULL ? check_lam(c, t, hint, type) : infer_lam(c, t, type);
  case TERM_PI:
    return elab_binder(c, t, CORE_PI, hint, type);
  case TERM_SIGMA:
    return elab_binder(c, t, CORE_SIGMA, hint, type);
  case TERM_UNIV:
    if (t->big)
      return nul(FAIL(c, "TYPE_UNIVERSE", "the universe level is above the largest universe Type %u", UNIVERSE_MAX - 1u));
    if (t->nat >= UNIVERSE_MAX)
      return nul(FAIL(c, "TYPE_UNIVERSE", "Type %llu is above the largest universe Type %u", (unsigned long long)t->nat, UNIVERSE_MAX - 1u));
    *type = val_univ(c->m, t->nat + 1u);
    return finish(c, core_leaf(c, CORE_UNIV, t->nat), hint, type);
  case TERM_ARITH:
    return elab_arith(c, t, hint, type);
  case TERM_PLAM:
    return check_plam(c, t, hint, type);
  case TERM_PAPP:
    return infer_papp(c, t, hint, type);
  case TERM_IVL:
    return nul(FAIL(c, "INTERVAL_TERM", "%d:%d: an interval term (~, /\\ or \\/) is not a term of a type", t->line, t->col));
  case TERM_SYS:
    return check_sys(c, t, hint, type, 0);
  }
  return NULL;
}

static const Core *elab(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *k;
  if (c->depth >= CHECK_DEPTH_LIMIT)
    return nul(FAIL(c, "CHECK_DEPTH", "the term is nested deeper than %u levels", CHECK_DEPTH_LIMIT));
  c->depth++;
  k = elab_inner(c, t, hint, type);
  /* Preserve a checked type's universe through evaluation and substitution.
     Reconstructing it from its normal form loses neutral type families. */
  if (k != NULL && *type != NULL && (*type)->kind == VAL_UNIV) {
    Core *typed = new_core(c, k->kind);
    if (typed == NULL)
      k = NULL;
    else {
      *typed = *k;
      typed->sort_level = (*type)->nat + 1u;
      k = typed;
    }
  }
  c->depth--;
  return k;
}

/* NULL when NAME is free, else the reason it is taken (rule R4). */
static const char *name_owner(const Checker *c, const char *name) {
  const Machine *m = c->m;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < sizeof RESERVED / sizeof RESERVED[0]; i++) {
    if (strcmp(RESERVED[i], name) == 0)
      return "a reserved name";
  }
  for (i = 0; i < BUILTIN_COUNT; i++) {
    if (strcmp(BUILTINS[i].name, name) == 0)
      return "a core name";
  }
  for (i = 0; i < m->family_count; i++) {
    if (strcmp(m->families[i].name, name) == 0)
      return "a domain family";
  }
  for (i = 0; i < m->ctor_count; i++) {
    for (j = 0; j < m->ctors[i].field_count; j++) {
      if (strcmp(m->ctors[i].fields[j].name, name) == 0)
        return "a domain field";
    }
    if (strcmp(m->ctors[i].name, name) == 0)
      return "a domain constructor";
  }
  for (i = 0; i < m->circuit_count; i++) {
    if (strcmp(m->circuits[i].name, name) == 0)
      return "already a circuit";
  }
  i = find_def(m, name);
  if (i == NO_DEF)
    return NULL;
  return m->defs[i].origin == ORIGIN_DOMAIN ? "a domain definition" : "already defined";
}

static int name_free(Checker *c, const char *name) {
  const char *why = name_owner(c, name);
  return why == NULL ? 1 : FAIL(c, "REFUSE_NAME", "%s is %s (rule R4)", name, why);
}

/* M4 s3: a recursive field is the family applied to its own parameters in
   order (the uniform occurrence; p = 0 is the family itself). Then the fold,
   transport and hcomp rules can treat the field type as RES or the family
   line. A parameter j is at the level j in the field scope, so its de Bruijn
   index is p - 1 - j. */
static int self_applied(const Core *t, uint32_t fam, uint32_t p) {
  uint32_t j;
  if (t->kind != CORE_OP || t->op != OP_FAMILY || t->inst != fam || t->argc != p)
    return 0;
  for (j = 0; j < p; j++)
    if (t->args[j]->kind != CORE_VAR || t->args[j]->index != p - 1u - j)
      return 0;
  return 1;
}

/* A recursive field must be the whole field type: the family at its own
   parameters. */
static int check_field(Checker *c, uint32_t fam, const Field *field, FieldInfo *out) {
  uint64_t level = 0;
  const Core *t;
  int whole;
  if (!name_free(c, field->name))
    return 0;
  c->self_uses = 0;
  t = check_type(c, field->type, &level);
  if (t == NULL)
    return 0;
  if (level > c->m->families[fam].universe)
    c->m->families[fam].universe = level;
  whole = self_applied(t, fam, c->m->families[fam].param_count);
  out->name = field->name;
  out->type = t;
  out->recursive = c->self_uses > 0u;
  if (out->recursive && !whole)
    return FAIL(c, "DOMAIN_FAMILY", "the recursive field %s must have the type %s applied to its own parameters", field->name, c->m->families[fam].name);
  return 1;
}

/* M4 s1: the boundary system of a path constructor, in the scope params,
   fields, binders (the order of the OP_PCON arguments). The binders are at
   the levels p + f .. p + f + k - 1. The system must cover the boundary of
   the cube: each face ~i_j and i_j is below the join of the branch faces.
   Lattice fact: (~i \/ i \/ ...) <= join iff each atom <= join, so the
   atom loop is the face_leq of the cube boundary and it names the
   uncovered face. Then check_sys checks the branches against Partial join
   Self: FACE_COVER holds by construction, TYPE_MISMATCH and FACE_AGREE
   come from check_sys. */
static const Core *boundary_core(Checker *c, uint32_t fam, const Ctor *ctor) {
  Machine *m = c->m;
  IvlHeap h = {m->arena, m->diag, m->def};
  uint32_t p = m->families[fam].param_count;
  uint32_t f = (uint32_t)ctor->field_count;
  uint32_t k = (uint32_t)ctor->binder_count;
  const Value **pvals = arena_alloc(m->arena, ((size_t)p + 1u) * sizeof *pvals);
  const Value *selfv;
  const Value *hint;
  const Value *type;
  const Ivl *join = ivl_const(&h, 0);
  const Term *b;
  uint32_t j;
  int e;
  if (pvals == NULL)
    return nul(FAIL(c, "OOM", "out of memory"));
  for (j = 0; j < p; j++)
    pvals[j] = val_var(m, j);
  selfv = val_op(m, VAL_OP, OP_FAMILY, fam, 0, pvals, p);
  for (b = ctor->boundary; b != NULL && join != NULL; b = b->next) {
    const Value *fv = here(c, elab_ivl(c, b->left));
    const Ivl *bf = fv == NULL ? NULL : face_norm(&h, fv->ivl);
    join = bf == NULL ? NULL : face_join(&h, join, bf);
  }
  if (join == NULL || selfv == NULL)
    return NULL;
  for (j = 0; j < k; j++) {
    for (e = 0; e < 2; e++) {
      const Ivl *atom = face_norm(&h, ivl_lit(&h, p + f + j, e == 0));
      if (atom == NULL)
        return NULL;
      if (face_leq(&h, atom, join) != 1)
        return nul(FAIL(c, "HIT_BOUNDARY", "the path constructor %s does not cover the face %s of its binder %s: the boundary joins to %s",
                        ctor->name, show(c, val_ivl(m, atom)), ctor->binders[j], show(c, val_ivl(m, join))));
    }
  }
  hint = val_make(m, OP_PARTIAL, 0, 2u, val_ivl(m, join), selfv, NULL);
  return hint == NULL ? NULL : check_sys(c, ctor->boundary, hint, &type, 0);
}

/* M4 s1: push the fields and the interval binders of a path constructor
   above the family parameters, check its boundary, and pop them. */
static int check_boundary(Checker *c, uint32_t fam, const Ctor *ctor, const FieldInfo *fields, const Core **out) {
  Machine *m = c->m;
  const Env *envp = c->env;
  size_t want = ctor->field_count + ctor->binder_count;
  size_t pushed = 0;
  size_t j;
  for (j = 0; j < ctor->field_count && pushed == j; j++)
    pushed += push(c, fields[j].name, eval_core(m, envp, fields[j].type)) ? 1u : 0u;
  for (j = 0; j < ctor->binder_count && pushed == ctor->field_count + j; j++)
    pushed += push_ivl(c, ctor->binders[j]) ? 1u : 0u;
  *out = pushed == want ? boundary_core(c, fam, ctor) : NULL;
  for (j = 0; j < pushed; j++)
    pop(c);
  return *out != NULL;
}

static int check_ctor(Checker *c, uint32_t fam, const Ctor *ctor) {
  Machine *m = c->m;
  FieldInfo *fields = arena_alloc(m->arena, (ctor->field_count + 1u) * sizeof *fields);
  CtorInfo *ci = &m->ctors[m->ctor_count];
  const Core *boundary = NULL;
  size_t j;
  if (fields == NULL)
    return FAIL(c, "OOM", "out of memory");
  if (!name_free(c, ctor->name))
    return 0;
  for (j = 0; j < ctor->field_count; j++) {
    size_t earlier;
    const char *name = ctor->fields[j].name;
    if (strcmp(name, ctor->name) == 0)
      return FAIL(c, "REFUSE_NAME", "%s is the constructor name (rule R4)", name);
    for (earlier = 0; earlier < j; earlier++)
      if (strcmp(name, ctor->fields[earlier].name) == 0)
        return FAIL(c, "REFUSE_NAME", "%s is already a field of this constructor (rule R4)", name);
    if (m->families[fam].ctor_count > 1u && strcmp(name, "tag") == 0)
      return FAIL(c, "DOMAIN_FAMILY", "tag is reserved for the JSON constructor tag in a family with multiple constructors");
    if (!check_field(c, fam, &ctor->fields[j], &fields[j]))
      return 0;
  }
  if (ctor->binder_count > 0u && !check_boundary(c, fam, ctor, fields, &boundary))
    return 0;
  ci->name = ctor->name;
  ci->family = fam;
  ci->fields = fields;
  ci->field_count = (uint32_t)ctor->field_count;
  ci->binder_count = (uint32_t)ctor->binder_count;
  ci->binders = ctor->binders;
  ci->boundary = boundary;
  m->ctor_count++;
  return 1;
}

/* A family is in scope in its own fields, so a field can be recursive. */
static int check_family(Checker *c, const Decl *d) {
  Machine *m = c->m;
  uint32_t fam = m->family_count;
  FamilyInfo *f = &m->families[fam];
  const Core **params = arena_alloc(m->arena, (d->param_count + 1u) * sizeof *params);
  size_t i;
  if (params == NULL)
    return FAIL(c, "OOM", "out of memory");
  for (i = 0; i < d->param_count; i++) {
    uint64_t level = 0;
    params[i] = check_type(c, d->params[i].type, &level);
    if (!push(c, d->params[i].name, here(c, params[i])))
      return 0;
  }
  f->name = d->name;
  f->param_types = params;
  f->param_count = (uint32_t)d->param_count;
  f->first_ctor = m->ctor_count;
  f->ctor_count = (uint32_t)d->ctor_count;
  f->path_ctors = 0;
  /* Classify the whole family before checking any constructor, so its
     boundaries cannot call the plain family fold while it is in scope. */
  for (i = 0; i < d->ctor_count; i++)
    f->path_ctors += d->ctors[i].binder_count > 0u ? 1u : 0u;
  m->family_count++;
  c->self = fam;
  for (i = 0; i < d->ctor_count; i++) {
    if (!check_ctor(c, fam, &d->ctors[i]))
      return 0;
  }
  return 1;
}

static int check_def(Checker *c, const Decl *d) {
  Machine *m = c->m;
  uint64_t level = 0;
  int sset = 0;
  const Core *t = check_sort(c, d->type, &level, &sset);
  const Value *tv = here(c, t);
  const Core *body = check(c, d->body, tv);
  DefInfo *info = &m->defs[m->def_count];
  if (body == NULL)
    return 0;
  info->name = d->name;
  info->origin = d->origin;
  info->body = body;
  info->type = tv;
  info->value = NULL;
  m->def_count++;
  return 1;
}

/* glue-lang: each binder has type Field. Binder number *AT goes to level *AT. */
static int circuit_binders(Checker *c, const Field *fs, size_t n, const char **names, uint32_t *at) {
  size_t i;
  for (i = 0; i < n; i++) {
    uint64_t level = 0;
    const Core *t = check_type(c, fs[i].type, &level);
    const Value *tv = t == NULL ? NULL : here(c, t);
    if (tv == NULL)
      return 0;
    if (!val_is(tv, OP_FIELD))
      return FAIL(c, "CIRCUIT_BINDER", "%d:%d: the circuit binder %s has type %s; M1 allows Field only", fs[i].line, fs[i].col, fs[i].name, show(c, tv));
    if (!push(c, fs[i].name, tv))
      return 0;
    names[(*at)++] = fs[i].name;
  }
  return 1;
}

/* M2 ruling b: a == b is the type Path Field a b. */
static const Core *field_path(Checker *c, const Core *a, const Core *b) {
  const Core *line = core_bind(c, CORE_PLAM, "i", NULL, op3(c, OP_FIELD, 0, 0, NULL, NULL, NULL));
  return a == NULL || b == NULL ? NULL : op3(c, OP_PATHP, 0, 3, line, a, b);
}

static int check_circuit(Checker *c, const Decl *d) {
  Machine *m = c->m;
  CircuitInfo *info = &m->circuits[m->circuit_count];
  uint32_t n = (uint32_t)(d->param_count + d->private_count);
  const char **names = arena_alloc(m->arena, (n + 1u) * sizeof *names);
  ConstraintInfo *ks = arena_alloc(m->arena, (d->constraint_count + 1u) * sizeof *ks);
  uint32_t at = 0;
  size_t i;
  for (i = 0; i < m->circuit_count; i++)
    if (strcmp(m->circuits[i].name, d->name) == 0)
      return FAIL(c, "REFUSE_NAME", "%s is a circuit already", d->name);
  if (names == NULL || ks == NULL)
    return FAIL(c, "OOM", "out of memory");
  if (!circuit_binders(c, d->params, d->param_count, names, &at) || !circuit_binders(c, d->privates, d->private_count, names, &at))
    return 0;
  for (i = 0; i < d->constraint_count; i++) {
    const Constraint *k = &d->constraints[i];
    int eq = k->kind == CONSTRAINT_EQ;
    const Core *lhs = field_term(c, k->lhs, eq ? "EQ_FIELD" : "RANGE8_FIELD", eq ? "== needs Field sides" : "range8 needs a Field operand");
    const Core *rhs = eq && lhs != NULL ? field_term(c, k->rhs, "EQ_FIELD", "== needs Field sides") : NULL;
    ks[i].kind = k->kind;
    ks[i].line = k->line;
    ks[i].col = k->col;
    ks[i].term = eq ? field_path(c, lhs, rhs) : lhs;
    if (ks[i].term == NULL)
      return 0;
  }
  info->name = d->name;
  info->binders = names;
  info->public_count = (uint32_t)d->param_count;
  info->private_count = (uint32_t)d->private_count;
  info->constraints = ks;
  info->constraint_count = (uint32_t)d->constraint_count;
  m->circuit_count++;
  return 1;
}

static int check_decl(Checker *c, const Decl *d) {
  /* M4 s1: a program family is checked first, so a malformed one gets its
     own code (HIT_BOUNDARY, FACE_AGREE, ...). A well formed one is still
     refused: rule R1 accepts no program family. */
  if (d->kind == DECL_FAMILY && d->origin == ORIGIN_PROGRAM)
    return check_family(c, d) ? FAIL(c, "REFUSE_DATA", "a program cannot declare a family (rule R1); only domain/domain.lang can") : 0;
  if (d->kind == DECL_AXIOM)
    return FAIL(c, "REFUSE_AXIOM", "an axiom has no proof (rule R2)");
  if (d->is_rec)
    return FAIL(c, "REFUSE_REC", "def rec is refused (rule R3); use fold or unfold");
  if (!name_free(c, d->name))
    return 0;
  if (d->kind == DECL_CIRCUIT)
    return check_circuit(c, d);
  return d->kind == DECL_FAMILY ? check_family(c, d) : check_def(c, d);
}

int check_program(Arena *arena, const DeclList *decls, Machine *m, Diag *diag) {
  Checker c;
  size_t ctors = 0;
  size_t i;
  memset(m, 0, sizeof *m);
  m->arena = arena;
  m->diag = diag;
  m->fuel = EVAL_FUEL_STEPS;
  for (i = 0; i < decls->count; i++)
    ctors += decls->items[i].ctor_count;
  m->defs = arena_alloc(arena, (decls->count + 1u) * sizeof *m->defs);
  m->families = arena_alloc(arena, (decls->count + 1u) * sizeof *m->families);
  m->ctors = arena_alloc(arena, (ctors + 1u) * sizeof *m->ctors);
  m->circuits = arena_alloc(arena, (decls->count + 1u) * sizeof *m->circuits);
  if (m->defs == NULL || m->families == NULL || m->ctors == NULL || m->circuits == NULL)
    return diag_fail(diag, "OOM", NULL, "out of memory");
  m->def_cap = (uint32_t)decls->count;
  m->family_cap = (uint32_t)decls->count;
  m->ctor_cap = (uint32_t)ctors;
  m->circuit_cap = (uint32_t)decls->count;
  memset(&c, 0, sizeof c);
  c.m = m;
  c.decls = decls;
  for (i = 0; i < decls->count; i++) {
    c.decl_index = i;
    c.scope = NULL;
    c.env = NULL;
    c.level = 0;
    c.depth = 0;
    c.self = NO_FAMILY;
    m->def = decls->items[i].name;
    if (!check_decl(&c, &decls->items[i]))
      return 0;
  }
  m->def = NULL;
  return 1;
}

int entry_of(Machine *m, const Value *type, Entry *out) {
  const Value *t = type;
  memset(out, 0, sizeof *out);
  while (t != NULL && t->kind == VAL_PI && out->param_count < ENTRY_PARAMS_MAX) {
    if (!val_is(t->dom, OP_NAT) && !val_is(t->dom, OP_FLAG))
      return 0;
    out->param_flag[out->param_count] = val_is(t->dom, OP_FLAG);
    t = closure_apply(m, t, val_var(m, out->param_count));
    out->param_count++;
  }
  out->result_flag = val_is(t, OP_FLAG);
  return val_is(t, OP_NAT) || val_is(t, OP_FLAG) || val_is(t, OP_FIELD);
}

static int parse_u64(const char *s, uint64_t *out) {
  uint64_t n = 0;
  size_t i;
  if (s[0] == '\0')
    return 0;
  for (i = 0; s[i] != '\0'; i++) {
    unsigned d = (unsigned)(unsigned char)s[i] - (unsigned)'0';
    if (d > 9u || n > (UINT64_MAX - d) / 10u)
      return 0;
    n = n * 10u + d;
  }
  *out = n;
  return 1;
}

/* An entry argument: a Nat in decimal, or a Flag as 0 or 1. */
static const Value *entry_arg(Machine *m, const char *text, int is_flag) {
  uint64_t n = 0;
  if (!parse_u64(text, &n) || (is_flag && n > 1u)) {
    diag_fail(m->diag, "EVAL_ARGS", m->def, "'%s' is not a %s", text, is_flag ? "Flag (0 or 1)" : "Nat (0 to 2^64-1)");
    return NULL;
  }
  return is_flag ? val_make(m, n == 1u ? OP_FLAG_YES : OP_FLAG_NO, 0, 0, NULL, NULL, NULL) : val_nat(m, n);
}

static int print_result(Machine *m, const char *name, const Value *v, FILE *out, FILE *err) {
  switch (v->kind) {
  case VAL_NAT:
    fprintf(out, "%llu\n", (unsigned long long)v->nat);
    return 0;
  case VAL_TRAP:
    fputs("trap\n", out);
    fprintf(err, "langc: EVAL_OVERFLOW: %s: a Nat operation overflowed past 2^64-1\n", name);
    return 0;
  case VAL_OP:
    if (val_is(v, OP_FLAG_YES) || val_is(v, OP_FLAG_NO)) {
      fprintf(out, "%d\n", val_is(v, OP_FLAG_YES));
      return 0;
    }
    if (val_is(v, OP_FIELD_LIT)) {
      fprintf(out, "%s\n", v->name);
      return 0;
    }
    break;
  case VAL_UNIV:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_IVL:
  case VAL_PLAM:
  case VAL_PAPP:
    break;
  }
  diag_fail(m->diag, "EVAL_ENTRY", name, "the entry did not compute a number");
  return 1;
}

int eval_command(Machine *m, const char *name, char *const *args, int arg_count, FILE *out, FILE *err) {
  uint32_t d = find_def(m, name);
  Entry entry;
  const Value *v;
  char *buf;
  uint32_t i;
  m->def = name;
  if (d == NO_DEF) {
    diag_fail(m->diag, "EVAL_ENTRY", name, "no definition has this name");
    return 1;
  }
  v = def_value(m, d);
  if (v == NULL)
    return 1;
  if (!entry_of(m, m->defs[d].type, &entry) && arg_count != 0) {
    diag_fail(m->diag, "EVAL_ARGS", name, "the definition is not an entry, so it takes no arguments");
    return 2;
  }
  if (!entry_of(m, m->defs[d].type, &entry)) {
    buf = arena_alloc(m->arena, PRINT_MAX);
    if (buf == NULL)
      return diag_fail(m->diag, "OOM", name, "out of memory") + 1;
    if (!value_print(m, NULL, 0, v, buf, PRINT_MAX)) {
      diag_fail(m->diag, "EVAL_PRINT", name, "the normal form exceeds the printer limits");
      return 1;
    }
    fprintf(out, "%s\n", buf);
    return 0;
  }
  if ((uint32_t)arg_count != entry.param_count) {
    diag_fail(m->diag, "EVAL_ARGS", name, "the entry takes %u arguments, found %d", entry.param_count, arg_count);
    return 2;
  }
  for (i = 0; i < entry.param_count && v != NULL; i++) {
    const Value *arg = entry_arg(m, args[i], entry.param_flag[i]);
    v = arg == NULL ? NULL : apply_value(m, v, arg);
  }
  if (v == NULL)
    return strcmp(m->diag->code != NULL ? m->diag->code : "", "EVAL_ARGS") == 0 ? 2 : 1;
  return print_result(m, name, v, out, err);
}
