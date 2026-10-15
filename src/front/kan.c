/* M3: the Kan operations (see front/kan.h). SHARED by the tcc kits. */
#include "front/kan.h"

/* The level of the fresh variable that reads the head former of a line.
   An interval literal is level * 2 + sign in a uint32_t, so a level must
   be below 2^31. The depth makes nested probes different. */
#define KAN_PROBE (1u << 30)

static const Value *kan_fail(Machine *m, const char *what) {
  diag_fail(m->diag, "INTERNAL", m->def, "the Kan evaluator found %s", what);
  return NULL;
}

static Core *kan_core(Machine *m, CoreKind kind) {
  Core *c = arena_alloc(m->arena, sizeof *c);
  if (c == NULL) {
    diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
    return NULL;
  }
  c->kind = kind;
  return c;
}

static const Core *var(Machine *m, uint32_t index) {
  Core *c = kan_core(m, CORE_VAR);
  if (c != NULL)
    c->index = index;
  return c;
}

/* An interval term. B is NULL for IVL_REV, A and B are NULL for the ends. */
static const Core *ivl(Machine *m, IvlOp op, const Core *a, const Core *b) {
  Core *c;
  if ((op == IVL_REV || op == IVL_MEET || op == IVL_JOIN) && (a == NULL || (op != IVL_REV && b == NULL)))
    return NULL;
  c = kan_core(m, CORE_IVL);
  if (c == NULL)
    return NULL;
  c->nat = op;
  c->a = a;
  c->b = b;
  return c;
}

/* KIND is CORE_LAM (a plain fun: no domain) or CORE_PLAM. */
static const Core *binder(Machine *m, CoreKind kind, const char *name, const Core *body) {
  Core *c;
  if (body == NULL)
    return NULL;
  c = kan_core(m, kind);
  if (c == NULL)
    return NULL;
  c->name = name;
  c->b = body;
  return c;
}

static const Core *app(Machine *m, const Core *fn, const Core *arg) {
  Core *c;
  if (fn == NULL || arg == NULL)
    return NULL;
  c = kan_core(m, CORE_APP);
  if (c == NULL)
    return NULL;
  c->a = fn;
  c->b = arg;
  return c;
}

/* An operation with ARGC (1 to 3) arguments A, B, C. */
static const Core *op(Machine *m, Op o, uint32_t argc, const Core *a, const Core *b, const Core *c) {
  const Core **args;
  Core *k;
  if (a == NULL || (argc > 1u && b == NULL) || (argc > 2u && c == NULL))
    return NULL;
  args = arena_alloc(m->arena, 3u * sizeof *args);
  if (args == NULL) {
    diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
    return NULL;
  }
  k = kan_core(m, CORE_OP);
  if (k == NULL)
    return NULL;
  args[0] = a;
  args[1] = b;
  args[2] = c;
  k->op = o;
  k->args = args;
  k->argc = argc;
  return k;
}

static const Core *zero(Machine *m) {
  return ivl(m, IVL_ZERO, NULL, NULL);
}

/* w r = transp (<j> DOM(L, ~j \/ r)) (phi \/ r) y. Y is the index of y,
   phi is at Y + 2 and L at Y + 3. R is r in this context, RJ is r under
   <j>. */
static const Core *pi_w(Machine *m, uint32_t y, const Core *r, const Core *rj) {
  const Core *dom = op(m, OP_KAN_DOM, 2u, var(m, y + 4u), ivl(m, IVL_JOIN, ivl(m, IVL_REV, var(m, 0), NULL), rj), NULL);
  return op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", dom), ivl(m, IVL_JOIN, var(m, y + 2u), r), var(m, y));
}

/* Environment [L, phi, f] (f is index 0):
   fun y => transp (<i> COD(L, i, w i)) phi (f (w i0)).
   Under fun y: y 0, f 1, phi 2, L 3. Under <i>: i 0, y 1, f 2, phi 3, L 4. */
static const Core *pi_rule(Machine *m) {
  const Core *cod = op(m, OP_KAN_COD, 3u, var(m, 4u), var(m, 0), pi_w(m, 1u, var(m, 0), var(m, 1u)));
  const Core *body = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", cod), var(m, 2u), app(m, var(m, 1u), pi_w(m, 0, zero(m), zero(m))));
  return binder(m, CORE_LAM, "y", body);
}

/* Environment [L, phi, p] (p is index 0):
   pack (transp (<i> DOM(L, i)) phi (p1 p)) (transp (<i> COD(L, i, v i)) phi (p2 p)),
   v i = transp (<j> DOM(L, i /\ j)) (phi \/ ~i) (p1 p).
   Under <i>: i 0, p 1, phi 2, L 3. Under <j>: j 0, i 1, p 2, phi 3, L 4.
   PACK, P1, P2: OP_PACK, OP_WITNESS, OP_PAYLOAD (Sigma) or OP_PAIR,
   OP_FIRST, OP_SECOND (Prod). */
static const Core *pair_rule(Machine *m, Op pack, Op p1, Op p2) {
  const Core *vdom = op(m, OP_KAN_DOM, 2u, var(m, 4u), ivl(m, IVL_MEET, var(m, 1u), var(m, 0)), NULL);
  const Core *v = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", vdom), ivl(m, IVL_JOIN, var(m, 2u), ivl(m, IVL_REV, var(m, 0), NULL)), op(m, p1, 1u, var(m, 1u), NULL, NULL));
  const Core *dom = op(m, OP_KAN_DOM, 2u, var(m, 3u), var(m, 0), NULL);
  const Core *cod = op(m, OP_KAN_COD, 3u, var(m, 3u), var(m, 0), v);
  const Core *first = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", dom), var(m, 1u), op(m, p1, 1u, var(m, 0), NULL, NULL));
  const Core *second = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", cod), var(m, 1u), op(m, p2, 1u, var(m, 0), NULL, NULL));
  return op(m, pack, 2u, first, second, NULL);
}

static const Value *instance(Machine *m, const Core *rule, const Value *line, const Value *phi, const Value *a) {
  const Env *env = env_push(m, env_push(m, env_push(m, NULL, line), phi), a);
  return rule == NULL || env == NULL ? NULL : eval_core(m, env, rule);
}

/* The closed types: no parameters, so transp gives its argument. */
static int closed_type(const Value *t) {
  return t->kind == VAL_UNIV || val_is(t, OP_NAT) || val_is(t, OP_FIELD) || val_is(t, OP_UNIT) || val_is(t, OP_FLAG);
}

const Value *kan_transp(Machine *m, const Value *line, const Value *phi, const Value *a) {
  const Value *args[3];
  const Value *head;
  if (line == NULL || phi == NULL || a == NULL)
    return NULL;
  if (phi->kind != VAL_IVL || line->kind != VAL_PLAM)
    return kan_fail(m, "a transp with a bad line or face");
  if (ivl_is_const(phi->ivl, 1))
    return a;
  /* Only the head is read. A Pi head at a fresh i is a Pi head at each r. */
  head = closure_apply(m, line, val_ivl_var(m, KAN_PROBE + m->depth));
  if (head == NULL)
    return NULL;
  if (closed_type(head))
    return a;
  if (head->kind == VAL_PI)
    return instance(m, pi_rule(m), line, phi, a);
  if (head->kind == VAL_SIGMA)
    return instance(m, pair_rule(m, OP_PACK, OP_WITNESS, OP_PAYLOAD), line, phi, a);
  if (val_is(head, OP_PROD))
    return instance(m, pair_rule(m, OP_PAIR, OP_FIRST, OP_SECOND), line, phi, a);
  args[0] = line;
  args[1] = phi;
  args[2] = a;
  return val_op(m, VAL_STUCK, OP_TRANSP, 0, 0, args, 3u);
}

static const Value *line_at(Machine *m, const Value *line, const Value *r) {
  const Value *t;
  if (line == NULL || r == NULL)
    return NULL;
  if (line->kind != VAL_PLAM)
    return kan_fail(m, "a template line that is not a path lambda");
  t = closure_apply(m, line, r);
  if (t == NULL)
    return NULL;
  if (t->kind == VAL_PI || t->kind == VAL_SIGMA || val_is(t, OP_PROD))
    return t;
  return kan_fail(m, "a template line that is not a Pi, Sigma or Prod");
}

const Value *kan_dom(Machine *m, const Value *line, const Value *r) {
  const Value *t = line_at(m, line, r);
  if (t == NULL)
    return NULL;
  return val_is(t, OP_PROD) ? t->args[0] : t->dom;
}

const Value *kan_cod(Machine *m, const Value *line, const Value *r, const Value *x) {
  const Value *t = line_at(m, line, r);
  if (t == NULL || x == NULL)
    return NULL;
  return val_is(t, OP_PROD) ? t->args[1] : closure_apply(m, t, x);
}
