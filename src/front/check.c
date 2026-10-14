/* The type checker (see front/check.h). SHARED by the tcc kits.

   Checking is bidirectional. A term is checked against an expected type (the
   hint) or its type is inferred. Two types are equal when their normal forms
   are equal (front/eval.c). The checker writes core terms (front/core.h). */
#include "front/check.h"

#include <string.h>

#include "front/fr.h"

#define CHECK_DEPTH_LIMIT 2000u
#define SPINE_MAX 64u
#define SHOW_MAX 512u
#define NAMES_MAX 64u
#define UNIVERSE_MAX 2u
#define PRINT_MAX 65536u
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
  RULE_EQ,
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
  RULE_SYMM,
  RULE_TRANS,
  RULE_CONG,
  RULE_TRANSPORT,
  RULE_NAT2,
  RULE_FLAG_IF,
  RULE_INTERVAL,
  RULE_PATH,
  RULE_PATHP,
  RULE_SYM,
  RULE_FUNEXT,
  RULE_HAPPLY
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
  {"Eq", 3, RULE_EQ, OP_EQ, OP_NAT, 0},
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
  {"refl", 0, RULE_REFL, OP_REFL, OP_EQ, 0},
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
  {"symm", 1, RULE_SYMM, OP_SYMM, OP_EQ, 0},
  {"trans", 2, RULE_TRANS, OP_TRANS, OP_EQ, 0},
  {"cong", 2, RULE_CONG, OP_CONG, OP_EQ, 0},
  {"transport", 3, RULE_TRANSPORT, OP_TRANSPORT, OP_EQ, 0},
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
  {"happly", 2, RULE_HAPPLY, OP_PATHP, OP_NAT, 0}
};

#define BUILTIN_COUNT ((uint32_t)(sizeof BUILTINS / sizeof BUILTINS[0]))

/* Names that no declaration can take (rule R4). */
static const char *const RESERVED[] = {"Bool", "Text", "transp", "hcomp", "Glue", "ua", "Partial", "SSet"};

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

static const Core *check_type(Checker *c, const Term *t, uint64_t *level) {
  const Value *type = NULL;
  const Core *k = infer(c, t, &type);
  if (k == NULL)
    return NULL;
  if (type == NULL || type->kind != VAL_UNIV)
    return nul(FAIL(c, "TYPE_MISMATCH", "expected a type, found a term of type %s", show(c, type)));
  *level = type->nat;
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

static const Core *infer_eq(Call *k, uint32_t i, const Value **type) {
  const Core *e = infer(k->c, k->args[i], type);
  if (e == NULL)
    return NULL;
  return val_is(*type, OP_EQ) ? e : nul(FAIL(k->c, "TYPE_MISMATCH", "%s needs a proof of type Eq A a b, found a term of type %s", k->b->name, show(k->c, *type)));
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

static const Core *rule_eq(Call *k) {
  Checker *c = k->c;
  uint64_t level = 0;
  const Core *t = check_type(c, k->args[0], &level);
  const Value *tv = here(c, t);
  const Core *a = check(c, k->args[1], tv);
  const Core *b = a == NULL ? NULL : check(c, k->args[2], tv);
  k->type = val_univ(c->m, 0);
  return op3(c, OP_EQ, 0, 3, t, a, b);
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
static const Core *refl_path(Call *k);
static const Core *cong_path(Call *k, const Core *p, const Value *pt);

static const Core *rule_refl(Call *k) {
  Checker *c = k->c;
  const Value *e = k->hint;
  if (val_is(e, OP_PATHP))
    return refl_path(k);
  if (!val_is(e, OP_EQ))
    return need_type(k, "Eq A a b or Path A a b");
  if (!conv_values(c->m, c->level, e->args[1], e->args[2]))
    return nul(FAIL(c, "TYPE_REFL", "refl cannot prove the equality: the left side normalizes to %s, the right side normalizes to %s", show(c, e->args[1]), show(c, e->args[2])));
  k->type = e;
  return op3(c, OP_REFL, 0, 0, NULL, NULL, NULL);
}

static const Core *rule_part(Call *k) {
  Checker *c = k->c;
  const Value *pt = NULL;
  const Core *p = infer(c, k->args[0], &pt);
  if (p == NULL)
    return NULL;
  if (!val_is(pt, OP_PROD))
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a pair, found a term of type %s", k->b->name, show(c, pt)));
  k->type = pt->args[k->b->part];
  return op3(c, k->b->op, 0, 1, p, NULL, NULL);
}

static const Core *rule_sigma_part(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = infer(c, k->args[0], &st);
  const Core *w;
  if (s == NULL)
    return NULL;
  if (st == NULL || st->kind != VAL_SIGMA)
    return nul(FAIL(c, "TYPE_MISMATCH", "%s needs a Sigma value, found a term of type %s", k->b->name, show(c, st)));
  w = op3(c, OP_WITNESS, 0, 1, s, NULL, NULL);
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

static int simple_scrutinee(const Value *t) {
  return val_is(t, OP_NAT) || val_is(t, OP_LIST);
}

/* fold takes its scrutinee last, so the scrutinee type sets the arity. */
static const Core *rule_fold(Call *k) {
  Checker *c = k->c;
  const Value *st = NULL;
  const Core *s = try_infer(c, k->args[k->argc - 1u], &st);
  uint32_t fam = val_is(st, OP_FAMILY) ? st->inst : NO_FAMILY;
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

static const Core *rule_symm(Call *k) {
  const Value *et = NULL;
  const Core *e = infer_eq(k, 0, &et);
  if (e == NULL)
    return NULL;
  k->type = tyop(k->c, OP_EQ, 3, et->args[0], et->args[2], et->args[1]);
  return op3(k->c, OP_SYMM, 0, 1, e, NULL, NULL);
}

static const Core *rule_trans(Call *k) {
  Checker *c = k->c;
  const Value *t1 = NULL;
  const Value *t2 = NULL;
  const Core *e1 = infer_eq(k, 0, &t1);
  const Core *e2 = e1 == NULL ? NULL : infer_eq(k, 1, &t2);
  if (e2 == NULL)
    return NULL;
  if (!conv_values(c->m, c->level, t1->args[0], t2->args[0]) || !conv_values(c->m, c->level, t1->args[2], t2->args[1]))
    return nul(FAIL(c, "TYPE_MISMATCH", "trans needs the right side of the first proof, %s, to equal the left side of the second, %s", show(c, t1->args[2]), show(c, t2->args[1])));
  k->type = tyop(c, OP_EQ, 3, t1->args[0], t1->args[1], t2->args[2]);
  return op3(c, OP_TRANS, 0, 2, e1, e2, NULL);
}

/* cong f e */
static const Core *rule_cong(Call *k) {
  Checker *c = k->c;
  const Value *et = NULL;
  const Value *bt = NULL;
  const Value *fv;
  const Value *hint_type = val_is(k->hint, OP_EQ) ? k->hint->args[0] : NULL;
  const Core *e = infer(c, k->args[1], &et);
  const Core *f;
  if (e != NULL && val_is(et, OP_PATHP))
    return cong_path(k, e, et);
  if (e != NULL && !val_is(et, OP_EQ))
    return nul(FAIL(c, "TYPE_MISMATCH", "cong needs a proof of type Eq A a b or a path of type Path A a b, found a term of type %s", show(c, et)));
  f = e == NULL ? NULL : fn_into(c, k->args[0], et->args[0], hint_type, &bt);
  if (f == NULL)
    return NULL;
  fv = here(c, f);
  k->type = tyop(c, OP_EQ, 3, bt, apply_value(c->m, fv, et->args[1]), apply_value(c->m, fv, et->args[2]));
  return op3(c, OP_CONG, 0, 2, f, e, NULL);
}

/* transport P e u */
static const Core *rule_transport(Call *k) {
  Checker *c = k->c;
  const Value *et = NULL;
  const Value *pt = NULL;
  const Value *cod;
  const Value *pv;
  const Core *e = infer_eq(k, 1, &et);
  const Core *p = e == NULL ? NULL : infer(c, k->args[0], &pt);
  const Core *u;
  if (p == NULL)
    return NULL;
  cod = pt != NULL && pt->kind == VAL_PI ? closure_apply(c->m, pt, val_var(c->m, c->level)) : NULL;
  if (cod == NULL || cod->kind != VAL_UNIV || !conv_values(c->m, c->level, pt->dom, et->args[0]))
    return nul(FAIL(c, "TYPE_MISMATCH", "transport needs a family of types over %s, found a term of type %s", show(c, et->args[0]), show(c, pt)));
  pv = here(c, p);
  u = check(c, k->args[2], apply_value(c->m, pv, et->args[1]));
  k->type = apply_value(c->m, pv, et->args[2]);
  return op3(c, OP_TRANSPORT, 0, 3, p, e, u);
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
    return k->index < cut ? k : core_index(c, CORE_VAR, k->index + by);
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

/* The core term P @ R, P : PT = PathP L a b, in a context of LEVEL binders.
   The core keeps a and b, so the evaluator can reduce a neutral P at i0 and
   i1 (D5). */
static const Core *core_papp(Checker *c, uint32_t level, const Core *p, const Value *pt, const Core *r) {
  const Core **ends;
  Core *k;
  if (p == NULL || r == NULL)
    return NULL;
  ends = arena_alloc(c->m->arena, 2u * sizeof *ends);
  k = ends == NULL ? NULL : new_core(c, CORE_PAPP);
  if (k == NULL)
    return c->m->diag->set ? NULL : nul(FAIL(c, "OOM", "out of memory"));
  ends[0] = quote_value(c->m, level, pt->args[1]);
  ends[1] = quote_value(c->m, level, pt->args[2]);
  if (ends[0] == NULL || ends[1] == NULL)
    return NULL;
  k->a = p;
  k->b = r;
  k->args = ends;
  k->argc = 2u;
  return k;
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
  case RULE_EQ:
    return rule_eq(k);
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
  case RULE_SYMM:
    return rule_symm(k);
  case RULE_TRANS:
    return rule_trans(k);
  case RULE_CONG:
    return rule_cong(k);
  case RULE_TRANSPORT:
    return rule_transport(k);
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
  k->type = val_op(c->m, VAL_OP, OP_FAMILY, ci->family, 0, vals, p);
  return core_op(c, OP_CTOR, ctor, 0, cores, n);
}

/* A field of a one-constructor family is a projection. */
static const Core *proj_app(Call *k, uint32_t ctor, uint32_t field) {
  Checker *c = k->c;
  const CtorInfo *ci = &c->m->ctors[ctor];
  const Value *xt = NULL;
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
    a = check(c, args[i], ty->dom);
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
  const Value *v = here(c, check_type(c, ann, &level));
  if (v == NULL)
    return 0;
  return conv_values(c->m, c->level, v, dom) ? 1 : FAIL(c, "TYPE_MISMATCH", "the binder type %s differs from the expected type %s", show(c, v), show(c, dom));
}

static const Core *check_lam(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *body;
  if (hint->kind != VAL_PI)
    return nul(FAIL(c, "TYPE_MISMATCH", "a function is not a term of type %s", show(c, hint)));
  if (t->left != NULL && !annotation_ok(c, t->left, hint->dom))
    return NULL;
  if (!push(c, t->name, hint->dom))
    return NULL;
  body = check(c, t->right, closure_apply(c->m, hint, c->env->value));
  pop(c);
  *type = hint;
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

static const Core *infer_lam(Checker *c, const Term *t, const Value **type) {
  uint64_t level = 0;
  const Value *dom;
  const Value *bt = NULL;
  const Core *body;
  const Core *quoted;
  if (t->left == NULL)
    return nul(FAIL(c, "TYPE_INFER", "fun %s => ... needs a binder type here", t->name));
  dom = here(c, check_type(c, t->left, &level));
  if (!push(c, t->name, dom))
    return NULL;
  body = infer(c, t->right, &bt);
  quoted = body == NULL ? NULL : quote_value(c->m, c->level, bt);
  pop(c);
  *type = val_closure(c->m, VAL_PI, t->name, dom, c->env, quoted);
  return core_bind(c, CORE_LAM, t->name, NULL, body);
}

/* (x : A) -> B and Sigma (x : A) B. */
static const Core *elab_binder(Checker *c, const Term *t, CoreKind kind, const Value *hint, const Value **type) {
  uint64_t la = 0;
  uint64_t lb = 0;
  const Core *a = check_type(c, t->left, &la);
  const Core *b;
  if (a == NULL || !push(c, t->name, here(c, a)))
    return NULL;
  b = check_type(c, t->right, &lb);
  pop(c);
  *type = val_univ(c->m, la > lb ? la : lb);
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
  }
  return NULL;
}

static const Core *elab(Checker *c, const Term *t, const Value *hint, const Value **type) {
  const Core *k;
  if (c->depth >= CHECK_DEPTH_LIMIT)
    return nul(FAIL(c, "CHECK_DEPTH", "the term is nested deeper than %u levels", CHECK_DEPTH_LIMIT));
  c->depth++;
  k = elab_inner(c, t, hint, type);
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

/* A recursive field must be the whole field type, in a family with no
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
  whole = t->kind == CORE_OP && t->op == OP_FAMILY && t->inst == fam && c->m->families[fam].param_count == 0u;
  out->name = field->name;
  out->type = t;
  out->recursive = c->self_uses > 0u;
  if (out->recursive && !whole)
    return FAIL(c, "DOMAIN_FAMILY", "the recursive field %s must have the type %s itself, in a family with no parameters", field->name, c->m->families[fam].name);
  return 1;
}

static int check_ctor(Checker *c, uint32_t fam, const Ctor *ctor) {
  Machine *m = c->m;
  FieldInfo *fields = arena_alloc(m->arena, (ctor->field_count + 1u) * sizeof *fields);
  CtorInfo *ci = &m->ctors[m->ctor_count];
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
  ci->name = ctor->name;
  ci->family = fam;
  ci->fields = fields;
  ci->field_count = (uint32_t)ctor->field_count;
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
  const Core *t = check_type(c, d->type, &level);
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
    ks[i].kind = k->kind;
    ks[i].line = k->line;
    ks[i].col = k->col;
    ks[i].lhs = field_term(c, k->lhs, eq ? "EQ_FIELD" : "RANGE8_FIELD", eq ? "== needs Field sides" : "range8 needs a Field operand");
    ks[i].rhs = eq && ks[i].lhs != NULL ? field_term(c, k->rhs, "EQ_FIELD", "== needs Field sides") : NULL;
    if (ks[i].lhs == NULL || (eq && ks[i].rhs == NULL))
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
  if (d->kind == DECL_FAMILY && d->origin == ORIGIN_PROGRAM)
    return FAIL(c, "REFUSE_DATA", "a program cannot declare a family (rule R1); only domain/domain.lang can");
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
