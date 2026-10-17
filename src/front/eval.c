/* The evaluator, the conversion test, quote and the printer (see
   front/core.h). SHARED by the tcc kits. */
#include "front/core.h"
#include "front/face.h"
#include "front/fr.h"
#include "front/kan.h"

#include <string.h>

#define PRINT_DEPTH_MAX 200u
#define PRINT_NAMES_MAX PRINT_DEPTH_MAX
#define VARIES 0xffu

static const char *const OP_NAMES[] = {
  [OP_NAT] = "Nat", [OP_FLAG] = "Flag", [OP_UNIT] = "Unit", [OP_PROD] = "Prod",
  [OP_SUM] = "Sum", [OP_OPTION] = "Option", [OP_LIST] = "List",
  [OP_FAMILY] = "?", [OP_UNIT_VAL] = "unit", [OP_FLAG_YES] = "flagYes",
  [OP_FLAG_NO] = "flagNo", [OP_PAIR] = "pair", [OP_INL] = "inl", [OP_INR] = "inr",
  [OP_NONE] = "none", [OP_SOME] = "some", [OP_NIL] = "nil", [OP_CONS] = "cons",
  [OP_PACK] = "pack", [OP_CTOR] = "?", [OP_PCON] = "?", [OP_FIRST] = "first",
  [OP_SECOND] = "second", [OP_EITHER] = "either", [OP_OPTION_ELIM] = "option",
  [OP_PURE] = "pure", [OP_MAP] = "map", [OP_BIND] = "bind", [OP_APPEND] = "append", [OP_FOLD_NAT] = "fold",
  [OP_FOLD_LIST] = "fold", [OP_FOLD_FAMILY] = "fold", [OP_FOLD_HIT] = "fold", [OP_FOLD_BLOCK] = "fold", [OP_ELIM] = "elim", [OP_ELIM_CORE] = "elim", [OP_ELIM_NAT] = "elim", [OP_UNFOLD] = "unfold", [OP_UNFOLD_NAT] = "unfold",
  [OP_FILTER] = "filter", [OP_WITNESS] = "witness", [OP_PAYLOAD] = "payload",
  [OP_NAT_ADD] = "natAdd", [OP_NAT_SUB] = "natSub",
  [OP_NAT_MUL] = "natMul", [OP_NAT_EQ] = "natEq", [OP_NAT_LE] = "natLe",
  [OP_FIELD] = "Field", [OP_FIELD_LIT] = "fieldLiteral", [OP_FIELD_ADD] = "fieldAdd",
  [OP_FIELD_SUB] = "fieldSub", [OP_FIELD_MUL] = "fieldMul",
  [OP_FLAG_IF] = "flagIf", [OP_PROJ] = "?", [OP_PATHP] = "PathP",
  [OP_INTERVAL] = "I", [OP_SSET] = "SSet", [OP_PARTIAL] = "Partial", [OP_SYS] = "?",
  [OP_TRANSP] = "transp", [OP_KAN_DOM] = "?", [OP_KAN_COD] = "?",
  [OP_HCOMP] = "hcomp", [OP_KAN_FIELD] = "?", [OP_GLUE] = "Glue",
  [OP_GLUE_ELEM] = "glue", [OP_UNGLUE] = "unglue", [OP_KAN_GLUE] = "?",
  [OP_KAN_L2E] = "lineToEquiv"
};

/* The number of arguments of each operation. VARIES: it comes from the
   family tables. */
static const unsigned char OP_ARITY[] = {
  [OP_NAT] = 0, [OP_FLAG] = 0, [OP_UNIT] = 0, [OP_PROD] = 2, [OP_SUM] = 2,
  [OP_OPTION] = 1, [OP_LIST] = 1, [OP_FAMILY] = VARIES,
  [OP_UNIT_VAL] = 0, [OP_FLAG_YES] = 0, [OP_FLAG_NO] = 0, [OP_PAIR] = 2,
  [OP_INL] = 1, [OP_INR] = 1, [OP_NONE] = 0, [OP_SOME] = 1, [OP_NIL] = 0,
  [OP_CONS] = 2, [OP_PACK] = 2, [OP_CTOR] = VARIES, [OP_PCON] = VARIES, [OP_FIRST] = 1,
  [OP_SECOND] = 1, [OP_EITHER] = 3, [OP_OPTION_ELIM] = 3, [OP_PURE] = 1,
  [OP_MAP] = 2, [OP_BIND] = 2, [OP_APPEND] = 2, [OP_FOLD_NAT] = 3, [OP_FOLD_LIST] = 3,
  [OP_FOLD_FAMILY] = VARIES, [OP_FOLD_HIT] = VARIES, [OP_FOLD_BLOCK] = VARIES, [OP_ELIM] = VARIES, [OP_ELIM_CORE] = 4, [OP_ELIM_NAT] = 4, [OP_UNFOLD] = 3, [OP_UNFOLD_NAT] = 3, [OP_FILTER] = 2, [OP_WITNESS] = 1,
  [OP_PAYLOAD] = 1, [OP_NAT_ADD] = 2, [OP_NAT_SUB] = 2, [OP_NAT_MUL] = 2,
  [OP_FIELD_ADD] = 2, [OP_FIELD_SUB] = 2, [OP_FIELD_MUL] = 2,
  [OP_NAT_EQ] = 2, [OP_NAT_LE] = 2, [OP_FLAG_IF] = 3, [OP_PROJ] = 1,
  [OP_PATHP] = 3, [OP_INTERVAL] = 0, [OP_SSET] = 0, [OP_PARTIAL] = 2,
  [OP_SYS] = VARIES, [OP_TRANSP] = 3, [OP_KAN_DOM] = 2, [OP_KAN_COD] = 3,
  [OP_HCOMP] = 4, [OP_KAN_FIELD] = 1, [OP_GLUE] = 3, [OP_GLUE_ELEM] = 3,
  [OP_UNGLUE] = 3, [OP_KAN_GLUE] = 2, [OP_KAN_L2E] = 1
};

typedef enum {
  SCRUT_VALUE,
  SCRUT_TRAP,
  SCRUT_NEUTRAL
} Scrut;

typedef struct {
  const Value **items;
  uint32_t count;
  uint32_t cap;
} ValBuf;

/* A list value: its cons cells, then the end (nil, a trap or a neutral). */
typedef struct {
  const Value *const *cells;
  uint32_t count;
  const Value *tail;
} Spine;

static const Value *oom(Machine *m) {
  diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
  return NULL;
}

static const Value *internal(Machine *m, const char *what) {
  diag_fail(m->diag, "INTERNAL", m->def, "the evaluator found %s", what);
  return NULL;
}

static int spend(Machine *m, uint64_t steps) {
  if (m->fuel >= steps) {
    m->fuel -= steps;
    return 1;
  }
  m->fuel = 0;
  return diag_fail(m->diag, "EVAL_FUEL", m->def, "the evaluation takes more than %u steps", EVAL_FUEL_STEPS);
}

static int enter(Machine *m) {
  if (m->depth < EVAL_DEPTH_LIMIT) {
    m->depth++;
    return 1;
  }
  return diag_fail(m->diag, "EVAL_DEPTH", m->def, "the evaluation nests deeper than %u levels", EVAL_DEPTH_LIMIT);
}

static Value *new_value(Machine *m, ValKind kind) {
  Value *v = arena_alloc(m->arena, sizeof *v);
  if (v == NULL) {
    oom(m);
    return NULL;
  }
  v->kind = kind;
  return v;
}

const Value *val_nat(Machine *m, uint64_t n) {
  Value *v = new_value(m, VAL_NAT);
  if (v != NULL)
    v->nat = n;
  return v;
}

static const Value *val_trap(Machine *m) {
  return new_value(m, VAL_TRAP);
}

static const Value *overflow(Machine *m) {
  m->overflowed = 1;
  return val_trap(m);
}

const Value *val_var(Machine *m, uint32_t level) {
  Value *v = new_value(m, VAL_VAR);
  if (v != NULL)
    v->nat = level;
  return v;
}

const Value *val_univ(Machine *m, uint64_t level) {
  Value *v = new_value(m, VAL_UNIV);
  if (v != NULL)
    v->nat = level;
  return v;
}

const Value *val_op(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t field, const Value *const *args, uint32_t argc) {
  const Value **copy;
  Value *v;
  uint32_t i;
  for (i = 0; i < argc; i++)
    if (args[i] == NULL)
      return NULL;
  copy = arena_alloc(m->arena, ((size_t)argc + 1u) * sizeof *copy);
  if (copy == NULL)
    return oom(m);
  v = new_value(m, kind);
  if (v == NULL)
    return NULL;
  for (i = 0; i < argc; i++)
    copy[i] = args[i];
  v->op = op;
  v->inst = inst;
  v->field = field;
  v->args = copy;
  v->argc = argc;
  return v;
}

static const Value *make(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c) {
  const Value *args[3];
  args[0] = a;
  args[1] = b;
  args[2] = c;
  return val_op(m, kind, op, inst, 0, args, argc);
}

const Value *val_make(Machine *m, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c) {
  return make(m, VAL_OP, op, inst, argc, a, b, c);
}

static const Value *constant(Machine *m, Op op) {
  return make(m, VAL_OP, op, 0, 0, NULL, NULL, NULL);
}

static const Value *flag(Machine *m, int yes) {
  return constant(m, yes ? OP_FLAG_YES : OP_FLAG_NO);
}

const Value *val_arrow(Machine *m, const Value *dom, const Value *cod) {
  Value *v;
  if (dom == NULL || cod == NULL)
    return NULL;
  v = new_value(m, VAL_PI);
  if (v == NULL)
    return NULL;
  v->dom = dom;
  v->arg = cod;
  return v;
}

const Value *val_closure(Machine *m, ValKind kind, const char *name, const Value *dom, const Env *env, const Core *body) {
  Value *v;
  if (body == NULL || (dom == NULL && kind != VAL_LAM && kind != VAL_PLAM))
    return NULL;
  v = new_value(m, kind);
  if (v == NULL)
    return NULL;
  v->name = name;
  v->dom = dom;
  v->env = env;
  v->body = body;
  return v;
}

int val_is(const Value *v, Op op) {
  return v != NULL && v->kind == VAL_OP && v->op == op;
}

const Env *env_push(Machine *m, const Env *env, const Value *value) {
  Env *e;
  if (value == NULL)
    return NULL;
  e = arena_alloc(m->arena, sizeof *e);
  if (e == NULL) {
    oom(m);
    return NULL;
  }
  e->value = value;
  e->next = env;
  return e;
}

static const Value *env_lookup(Machine *m, const Env *env, uint32_t index) {
  uint32_t i;
  for (i = 0; i < index && env != NULL; i++)
    env = env->next;
  if (env == NULL)
    return internal(m, "a variable outside its scope");
  return env->value;
}

const Value *def_value(Machine *m, uint32_t index) {
  DefInfo *d;
  if (index >= m->def_count)
    return internal(m, "an unknown definition");
  d = &m->defs[index];
  if (d->value == NULL)
    d->value = eval_core(m, NULL, d->body);
  return d->value;
}

static Scrut scrut_of(const Value *v) {
  switch (v->kind) {
  case VAL_TRAP:
    return SCRUT_TRAP;
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    return SCRUT_NEUTRAL;
  case VAL_NAT:
  case VAL_UNIV:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_OP:
  case VAL_IVL:
  case VAL_PLAM:
    return SCRUT_VALUE;
  }
  return SCRUT_VALUE;
}

/* 1 when the scrutinee S stops the operation: a trap gives the trap, a
   neutral S gives the stuck operation. */
static int blocked(Machine *m, const Value *s, Op op, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n, const Value **out) {
  switch (scrut_of(s)) {
  case SCRUT_TRAP:
    *out = s;
    return 1;
  case SCRUT_NEUTRAL:
    *out = val_op(m, VAL_STUCK, op, inst, field, a, n);
    return 1;
  case SCRUT_VALUE:
    return 0;
  }
  return 0;
}

static void buf_init(ValBuf *buf) {
  buf->items = NULL;
  buf->count = 0;
  buf->cap = 0;
}

static int buf_push(Machine *m, ValBuf *buf, const Value *v) {
  const Value **grown;
  uint32_t cap;
  uint32_t i;
  if (v == NULL)
    return 0;
  if (buf->count < buf->cap) {
    buf->items[buf->count++] = v;
    return 1;
  }
  cap = buf->cap == 0 ? 16u : buf->cap * 2u;
  grown = arena_alloc(m->arena, (size_t)cap * sizeof *grown);
  if (grown == NULL) {
    oom(m);
    return 0;
  }
  for (i = 0; i < buf->count; i++)
    grown[i] = buf->items[i];
  buf->items = grown;
  buf->cap = cap;
  buf->items[buf->count++] = v;
  return 1;
}

static int spine_of(Machine *m, const Value *xs, Spine *out) {
  ValBuf cells;
  buf_init(&cells);
  while (val_is(xs, OP_CONS)) {
    if (!buf_push(m, &cells, xs) || !spend(m, 1))
      return 0;
    xs = xs->args[1];
  }
  out->cells = cells.items;
  out->count = cells.count;
  out->tail = xs;
  return xs != NULL;
}

static const Value *build_list(Machine *m, const Value *const *items, uint32_t count, const Value *tail) {
  const Value *acc = tail;
  uint32_t i;
  for (i = count; i > 0 && acc != NULL; i--)
    acc = make(m, VAL_OP, OP_CONS, 0, 2, items[i - 1u], acc, NULL);
  return acc;
}

/* The end of a list result: nil stays nil, a trap stays a trap, a neutral
   end gives OP FIRST END. */
static const Value *list_tail(Machine *m, const Value *tail, Op op, uint32_t inst, const Value *first) {
  switch (scrut_of(tail)) {
  case SCRUT_TRAP:
    return tail;
  case SCRUT_NEUTRAL:
    return make(m, VAL_STUCK, op, inst, 2, first, tail, NULL);
  case SCRUT_VALUE:
    break;
  }
  return val_is(tail, OP_NIL) ? tail : internal(m, "a list that does not end in nil");
}

/* M3 s2: a partial element of a function type at ARG is the system of the
   branches at ARG. The faces do not change. */
static const Value *sys_apply(Machine *m, const Value *sys, const Value *arg) {
  const Value **out = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *out);
  uint32_t k;
  if (out == NULL)
    return oom(m);
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    out[k] = sys->args[k];
    out[k + 1u] = apply_value(m, sys->args[k + 1u], arg);
    if (out[k + 1u] == NULL)
      return NULL;
  }
  return val_op(m, VAL_OP, OP_SYS, sys->inst, 0, out, sys->argc);
}

const Value *apply_value(Machine *m, const Value *fn, const Value *arg) {
  Value *v;
  if (fn == NULL || arg == NULL)
    return NULL;
  switch (fn->kind) {
  case VAL_LAM:
    return closure_apply(m, fn, arg);
  case VAL_TRAP:
    return fn;
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_PAPP:
    v = new_value(m, VAL_APP);
    if (v == NULL)
      return NULL;
    v->dom = fn;
    v->arg = arg;
    return v;
  case VAL_NAT:
  case VAL_UNIV:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_OP:
  case VAL_IVL:
  case VAL_PLAM:
    break;
  }
  if (val_is(fn, OP_SYS))
    return sys_apply(m, fn, arg);
  return internal(m, "an application of a value that is not a function");
}

#define OPEN_BUCKETS 65536u

/* M3 s5-i: the key of an argument that is a variable:  level * 4 + 1 for a
   term variable, level * 4 + 2 for an interval variable i;  0 for another
   argument. */
static uint64_t open_key(const Value *arg) {
  const Ivl *x = arg->ivl;
  if (arg->kind == VAL_VAR)
    return ((uint64_t)arg->nat << 2) + 1u;
  if (arg->kind != VAL_IVL || x == NULL || x->count != 1u || x->starts[1] - x->starts[0] != 1u || (x->lits[x->starts[0]] & 1u) != 0)
    return 0;
  return ((uint64_t)(x->lits[x->starts[0]] >> 1) << 2) + 2u;
}

static uint32_t open_bucket(const Value *binder, uint64_t key) {
  uint64_t h = ((uint64_t)(uintptr_t)binder >> 4) ^ (key * 0x9E3779B97F4A7C15u);
  return (uint32_t)(h ^ (h >> 29)) & (OPEN_BUCKETS - 1u);
}

/* M3 s5-i: a closure opened again at the same variable gives the value of
   the first opening (evaluation is pure, and the arena is never reset).
   Each tube application of a Kan rule template (hcomp_constant, the pair
   and Pi rules) opens the tube of the level above at one level, so without
   the memo each level evaluates the templates of the level above again.
   A head probe (m->probe) is not kept:  its transps stay stuck. */
const Value *closure_apply(Machine *m, const Value *binder, const Value *arg) {
  const Env *env;
  const OpenMemo *p;
  OpenMemo *q;
  const Value *out;
  uint64_t key;
  uint32_t b;
  if (binder == NULL || arg == NULL)
    return NULL;
  if (binder->body == NULL)
    return binder->arg;
  key = m->probe > 0 ? 0 : open_key(arg);
  b = open_bucket(binder, key);
  if (key != 0 && m->opens == NULL)
    m->opens = arena_alloc(m->arena, OPEN_BUCKETS * sizeof *m->opens);
  for (p = key != 0 && m->opens != NULL ? m->opens[b] : NULL; p != NULL; p = p->next)
    if (p->binder == binder && p->key == key)
      return p->out;
  env = env_push(m, binder->env, arg);
  out = env == NULL ? NULL : eval_core(m, env, binder->body);
  q = key != 0 && out != NULL && m->opens != NULL ? arena_alloc(m->arena, sizeof *q) : NULL;
  if (q != NULL) {
    q->binder = binder;
    q->key = key;
    q->out = out;
    q->next = m->opens[b];
    m->opens[b] = q;
  }
  return out;
}

static const Value *reduce_part(Machine *m, Op op, const Value *const *a, uint32_t n, Op ctor, uint32_t index);

/* M3 s2: a projection of a partial element is the system of the projected
   branches, as sys_apply. The faces do not change. */
static const Value *sys_project(Machine *m, Op op, const Value *sys, Op ctor, uint32_t index) {
  const Value **out = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *out);
  uint32_t k;
  if (out == NULL)
    return oom(m);
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    out[k] = sys->args[k];
    out[k + 1u] = reduce_part(m, op, &sys->args[k + 1u], 1u, ctor, index);
    if (out[k + 1u] == NULL)
      return NULL;
  }
  return val_op(m, VAL_OP, OP_SYS, sys->inst, 0, out, sys->argc);
}

static const Value *reduce_part(Machine *m, Op op, const Value *const *a, uint32_t n, Op ctor, uint32_t index) {
  const Value *out;
  if (blocked(m, a[0], op, 0, 0, a, n, &out))
    return out;
  if (val_is(a[0], OP_SYS))
    return sys_project(m, op, a[0], ctor, index);
  /* M3 s5: lineToEquiv L unfolds to its pack on a projection. */
  if (ctor == OP_PACK && val_is(a[0], OP_KAN_L2E)) {
    out = kan_l2e(m, a[0]->args[0]);
    return out == NULL ? NULL : reduce_part(m, op, &out, 1u, ctor, index);
  }
  if (!val_is(a[0], ctor))
    return internal(m, "a projection of a value of the wrong type");
  return a[0]->args[index];
}

/* M3 s3: OP_KAN_FIELD (kan.c only) gives field INST of a constructor value.
   kan.c tests the constructor on each branch face first. Split disjunctive
   faces and apply their substitutions before projecting a branch. */
static const Value *reduce_field(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  IvlHeap h = {m->arena, m->diag, m->def};
  ValBuf out;
  const Value *v;
  uint32_t bound = 0;
  uint32_t k;
  uint32_t j;
  if (blocked(m, a[0], OP_KAN_FIELD, inst, 0, a, n, &v))
    return v;
  if (!val_is(a[0], OP_SYS))
    return a[0]->kind == VAL_OP && inst < a[0]->argc ? a[0]->args[inst] : internal(m, "a Kan field of a value that is not a constructor");
  if (!value_bound(m, a[0], &bound))
    return NULL;
  if (bound >= KAN_PROBE)
    return val_op(m, VAL_STUCK, OP_KAN_FIELD, inst, 0, a, n);
  buf_init(&out);
  for (k = 0; k + 1u < a[0]->argc; k += 2u)
    for (j = 0; j < a[0]->args[k]->ivl->count; j++) {
      const Ivl *face = face_conj(&h, a[0]->args[k]->ivl, j);
      v = face == NULL ? NULL : value_on_face(m, bound, a[0]->args[k + 1u], face, 0);
      if (v == NULL || !buf_push(m, &out, val_ivl(m, face))
          || !buf_push(m, &out, reduce_field(m, inst, &v, 1u)))
        return NULL;
    }
  return val_op(m, VAL_OP, OP_SYS, out.count / 2u, 0, out.items, out.count);
}

static const Value *reduce_either(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[2], OP_EITHER, 0, 0, a, n, &out))
    return out;
  if (val_is(a[2], OP_INL))
    return apply_value(m, a[0], a[2]->args[0]);
  if (val_is(a[2], OP_INR))
    return apply_value(m, a[1], a[2]->args[0]);
  return internal(m, "an either of a value that is not a Sum");
}

static const Value *reduce_option(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[2], OP_OPTION_ELIM, 0, 0, a, n, &out))
    return out;
  if (val_is(a[2], OP_NONE))
    return a[0];
  if (val_is(a[2], OP_SOME))
    return apply_value(m, a[1], a[2]->args[0]);
  return internal(m, "an option of a value that is not an Option");
}

static const Value *reduce_pure(Machine *m, uint32_t inst, const Value *x) {
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return make(m, VAL_OP, OP_SOME, 0, 1, x, NULL, NULL);
  case CARRIER_LIST:
    return make(m, VAL_OP, OP_CONS, 0, 2, x, constant(m, OP_NIL), NULL);
  case CARRIER_SUM:
    return make(m, VAL_OP, OP_INR, 0, 1, x, NULL, NULL);
  }
  return internal(m, "an unknown carrier");
}

static const Value *map_list(Machine *m, const Value *f, const Value *xs) {
  Spine s;
  ValBuf out;
  uint32_t i;
  if (!spine_of(m, xs, &s))
    return NULL;
  buf_init(&out);
  for (i = 0; i < s.count; i++)
    if (!buf_push(m, &out, apply_value(m, f, s.cells[i]->args[0])))
      return NULL;
  return build_list(m, out.items, out.count, list_tail(m, s.tail, OP_MAP, CARRIER_LIST, f));
}

static const Value *reduce_map(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *x = a[1];
  const Value *out;
  if (blocked(m, x, OP_MAP, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return val_is(x, OP_SOME) ? make(m, VAL_OP, OP_SOME, 0, 1, apply_value(m, a[0], x->args[0]), NULL, NULL) : x;
  case CARRIER_SUM:
    return val_is(x, OP_INR) ? make(m, VAL_OP, OP_INR, 0, 1, apply_value(m, a[0], x->args[0]), NULL, NULL) : x;
  case CARRIER_LIST:
    return map_list(m, a[0], x);
  }
  return internal(m, "an unknown carrier");
}

static int buf_append(Machine *m, ValBuf *buf, const Spine *s) {
  uint32_t i;
  for (i = 0; i < s->count; i++)
    if (!buf_push(m, buf, s->cells[i]->args[0]))
      return 0;
  return 1;
}

/* A list bind concatenates the lists F X. A neutral end in any of them
   leaves the whole bind stuck. */
/* M6 s1 (D1): append xs ys. The known cells of xs go over ys. A neutral end
   of xs gives the cells over append END ys, or xs when ys is nil (the right
   unit). A trap end of xs stays. */
static const Value *append_values(Machine *m, const Value *xs, const Value *ys) {
  Spine s;
  ValBuf out;
  if (xs == NULL || ys == NULL || !spine_of(m, xs, &s))
    return NULL;
  switch (scrut_of(s.tail)) {
  case SCRUT_TRAP:
    return xs;
  case SCRUT_NEUTRAL:
    if (val_is(ys, OP_NIL))
      return xs;
    ys = make(m, VAL_STUCK, OP_APPEND, 0, 2, s.tail, ys, NULL);
    break;
  case SCRUT_VALUE:
    if (!val_is(s.tail, OP_NIL))
      return internal(m, "a list that does not end in nil");
    break;
  }
  buf_init(&out);
  if (!buf_append(m, &out, &s))
    return NULL;
  return build_list(m, out.items, out.count, ys);
}

/* M6 s1 (D2): bind nil f = nil, bind (cons x xs) f = append (f x) (bind xs f).
   A neutral end gives bind END f under the known cells. */
static const Value *bind_list(Machine *m, const Value *const *a) {
  Spine s;
  const Value *acc = NULL;
  uint32_t i;
  if (!spine_of(m, a[0], &s))
    return NULL;
  switch (scrut_of(s.tail)) {
  case SCRUT_TRAP:
    acc = s.tail;
    break;
  case SCRUT_NEUTRAL:
    acc = make(m, VAL_STUCK, OP_BIND, CARRIER_LIST, 2, s.tail, a[1], NULL);
    break;
  case SCRUT_VALUE:
    acc = list_tail(m, s.tail, OP_BIND, CARRIER_LIST, a[1]);
    break;
  }
  for (i = s.count; i > 0 && acc != NULL; i--)
    acc = append_values(m, apply_value(m, a[1], s.cells[i - 1u]->args[0]), acc);
  return acc;
}

static const Value *reduce_bind(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *x = a[0];
  const Value *out;
  if (blocked(m, x, OP_BIND, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return val_is(x, OP_SOME) ? apply_value(m, a[1], x->args[0]) : x;
  case CARRIER_SUM:
    return val_is(x, OP_INR) ? apply_value(m, a[1], x->args[0]) : x;
  case CARRIER_LIST:
    return bind_list(m, a);
  }
  return internal(m, "an unknown carrier");
}

/* M6 s1 (D3): elim P cnil ccons l at List. nil gives cnil;  cons a l gives
   ccons a l (elim P cnil ccons l), from the last cell. A trap end gives the
   trap;  a neutral end gives the stuck elim at the end. */
static const Value *elim_list(Machine *m, uint32_t inst, const Value *const *a) {
  Spine s;
  const Value *end[4];
  const Value *acc = NULL;
  uint32_t i;
  if (!spine_of(m, a[3], &s))
    return NULL;
  end[0] = a[0];
  end[1] = a[1];
  end[2] = a[2];
  end[3] = s.tail;
  if (!blocked(m, s.tail, OP_ELIM_CORE, inst, 0, end, 4u, &acc))
    acc = a[1];
  for (i = s.count; i > 0 && acc != NULL; i--) {
    const Value *cell = s.cells[i - 1u];
    acc = apply_value(m, apply_value(m, apply_value(m, a[2], cell->args[0]), cell->args[1]), acc);
  }
  return acc;
}

/* M6 s1 (D3): elim P c1 c2 x at a core carrier (inst: the Carrier). It
   reduces at a constructor and is neutral at a neutral scrutinee. */
static const Value *elim_core(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *x = a[3];
  const Value *out;
  if (blocked(m, x, OP_ELIM_CORE, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return val_is(x, OP_SOME) ? apply_value(m, a[2], x->args[0]) : a[1];
  case CARRIER_SUM:
    return apply_value(m, val_is(x, OP_INL) ? a[1] : a[2], x->args[0]);
  case CARRIER_LIST:
    return elim_list(m, inst, a);
  }
  return internal(m, "an unknown carrier");
}

/* M6 s2 (D5): 1 when V is the successor spine natAdd x k:  x neutral, k a
   literal (k > 0, by nat_offset). */
static int nat_spine(const Value *v, const Value **x, uint64_t *k) {
  if (v->kind != VAL_STUCK || v->op != OP_NAT_ADD || v->args[1]->kind != VAL_NAT)
    return 0;
  *x = v->args[0];
  *k = v->args[1]->nat;
  return 1;
}

/* M6 s2 (D5): natAdd x y with a neutral operand. A zero offset is removed, a
   literal moves to the right, and nested literal offsets combine (with the
   overflow trap of literal addition). Two neutral operands give STUCK. */
static const Value *nat_offset(Machine *m, const Value *x, const Value *y, const Value *stuck) {
  const Value *lit = x->kind == VAL_NAT ? x : y;
  const Value *base = x->kind == VAL_NAT ? y : x;
  const Value *args[2];
  uint64_t j = 0;
  if (lit->kind != VAL_NAT)
    return stuck;
  if (lit->nat == 0)
    return base;
  args[0] = base;
  if (!nat_spine(base, &args[0], &j))
    j = 0;
  if (j + lit->nat < j)
    return overflow(m);
  args[1] = val_nat(m, j + lit->nat);
  return args[1] == NULL ? NULL : val_op(m, VAL_STUCK, OP_NAT_ADD, 0, 0, args, 2u);
}

/* M6 s2 (D6): the index natAdd x i of step i of a spine over X (D5), or the
   literal i when X is NULL. */
static const Value *nat_index(Machine *m, const Value *x, uint64_t i) {
  const Value *k = val_nat(m, i);
  return x == NULL || k == NULL ? k : nat_offset(m, x, k, NULL);
}

/* fold step z n: step applied n times to z. At the successor spine natAdd x k
   (D5), step applied k times to the stuck fold step z x. */
static const Value *reduce_fold_nat(Machine *m, const Value *const *a, uint32_t n) {
  const Value *acc = a[1];
  const Value *out;
  uint64_t count = 0;
  uint64_t k;
  if (nat_spine(a[2], &out, &count)) {
    const Value *end[3];
    end[0] = a[0];
    end[1] = a[1];
    end[2] = out;
    if (!blocked(m, out, OP_FOLD_NAT, 0, 0, end, 3u, &acc))
      return internal(m, "a successor spine over a value that is not neutral");
  } else if (blocked(m, a[2], OP_FOLD_NAT, 0, 0, a, n, &out))
    return out;
  else if (a[2]->kind != VAL_NAT)
    return internal(m, "a fold of a value that is not a Nat");
  else
    count = a[2]->nat;
  for (k = 0; k < count && acc != NULL; k++)
    acc = spend(m, 1) ? apply_value(m, a[0], acc) : NULL;
  return acc;
}

/* M6 s2 (D6): elim P z s n at Nat. The literal k gives s (k - 1) (... (s 0 z)).
   The spine natAdd x k gives s (natAdd x (k - 1)) (... (s x (elim P z s x))).
   It is neutral at a neutral n and the trap at a trap. */
static const Value *elim_nat(Machine *m, const Value *const *a, uint32_t n) {
  const Value *acc = a[1];
  const Value *x = NULL;
  const Value *out;
  uint64_t count = 0;
  uint64_t i;
  if (nat_spine(a[3], &x, &count)) {
    const Value *end[4];
    end[0] = a[0];
    end[1] = a[1];
    end[2] = a[2];
    end[3] = x;
    if (!blocked(m, x, OP_ELIM_NAT, 0, 0, end, 4u, &acc))
      return internal(m, "a successor spine over a value that is not neutral");
  } else if (blocked(m, a[3], OP_ELIM_NAT, 0, 0, a, n, &out))
    return out;
  else if (a[3]->kind != VAL_NAT)
    return internal(m, "an elim of a value that is not a Nat");
  else
    count = a[3]->nat;
  for (i = 0; i < count && acc != NULL; i++)
    acc = spend(m, 1) ? apply_value(m, apply_value(m, a[2], nat_index(m, x, i)), acc) : NULL;
  return acc;
}

/* fold f z xs: the right fold. The end of the list is folded first. */
static const Value *reduce_fold_list(Machine *m, const Value *const *a) {
  const Value *acc = NULL;
  Spine s;
  uint32_t i;
  if (!spine_of(m, a[2], &s))
    return NULL;
  switch (scrut_of(s.tail)) {
  case SCRUT_TRAP:
    acc = s.tail;
    break;
  case SCRUT_NEUTRAL:
    acc = make(m, VAL_STUCK, OP_FOLD_LIST, 0, 3, a[0], a[1], s.tail);
    break;
  case SCRUT_VALUE:
    acc = val_is(s.tail, OP_NIL) ? a[1] : internal(m, "a list that does not end in nil");
    break;
  }
  for (i = s.count; i > 0 && acc != NULL; i--)
    acc = apply_value(m, apply_value(m, a[0], s.cells[i - 1u]->args[0]), acc);
  return acc;
}

static const Value *stuck_fold(Machine *m, Op op, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x) {
  const Value **args = arena_alloc(m->arena, ((size_t)ncases + 1u) * sizeof *args);
  uint32_t i;
  if (args == NULL)
    return oom(m);
  for (i = 0; i < ncases; i++)
    args[i] = cases[i];
  args[ncases] = x;
  return val_op(m, VAL_STUCK, op, family, 0, args, ncases + 1u);
}

/* fold case... x over a domain family: the case of the constructor of X,
   applied to its fields. A recursive field arrives folded. */
static const Value *fold_family(Machine *m, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x) {
  const FamilyInfo *fam;
  const CtorInfo *ctor;
  const Value *acc;
  const Value *field;
  uint32_t i;
  if (x == NULL)
    return NULL;
  switch (scrut_of(x)) {
  case SCRUT_TRAP:
    return x;
  case SCRUT_NEUTRAL:
    return stuck_fold(m, OP_FOLD_FAMILY, family, cases, ncases, x);
  case SCRUT_VALUE:
    break;
  }
  if (!val_is(x, OP_CTOR) || x->inst >= m->ctor_count || family >= m->family_count)
    return internal(m, "a fold of a value that is not in the family");
  fam = &m->families[family];
  ctor = &m->ctors[x->inst];
  if (x->inst < fam->first_ctor || x->inst - fam->first_ctor >= ncases || x->argc != fam->param_count + ctor->field_count)
    return internal(m, "a fold of a constructor of another family");
  if (!enter(m))
    return NULL;
  acc = cases[x->inst - fam->first_ctor];
  for (i = 0; i < ctor->field_count && acc != NULL; i++) {
    field = x->args[fam->param_count + i];
    acc = apply_value(m, acc, ctor->fields[i].recursive ? fold_family(m, family, cases, ncases, field) : field);
  }
  m->depth--;
  return acc;
}

/* M5 s5 (D7): fold MA MB case... x at the mutual block whose first family
   is FAMILY. A: MA, MB, then one case for each constructor of the block
   (the first family, then the second). A case of the first family gets
   the fields; a recursive field and a partner field arrive folded. A case
   of the second family gets the fold of the index first, then the fields
   (a recursive field arrives folded). */
static const Value *fold_block(Machine *m, uint32_t family, const Value *const *a, uint32_t ncases, const Value *x) {
  const FamilyInfo *fam;
  const FamilyInfo *own;
  const CtorInfo *ctor;
  const Value *acc;
  const Value *field;
  uint32_t i;
  if (x == NULL)
    return NULL;
  switch (scrut_of(x)) {
  case SCRUT_TRAP:
    return x;
  case SCRUT_NEUTRAL:
    return stuck_fold(m, OP_FOLD_BLOCK, family, a, ncases + 2u, x);
  case SCRUT_VALUE:
    break;
  }
  fam = &m->families[family];
  if (x->inst >= m->ctor_count || x->inst < fam->first_ctor || x->inst - fam->first_ctor >= ncases)
    return internal(m, "a block fold of a constructor of another block");
  ctor = &m->ctors[x->inst];
  own = &m->families[ctor->family];
  if ((ctor->family != family && ctor->family != fam->partner) || x->argc != own->param_count + ctor->field_count)
    return internal(m, "a block fold of a constructor of another block");
  if (!enter(m))
    return NULL;
  acc = a[2u + x->inst - fam->first_ctor];
  if (ctor->family != family)
    acc = apply_value(m, acc, fold_block(m, family, a, ncases, x->args[fam->param_count]));
  for (i = 0; i < ctor->field_count && acc != NULL; i++) {
    field = x->args[own->param_count + i];
    acc = apply_value(m, acc, ctor->fields[i].recursive ? fold_block(m, family, a, ncases, field) : field);
  }
  m->depth--;
  return acc;
}

static const Value *papp_value(Machine *m, const Value *p, const Value *r, const Env *env, const Core *const *ends);
static const Value *fold_hit(Machine *m, Op o, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const Value *x);

int core_free_levels(Machine *m, const Core *core, uint32_t depth, uint32_t free_count, unsigned char *used) {
  uint32_t i;
  uint32_t body_depth;
  int ok;
  if (core == NULL || !enter(m))
    return 0;
  if (core->kind == CORE_VAR && core->index < depth && depth - 1u - core->index < free_count)
    used[depth - 1u - core->index] = 1;
  body_depth = depth + (core->kind == CORE_LAM || core->kind == CORE_PI
    || core->kind == CORE_SIGMA || core->kind == CORE_PLAM ? 1u : 0u);
  ok = (core->a == NULL || core_free_levels(m, core->a, depth, free_count, used))
    && (core->b == NULL || core_free_levels(m, core->b, body_depth, free_count, used));
  for (i = 0; ok && i < core->argc; i++)
    ok = core_free_levels(m, core->args[i], depth, free_count, used);
  m->depth--;
  return ok;
}

/* Apply a constructor case to fields whose recursive entries are folded. */
static const Value *eval_hit_case(Machine *m, Op o, uint32_t family, const Value *const *cases, uint32_t ncases, const Value *x, const Value *const *fields) {
  const FamilyInfo *fam;
  const CtorInfo *ctor;
  const Value *acc;
  uint32_t start;
  uint32_t i;
  if (x == NULL)
    return NULL;
  if (family >= m->family_count || !(val_is(x, OP_CTOR) || val_is(x, OP_PCON)) || x->inst >= m->ctor_count)
    return internal(m, "a case applied to a value that is not a constructor");
  fam = &m->families[family];
  ctor = &m->ctors[x->inst];
  start = fam->param_count + ctor->field_count;
  if (ctor->family != family || x->inst < fam->first_ctor || x->inst - fam->first_ctor >= ncases
      || x->argc != start + (val_is(x, OP_PCON) ? ctor->binder_count : 0u))
    return internal(m, "a case applied to a constructor of another family");
  acc = cases[x->inst - fam->first_ctor];
  for (i = 0; i < ctor->field_count && acc != NULL; i++) {
    acc = apply_value(m, acc, o == OP_ELIM ? x->args[fam->param_count + i] : fields[i]);
    if (o == OP_ELIM && ctor->fields[i].recursive && acc != NULL)
      acc = apply_value(m, acc, fields[i]);
  }
  for (i = start; i < x->argc && acc != NULL; i++)
    acc = papp_value(m, acc, x->args[i], NULL, NULL);
  return acc;
}

/* M4 s3: a partial element of the family folds branchwise; the faces do
   not change (the tube of kan_fold_hcomp at i is a system). */
static const Value *fold_hit_sys(Machine *m, Op o, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const Value *sys) {
  const Value **out = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *out);
  uint32_t k;
  if (out == NULL)
    return oom(m);
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    out[k] = sys->args[k];
    out[k + 1u] = fold_hit(m, o, family, res, cases, ncases, sys->args[k + 1u]);
    if (out[k + 1u] == NULL)
      return NULL;
  }
  return val_op(m, VAL_OP, OP_SYS, sys->inst, 0, out, sys->argc);
}

/* M4 s3: fold B case... x at a HIT, stuck on a neutral X. */
static const Value *stuck_fold_hit(Machine *m, Op o, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const Value *x) {
  const Value **args = arena_alloc(m->arena, ((size_t)ncases + 2u) * sizeof *args);
  uint32_t i;
  if (args == NULL)
    return oom(m);
  args[0] = res;
  for (i = 0; i < ncases; i++)
    args[i + 1u] = cases[i];
  args[ncases + 1u] = x;
  return val_op(m, VAL_STUCK, o, family, 0, args, ncases + 2u);
}

/* Recover a path field's checked ends in the constructor's original
   field environment before mapping it to a recursive image or IH. */
static const Value *fold_path_field(Machine *m, Op o, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const CtorInfo *ctor, const Value *x, uint32_t field) {
  const Env *env = NULL;
  const Value *type;
  uint32_t i;
  uint32_t at = m->families[family].param_count + field;
  for (i = 0; i < at; i++) {
    env = env_push(m, env, x->args[i]);
    if (env == NULL)
      return NULL;
  }
  type = eval_core(m, env, ctor->fields[field].type);
  if (type == NULL)
    return NULL;
  if (!val_is(type, OP_PATHP) || type->argc != 3u)
    return internal(m, "a recursive path field without a path type");
  return kan_fold_path_field(m, o, family, res, cases, ncases, x->args[at], type->args[1], type->args[2]);
}

/* M4 s3 (D6, D11): fold B case... x at a HIT. A point constructor: its
   case at its fields (a recursive field arrives folded). A path
   constructor at its interval arguments: its case at its fields, then at
   each interval argument. An hcomp value: the Kan rule (kan_fold_hcomp). A
   system: branchwise (fold_hit_sys). */
static const Value *fold_hit(Machine *m, Op o, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const Value *x) {
  const FamilyInfo *fam;
  const CtorInfo *ctor;
  const Value *acc;
  const Value **images;
  uint32_t fields;
  uint32_t ivls;
  uint32_t i;
  if (x == NULL)
    return NULL;
  switch (scrut_of(x)) {
  case SCRUT_TRAP:
    return x;
  case SCRUT_NEUTRAL:
    return stuck_fold_hit(m, o, family, res, cases, ncases, x);
  case SCRUT_VALUE:
    break;
  }
  if (family >= m->family_count)
    return internal(m, "a fold at an unknown family");
  if (val_is(x, OP_HCOMP) && o == OP_ELIM)
    return m->families[family].path_ctors == 0u ? stuck_fold_hit(m, o, family, res, cases, ncases, x)
      : kan_elim_hcomp(m, family, res, cases, ncases, x);
  if (val_is(x, OP_HCOMP))
    return kan_fold_hcomp(m, family, res, cases, ncases, x);
  if (val_is(x, OP_SYS))
    return fold_hit_sys(m, o, family, res, cases, ncases, x);
  if (!(val_is(x, OP_CTOR) || val_is(x, OP_PCON)) || x->inst >= m->ctor_count)
    return internal(m, "a fold of a value that is not in the family");
  fam = &m->families[family];
  ctor = &m->ctors[x->inst];
  fields = fam->param_count + ctor->field_count;
  ivls = val_is(x, OP_PCON) ? ctor->binder_count : 0u;
  if (x->inst < fam->first_ctor || x->inst - fam->first_ctor >= ncases || x->argc != fields + ivls)
    return internal(m, "a fold of a constructor of another family");
  if (!enter(m))
    return NULL;
  images = arena_alloc(m->arena, ((size_t)ctor->field_count + 1u) * sizeof *images);
  if (images == NULL) {
    m->depth--;
    return oom(m);
  }
  for (i = 0; i < ctor->field_count; i++) {
    const Value *field = x->args[fam->param_count + i];
    images[i] = ctor->fields[i].recursive == 1 ? fold_hit(m, o, family, res, cases, ncases, field)
      : ctor->fields[i].recursive == 2 ? fold_path_field(m, o, family, res, cases, ncases, ctor, x, i) : field;
    if (images[i] == NULL) {
      m->depth--;
      return NULL;
    }
  }
  acc = eval_hit_case(m, o, family, cases, ncases, x, images);
  m->depth--;
  return acc;
}

static int interval_levels(Machine *m, uint32_t level, uint32_t free_count, const Value *v, unsigned char *levels);

/* Map a boundary path pointwise, preserving fields that already denote
   their images. Use a fresh interval above every free input variable. */
static const Value *fold_boundary_path(Machine *m, const CtorInfo *boundary, const Value *res, const Value *const *cases, uint32_t ncases, const Value *const *fields, const Value *const *ihs, const Value *p) {
  const Env *env = NULL;
  const Value *body;
  const Core *core;
  unsigned char *levels;
  unsigned char *used;
  uint32_t level = 0;
  uint32_t i;
  if (!value_bound(m, p, &level) || !value_bound(m, res, &level))
    return NULL;
  for (i = 0; i < ncases; i++)
    if (!value_bound(m, cases[i], &level))
      return NULL;
  for (i = 0; i < boundary->field_count; i++)
    if (!value_bound(m, fields[i], &level) || (ihs != NULL && !value_bound(m, ihs[i], &level)))
      return NULL;
  body = eval_fold_hit_boundary(m, boundary, res, cases, ncases, fields, ihs,
    papp_value(m, p, val_ivl_var(m, level), NULL, NULL));
  core = body == NULL ? NULL : quote_value(m, level + 1u, body);
  if (core == NULL)
    return NULL;
  levels = arena_alloc(m->arena, (size_t)level + 1u);
  used = arena_alloc(m->arena, (size_t)level + 1u);
  if (levels == NULL || used == NULL)
    return oom(m);
  if (!interval_levels(m, level + 1u, level, body, levels)
      || !core_free_levels(m, core, level + 1u, level, used))
    return NULL;
  for (i = 0; i < level; i++) {
    const Value *v = !used[i] ? val_nat(m, 0) : levels[i] ? val_ivl_var(m, i) : val_var(m, i);
    env = env_push(m, env, v);
    if (env == NULL)
      return NULL;
  }
  return val_closure(m, VAL_PLAM, "j", NULL, env, core);
}

/* Boundary field variables already denote their images in RES. Preserve
   them when folding a constructor around a recursive field. M4 s4: a path
   field variable applied to an interval (p @ j) is preserved too; a path
   field inside a boundary constructor folds pointwise (kan_fold_path_field). */
const Value *eval_fold_hit_boundary(Machine *m, const CtorInfo *boundary, const Value *res, const Value *const *cases, uint32_t ncases, const Value *const *fields, const Value *const *ihs, const Value *x) {
  Op o = ihs == NULL ? OP_FOLD_HIT : OP_ELIM;
  const CtorInfo *ctor;
  const Value **images;
  const Value *out;
  uint32_t p = m->families[boundary->family].param_count;
  uint32_t i;
  if (x == NULL)
    return NULL;
  for (i = 0; i < boundary->field_count; i++) {
    const Value *head = x->kind == VAL_PAPP ? x->dom : x;
    if (boundary->fields[i].recursive && head->kind == VAL_VAR && fields[i]->kind == VAL_VAR && head->nat == fields[i]->nat)
      return ihs == NULL ? x : x->kind == VAL_PAPP ? papp_value(m, ihs[i], x->arg, NULL, NULL) : ihs[i];
  }
  if (!(val_is(x, OP_CTOR) || val_is(x, OP_PCON)))
    return fold_hit(m, o, boundary->family, res, cases, ncases, x);
  if (x->inst >= m->ctor_count || m->ctors[x->inst].family != boundary->family)
    return internal(m, "a boundary constructor of another family");
  if (!enter(m))
    return NULL;
  ctor = &m->ctors[x->inst];
  images = arena_alloc(m->arena, ((size_t)ctor->field_count + 1u) * sizeof *images);
  if (images == NULL) {
    m->depth--;
    return oom(m);
  }
  for (i = 0; i < ctor->field_count; i++) {
    images[i] = ctor->fields[i].recursive == 1 ? eval_fold_hit_boundary(m, boundary, res, cases, ncases, fields, ihs, x->args[p + i])
      : ctor->fields[i].recursive == 2 ? fold_boundary_path(m, boundary, res, cases, ncases, fields, ihs, x->args[p + i]) : x->args[p + i];
    if (images[i] == NULL) {
      m->depth--;
      return NULL;
    }
  }
  out = eval_hit_case(m, o, boundary->family, cases, ncases, x, images);
  m->depth--;
  return out;
}

static Core *new_core(Machine *m, CoreKind kind);
static const Core *core_leaf(Machine *m, CoreKind kind, uint64_t nat);

/* M6 s3 (D8): the de Bruijn index INDEX, for the step closure. */
static const Core *core_index(Machine *m, uint32_t index) {
  Core *c = new_core(m, CORE_VAR);
  if (c != NULL)
    c->index = index;
  return c;
}

/* M6 s3 (D8): the core name OP with ARGC of the arguments X, Y, Z. */
static const Core *core_call(Machine *m, Op op, uint32_t argc, const Core *x, const Core *y, const Core *z) {
  const Core **args = arena_alloc(m->arena, 3u * sizeof *args);
  Core *c;
  if (args == NULL) {
    oom(m);
    return NULL;
  }
  args[0] = x;
  args[1] = y;
  args[2] = z;
  if (x == NULL || (argc > 1u && y == NULL) || (argc > 2u && z == NULL))
    return NULL;
  c = new_core(m, CORE_OP);
  if (c == NULL)
    return NULL;
  c->op = op;
  c->args = args;
  c->argc = argc;
  return c;
}

/* M6 s3 (D8, D9): the unfold step at a neutral step result R, with LIMIT for
   the rest. The List form gives
   option nil (fun p => cons (first p) (unfold g LIMIT (second p))) R, the Nat
   form gives option 0 (fun r => natAdd (unfold g LIMIT r) 1) R. The closure
   keeps LIMIT at index 1 and g at index 2. */
static const Value *unfold_step(Machine *m, Op form, const Value *g, const Value *limit, const Value *r) {
  const Core *p = core_index(m, 0);
  const Core *rest = form == OP_UNFOLD ? core_call(m, OP_SECOND, 1u, p, NULL, NULL) : p;
  const Core *call = core_call(m, form, 3u, core_index(m, 2), core_index(m, 1), rest);
  const Core *body = form == OP_UNFOLD ? core_call(m, OP_CONS, 2u, core_call(m, OP_FIRST, 1u, p, NULL, NULL), call, NULL) : core_call(m, OP_NAT_ADD, 2u, call, core_leaf(m, CORE_NAT, 1), NULL);
  const Env *env = limit == NULL ? NULL : env_push(m, env_push(m, NULL, g), limit);
  const Value *args[3];
  args[0] = form == OP_UNFOLD ? constant(m, OP_NIL) : val_nat(m, 0);
  args[1] = body == NULL || env == NULL ? NULL : val_closure(m, VAL_LAM, form == OP_UNFOLD ? "p" : "r", NULL, env, body);
  args[2] = r;
  return args[0] == NULL || args[1] == NULL ? NULL : val_op(m, VAL_STUCK, OP_OPTION_ELIM, 0, 0, args, 3u);
}

/* 1 when the step result R ends the unfold. *TAIL is then the end:  nil (0 in
   the Nat form) at none, the trap at a trap, and the step through option (D8)
   at a neutral R. X and LEFT give the limit of this step (nat_index). */
static int unfold_end(Machine *m, Op form, const Value *g, const Value *x, uint64_t left, const Value *r, const Value **tail) {
  const Value *p = val_is(r, OP_SOME) ? r->args[0] : r;
  if (val_is(r, OP_NONE)) {
    *tail = form == OP_UNFOLD ? constant(m, OP_NIL) : val_nat(m, 0);
    return 1;
  }
  /* Nat counts the some constructor. Its payload is only the next seed. */
  if (form == OP_UNFOLD_NAT && val_is(r, OP_SOME))
    return 0;
  switch (scrut_of(p)) {
  case SCRUT_TRAP:
    *tail = p;
    return 1;
  case SCRUT_NEUTRAL:
    if (val_is(r, OP_SOME))
      return 0;
    *tail = unfold_step(m, form, g, nat_index(m, x, left - 1u), r);
    return 1;
  case SCRUT_VALUE:
    break;
  }
  if (val_is(r, OP_SOME) && val_is(p, OP_PAIR))
    return 0;
  *tail = internal(m, form == OP_UNFOLD ? "an unfold step that is not an Option of a pair" : "an unfold step that is not an Option");
  return 1;
}

/* M6 s3 (D9): the value of an unfold that ends at TAIL after STEPS steps:  the
   list of ITEMS on TAIL, or in the Nat form natAdd TAIL STEPS. */
static const Value *unfold_result(Machine *m, Op form, const ValBuf *items, uint64_t steps, const Value *tail) {
  const Value *k;
  if (tail == NULL)
    return NULL;
  if (form == OP_UNFOLD)
    return build_list(m, items->items, items->count, tail);
  if (tail->kind == VAL_NAT)
    return val_nat(m, steps);
  if (scrut_of(tail) == SCRUT_TRAP)
    return tail;
  k = val_nat(m, steps);
  return k == NULL ? NULL : nat_offset(m, tail, k, NULL);
}

/* unfold g limit seed: at most LIMIT steps. The List form gives the list of
   the step results;  the Nat form (D9) gives the number of steps. At the
   successor spine natAdd x k (D5), k steps and then the stuck unfold g x seed.
   A neutral step result goes through option (D8). */
static const Value *reduce_unfold(Machine *m, Op form, const Value *const *a, uint32_t n) {
  const Value *seed = a[2];
  const Value *x = NULL;
  const Value *out;
  const Value *r;
  const Value *tail = NULL;
  ValBuf items;
  uint64_t count = 0;
  uint64_t i;
  if (!nat_spine(a[1], &x, &count)) {
    if (blocked(m, a[1], form, 0, 0, a, n, &out))
      return out;
    if (a[1]->kind != VAL_NAT)
      return internal(m, "an unfold limit that is not a Nat");
    count = a[1]->nat;
  }
  buf_init(&items);
  for (i = 0; i < count; i++) {
    r = spend(m, 1) ? apply_value(m, a[0], seed) : NULL;
    if (r == NULL)
      return NULL;
    if (unfold_end(m, form, a[0], x, count - i, r, &tail))
      return unfold_result(m, form, &items, i, tail);
    if (form == OP_UNFOLD && !buf_push(m, &items, reduce_part(m, OP_FIRST, r->args, 1u, OP_PAIR, 0)))
      return NULL;
    seed = form == OP_UNFOLD ? reduce_part(m, OP_SECOND, r->args, 1u, OP_PAIR, 1) : r->args[0];
    if (seed == NULL)
      return NULL;
  }
  if (x != NULL)
    tail = make(m, VAL_STUCK, form, 0, 3, a[0], x, seed);
  else
    tail = form == OP_UNFOLD ? constant(m, OP_NIL) : val_nat(m, 0);
  return unfold_result(m, form, &items, count, tail);
}

static const Value *filter_option(Machine *m, const Value *const *a, uint32_t n) {
  const Value *test;
  const Value *out;
  if (!val_is(a[1], OP_SOME))
    return a[1];
  test = apply_value(m, a[0], a[1]->args[0]);
  if (test == NULL)
    return NULL;
  if (blocked(m, test, OP_FILTER, CARRIER_OPTION, 0, a, n, &out))
    return out;
  return val_is(test, OP_FLAG_YES) ? a[1] : constant(m, OP_NONE);
}

static const Value *filter_list(Machine *m, const Value *const *a) {
  const Value *test;
  Spine s;
  ValBuf kept;
  uint32_t i;
  if (!spine_of(m, a[1], &s))
    return NULL;
  buf_init(&kept);
  for (i = 0; i < s.count; i++) {
    test = apply_value(m, a[0], s.cells[i]->args[0]);
    if (test == NULL)
      return NULL;
    switch (scrut_of(test)) {
    case SCRUT_TRAP:
      return build_list(m, kept.items, kept.count, test);
    case SCRUT_NEUTRAL:
      return build_list(m, kept.items, kept.count, make(m, VAL_STUCK, OP_FILTER, CARRIER_LIST, 2, a[0], s.cells[i], NULL));
    case SCRUT_VALUE:
      break;
    }
    if (val_is(test, OP_FLAG_YES) && !buf_push(m, &kept, s.cells[i]->args[0]))
      return NULL;
  }
  return build_list(m, kept.items, kept.count, list_tail(m, s.tail, OP_FILTER, CARRIER_LIST, a[0]));
}

static const Value *reduce_filter(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[1], OP_FILTER, inst, 0, a, n, &out))
    return out;
  switch ((Carrier)inst) {
  case CARRIER_OPTION:
    return filter_option(m, a, n);
  case CARRIER_LIST:
    return filter_list(m, a);
  case CARRIER_SUM:
    break;
  }
  return internal(m, "a filter with no instance");
}

/* 1 when both arguments are numbers. Else *OUT is the trap or the stuck
   operation. */
/* glue-lang: a Field literal holds its canonical decimal in name. */
static const Value *val_field(Machine *m, const char *text) {
  Value *v = new_value(m, VAL_OP);
  char *copy = v == NULL || text == NULL ? NULL : arena_strndup(m->arena, text, strlen(text));
  if (copy == NULL)
    return oom(m);
  v->op = OP_FIELD_LIT;
  v->name = copy;
  return v;
}

/* glue-lang: Field arithmetic modulo r. A neutral argument gives the stuck
   operation, which the circuit lowering reads. */
static const Value *field_op(Machine *m, Op op, const Value *const *a, uint32_t n) {
  typedef void (*FrOp)(Fr *, const Fr *, const Fr *);
  FrOp f = op == OP_FIELD_SUB ? fr_sub : op == OP_FIELD_MUL ? fr_mul : fr_add;
  const Value *out;
  char text[FR_TEXT_MAX];
  Fr x;
  Fr y;
  Fr z;
  if (a[0]->kind == VAL_TRAP || a[1]->kind == VAL_TRAP)
    return a[0]->kind == VAL_TRAP ? a[0] : a[1];
  if (blocked(m, a[0], op, 0, 0, a, n, &out) || blocked(m, a[1], op, 0, 0, a, n, &out))
    return out;
  if (!val_is(a[0], OP_FIELD_LIT) || !val_is(a[1], OP_FIELD_LIT)
      || !fr_parse(a[0]->name, strlen(a[0]->name), &x) || !fr_parse(a[1]->name, strlen(a[1]->name), &y))
    return internal(m, "a Field operation on a value that is not a Field literal");
  f(&z, &x, &y);
  fr_print(&z, text);
  return val_field(m, text);
}

static int nat_args(Machine *m, Op op, const Value *const *a, uint32_t n, uint64_t *x, uint64_t *y, const Value **out) {
  *out = a[0]->kind == VAL_TRAP ? a[0] : a[1];
  if (a[0]->kind == VAL_TRAP || a[1]->kind == VAL_TRAP)
    return 0;
  if (blocked(m, a[0], op, 0, 0, a, n, out) || blocked(m, a[1], op, 0, 0, a, n, out))
    return 0;
  if (a[0]->kind != VAL_NAT || a[1]->kind != VAL_NAT) {
    *out = internal(m, "a Nat operation on a value that is not a Nat");
    return 0;
  }
  *x = a[0]->nat;
  *y = a[1]->nat;
  return 1;
}

static const Value *nat_add(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, OP_NAT_ADD, a, n, &x, &y, &out))
    return out != NULL && out->kind == VAL_STUCK ? nat_offset(m, a[0], a[1], out) : out;
  return x + y < x ? overflow(m) : val_nat(m, x + y);
}

static const Value *nat_sub(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, OP_NAT_SUB, a, n, &x, &y, &out))
    return out;
  return val_nat(m, x < y ? 0 : x - y);
}

static const Value *nat_mul(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, OP_NAT_MUL, a, n, &x, &y, &out))
    return out;
  return y != 0 && x > UINT64_MAX / y ? overflow(m) : val_nat(m, x * y);
}

static const Value *nat_test(Machine *m, Op op, const Value *const *a, uint32_t n) {
  const Value *out;
  uint64_t x = 0;
  uint64_t y = 0;
  if (!nat_args(m, op, a, n, &x, &y, &out))
    return out;
  return flag(m, op == OP_NAT_EQ ? x == y : x <= y);
}

static const Value *reduce_flag_if(Machine *m, const Value *const *a, uint32_t n) {
  const Value *out;
  if (blocked(m, a[0], OP_FLAG_IF, 0, 0, a, n, &out))
    return out;
  if (val_is(a[0], OP_FLAG_YES))
    return a[1];
  if (val_is(a[0], OP_FLAG_NO))
    return a[2];
  return internal(m, "a flagIf of a value that is not a Flag");
}

static const Value *reduce_proj(Machine *m, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n) {
  const Value *x = a[0];
  const Value *out;
  uint32_t at;
  if (blocked(m, x, OP_PROJ, inst, field, a, n, &out))
    return out;
  if (!val_is(x, OP_CTOR) || x->inst != inst || inst >= m->ctor_count || m->ctors[inst].family >= m->family_count)
    return internal(m, "a projection of a value of the wrong family");
  at = m->families[m->ctors[inst].family].param_count + field;
  return at < x->argc ? x->args[at] : internal(m, "a projection of a missing field");
}

static uint32_t varied_arity(const Machine *m, Op op, uint32_t inst) {
  const CtorInfo *c = inst < m->ctor_count ? &m->ctors[inst] : NULL;
  const FamilyInfo *f = inst < m->family_count ? &m->families[inst] : NULL;
  if (op == OP_CTOR && c != NULL && c->family < m->family_count)
    return m->families[c->family].param_count + c->field_count;
  if (op == OP_PCON && c != NULL && c->family < m->family_count)
    return m->families[c->family].param_count + c->field_count + c->binder_count;
  if (op == OP_FAMILY && f != NULL)
    return f->param_count;
  if (op == OP_FOLD_FAMILY && f != NULL)
    return f->ctor_count + 1u;
  if ((op == OP_FOLD_HIT || op == OP_ELIM) && f != NULL)
    return f->ctor_count + 2u;
  if (op == OP_FOLD_BLOCK && f != NULL && f->partner < m->family_count)
    return f->ctor_count + m->families[f->partner].ctor_count + 3u;
  if (op == OP_SYS)
    return inst * 2u;
  return UINT32_MAX;
}

static int arity_ok(const Machine *m, Op op, uint32_t inst, uint32_t n) {
  if ((size_t)op >= sizeof OP_ARITY)
    return 0;
  if (OP_ARITY[op] != VARIES)
    return n == (uint32_t)OP_ARITY[op];
  return n == varied_arity(m, op, inst);
}

/* M2 s4: the face normal form of an interval value. So conv compares two
   faces by their normal forms. */
static const Value *face_value(Machine *m, const Value *v) {
  IvlHeap h = {m->arena, m->diag, m->def};
  const Ivl *face = v->kind == VAL_IVL ? face_norm(&h, v->ivl) : NULL;
  if (face == NULL)
    return m->diag->set ? NULL : internal(m, "a face is not an interval value");
  return val_ivl(m, face);
}

/* M2 s4: a system with a branch on the face 1 is that branch (CCHM). Else
   the system is a canonical value with each face in face normal form. */
static const Value *reduce_sys(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Value **out = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *out);
  uint32_t k;
  if (out == NULL)
    return oom(m);
  for (k = 0; k + 1u < n; k += 2u) {
    out[k] = face_value(m, a[k]);
    if (out[k] == NULL)
      return NULL;
    if (ivl_is_const(out[k]->ivl, 1))
      return a[k + 1u];
    out[k + 1u] = a[k + 1u];
  }
  return val_op(m, VAL_OP, OP_SYS, inst, 0, out, n);
}

/* M4 s1: the boundary rule. ENV holds the parameters, the fields and the
   interval arguments of the path constructor CI, in that order. The first
   branch whose face is i1 gives the value. */
const Value *pcon_boundary(Machine *m, const CtorInfo *ci, const Env *env) {
  const Core *sys = ci->boundary;
  uint32_t k;
  if (sys == NULL)
    return internal(m, "a point constructor used as a path constructor");
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    const Value *fv = eval_core(m, env, sys->args[k]);
    fv = fv == NULL ? NULL : face_value(m, fv);
    if (fv == NULL)
      return NULL;
    if (ivl_is_const(fv->ivl, 1))
      return eval_core(m, env, sys->args[k + 1u]);
  }
  return NULL;
}

/* M4 s1: a path constructor at its arguments reduces by the boundary rule
   when a boundary face is i1, else it is canonical. */
static const Value *reduce_pcon(Machine *m, uint32_t inst, const Value *const *a, uint32_t n) {
  const Env *env = NULL;
  const Value *v;
  uint32_t i;
  if (inst >= m->ctor_count)
    return internal(m, "a path constructor outside the family tables");
  for (i = 0; i < n; i++)
    env = env_push(m, env, a[i]);
  v = pcon_boundary(m, &m->ctors[inst], env);
  if (v != NULL || m->diag->set)
    return v;
  return val_op(m, VAL_OP, OP_PCON, inst, 0, a, n);
}

/* M3 D4: Partial i1 A is A. Under a system branch the restriction makes the
   face i1, so a partial element there has the type A. */
static const Value *reduce_partial(Machine *m, const Value *const *a) {
  const Value *face = face_value(m, a[0]);
  if (face == NULL)
    return NULL;
  return ivl_is_const(face->ivl, 1) ? a[1] : val_make(m, OP_PARTIAL, 0, 2u, face, a[1], NULL);
}

/* M3 s4: a projection of one value. The argument array is in the arena,
   because a stuck projection keeps it. */
static const Value *project(Machine *m, Op op, const Value *v, Op ctor, uint32_t index) {
  const Value **a = arena_alloc(m->arena, sizeof *a);
  if (a == NULL)
    return oom(m);
  a[0] = v;
  return reduce_part(m, op, a, 1u, ctor, index);
}

/* M3 s4: the Glue operations. args: A (a, g), the face, the system. On the
   face i1 the system is its branch body (reduce_sys): Glue A [ i1 -> pack T e ]
   is T, glue [ i1 -> t ] a is t and unglue g is first e g. unglue (glue s a)
   is a. Else Glue and glue are canonical values and unglue is stuck. */
static const Value *reduce_glue(Machine *m, const Value *const *a) {
  const Value *face = face_value(m, a[1]);
  if (face == NULL)
    return NULL;
  return ivl_is_const(face->ivl, 1) ? project(m, OP_WITNESS, a[2], OP_PACK, 0) : val_make(m, OP_GLUE, 0, 3u, a[0], face, a[2]);
}

static const Value *reduce_glue_elem(Machine *m, const Value *const *a) {
  const Value *face = face_value(m, a[1]);
  if (face == NULL)
    return NULL;
  return ivl_is_const(face->ivl, 1) ? a[2] : val_make(m, OP_GLUE_ELEM, 0, 3u, a[0], face, a[2]);
}

static const Value *reduce_unglue(Machine *m, const Value *const *a, uint32_t n) {
  const Value *face = face_value(m, a[1]);
  const Value *e;
  const Value *f;
  const Value *out;
  if (face == NULL)
    return NULL;
  if (ivl_is_const(face->ivl, 1)) {
    e = project(m, OP_PAYLOAD, a[2], OP_PACK, 1u);
    f = e == NULL ? NULL : project(m, OP_WITNESS, e, OP_PACK, 0);
    return f == NULL ? NULL : apply_value(m, f, a[0]);
  }
  if (val_is(a[0], OP_GLUE_ELEM))
    return a[0]->args[0];
  /* A term checked on an outer face can still be a partial system when
     evaluated before that face is selected. Preserve its branches. */
  if (val_is(a[0], OP_SYS)) {
    const Value *sys = a[0];
    const Value **out = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *out);
    uint32_t k;
    if (out == NULL)
      return oom(m);
    for (k = 0; k + 1u < sys->argc; k += 2u) {
      const Value *args[3] = {sys->args[k + 1u], face, a[2]};
      out[k] = sys->args[k];
      out[k + 1u] = reduce_unglue(m, args, 3u);
      if (out[k + 1u] == NULL)
        return NULL;
    }
    return val_op(m, VAL_OP, OP_SYS, sys->inst, 0, out, sys->argc);
  }
  if (blocked(m, a[0], OP_UNGLUE, 0, 0, a, n, &out))
    return out;
  return internal(m, "unglue of a value that is not a glue element");
}

static const Value *reduce_op(Machine *m, Op op, uint32_t inst, uint32_t field, const Value *const *a, uint32_t n) {
  if (!arity_ok(m, op, inst, n))
    return internal(m, "an operation with the wrong number of arguments");
  switch (op) {
  case OP_NAT:
  case OP_FIELD:
  case OP_FIELD_LIT:
  case OP_FLAG:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_CTOR:
  case OP_PATHP:
  case OP_INTERVAL:
  case OP_SSET:
    return val_op(m, VAL_OP, op, inst, field, a, n);
  case OP_PCON:
    return reduce_pcon(m, inst, a, n);
  case OP_PARTIAL:
    return reduce_partial(m, a);
  case OP_SYS:
    return reduce_sys(m, inst, a, n);
  case OP_TRANSP:
    return kan_transp(m, a[0], face_value(m, a[1]), a[2]);
  case OP_KAN_DOM:
    return kan_dom(m, a[0], a[1], inst);
  case OP_KAN_COD:
    return kan_cod(m, a[0], a[1], a[2]);
  case OP_HCOMP:
    return kan_hcomp(m, a[0], face_value(m, a[1]), a[2], a[3]);
  case OP_KAN_FIELD:
    return reduce_field(m, inst, a, n);
  case OP_KAN_GLUE:
    return kan_glue(m, a[0], a[1], inst);
  case OP_KAN_L2E:
    return val_op(m, VAL_OP, OP_KAN_L2E, 0, 0, a, 1u);
  case OP_GLUE:
    return reduce_glue(m, a);
  case OP_GLUE_ELEM:
    return reduce_glue_elem(m, a);
  case OP_UNGLUE:
    return reduce_unglue(m, a, n);
  case OP_FIRST:
    return reduce_part(m, op, a, n, OP_PAIR, 0);
  case OP_SECOND:
    return reduce_part(m, op, a, n, OP_PAIR, 1);
  case OP_WITNESS:
    return reduce_part(m, op, a, n, OP_PACK, 0);
  case OP_PAYLOAD:
    return reduce_part(m, op, a, n, OP_PACK, 1);
  case OP_EITHER:
    return reduce_either(m, a, n);
  case OP_OPTION_ELIM:
    return reduce_option(m, a, n);
  case OP_PURE:
    return reduce_pure(m, inst, a[0]);
  case OP_MAP:
    return reduce_map(m, inst, a, n);
  case OP_BIND:
    return reduce_bind(m, inst, a, n);
  case OP_APPEND:
    return append_values(m, a[0], a[1]);
  case OP_FOLD_NAT:
    return reduce_fold_nat(m, a, n);
  case OP_FOLD_LIST:
    return reduce_fold_list(m, a);
  case OP_FOLD_FAMILY:
    return fold_family(m, inst, a, n - 1u, a[n - 1u]);
  case OP_FOLD_HIT:
    return fold_hit(m, OP_FOLD_HIT, inst, a[0], a + 1, n - 2u, a[n - 1u]);
  case OP_FOLD_BLOCK:
    return fold_block(m, inst, a, n - 3u, a[n - 1u]);
  case OP_ELIM:
    return fold_hit(m, OP_ELIM, inst, a[0], a + 1, n - 2u, a[n - 1u]);
  case OP_ELIM_CORE:
    return elim_core(m, inst, a, n);
  case OP_ELIM_NAT:
    return elim_nat(m, a, n);
  case OP_UNFOLD:
  case OP_UNFOLD_NAT:
    return reduce_unfold(m, op, a, n);
  case OP_FILTER:
    return reduce_filter(m, inst, a, n);
  case OP_FIELD_ADD:
  case OP_FIELD_SUB:
  case OP_FIELD_MUL:
    return field_op(m, op, a, n);
  case OP_NAT_ADD:
    return nat_add(m, a, n);
  case OP_NAT_SUB:
    return nat_sub(m, a, n);
  case OP_NAT_MUL:
    return nat_mul(m, a, n);
  case OP_NAT_EQ:
  case OP_NAT_LE:
    return nat_test(m, op, a, n);
  case OP_FLAG_IF:
    return reduce_flag_if(m, a, n);
  case OP_PROJ:
    return reduce_proj(m, inst, field, a, n);
  }
  return internal(m, "an unknown operation");
}

static const Value *eval_op(Machine *m, const Env *env, const Core *c) {
  const Value **args = arena_alloc(m->arena, ((size_t)c->argc + 1u) * sizeof *args);
  uint32_t i;
  if (args == NULL)
    return oom(m);
  if (c->op == OP_FIELD_LIT)
    return val_field(m, c->name);
  for (i = 0; i < c->argc; i++) {
    args[i] = eval_core(m, env, c->args[i]);
    if (args[i] == NULL)
      return NULL;
  }
  return reduce_op(m, c->op, c->inst, c->field, args, c->argc);
}

/* M2: the interval and the Path values. */
static IvlHeap ivl_heap(Machine *m) {
  IvlHeap h;
  h.arena = m->arena;
  h.diag = m->diag;
  h.def = m->def;
  return h;
}

const Value *val_ivl(Machine *m, const Ivl *ivl) {
  Value *v;
  if (ivl == NULL)
    return NULL;
  v = new_value(m, VAL_IVL);
  if (v != NULL)
    v->ivl = ivl;
  return v;
}

const Value *val_ivl_var(Machine *m, uint32_t level) {
  IvlHeap h = ivl_heap(m);
  return val_ivl(m, ivl_lit(&h, level, 0));
}

/* A path lambda or a lambda or Pi with domain I binds an interval variable. */
static int ivl_binder(const Value *v) {
  return v->kind == VAL_PLAM || ((v->kind == VAL_LAM || v->kind == VAL_PI) && val_is(v->dom, OP_INTERVAL));
}

const Value *val_ivl_end(Machine *m, int one) {
  IvlHeap h = ivl_heap(m);
  return val_ivl(m, ivl_const(&h, one));
}

static const Ivl *ivl_of(Machine *m, const Value *v) {
  if (v == NULL)
    return NULL;
  if (v->kind != VAL_IVL) {
    internal(m, "an interval operand that is not an interval");
    return NULL;
  }
  return v->ivl;
}

static const Value *eval_ivl(Machine *m, const Env *env, const Core *c) {
  IvlHeap h = ivl_heap(m);
  const Ivl *a;
  const Ivl *b;
  switch ((IvlOp)c->nat) {
  case IVL_ZERO:
    return val_ivl(m, ivl_const(&h, 0));
  case IVL_ONE:
    return val_ivl(m, ivl_const(&h, 1));
  case IVL_REV:
    a = ivl_of(m, eval_core(m, env, c->a));
    return a == NULL ? NULL : val_ivl(m, ivl_rev(&h, a));
  case IVL_MEET:
  case IVL_JOIN:
    a = ivl_of(m, eval_core(m, env, c->a));
    b = a == NULL ? NULL : ivl_of(m, eval_core(m, env, c->b));
    if (b == NULL)
      return NULL;
    return val_ivl(m, c->nat == IVL_MEET ? ivl_meet(&h, a, b) : ivl_join(&h, a, b));
  }
  return internal(m, "an unknown interval operation");
}

static const Value *papp_value(Machine *m, const Value *p, const Value *r, const Env *env, const Core *const *ends);

/* M3 s2: a partial element of a path type at R is the system of the
   branches at R, as sys_apply. The faces do not change. */
static const Value *sys_papp(Machine *m, const Value *sys, const Value *r, const Env *env, const Core *const *ends) {
  const Value **out = arena_alloc(m->arena, ((size_t)sys->argc + 1u) * sizeof *out);
  uint32_t k;
  if (out == NULL)
    return oom(m);
  for (k = 0; k + 1u < sys->argc; k += 2u) {
    out[k] = sys->args[k];
    out[k + 1u] = papp_value(m, sys->args[k + 1u], r, env, ends);
    if (out[k + 1u] == NULL)
      return NULL;
  }
  return val_op(m, VAL_OP, OP_SYS, sys->inst, 0, out, sys->argc);
}

/* P @ R. ENDS: the two ends of the type of P in ENV. They are NULL only in
   the eta rule of conv, where R is a fresh variable. */
static const Value *papp_value(Machine *m, const Value *p, const Value *r, const Env *env, const Core *const *ends) {
  Value *v;
  const Value **vals;
  int one;
  if (p == NULL || r == NULL)
    return NULL;
  if (r->kind != VAL_IVL)
    return internal(m, "a path applied to a value that is not an interval");
  if (p->kind == VAL_PLAM)
    return closure_apply(m, p, r);
  if (p->kind == VAL_TRAP)
    return p;
  one = ivl_is_const(r->ivl, 1);
  if (ends != NULL && (one || ivl_is_const(r->ivl, 0)))
    return eval_core(m, env, ends[one]);
  if (val_is(p, OP_SYS))
    return sys_papp(m, p, r, env, ends);
  if (scrut_of(p) != SCRUT_NEUTRAL)
    return internal(m, "an application of a value that is not a path");
  v = new_value(m, VAL_PAPP);
  if (v == NULL)
    return NULL;
  v->dom = p;
  v->arg = r;
  if (ends == NULL)
    return v;
  vals = arena_alloc(m->arena, 2u * sizeof *vals);
  if (vals == NULL)
    return oom(m);
  vals[0] = eval_core(m, env, ends[0]);
  vals[1] = eval_core(m, env, ends[1]);
  if (vals[0] == NULL || vals[1] == NULL)
    return NULL;
  v->args = vals;
  v->argc = 2u;
  return v;
}

static const Value *eval_inner(Machine *m, const Env *env, const Core *c) {
  switch (c->kind) {
  case CORE_VAR:
    return env_lookup(m, env, c->index);
  case CORE_GLOBAL:
    return def_value(m, c->index);
  case CORE_NAT:
    return val_nat(m, c->nat);
  case CORE_TRAP:
    return val_trap(m);
  case CORE_UNIV:
    return val_univ(m, c->nat);
  case CORE_LAM:
    return val_closure(m, VAL_LAM, c->name, c->a == NULL ? NULL : eval_core(m, env, c->a), env, c->b);
  case CORE_PI:
    return val_closure(m, VAL_PI, c->name, eval_core(m, env, c->a), env, c->b);
  case CORE_SIGMA:
    return val_closure(m, VAL_SIGMA, c->name, eval_core(m, env, c->a), env, c->b);
  case CORE_APP:
    return apply_value(m, eval_core(m, env, c->a), eval_core(m, env, c->b));
  case CORE_OP:
    return eval_op(m, env, c);
  case CORE_IVL:
    return eval_ivl(m, env, c);
  case CORE_PLAM:
    return val_closure(m, VAL_PLAM, c->name, NULL, env, c->b);
  case CORE_PAPP:
    return papp_value(m, eval_core(m, env, c->a), eval_core(m, env, c->b), env, c->argc == 2u ? c->args : NULL);
  }
  return internal(m, "an unknown core term");
}

const Value *eval_core(Machine *m, const Env *env, const Core *c) {
  const Value *v;
  if (c == NULL || !spend(m, 1) || !enter(m))
    return NULL;
  v = eval_inner(m, env, c);
  if (v != NULL && c->sort_level != 0u && v->sort_level != c->sort_level) {
    Value *typed = new_value(m, v->kind);
    if (typed == NULL)
      v = NULL;
    else {
      *typed = *v;
      typed->sort_level = c->sort_level;
      v = typed;
    }
  }
  m->depth--;
  return v;
}

/* Eta: a function is equal to G when the two agree on a fresh variable. */
static int conv_fun(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *x = ivl_binder(a) || ivl_binder(b) ? val_ivl_var(m, level) : val_var(m, level);
  return conv_values(m, level + 1u, apply_value(m, a, x), apply_value(m, b, x));
}

static int conv_binder(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *x = ivl_binder(a) || ivl_binder(b) ? val_ivl_var(m, level) : val_var(m, level);
  return conv_values(m, level, a->dom, b->dom) && conv_values(m, level + 1u, closure_apply(m, a, x), closure_apply(m, b, x));
}

static int same_head(const Value *a, const Value *b) {
  return a->op == b->op && a->inst == b->inst && a->field == b->field && a->argc == b->argc;
}

/* Product and Sigma eta: compare a pair with the projections of a neutral. */
static int conv_pair(Machine *m, uint32_t level, const Value *pair, const Value *neutral) {
  Op first = pair->op == OP_PAIR ? OP_FIRST : OP_WITNESS;
  Op second = pair->op == OP_PAIR ? OP_SECOND : OP_PAYLOAD;
  return conv_values(m, level, pair->args[0], make(m, VAL_STUCK, first, 0, 1, neutral, NULL, NULL))
      && conv_values(m, level, pair->args[1], make(m, VAL_STUCK, second, 0, 1, neutral, NULL, NULL));
}

/* Path eta: a path is equal to Q when the two agree on a fresh interval
   variable. The other side must be a path or a neutral. */
static int conv_path(Machine *m, uint32_t level, const Value *a, const Value *b) {
  const Value *x;
  if ((a->kind != VAL_PLAM && scrut_of(a) != SCRUT_NEUTRAL) || (b->kind != VAL_PLAM && scrut_of(b) != SCRUT_NEUTRAL))
    return 0;
  x = val_ivl_var(m, level);
  return conv_values(m, level + 1u, papp_value(m, a, x, NULL, NULL), papp_value(m, b, x, NULL, NULL));
}

/* Record the free interval levels before building an identity environment.
   Opening closures also exposes interval occurrences in their bodies. */
static int interval_levels(Machine *m, uint32_t level, uint32_t free_count, const Value *v, unsigned char *levels) {
  uint32_t i;
  int ok = 1;
  if (v == NULL || !enter(m))
    return 0;
  switch (v->kind) {
  case VAL_IVL:
    for (i = 0; i < v->ivl->starts[v->ivl->count]; i++)
      if (v->ivl->lits[i] / 2u < free_count)
        levels[v->ivl->lits[i] / 2u] = 1;
    break;
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
    ok = (v->dom == NULL || interval_levels(m, level, free_count, v->dom, levels))
      && interval_levels(m, level + 1u, free_count,
        closure_apply(m, v, ivl_binder(v) ? val_ivl_var(m, level) : val_var(m, level)), levels);
    break;
  case VAL_APP:
  case VAL_PAPP:
    ok = interval_levels(m, level, free_count, v->dom, levels)
      && interval_levels(m, level, free_count, v->arg, levels);
    for (i = 0; ok && i < v->argc; i++)
      ok = interval_levels(m, level, free_count, v->args[i], levels);
    break;
  case VAL_OP:
  case VAL_STUCK:
    for (i = 0; ok && i < v->argc; i++)
      ok = interval_levels(m, level, free_count, v->args[i], levels);
    break;
  case VAL_NAT:
  case VAL_TRAP:
  case VAL_UNIV:
  case VAL_VAR:
    break;
  }
  m->depth--;
  return ok;
}

static int free_bound(Machine *m, uint32_t level, uint32_t open, const Value *v, uint32_t *bound);

/* M3 s5: a closure is not opened, because its body would evaluate again
   (with each Kan operation in it). Each value of its environment counts,
   so the bound is above the free levels of the body. */
static int env_bound(Machine *m, uint32_t level, uint32_t open, const Env *env, uint32_t *bound) {
  int ok = 1;
  for (; ok && env != NULL; env = env->next)
    ok = env->value == NULL || free_bound(m, level, open, env->value, bound);
  return ok;
}

/* M3 s2: raise *BOUND above each free level of V below OPEN. */
static int free_bound(Machine *m, uint32_t level, uint32_t open, const Value *v, uint32_t *bound) {
  uint32_t i;
  int ok = 1;
  if (v == NULL || !enter(m))
    return 0;
  switch (v->kind) {
  case VAL_IVL:
    for (i = 0; i < v->ivl->starts[v->ivl->count]; i++)
      if (v->ivl->lits[i] / 2u < open && v->ivl->lits[i] / 2u >= *bound)
        *bound = v->ivl->lits[i] / 2u + 1u;
    break;
  case VAL_VAR:
    if (v->nat < open && v->nat >= *bound)
      *bound = (uint32_t)v->nat + 1u;
    break;
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
    ok = (v->dom == NULL || free_bound(m, level, open, v->dom, bound))
      && (v->body == NULL ? free_bound(m, level, open, v->arg, bound) : env_bound(m, level, open, v->env, bound));
    break;
  case VAL_APP:
  case VAL_PAPP:
    ok = free_bound(m, level, open, v->dom, bound) && free_bound(m, level, open, v->arg, bound);
    for (i = 0; ok && i < v->argc; i++)
      ok = free_bound(m, level, open, v->args[i], bound);
    break;
  case VAL_OP:
  case VAL_STUCK:
    for (i = 0; ok && i < v->argc; i++)
      ok = free_bound(m, level, open, v->args[i], bound);
    break;
  case VAL_NAT:
  case VAL_TRAP:
  case VAL_UNIV:
    break;
  }
  m->depth--;
  return ok;
}

int value_bound(Machine *m, const Value *v, uint32_t *bound) {
  /* A nested scan must count the open variables of its caller as free.
     Depth also grows with the closure binders opened by free_bound, so
     this start is above every open variable of an active outer scan. */
  uint32_t open = VALUE_BOUND_OPEN + m->depth;
  return free_bound(m, open, open, v, bound);
}

/* Substitute a conjunction into a value, preserving the distinction between
   ordinary variables and interval variables in the identity environment. */
const Value *value_on_face(Machine *m, uint32_t level, const Value *v, const Ivl *face, uint32_t k) {
  unsigned char *levels = arena_alloc(m->arena, (size_t)level + 1u);
  const Env *env = NULL;
  uint32_t i;
  if (levels == NULL)
    return oom(m);
  if (!interval_levels(m, level, level, v, levels))
    return NULL;
  for (i = 0; i < level; i++) {
    int one = 0;
    const Value *x = face_conj_sets(face, k, i, &one) ? val_ivl_end(m, one)
      : levels[i] ? val_ivl_var(m, i) : val_var(m, i);
    env = env_push(m, env, x);
    if (env == NULL)
      return NULL;
  }
  return eval_core(m, env, quote_value(m, level, v));
}

/* Systems are equal when their covers coincide and their bodies agree on
   every overlap. Branch order, duplicates and face decomposition do not
   change a partial element. */
static int conv_sys(Machine *m, uint32_t level, const Value *a, const Value *b) {
  IvlHeap h = ivl_heap(m);
  const Ivl *ac = ivl_const(&h, 0);
  const Ivl *bc = ivl_const(&h, 0);
  uint32_t i, j, k;
  for (i = 0; i < a->argc && ac != NULL; i += 2u)
    ac = face_join(&h, ac, a->args[i]->ivl);
  for (i = 0; i < b->argc && bc != NULL; i += 2u)
    bc = face_join(&h, bc, b->args[i]->ivl);
  if (ac == NULL || bc == NULL || !face_equal(ac, bc))
    return 0;
  for (i = 0; i < a->argc; i += 2u) {
    for (j = 0; j < b->argc; j += 2u) {
      const Ivl *meet = face_meet(&h, a->args[i]->ivl, b->args[j]->ivl);
      if (meet == NULL)
        return 0;
      for (k = 0; k < meet->count; k++) {
        const Value *x = value_on_face(m, level, a->args[i + 1u], meet, k);
        const Value *y = value_on_face(m, level, b->args[j + 1u], meet, k);
        if (!conv_values(m, level, x, y))
          return 0;
      }
    }
  }
  return 1;
}

/* M3 s4: Glue eta. A neutral G is equal to glue [ s ] a when unglue G is
   equal to a and, on each conjunction of the face, G is equal to the branch
   of s. The face and the system come from the glue value, not from a type.
   The stuck unglue keeps s, because conv compares two stuck unglue values
   by G only (as the ends of a VAL_PAPP). */
static int conv_glue(Machine *m, uint32_t level, const Value *glue, const Value *g) {
  const Ivl *face = glue->args[1]->ivl;
  uint32_t k;
  if (!conv_values(m, level, glue->args[0], make(m, VAL_STUCK, OP_UNGLUE, 0, 3u, g, glue->args[1], glue->args[2])))
    return 0;
  for (k = 0; k < face->count; k++)
    if (!conv_values(m, level, value_on_face(m, level, g, face, k), value_on_face(m, level, glue->args[2], face, k)))
      return 0;
  return 1;
}

/* The last argument of an operation is compared in the loop, so a long
   list does not nest. */
static int conv_loop(Machine *m, uint32_t level, const Value *a, const Value *b) {
  uint32_t i;
  while (a != NULL && b != NULL && spend(m, 1)) {
    if (a->kind == VAL_PLAM || b->kind == VAL_PLAM)
      return conv_path(m, level, a, b);
    if (a->kind == VAL_LAM || b->kind == VAL_LAM)
      return conv_fun(m, level, a, b);
    /* M3 s5: two lineToEquiv compare their lines;  one unfolds to its pack. */
    if (val_is(a, OP_KAN_L2E) != val_is(b, OP_KAN_L2E)) {
      a = val_is(a, OP_KAN_L2E) ? kan_l2e(m, a->args[0]) : a;
      b = val_is(b, OP_KAN_L2E) ? kan_l2e(m, b->args[0]) : b;
      continue;
    }
    if ((val_is(a, OP_PAIR) || val_is(a, OP_PACK)) && scrut_of(b) == SCRUT_NEUTRAL)
      return conv_pair(m, level, a, b);
    if ((val_is(b, OP_PAIR) || val_is(b, OP_PACK)) && scrut_of(a) == SCRUT_NEUTRAL)
      return conv_pair(m, level, b, a);
    if (val_is(a, OP_GLUE_ELEM) && scrut_of(b) == SCRUT_NEUTRAL)
      return conv_glue(m, level, a, b);
    if (val_is(b, OP_GLUE_ELEM) && scrut_of(a) == SCRUT_NEUTRAL)
      return conv_glue(m, level, b, a);
    if (a->kind != b->kind)
      return 0;
    switch (a->kind) {
    case VAL_NAT:
    case VAL_UNIV:
    case VAL_VAR:
      return a->nat == b->nat;
    case VAL_TRAP:
      /* Overflow is not equality evidence. Equating traps would compose
         successor laws specialized at UINT64_MAX into a path from 0 to 1. */
      return 0;
    case VAL_LAM:
    case VAL_PLAM:
      return 0;
    case VAL_IVL:
      return ivl_equal(a->ivl, b->ivl);
    case VAL_PAPP:
      if (a->arg->kind != VAL_IVL || b->arg->kind != VAL_IVL || !ivl_equal(a->arg->ivl, b->arg->ivl))
        return 0;
      a = a->dom;
      b = b->dom;
      break;
    case VAL_PI:
    case VAL_SIGMA:
      return conv_binder(m, level, a, b);
    case VAL_APP:
      if (!conv_values(m, level, a->dom, b->dom))
        return 0;
      a = a->arg;
      b = b->arg;
      break;
    case VAL_OP:
    case VAL_STUCK:
      if (val_is(a, OP_SYS) && val_is(b, OP_SYS))
        return conv_sys(m, level, a, b);
      if (a->kind == VAL_STUCK && a->op == OP_UNGLUE && b->op == OP_UNGLUE) {
        a = a->args[0];
        b = b->args[0];
        break;
      }
      if (!same_head(a, b))
        return 0;
      if (a->op == OP_FIELD_LIT)
        return strcmp(a->name, b->name) == 0;
      if (a->argc == 0)
        return 1;
      for (i = 0; i + 1u < a->argc; i++)
        if (!conv_values(m, level, a->args[i], b->args[i]))
          return 0;
      a = a->args[a->argc - 1u];
      b = b->args[b->argc - 1u];
      break;
    }
  }
  return 0;
}

int conv_values(Machine *m, uint32_t level, const Value *a, const Value *b) {
  int same;
  if (a == NULL || b == NULL || !enter(m))
    return 0;
  same = conv_loop(m, level, a, b);
  m->depth--;
  return same;
}

static Core *new_core(Machine *m, CoreKind kind) {
  Core *c = arena_alloc(m->arena, sizeof *c);
  if (c == NULL) {
    oom(m);
    return NULL;
  }
  c->kind = kind;
  return c;
}

static const Core *core_leaf(Machine *m, CoreKind kind, uint64_t nat) {
  Core *c = new_core(m, kind);
  if (c != NULL)
    c->nat = nat;
  return c;
}

static const Core *quote_var(Machine *m, uint32_t level, const Value *v) {
  Core *c;
  if (v->nat >= level) {
    internal(m, "a variable above its level");
    return NULL;
  }
  c = new_core(m, CORE_VAR);
  if (c != NULL)
    c->index = level - 1u - (uint32_t)v->nat;
  return c;
}

static const Core *quote_app(Machine *m, uint32_t level, const Value *v) {
  const Core *fn = quote_value(m, level, v->dom);
  const Core *arg = quote_value(m, level, v->arg);
  Core *c;
  if (fn == NULL || arg == NULL)
    return NULL;
  c = new_core(m, CORE_APP);
  if (c == NULL)
    return NULL;
  c->a = fn;
  c->b = arg;
  return c;
}

static const Core *quote_binder(Machine *m, uint32_t level, const Value *v, CoreKind kind) {
  int bare = kind == CORE_PLAM || (kind == CORE_LAM && v->dom == NULL);
  const Core *dom = bare ? NULL : quote_value(m, level, v->dom);
  const Value *x = ivl_binder(v) ? val_ivl_var(m, level) : val_var(m, level);
  const Core *body = quote_value(m, level + 1u, closure_apply(m, v, x));
  Core *c;
  if (body == NULL || (dom == NULL && !bare))
    return NULL;
  c = new_core(m, kind);
  if (c == NULL)
    return NULL;
  c->name = v->name;
  c->a = dom;
  c->b = body;
  return c;
}

static const Core *quote_op(Machine *m, uint32_t level, const Value *v) {
  const Core **args = arena_alloc(m->arena, ((size_t)v->argc + 1u) * sizeof *args);
  Core *c;
  uint32_t i;
  if (args == NULL) {
    oom(m);
    return NULL;
  }
  for (i = 0; i < v->argc; i++) {
    args[i] = quote_value(m, level, v->args[i]);
    if (args[i] == NULL)
      return NULL;
  }
  c = new_core(m, CORE_OP);
  if (c == NULL)
    return NULL;
  c->op = v->op;
  c->inst = v->inst;
  c->field = v->field;
  c->name = v->name; /* OP_FIELD_LIT: the digits */
  c->args = args;
  c->argc = v->argc;
  return c;
}

static const Core *core_ivl(Machine *m, IvlOp op, const Core *a, const Core *b) {
  Core *c = new_core(m, CORE_IVL);
  if (c == NULL)
    return NULL;
  c->nat = op;
  c->a = a;
  c->b = b;
  return c;
}

static const Core *quote_lit(Machine *m, uint32_t level, uint32_t lit) {
  Core *c;
  if (lit / 2u >= level) {
    internal(m, "an interval variable above its level");
    return NULL;
  }
  c = new_core(m, CORE_VAR);
  if (c == NULL)
    return NULL;
  c->index = level - 1u - lit / 2u;
  return (lit & 1u) == 0u ? c : core_ivl(m, IVL_REV, c, NULL);
}

/* The normal form as a join of meets of literals. */
static const Core *quote_ivl(Machine *m, uint32_t level, const Ivl *ivl) {
  const Core *out = NULL;
  uint32_t k, i;
  for (k = 0; k < ivl->count; k++) {
    const Core *conj = NULL;
    for (i = ivl->starts[k]; i < ivl->starts[k + 1u]; i++) {
      const Core *lit = quote_lit(m, level, ivl->lits[i]);
      if (lit == NULL)
        return NULL;
      conj = conj == NULL ? lit : core_ivl(m, IVL_MEET, conj, lit);
      if (conj == NULL)
        return NULL;
    }
    if (conj == NULL)
      conj = core_ivl(m, IVL_ONE, NULL, NULL);
    out = out == NULL ? conj : core_ivl(m, IVL_JOIN, out, conj);
    if (out == NULL)
      return NULL;
  }
  return out != NULL ? out : core_ivl(m, IVL_ZERO, NULL, NULL);
}

static const Core *quote_papp(Machine *m, uint32_t level, const Value *v) {
  const Core *p = quote_value(m, level, v->dom);
  const Core *r = quote_value(m, level, v->arg);
  const Core **ends;
  Core *c;
  if (p == NULL || r == NULL)
    return NULL;
  c = new_core(m, CORE_PAPP);
  if (c == NULL)
    return NULL;
  c->a = p;
  c->b = r;
  if (v->argc != 2u)
    return c;
  ends = arena_alloc(m->arena, 2u * sizeof *ends);
  if (ends == NULL) {
    oom(m);
    return NULL;
  }
  ends[0] = quote_value(m, level, v->args[0]);
  ends[1] = quote_value(m, level, v->args[1]);
  if (ends[0] == NULL || ends[1] == NULL)
    return NULL;
  c->args = ends;
  c->argc = 2u;
  return c;
}

static const Core *quote_inner(Machine *m, uint32_t level, const Value *v) {
  switch (v->kind) {
  case VAL_NAT:
    return core_leaf(m, CORE_NAT, v->nat);
  case VAL_TRAP:
    return core_leaf(m, CORE_TRAP, 0);
  case VAL_UNIV:
    return core_leaf(m, CORE_UNIV, v->nat);
  case VAL_VAR:
    return quote_var(m, level, v);
  case VAL_APP:
    return quote_app(m, level, v);
  case VAL_LAM:
    return quote_binder(m, level, v, CORE_LAM);
  case VAL_PI:
    return quote_binder(m, level, v, CORE_PI);
  case VAL_SIGMA:
    return quote_binder(m, level, v, CORE_SIGMA);
  case VAL_OP:
  case VAL_STUCK:
    return quote_op(m, level, v);
  case VAL_IVL:
    return quote_ivl(m, level, v->ivl);
  case VAL_PLAM:
    return quote_binder(m, level, v, CORE_PLAM);
  case VAL_PAPP:
    return quote_papp(m, level, v);
  }
  internal(m, "an unknown value");
  return NULL;
}

const Core *quote_value(Machine *m, uint32_t level, const Value *v) {
  const Core *c;
  if (v == NULL || !enter(m))
    return NULL;
  c = quote_inner(m, level, v);
  if (c != NULL && v->sort_level != 0u) {
    Core *typed = new_core(m, c->kind);
    if (typed == NULL)
      c = NULL;
    else {
      *typed = *c;
      typed->sort_level = v->sort_level;
      c = typed;
    }
  }
  m->depth--;
  return c;
}

const char *op_name(const Machine *m, Op op, uint32_t inst, uint32_t field) {
  if (op == OP_FAMILY)
    return inst < m->family_count ? m->families[inst].name : "?";
  if (op == OP_CTOR || op == OP_PCON)
    return inst < m->ctor_count ? m->ctors[inst].name : "?";
  if (op == OP_PROJ)
    return inst < m->ctor_count && field < m->ctors[inst].field_count ? m->ctors[inst].fields[field].name : "?";
  return (size_t)op < sizeof OP_NAMES / sizeof OP_NAMES[0] ? OP_NAMES[op] : "?";
}

/* The printer keeps len + 4 <= cap, so "..." always fits. */
typedef struct {
  Machine *m;
  char *buf;
  size_t cap;
  size_t len;
  int full;
  const char *const *names;
  uint32_t name_count;
  const char *extra[PRINT_NAMES_MAX];
  uint32_t extra_count;
  unsigned depth;
  int fields_free; /* M6 s1: a binder may keep a field name (messages) */
} Printer;

static void put(Printer *p, const char *s) {
  size_t n = strlen(s);
  if (p->full)
    return;
  if (p->len + n + 4u <= p->cap) {
    memcpy(p->buf + p->len, s, n + 1u);
    p->len += n;
    return;
  }
  memcpy(p->buf + p->len, "...", 4u);
  p->len += 3u;
  p->full = 1;
}

static const char *var_name(const Printer *p, uint64_t level) {
  if (level < p->name_count && p->names[level] != NULL)
    return p->names[level];
  if (level >= p->name_count && level - p->name_count < p->extra_count
      && level - p->name_count < PRINT_NAMES_MAX)
    return p->extra[level - p->name_count];
  return "_";
}

static void print_value(Printer *p, const Value *v, int atom);

static int is_chain(const Value *v) {
  return v != NULL && (v->kind == VAL_OP || v->kind == VAL_STUCK) && v->argc > 0;
}

/* A printed binder must not capture another variable or a global name. */
/* M2: the interval and Path names of the checker. */
static const char *const PATH_NAMES[] = {"I", "i0", "i1", "Path"};

static int name_used(const Printer *p, const char *name) {
  uint32_t i;
  uint32_t j;
  if (name[0] == '%')
    return 1; /* checker-generated eta names are not surface identifiers */
  for (i = 0; i < p->name_count; i++)
    if (p->names[i] != NULL && strcmp(name, p->names[i]) == 0)
      return 1;
  for (i = 0; i < p->extra_count && i < PRINT_NAMES_MAX; i++)
    if (strcmp(name, p->extra[i]) == 0)
      return 1;
  for (i = 0; i < sizeof OP_NAMES / sizeof OP_NAMES[0]; i++)
    if (strcmp(name, OP_NAMES[i]) == 0)
      return 1;
  for (i = 0; i < sizeof PATH_NAMES / sizeof PATH_NAMES[0]; i++)
    if (strcmp(name, PATH_NAMES[i]) == 0)
      return 1;
  for (i = 0; i < p->m->def_count; i++)
    if (strcmp(name, p->m->defs[i].name) == 0)
      return 1;
  for (i = 0; i < p->m->family_count; i++)
    if (strcmp(name, p->m->families[i].name) == 0)
      return 1;
  for (i = 0; i < p->m->ctor_count; i++) {
    const CtorInfo *c = &p->m->ctors[i];
    if (strcmp(name, c->name) == 0)
      return 1;
    for (j = 0; !p->fields_free && j < c->field_count; j++)
      if (strcmp(name, c->fields[j].name) == 0)
        return 1;
  }
  return 0;
}

static void print_binder(Printer *p, const Value *v) {
  uint32_t level = p->name_count + p->extra_count;
  const char *name = v->name != NULL ? v->name : "x";
  char fresh[32];
  uint32_t suffix = 0;
  const Value *x = ivl_binder(v) ? val_ivl_var(p->m, level) : val_var(p->m, level);
  const Value *body = closure_apply(p->m, v, x);
  int binder_dom = v->dom != NULL && (v->dom->kind == VAL_PI || v->dom->kind == VAL_SIGMA || v->dom->kind == VAL_LAM || v->dom->kind == VAL_UNIV);
  while (name_used(p, name)) {
    snprintf(fresh, sizeof fresh, "_x%u", suffix++);
    name = fresh;
  }
  switch (v->kind) {
  case VAL_LAM:
    put(p, "fun ");
    put(p, name);
    put(p, " => ");
    break;
  case VAL_PI:
    if (v->name == NULL) {
      print_value(p, v->dom, binder_dom);
      put(p, " -> ");
      break;
    }
    put(p, "(");
    put(p, name);
    put(p, " : ");
    print_value(p, v->dom, 0);
    put(p, ") -> ");
    break;
  case VAL_SIGMA:
    put(p, "Sigma (");
    put(p, name);
    put(p, " : ");
    print_value(p, v->dom, 0);
    put(p, ") ");
    break;
  case VAL_PLAM:
    put(p, "<");
    put(p, name);
    put(p, "> ");
    break;
  case VAL_NAT:
  case VAL_TRAP:
  case VAL_UNIV:
  case VAL_OP:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
  case VAL_IVL:
  case VAL_PAPP:
    break;
  }
  if (p->extra_count < PRINT_NAMES_MAX)
    p->extra[p->extra_count] = name;
  p->extra_count++;
  print_value(p, body, v->kind == VAL_SIGMA);
  p->extra_count--;
}

/* An interval normal form: a join of meets of literals. */
static void print_ivl(Printer *p, const Ivl *ivl, int atom) {
  uint32_t k, i;
  int wrap = atom && (ivl->count > 1u || (ivl->count == 1u && ivl->starts[1] - ivl->starts[0] > 1u));
  if (ivl->count == 0u) {
    put(p, "i0");
    return;
  }
  if (wrap)
    put(p, "(");
  for (k = 0; k < ivl->count; k++) {
    uint32_t len = ivl->starts[k + 1u] - ivl->starts[k];
    int group = ivl->count > 1u && len > 1u;
    if (k > 0u)
      put(p, " \\/ ");
    if (len == 0u)
      put(p, "i1");
    if (group)
      put(p, "(");
    for (i = ivl->starts[k]; i < ivl->starts[k + 1u]; i++) {
      if (i > ivl->starts[k])
        put(p, " /\\ ");
      if ((ivl->lits[i] & 1u) != 0u)
        put(p, "~");
      put(p, var_name(p, ivl->lits[i] / 2u));
    }
    if (group)
      put(p, ")");
  }
  if (wrap)
    put(p, ")");
}

/* A when V is PathP over a constant line <i> A, else NULL. The line is
   constant when it is the same at i0 and at a fresh variable. */
static const Value *path_line(Printer *p, const Value *v) {
  uint32_t level = p->name_count + p->extra_count;
  const Value *a0;
  if (v->op != OP_PATHP || v->argc != 3u || v->args[0]->kind != VAL_PLAM)
    return NULL;
  a0 = closure_apply(p->m, v->args[0], val_ivl_end(p->m, 0));
  return conv_values(p->m, level + 1u, a0, closure_apply(p->m, v->args[0], val_ivl_var(p->m, level))) ? a0 : NULL;
}

static void print_other(Printer *p, const Value *v, int atom) {
  char num[32];
  if (v == NULL) {
    put(p, "?");
    return;
  }
  switch (v->kind) {
  case VAL_NAT:
    snprintf(num, sizeof num, "%llu", (unsigned long long)v->nat);
    put(p, num);
    return;
  case VAL_TRAP:
    put(p, "trap");
    return;
  case VAL_VAR:
    put(p, var_name(p, v->nat));
    return;
  case VAL_OP:
  case VAL_STUCK:
    put(p, val_is(v, OP_FIELD_LIT) ? v->name : op_name(p->m, v->op, v->inst, v->field));
    return;
  case VAL_IVL:
    print_ivl(p, v->ivl, atom);
    return;
  case VAL_UNIV:
  case VAL_APP:
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
  case VAL_PAPP:
    break;
  }
  if (atom)
    put(p, "(");
  switch (v->kind) {
  case VAL_UNIV:
    snprintf(num, sizeof num, "Type %llu", (unsigned long long)v->nat);
    put(p, num);
    break;
  case VAL_APP:
    print_value(p, v->dom, v->dom->kind != VAL_APP && v->dom->kind != VAL_VAR);
    put(p, " ");
    print_value(p, v->arg, 1);
    break;
  case VAL_PAPP:
    print_value(p, v->dom, v->dom->kind != VAL_APP && v->dom->kind != VAL_PAPP && v->dom->kind != VAL_VAR);
    put(p, " @ ");
    print_value(p, v->arg, 1);
    break;
  case VAL_LAM:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
    print_binder(p, v);
    break;
  case VAL_NAT:
  case VAL_TRAP:
  case VAL_VAR:
  case VAL_OP:
  case VAL_STUCK:
  case VAL_IVL:
    break;
  }
  if (atom)
    put(p, ")");
}

/* M3 s4: Glue A [ ... ], glue [ ... ] a and unglue g. The face is the join
   of the branch faces, so the printer does not show it. */
static int is_glue(const Value *v) {
  return (v->kind == VAL_OP || v->kind == VAL_STUCK) && v->argc == 3u
      && (v->op == OP_GLUE || v->op == OP_GLUE_ELEM || v->op == OP_UNGLUE);
}

static void print_glue(Printer *p, const Value *v, int atom) {
  if (atom)
    put(p, "(");
  put(p, op_name(p->m, v->op, v->inst, v->field));
  put(p, " ");
  if (v->op == OP_GLUE) {
    print_value(p, v->args[0], 1);
    put(p, " ");
  }
  if (v->op != OP_UNGLUE)
    print_value(p, v->args[2], 0);
  if (v->op == OP_GLUE_ELEM)
    put(p, " ");
  if (v->op != OP_GLUE)
    print_value(p, v->args[0], 1);
  if (atom)
    put(p, ")");
}

/* M4 s1: a path constructor at its interval arguments prints as its name,
   the parameters and the fields as atoms, then ` @ r` for each interval
   argument. */
static int is_pcon(const Value *v) {
  return (v->kind == VAL_OP || v->kind == VAL_STUCK) && v->op == OP_PCON;
}

static void print_pcon(Printer *p, const Value *v, int atom) {
  uint32_t k = v->inst < p->m->ctor_count ? p->m->ctors[v->inst].binder_count : 0u;
  uint32_t fixed = v->argc >= k ? v->argc - k : v->argc;
  uint32_t i;
  if (atom)
    put(p, "(");
  put(p, op_name(p->m, v->op, v->inst, v->field));
  for (i = 0; i < v->argc; i++) {
    put(p, i < fixed ? " " : " @ ");
    print_value(p, v->args[i], 1);
  }
  if (atom)
    put(p, ")");
}

/* An operation prints as its name and its arguments. The last argument is
   printed in the loop, so a long list does not nest. */
static void print_value(Printer *p, const Value *v, int atom) {
  const Value *line;
  uint32_t open = 0;
  uint32_t i;
  /* A body under a binder that fails to evaluate (OOM) is NULL;  the
     diagnostic is set and value_print reports it. */
  if (v == NULL) {
    p->full = 1;
    return;
  }
  if (p->depth >= PRINT_DEPTH_MAX) {
    put(p, "...");
    p->full = 1;
    return;
  }
  p->depth++;
  if (val_is(v, OP_SYS)) {
    /* M2 s4: [ phi -> t, psi -> u ] */
    put(p, "[ ");
    for (i = 0; i + 1u < v->argc; i += 2u) {
      if (i > 0u)
        put(p, ", ");
      print_value(p, v->args[i], 0);
      put(p, " -> ");
      print_value(p, v->args[i + 1u], 0);
    }
    put(p, " ]");
    p->depth--;
    return;
  }
  if (is_glue(v)) {
    print_glue(p, v, atom);
    p->depth--;
    return;
  }
  if (is_pcon(v)) {
    print_pcon(p, v, atom);
    p->depth--;
    return;
  }
  while (is_chain(v) && !p->full) {
    if (atom) {
      put(p, "(");
      open++;
    }
    line = path_line(p, v);
    put(p, line != NULL ? "Path" : op_name(p->m, v->op, v->inst, v->field));
    for (i = 0; i + 1u < v->argc; i++) {
      put(p, " ");
      print_value(p, i == 0u && line != NULL ? line : v->args[i], 1);
    }
    put(p, " ");
    v = v->args[v->argc - 1u];
    atom = 1;
  }
  print_other(p, v, atom);
  for (i = 0; i < open; i++)
    put(p, ")");
  p->depth--;
}

static int print_with(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap, int fields_free) {
  Printer p;
  if (cap == 0)
    return 0;
  buf[0] = '\0';
  if (cap < 4u)
    return 0;
  memset(&p, 0, sizeof p);
  p.m = m;
  p.buf = buf;
  p.cap = cap;
  p.names = names;
  p.name_count = name_count;
  p.fields_free = fields_free;
  print_value(&p, v, 0);
  return !p.full && !m->diag->set;
}

int value_print(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap) {
  return print_with(m, names, name_count, v, buf, cap, 0);
}

/* M6 s1 (D3): as value_print, but a binder can keep a field name (the case
   types of the ELIM_CASES and HIT_CASES messages). */
int value_print_case(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap) {
  return print_with(m, names, name_count, v, buf, cap, 1);
}
