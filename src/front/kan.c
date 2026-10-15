/* M3: the Kan operations (see front/kan.h). SHARED by the tcc kits. */
#include "front/kan.h"

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

/* An operation with N arguments ARGS (OP_HCOMP: 4, OP_SYS: 2 per branch,
   INST = the branch count). */
static const Core *opn(Machine *m, Op o, uint32_t inst, const Core *const *args, uint32_t n) {
  const Core **copy = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *copy);
  Core *k;
  uint32_t i;
  if (copy == NULL) {
    diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
    return NULL;
  }
  for (i = 0; i < n; i++) {
    if (args[i] == NULL)
      return NULL;
    copy[i] = args[i];
  }
  k = kan_core(m, CORE_OP);
  if (k == NULL)
    return NULL;
  k->op = o;
  k->inst = inst;
  k->args = copy;
  k->argc = n;
  return k;
}

/* fun i => BODY with an interval binder: A is an OP_INTERVAL core, so the
   value keeps its domain (eval.c ivl_binder). */
static const Core *tube(Machine *m, const Core *body) {
  Core *c;
  const Core *dom = opn(m, OP_INTERVAL, 0, NULL, 0);
  if (dom == NULL || body == NULL)
    return NULL;
  c = kan_core(m, CORE_LAM);
  if (c == NULL)
    return NULL;
  c->name = "i";
  c->a = dom;
  c->b = body;
  return c;
}

/* [ FACE -> BODY ], one branch. */
static const Core *sys1(Machine *m, const Core *face, const Core *body) {
  const Core *args[2];
  args[0] = face;
  args[1] = body;
  return opn(m, OP_SYS, 1u, args, 2u);
}

/* [ F0 -> B0, F1 -> B1 ], two branches. */
static const Core *sys2(Machine *m, const Core *f0, const Core *b0, const Core *f1, const Core *b1) {
  const Core *args[4];
  args[0] = f0;
  args[1] = b0;
  args[2] = f1;
  args[3] = b1;
  return opn(m, OP_SYS, 2u, args, 4u);
}

static const Core *hcomp4(Machine *m, const Core *a, const Core *phi, const Core *u, const Core *u0) {
  const Core *args[4];
  args[0] = a;
  args[1] = phi;
  args[2] = u;
  args[3] = u0;
  return opn(m, OP_HCOMP, 0, args, 4u);
}

static const Core *zero(Machine *m) {
  return ivl(m, IVL_ZERO, NULL, NULL);
}

static const Core *one(Machine *m) {
  return ivl(m, IVL_ONE, NULL, NULL);
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

/* P @ R. E0 and E1 are the ends of the type of P (D5), or both NULL when P
   is a line of types. */
static const Core *papp(Machine *m, const Core *p, const Core *r, const Core *e0, const Core *e1) {
  const Core **ends = NULL;
  Core *c;
  if (p == NULL || r == NULL || (e0 == NULL) != (e1 == NULL))
    return NULL;
  if (e0 != NULL) {
    ends = arena_alloc(m->arena, 2u * sizeof *ends);
    if (ends == NULL) {
      diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
      return NULL;
    }
    ends[0] = e0;
    ends[1] = e1;
  }
  c = kan_core(m, CORE_PAPP);
  if (c == NULL)
    return NULL;
  c->a = p;
  c->b = r;
  c->args = ends;
  c->argc = e0 == NULL ? 0 : 2u;
  return c;
}

/* A[r][j] for the PathP line L (index L) at the core interval R, J the
   index of j. */
static const Core *path_cell(Machine *m, uint32_t l, const Core *r, uint32_t j) {
  return papp(m, op(m, OP_KAN_DOM, 2u, var(m, l), r, NULL), var(m, j), NULL, NULL);
}

/* The end E (i0 or i1) of the PathP line L (index L) at R. */
static const Core *path_end(Machine *m, uint32_t l, const Core *r, const Core *e) {
  return op(m, OP_KAN_COD, 3u, var(m, l), r, e);
}

/* transp (<i> T k) i (X) with T k = A[i \/ k][j], under <i> with i 0,
   j 1 and L at index 4. */
static const Core *path_step(Machine *m, const Core *x) {
  const Core *cell = path_cell(m, 5u, ivl(m, IVL_JOIN, var(m, 1u), var(m, 0)), 2u);
  return op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "k", cell), var(m, 0), x);
}

/* Environment [L, phi, p] (p is index 0), L = <i> PathP (A[i]) a[i] b[i]:
   <j> comp (<i> A[i][j]) (phi \/ ~j \/ j) (fun i => [ phi -> p @ j, ~j -> a[i], j -> b[i] ]) (p @ j),
   and comp is the core term of check.c rule_comp, with the transp in each
   branch:
   <j> hcomp A[i1][j] (phi \/ ~j \/ j)
         (fun i => [ phi -> S (p @ j), ~j -> S a[i], j -> S b[i] ])
         (transp (<i> A[i][j]) i0 (p @ j)),  S = transp (<k> A[i \/ k][j]) i.
   Under <j>: j 0, p 1, phi 2, L 3. Under <i> or fun i: i 0, j 1, p 2,
   phi 3, L 4. Under <k>: k 0, i 1, j 2, p 3, phi 4, L 5. */
static const Core *pathp_rule(Machine *m) {
  const Core *face = ivl(m, IVL_JOIN, ivl(m, IVL_JOIN, var(m, 2u), ivl(m, IVL_REV, var(m, 0), NULL)), var(m, 0));
  const Core *pj = papp(m, var(m, 1u), var(m, 0), path_end(m, 3u, zero(m), zero(m)), path_end(m, 3u, zero(m), one(m)));
  const Core *base = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", path_cell(m, 4u, var(m, 0), 1u)), zero(m), pj);
  const Core *pi = papp(m, var(m, 2u), var(m, 1u), path_end(m, 4u, zero(m), zero(m)), path_end(m, 4u, zero(m), one(m)));
  const Core *args[6];
  args[0] = var(m, 3u);
  args[1] = path_step(m, pi);
  args[2] = ivl(m, IVL_REV, var(m, 1u), NULL);
  args[3] = path_step(m, path_end(m, 4u, var(m, 0), zero(m)));
  args[4] = var(m, 1u);
  args[5] = path_step(m, path_end(m, 4u, var(m, 0), one(m)));
  return binder(m, CORE_PLAM, "j", hcomp4(m, path_cell(m, 3u, one(m), 0), face, tube(m, opn(m, OP_SYS, 3u, args, 6u)), base));
}

static const Value *instance(Machine *m, const Core *rule, const Value *line, const Value *phi, const Value *a) {
  const Env *env = env_push(m, env_push(m, env_push(m, NULL, line), phi), a);
  return rule == NULL || env == NULL ? NULL : eval_core(m, env, rule);
}

/* The closed types: no parameters, so transp gives its argument. */
static int closed_type(const Value *t) {
  return t->kind == VAL_UNIV || val_is(t, OP_NAT) || val_is(t, OP_FIELD) || val_is(t, OP_UNIT) || val_is(t, OP_FLAG);
}

/* M3 s3: the coproduct formers and their constructors. */
static int data_type(const Value *t) {
  return val_is(t, OP_SUM) || val_is(t, OP_OPTION) || val_is(t, OP_LIST);
}

static int data_ctor(const Value *v) {
  return val_is(v, OP_INL) || val_is(v, OP_INR) || val_is(v, OP_NONE) || val_is(v, OP_SOME) || val_is(v, OP_NIL) || val_is(v, OP_CONS);
}

/* transp (<i> T[i]) phi x, with T[i] = COD(L, i, i0) (the right type of a
   Sum) when COD is 1, else DOM(L, i). X, PHI and L are indices outside <i>. */
static const Core *field_transp(Machine *m, int cod, uint32_t x, uint32_t phi, uint32_t l) {
  const Core *t = cod ? op(m, OP_KAN_COD, 3u, var(m, l + 1u), var(m, 0), zero(m)) : op(m, OP_KAN_DOM, 2u, var(m, l + 1u), var(m, 0), NULL);
  return op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", t), var(m, phi), var(m, x));
}

/* M3-PLAN 3.4 (s3): transp on Sum, Option or List gives the constructor of
   A with transp on each field. Environment [L, phi, x] (x is index 0):
   C (transp (<i> T[i]) phi x). For cons, [L, phi, x, xs] (xs is index 0):
   cons (transp (<i> DOM(L, i)) phi x) (transp L phi xs). */
static const Value *transp_data(Machine *m, const Value *line, const Value *phi, const Value *a) {
  const Env *env;
  const Core *rule;
  if (val_is(a, OP_NONE) || val_is(a, OP_NIL))
    return a;
  if (!val_is(a, OP_CONS))
    return instance(m, op(m, val_is(a, OP_INL) ? OP_INL : val_is(a, OP_INR) ? OP_INR : OP_SOME, 1u, field_transp(m, val_is(a, OP_INR), 0, 1u, 2u), NULL, NULL), line, phi, a->args[0]);
  env = env_push(m, env_push(m, env_push(m, env_push(m, NULL, line), phi), a->args[0]), a->args[1]);
  rule = op(m, OP_CONS, 2u, field_transp(m, 0, 1u, 2u, 3u), op(m, OP_TRANSP, 3u, var(m, 3u), var(m, 2u), var(m, 0)), NULL);
  return env == NULL || rule == NULL ? NULL : eval_core(m, env, rule);
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
  if (val_is(head, OP_PATHP))
    return instance(m, pathp_rule(m), line, phi, a);
  if (data_type(head) && data_ctor(a))
    return transp_data(m, line, phi, a);
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
  if (t->kind == VAL_PI || t->kind == VAL_SIGMA || val_is(t, OP_PROD) || val_is(t, OP_PATHP) || data_type(t))
    return t;
  return kan_fail(m, "a template line that is not a Pi, Sigma, Prod, PathP, Sum, Option or List");
}

/* For a PathP line: the inner line A[r]. For a Sum: the left type. For an
   Option or a List: the element type. */
const Value *kan_dom(Machine *m, const Value *line, const Value *r) {
  const Value *t = line_at(m, line, r);
  if (t == NULL)
    return NULL;
  return val_is(t, OP_PROD) || val_is(t, OP_PATHP) || data_type(t) ? t->args[0] : t->dom;
}

/* For a PathP line: X is i0 or i1 and gives that end at r. */
const Value *kan_cod(Machine *m, const Value *line, const Value *r, const Value *x) {
  const Value *t = line_at(m, line, r);
  if (t == NULL || x == NULL)
    return NULL;
  if (val_is(t, OP_PATHP)) {
    if (x->kind != VAL_IVL || !(ivl_is_const(x->ivl, 0) || ivl_is_const(x->ivl, 1)))
      return kan_fail(m, "a PathP end that is not i0 or i1");
    return t->args[ivl_is_const(x->ivl, 1) ? 2 : 1];
  }
  if (val_is(t, OP_OPTION) || val_is(t, OP_LIST))
    return kan_fail(m, "a codomain of an Option or List line");
  return val_is(t, OP_PROD) || val_is(t, OP_SUM) ? t->args[1] : closure_apply(m, t, x);
}

/* The constant line <_> T, so that OP_KAN_COD gives the codomain of a type
   T. Under the binder: the interval variable 0, T 1. */
static const Value *const_line(Machine *m, const Value *t) {
  const Env *env = env_push(m, NULL, t);
  const Core *body = var(m, 1u);
  return env == NULL || body == NULL ? NULL : val_closure(m, VAL_PLAM, "_", NULL, env, body);
}

/* Environment [L, phi, u, f] (f is index 0), L = <_> (x : A) -> B:
   fun x => hcomp (COD(L, i0, x)) phi (fun i => [ phi -> (u i) x ]) (f x).
   Under fun x: x 0, f 1, u 2, phi 3, L 4. Under fun i: i 0, x 1, f 2, u 3,
   phi 4, L 5. */
static const Core *hcomp_pi_rule(Machine *m) {
  const Core *cod = op(m, OP_KAN_COD, 3u, var(m, 4u), zero(m), var(m, 0));
  const Core *u = tube(m, sys1(m, var(m, 4u), app(m, app(m, var(m, 3u), var(m, 0)), var(m, 1u))));
  return binder(m, CORE_LAM, "x", hcomp4(m, cod, var(m, 3u), u, app(m, var(m, 1u), var(m, 0))));
}

static const Env *env4(Machine *m, const Value *line, const Value *phi, const Value *u, const Value *u0) {
  return env_push(m, env_push(m, env_push(m, env_push(m, NULL, line), phi), u), u0);
}

static const Value *instance4(Machine *m, const Core *rule, const Value *line, const Value *phi, const Value *u, const Value *u0) {
  const Env *env = env4(m, line, phi, u, u0);
  return rule == NULL || env == NULL ? NULL : eval_core(m, env, rule);
}

/* Environment [L, phi, u, p] (p is index 0), L = <_> T, T a Sigma or Prod
   with the first component A = DOM(L, i0) and u1 i = p1 (u i):
   fun r => hfill A phi u1 (p1 p) r
        = fun r => hcomp A (phi \/ ~r) (fun i => [ phi -> u1 (i /\ r), ~r -> p1 p ]) (p1 p).
   Under fun r: r 0, p 1, u 2, phi 3, L 4. Under fun i: i 0, r 1, p 2, u 3,
   phi 4, L 5. */
static const Core *pair_fill(Machine *m, Op p1) {
  const Core *u1 = op(m, p1, 1u, app(m, var(m, 3u), ivl(m, IVL_MEET, var(m, 0), var(m, 1u))), NULL, NULL);
  const Core *u = tube(m, sys2(m, var(m, 4u), u1, ivl(m, IVL_REV, var(m, 1u), NULL), op(m, p1, 1u, var(m, 2u), NULL, NULL)));
  const Core *face = ivl(m, IVL_JOIN, var(m, 3u), ivl(m, IVL_REV, var(m, 0), NULL));
  return tube(m, hcomp4(m, op(m, OP_KAN_DOM, 2u, var(m, 4u), zero(m), NULL), face, u, op(m, p1, 1u, var(m, 1u), NULL, NULL)));
}

/* Environment [L, phi, u, p, H] (H is index 0), H = pair_fill, B[r] =
   COD(L, i0, H r), u2 i = p2 (u i):
   pack (hcomp A phi u1 (p1 p)) (comp (<i> B[i]) phi u2 (p2 p)), and comp
   is the core term of check.c rule_comp:
   hcomp B[i1] phi (fun i => [ phi -> transp (<j> B[i \/ j]) i (u2 i) ])
     (transp (<i> B[i]) i0 (p2 p)).
   Under fun i or <i>: i 0, H 1, p 2, u 3, phi 4, L 5. Under <j>: j 0, i 1,
   H 2, p 3, u 4, phi 5, L 6. */
static const Core *hcomp_pair_rule(Machine *m, Op pack, Op p1, Op p2) {
  const Core *first = hcomp4(m, op(m, OP_KAN_DOM, 2u, var(m, 4u), zero(m), NULL), var(m, 3u),
                             tube(m, op(m, p1, 1u, app(m, var(m, 3u), var(m, 0)), NULL, NULL)), op(m, p1, 1u, var(m, 1u), NULL, NULL));
  const Core *bj = op(m, OP_KAN_COD, 3u, var(m, 6u), zero(m), app(m, var(m, 2u), ivl(m, IVL_JOIN, var(m, 1u), var(m, 0))));
  const Core *fill = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", bj), var(m, 0), op(m, p2, 1u, app(m, var(m, 3u), var(m, 0)), NULL, NULL));
  const Core *bi = op(m, OP_KAN_COD, 3u, var(m, 5u), zero(m), app(m, var(m, 1u), var(m, 0)));
  const Core *base = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "i", bi), zero(m), op(m, p2, 1u, var(m, 1u), NULL, NULL));
  const Core *b1 = op(m, OP_KAN_COD, 3u, var(m, 4u), zero(m), app(m, var(m, 0), one(m)));
  const Core *second = hcomp4(m, b1, var(m, 3u), tube(m, sys1(m, var(m, 4u), fill)), base);
  return op(m, pack, 2u, first, second, NULL);
}

/* Environment [L, phi, u, p] (p is index 0), L = <_> PathP (A) a b:
   <j> hcomp A[j] (phi \/ ~j \/ j) (fun i => [ phi -> (u i) @ j, ~j -> a, j -> b ]) (p @ j).
   Under <j>: j 0, p 1, u 2, phi 3, L 4. Under fun i: i 0, j 1, p 2, u 3,
   phi 4, L 5. */
static const Core *hcomp_pathp_rule(Machine *m) {
  const Core *face = ivl(m, IVL_JOIN, ivl(m, IVL_JOIN, var(m, 3u), ivl(m, IVL_REV, var(m, 0), NULL)), var(m, 0));
  const Core *base = papp(m, var(m, 1u), var(m, 0), path_end(m, 4u, zero(m), zero(m)), path_end(m, 4u, zero(m), one(m)));
  const Core *args[6];
  args[0] = var(m, 4u);
  args[1] = papp(m, app(m, var(m, 3u), var(m, 0)), var(m, 1u), path_end(m, 5u, zero(m), zero(m)), path_end(m, 5u, zero(m), one(m)));
  args[2] = ivl(m, IVL_REV, var(m, 1u), NULL);
  args[3] = path_end(m, 5u, zero(m), zero(m));
  args[4] = var(m, 1u);
  args[5] = path_end(m, 5u, zero(m), one(m));
  return binder(m, CORE_PLAM, "j", hcomp4(m, path_cell(m, 4u, zero(m), 0), face, tube(m, opn(m, OP_SYS, 3u, args, 6u)), base));
}

static const Value *hcomp_pair(Machine *m, const Value *a, const Value *phi, const Value *u, const Value *u0, Op pack, Op p1, Op p2) {
  const Value *line = const_line(m, a);
  const Value *h = line == NULL ? NULL : instance4(m, pair_fill(m, p1), line, phi, u, u0);
  const Env *env = h == NULL ? NULL : env_push(m, env4(m, line, phi, u, u0), h);
  const Core *rule = env == NULL ? NULL : hcomp_pair_rule(m, pack, p1, p2);
  return rule == NULL ? NULL : eval_core(m, env, rule);
}

/* 1 when B on conjunction K of FACE converts to U0 there. LEVEL is above
   each free level of B and U0. */
static int same_on(Machine *m, uint32_t level, const Value *b, const Value *u0, const Ivl *face, uint32_t k) {
  const Value *x = value_on_face(m, level, b, face, k);
  const Value *y = x == NULL ? NULL : value_on_face(m, level, u0, face, k);
  return y != NULL && conv_values(m, level, x, y);
}

/* The closed-type rule (M3-PLAN 3.4): 1 when each branch of u i (i fresh)
   converts to U0 on its face. U0 does not use i, so the conversion is also
   the constancy test. A free level at KAN_PROBE or above (a head probe of
   kan_transp) gives 0:  the probe reads only the head. */
static int hcomp_constant(Machine *m, const Value *u, const Value *u0) {
  uint32_t bound = 0;
  uint32_t j;
  uint32_t k;
  const Value *s;
  if (!value_bound(m, u, &bound) || !value_bound(m, u0, &bound) || bound >= KAN_PROBE)
    return 0;
  s = apply_value(m, u, val_ivl_var(m, bound));
  if (s == NULL)
    return 0;
  if (!val_is(s, OP_SYS))
    return conv_values(m, bound + 1u, s, u0);
  for (j = 0; j + 1u < s->argc; j += 2u)
    for (k = 0; k < s->args[j]->ivl->count; k++)
      if (!same_on(m, bound + 1u, s->args[j + 1u], u0, s->args[j]->ivl, k))
        return 0;
  return 1;
}

/* 1 when u i (i fresh) is a value with the head of U0 on each conjunction
   of the face of each branch (M3-PLAN 3.4, s3). A free level at KAN_PROBE
   or above gives 0, as in hcomp_constant. */
static int hcomp_same_ctor(Machine *m, const Value *u, const Value *u0) {
  uint32_t bound = 0;
  uint32_t j;
  uint32_t k;
  const Value *s;
  const Value *x;
  if (!value_bound(m, u, &bound) || bound >= KAN_PROBE)
    return 0;
  s = apply_value(m, u, val_ivl_var(m, bound));
  if (s == NULL)
    return 0;
  if (!val_is(s, OP_SYS))
    return val_is(s, u0->op);
  for (j = 0; j + 1u < s->argc; j += 2u)
    for (k = 0; k < s->args[j]->ivl->count; k++) {
      x = value_on_face(m, bound + 1u, s->args[j + 1u], s->args[j]->ivl, k);
      if (x == NULL || !val_is(x, u0->op))
        return 0;
    }
  return 1;
}

/* hcomp T phi (fun i => field N of (u i)) x. T, PHI, U and X are indices
   outside fun i. OP_KAN_FIELD distributes over the system u i, so the tube
   keeps the faces of u and has no nested system. */
static const Core *field_hcomp(Machine *m, uint32_t n, uint32_t t, uint32_t phi, uint32_t u, uint32_t x) {
  const Core *args[1];
  args[0] = app(m, var(m, u + 1u), var(m, 0));
  return hcomp4(m, var(m, t), var(m, phi), tube(m, opn(m, OP_KAN_FIELD, n, args, 1u)), var(m, x));
}

/* M3-PLAN 3.4 (s3), the data rule for A a Sum, Option or List: U0 and each
   branch of u are the same constructor C. Then C with hcomp on each field.
   Environment [T, phi, u, x] (x is index 0, T its type): C (field_hcomp).
   For cons, [A, List A, phi, u, x, xs] (xs is index 0). NULL without a
   diagnostic when the rule does not apply. */
static const Value *hcomp_data(Machine *m, const Value *a, const Value *phi, const Value *u, const Value *u0) {
  const Env *env;
  const Core *rule;
  if (!data_ctor(u0) || !hcomp_same_ctor(m, u, u0))
    return NULL;
  if (val_is(u0, OP_NONE) || val_is(u0, OP_NIL))
    return u0;
  if (!val_is(u0, OP_CONS))
    return instance4(m, op(m, u0->op, 1u, field_hcomp(m, 0, 3u, 2u, 1u, 0), NULL, NULL), a->args[val_is(u0, OP_INR) ? 1 : 0], phi, u, u0->args[0]);
  env = env_push(m, env_push(m, env4(m, a->args[0], a, phi, u), u0->args[0]), u0->args[1]);
  rule = op(m, OP_CONS, 2u, field_hcomp(m, 0, 5u, 3u, 2u, 1u), field_hcomp(m, 1u, 4u, 3u, 2u, 0), NULL);
  return env == NULL || rule == NULL ? NULL : eval_core(m, env, rule);
}

/* M3-PLAN 3.4 (s2): the face i1 rule, then the rule of the head former of
   A, else a stuck value. No rule gives u0 for the face i0 on a general A
   (D8). */
const Value *kan_hcomp(Machine *m, const Value *a, const Value *phi, const Value *u, const Value *u0) {
  const Value *args[4];
  if (a == NULL || phi == NULL || u == NULL || u0 == NULL)
    return NULL;
  if (phi->kind != VAL_IVL)
    return kan_fail(m, "an hcomp with a bad face");
  if (ivl_is_const(phi->ivl, 1))
    return apply_value(m, u, val_ivl_end(m, 1));
  if (closed_type(a) && a->kind != VAL_UNIV) {
    if (hcomp_constant(m, u, u0))
      return u0;
    if (m->diag->set)
      return NULL;
  }
  if (a->kind == VAL_PI)
    return instance4(m, hcomp_pi_rule(m), const_line(m, a), phi, u, u0);
  if (a->kind == VAL_SIGMA)
    return hcomp_pair(m, a, phi, u, u0, OP_PACK, OP_WITNESS, OP_PAYLOAD);
  if (val_is(a, OP_PROD))
    return hcomp_pair(m, a, phi, u, u0, OP_PAIR, OP_FIRST, OP_SECOND);
  if (val_is(a, OP_PATHP))
    return instance4(m, hcomp_pathp_rule(m), const_line(m, a), phi, u, u0);
  if (data_type(a)) {
    const Value *v = hcomp_data(m, a, phi, u, u0);
    if (v != NULL || m->diag->set)
      return v;
  }
  args[0] = a;
  args[1] = phi;
  args[2] = u;
  args[3] = u0;
  return val_op(m, VAL_STUCK, OP_HCOMP, 0, 0, args, 4u);
}
