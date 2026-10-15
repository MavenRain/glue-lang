/* M3: the Kan operations (see front/kan.h). SHARED by the tcc kits. */
#include "front/kan.h"
#include "front/face.h"

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

/* M3 s5: a head that a Kan rule of kan_transp reads. */
static int type_former(const Value *t) {
  return closed_type(t) || t->kind == VAL_PI || t->kind == VAL_SIGMA || val_is(t, OP_PROD) || val_is(t, OP_PATHP) || data_type(t) || val_is(t, OP_GLUE);
}

/* M3 s5: the head of LINE, its body at a probe level. Only the head former
   is read, so each transp in the body stays stuck (m->probe), else each
   probe evaluates the probes of the body again (exponential, M3 s5-h). The
   Kan rules of the universe refuse a probe level, so a head that is not a
   type former is read again at the level above the line (an hcomp in
   Type 0 at i gives a Glue head there). */
static const Value *probe_head(Machine *m, const Value *line) {
  const Value *head;
  uint32_t ell = 0;
  m->probe++;
  head = closure_apply(m, line, val_ivl_var(m, KAN_PROBE + m->depth));
  m->probe--;
  if (head == NULL || type_former(head) || !value_bound(m, line, &ell) || ell >= KAN_PROBE)
    return head;
  return closure_apply(m, line, val_ivl_var(m, ell));
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

/* RULE in the environment VALS (VALS[N - 1] is index 0). N > 0. */
static const Value *run(Machine *m, const Core *rule, const Value *const *vals, uint32_t n) {
  const Env *env = NULL;
  uint32_t i;
  for (i = 0; i < n; i++) {
    env = vals[i] == NULL ? NULL : env_push(m, env, vals[i]);
    if (env == NULL)
      return NULL;
  }
  return rule == NULL ? NULL : eval_core(m, env, rule);
}

static const Value *run1(Machine *m, const Core *rule, const Value *v) {
  return run(m, rule, &v, 1u);
}

static const Core **kan_args(Machine *m, uint32_t n) {
  const Core **args = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *args);
  if (args == NULL)
    diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
  return args;
}

static const Value **kan_vals(Machine *m, uint32_t n) {
  const Value **vals = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *vals);
  if (vals == NULL)
    diag_fail(m->diag, "OOM", m->def, "the arena limit is reached");
  return vals;
}

/* KIND (NAME : A) B, KIND = CORE_SIGMA or CORE_PI. */
static const Core *dep(Machine *m, CoreKind kind, const char *name, const Core *a, const Core *b) {
  Core *c;
  if (a == NULL || b == NULL)
    return NULL;
  c = kan_core(m, kind);
  if (c == NULL)
    return NULL;
  c->name = name;
  c->a = a;
  c->b = b;
  return c;
}

static const Core *sigma(Machine *m, const char *name, const Core *a, const Core *b) {
  return dep(m, CORE_SIGMA, name, a, b);
}

static const Core *proj(Machine *m, Op o, const Core *x) {
  return op(m, o, 1u, x, NULL, NULL);
}

/* V with LEVEL = i1 (ONE) or i0. LEVEL is the top free level of V. */
static const Value *at_end(Machine *m, uint32_t level, const Value *v, int one) {
  IvlHeap h = {m->arena, m->diag, m->def};
  const Ivl *f = ivl_lit(&h, level, !one);
  return f == NULL || v == NULL ? NULL : value_on_face(m, level + 1u, v, f, 0);
}

/* Part SEL of the Glue type G (see kan_glue), or NULL. */
static const Value *glue_part(const Value *g, uint32_t sel) {
  if (!val_is(g, OP_GLUE))
    return NULL;
  if (sel == 0)
    return g->args[0];
  if (!val_is(g->args[2], OP_SYS) || sel > g->args[2]->inst)
    return NULL;
  return g->args[2]->args[2u * (sel - 1u) + 1u];
}

const Value *kan_glue(Machine *m, const Value *line, const Value *r, uint32_t sel) {
  const Value *args[2];
  const Value *g;
  const Value *part;
  uint32_t ell = 0;
  if (line == NULL || r == NULL)
    return NULL;
  if (line->kind != VAL_PLAM || r->kind != VAL_IVL)
    return kan_fail(m, "a Glue part with a bad line or end");
  g = closure_apply(m, line, r);
  part = g == NULL ? NULL : glue_part(g, sel);
  if (part != NULL || g == NULL)
    return part;
  if ((ivl_is_const(r->ivl, 0) || ivl_is_const(r->ivl, 1)) && value_bound(m, line, &ell) && ell < KAN_PROBE) {
    g = closure_apply(m, line, val_ivl_var(m, ell));
    part = g == NULL ? NULL : glue_part(g, sel);
    if (part != NULL)
      return at_end(m, ell, part, ivl_is_const(r->ivl, 1));
  }
  if (m->diag->set)
    return NULL;
  args[0] = line;
  args[1] = r;
  return val_op(m, VAL_STUCK, OP_KAN_GLUE, sel, 0, args, 2u);
}

/* fun i => [ F0 -> B0 i, ..., F(N-1) -> B(N-1) i ], VALS = F0, B0, ...
   Under fun i: i 0, VALS[p] 2N - p. */
static const Value *tube_of(Machine *m, const Value *const *vals, uint32_t n) {
  const Core **args = kan_args(m, 2u * n);
  uint32_t k;
  if (args == NULL)
    return NULL;
  for (k = 0; k < n; k++) {
    args[2u * k] = var(m, 2u * n - 2u * k);
    args[2u * k + 1u] = app(m, var(m, 2u * n - 2u * k - 1u), var(m, 0));
  }
  return run(m, tube(m, opn(m, OP_SYS, n, args, 2u * n)), vals, 2u * n);
}

/* glue [ VALS[2] -> VALS[3], ... ] VALS[0] (O = OP_GLUE_ELEM) or
   Glue VALS[0] [ VALS[2] -> VALS[3], ... ] (O = OP_GLUE) with the face
   VALS[1], N branches. VALS[p] is index 2N + 1 - p. */
static const Value *glue_elem(Machine *m, Op o, const Value *const *vals, uint32_t n) {
  const Core **args = kan_args(m, 2u * n);
  uint32_t k;
  if (args == NULL)
    return NULL;
  for (k = 0; k < n; k++) {
    args[2u * k] = var(m, 2u * n - 1u - 2u * k);
    args[2u * k + 1u] = var(m, 2u * n - 2u - 2u * k);
  }
  return run(m, op(m, o, 3u, var(m, 2u * n + 1u), var(m, 2u * n), opn(m, OP_SYS, n, args, 2u * n)), vals, 2u * n + 2u);
}

/* M3 s5: ghcomp A phi u u0 = hcomp A (phi \/ ~phi) (fun i => [ phi -> u i, ~phi -> u0 ]) u0,
   so phi = i0 gives u0 by the face i1 rule. Environment [A, phi, u, u0]
   (u0 is index 0). Under fun i: i 0, u0 1, u 2, phi 3, A 4. */
static const Core *ghcomp_rule(Machine *m) {
  const Core *u = tube(m, sys2(m, var(m, 3u), app(m, var(m, 2u), var(m, 0)), ivl(m, IVL_REV, var(m, 3u), NULL), var(m, 1u)));
  return hcomp4(m, var(m, 3u), ivl(m, IVL_JOIN, var(m, 2u), ivl(m, IVL_REV, var(m, 2u), NULL)), u, var(m, 0));
}

/* gcomp (<i> A) phi u u0 = ghcomp A(i1) phi (fun i => [ phi -> transp (<j> A(i \/ j)) i (u i) ])
   (transp (<i> A) i0 u0). The tube: environment [L, phi, u]; under fun i:
   i 0, u 1, phi 2, L 3;  under <j>: j 0, i 1, u 2, phi 3, L 4. */
static const Value *gcomp(Machine *m, const Value *line, const Value *phi, const Value *u, const Value *u0) {
  const Core *aj = papp(m, var(m, 4u), ivl(m, IVL_JOIN, var(m, 1u), var(m, 0)), NULL, NULL);
  const Core *fill = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", aj), var(m, 0), app(m, var(m, 1u), var(m, 0)));
  const Value *t[3];
  const Value *b[2];
  const Value *g[4];
  t[0] = line;
  t[1] = phi;
  t[2] = u;
  b[0] = line;
  b[1] = u0;
  g[0] = run1(m, papp(m, var(m, 0), one(m), NULL, NULL), line);
  g[1] = phi;
  g[2] = run(m, tube(m, sys1(m, var(m, 2u), fill)), t, 3u);
  g[3] = run(m, op(m, OP_TRANSP, 3u, var(m, 1u), zero(m), var(m, 0)), b, 2u);
  return run(m, ghcomp_rule(m), g, 4u);
}

/* M3 s5 (Cubical Agda transpGlue, CCHM 6.2): transp L psi u0 for
   L = <i> Glue A [ phik -> pack Tk ek ]. Brief formulas: a0 = unglue u0;
   deltak = forall i. phik;  a1' = gcomp (<i> A) (psi \/ delta)
   (fun i => [ psi -> a0, deltak -> first ek(i) tk(i) ]) a0 with
   tk(i) = transp (<j> Tk(i /\ j)) (psi \/ ~i) u0;  per branch k the fiber
   extension fextk = hcomp fibk (xi \/ ~xi) (fun j => [ xi -> second ctrk wk @ j,
   ~xi -> first ctrk ]) (first ctrk), xi = psi \/ delta;  then
   glue [ phik(1) -> first fextk ] (hcomp A(1) (phi(1) \/ psi)
   (fun j => [ phik(1) -> second fextk @ ~j, psi -> a1' ]) a1').
   Systems are eager, so each branch on a face of the inputs is built per
   conjunction c on the inputs put on c. The components are read at a fresh
   level ELL;  the ends come from them by at_end. NULL without a diagnostic
   when the rule does not apply. */
static const Value *transp_glue(Machine *m, const Value *line, const Value *psi, const Value *u0) {
  IvlHeap h = {m->arena, m->diag, m->def};
  uint32_t ell = 0;
  uint32_t n, k, c, e, count, xn;
  const Value *g, *sys, *a0, *a1p, *a1, *nxi, *ctr, *fext;
  const Value *v[4];
  const Value **pairs, **out, **ts;
  const Ivl **deltas;
  const Ivl *delta, *xi, *xf;
  if (!value_bound(m, line, &ell) || !value_bound(m, psi, &ell) || !value_bound(m, u0, &ell) || ell >= KAN_PROBE)
    return NULL;
  g = closure_apply(m, line, val_ivl_var(m, ell));
  if (g == NULL || !val_is(g, OP_GLUE) || !val_is(g->args[2], OP_SYS))
    return NULL;
  sys = g->args[2];
  n = sys->inst;
  deltas = arena_alloc(m->arena, ((size_t)n + 1u) * sizeof *deltas);
  delta = ivl_const(&h, 0);
  count = 0;
  for (k = 0; deltas != NULL && delta != NULL && k < n; k++) {
    deltas[k] = face_forall(&h, sys->args[2u * k]->ivl, ell);
    delta = deltas[k] == NULL ? NULL : face_join(&h, delta, deltas[k]);
    count += deltas[k] == NULL ? 0 : deltas[k]->count;
  }
  xi = delta == NULL ? NULL : face_join(&h, psi->ivl, delta);
  /* Reused for the n Glue branches after the count persistent faces. */
  pairs = kan_vals(m, 2u * (count > n ? count : n) + 2u);
  if (deltas == NULL || xi == NULL || pairs == NULL)
    return NULL;
  /* a0 = unglue u0 at the end i0 */
  v[0] = u0;
  v[1] = at_end(m, ell, g->args[1], 0);
  v[2] = at_end(m, ell, sys, 0);
  a0 = run(m, op(m, OP_UNGLUE, 3u, var(m, 2u), var(m, 1u), var(m, 0)), v, 3u);
  /* the tube of a1': under fun i: i 0, u0c 1, Lc 2, psi 3, L 4;  under <j>: j 0, i 1, u0c 2, Lc 3, psi 4, L 5 */
  pairs[0] = psi;
  pairs[1] = run1(m, tube(m, var(m, 1u)), a0);
  e = 1;
  for (k = 0; k < n; k++)
    for (c = 0; c < deltas[k]->count; c++, e++) {
      const Core *kg[2];
      const Core *tk;
      kg[0] = var(m, 4u);
      kg[1] = var(m, 0);
      tk = op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", papp(m, var(m, 3u), ivl(m, IVL_MEET, var(m, 1u), var(m, 0)), NULL, NULL)),
              ivl(m, IVL_JOIN, var(m, 3u), ivl(m, IVL_REV, var(m, 0), NULL)), var(m, 1u));
      xf = face_conj(&h, deltas[k], c);
      v[0] = line;
      v[1] = psi;
      v[2] = value_on_face(m, ell + 1u, line, deltas[k], c);
      v[3] = value_on_face(m, ell + 1u, u0, deltas[k], c);
      pairs[2u * e] = xf == NULL ? NULL : val_ivl(m, xf);
      pairs[2u * e + 1u] = run(m, tube(m, app(m, proj(m, OP_WITNESS, proj(m, OP_PAYLOAD, opn(m, OP_KAN_GLUE, 1u + k, kg, 2u))), tk)), v, 4u);
    }
  {
    const Core *kg[2];
    kg[0] = var(m, 1u);
    kg[1] = var(m, 0);
    a1p = gcomp(m, run1(m, binder(m, CORE_PLAM, "i", opn(m, OP_KAN_GLUE, 0, kg, 2u)), line), val_ivl(m, xi), tube_of(m, pairs, count + 1u), a0);
  }
  /* tc = tk(1) on conjunction c of xi (on psi:  u0;  on deltak:  transp of Tk) */
  xn = xi->count;
  ts = kan_vals(m, 2u * xn + 2u);
  out = kan_vals(m, 2u * n + 2u);
  nxi = val_ivl(m, face_norm(&h, ivl_rev(&h, xi)));
  if (a1p == NULL || ts == NULL || out == NULL || nxi == NULL)
    return NULL;
  for (c = 0; c < xn; c++) {
    xf = face_conj(&h, xi, c);
    v[0] = value_on_face(m, ell + 1u, line, xi, c);
    v[1] = value_on_face(m, ell + 1u, psi, xi, c);
    v[2] = value_on_face(m, ell + 1u, u0, xi, c);
    ts[2u * c] = xf == NULL ? NULL : val_ivl(m, xf);
    ts[2u * c + 1u] = run(m, op(m, OP_TRANSP, 3u, var(m, 2u), var(m, 1u), var(m, 0)), v, 3u);
  }
  /* a1 tube pairs in PAIRS:  phik(1) -> alphak @ ~j, then psi -> a1' */
  out[0] = NULL;
  out[1] = at_end(m, ell, g->args[1], 1);
  out[1] = out[1] == NULL ? NULL : val_ivl(m, face_norm(&h, out[1]->ivl));
  for (k = 0; k < n; k++) {
    const Value *pk = at_end(m, ell, sys->args[2u * k + 1u], 1);
    const Value **fs = kan_vals(m, 2u * xn + 2u);
    v[0] = pk;
    v[1] = a1p;
    /* ctrk = second ek(1) a1' */
    ctr = run(m, app(m, proj(m, OP_PAYLOAD, proj(m, OP_PAYLOAD, var(m, 1u))), var(m, 0)), v, 2u);
    if (fs == NULL || ctr == NULL)
      return NULL;
    /* under fun j: j 0, tc 1, a1' 2, ctr 3;  under <_>: a1' 4. wc = pack tc (<_> a1') */
    for (c = 0; c < xn; c++) {
      const Core *w = op(m, OP_PACK, 2u, var(m, 1u), binder(m, CORE_PLAM, "_", var(m, 3u)), NULL);
      v[0] = ctr;
      v[1] = a1p;
      v[2] = ts[2u * c + 1u];
      fs[2u * c] = ts[2u * c];
      fs[2u * c + 1u] = run(m, tube(m, papp(m, app(m, proj(m, OP_PAYLOAD, var(m, 3u)), w), var(m, 0), proj(m, OP_WITNESS, var(m, 3u)), w)), v, 3u);
    }
    fs[2u * xn] = nxi;
    fs[2u * xn + 1u] = run1(m, tube(m, proj(m, OP_WITNESS, var(m, 1u))), ctr);
    /* fibk = Sigma (x : Tk(1)) (Path A(1) (first ek(1) x) a1'):  env [A(1), a1', pk];  under x: x 0, pk 1, a1' 2, A(1) 3 */
    v[0] = at_end(m, ell, g->args[0], 1);
    v[1] = a1p;
    v[2] = pk;
    v[3] = run(m, sigma(m, "x", proj(m, OP_WITNESS, var(m, 0)),
                        op(m, OP_PATHP, 3u, binder(m, CORE_PLAM, "_", var(m, 4u)), app(m, proj(m, OP_WITNESS, proj(m, OP_PAYLOAD, var(m, 1u))), var(m, 0)), var(m, 2u))),
               v, 3u);
    xf = face_join(&h, xi, nxi->ivl);
    fext = v[3] == NULL || xf == NULL ? NULL : kan_hcomp(m, v[3], val_ivl(m, xf), tube_of(m, fs, xn + 1u), run1(m, proj(m, OP_WITNESS, var(m, 0)), ctr));
    if (fext == NULL)
      return NULL;
    /* alphak @ ~j:  env [pk, a1', t1k, alphak];  under fun j: j 0, alpha 1, t1 2, a1' 3, pk 4 */
    v[0] = pk;
    v[1] = a1p;
    v[2] = run1(m, proj(m, OP_WITNESS, var(m, 0)), fext);
    v[3] = run1(m, proj(m, OP_PAYLOAD, var(m, 0)), fext);
    pairs[2u * k] = at_end(m, ell, sys->args[2u * k], 1);
    pairs[2u * k + 1u] = run(m, tube(m, papp(m, var(m, 1u), ivl(m, IVL_REV, var(m, 0), NULL), app(m, proj(m, OP_WITNESS, proj(m, OP_PAYLOAD, var(m, 4u))), var(m, 2u)), var(m, 3u))), v, 4u);
    out[2u + 2u * k] = pairs[2u * k];
    out[3u + 2u * k] = v[2];
  }
  pairs[2u * n] = psi;
  pairs[2u * n + 1u] = run1(m, tube(m, var(m, 1u)), a1p);
  xf = out[1] == NULL ? NULL : face_join(&h, out[1]->ivl, psi->ivl);
  a1 = xf == NULL ? NULL : kan_hcomp(m, at_end(m, ell, g->args[0], 1), val_ivl(m, xf), tube_of(m, pairs, n + 1u), a1p);
  out[0] = a1;
  return a1 == NULL ? NULL : glue_elem(m, OP_GLUE_ELEM, out, n);
}

/* M3 s5 (Agda hcompGlue): A = Glue B [ phik -> pack Tk ek ]:
   glue [ phik -> hcomp Tk psi u u0 ] (hcomp B (psi \/ phi)
     (fun i => [ psi -> unglue (u i), phik -> first ek (hfill Tk psi u u0 i) ]) (unglue u0)).
   Each phik branch is built per conjunction c of phik on the inputs put
   on c (eager systems). NULL without a diagnostic when the rule does not
   apply. */
static const Value *hcomp_glue(Machine *m, const Value *a, const Value *psi, const Value *u, const Value *u0) {
  IvlHeap h = {m->arena, m->diag, m->def};
  const Value *sys = a->args[2];
  const Value **pairs, **out;
  const Value *v[4];
  const Ivl *f;
  uint32_t bound = 0;
  uint32_t k, c, e, count = 0;
  if (!val_is(sys, OP_SYS) || !value_bound(m, a, &bound) || !value_bound(m, psi, &bound) || !value_bound(m, u, &bound) || !value_bound(m, u0, &bound) || bound >= KAN_PROBE)
    return NULL;
  for (k = 0; k < sys->inst; k++)
    count += sys->args[2u * k]->ivl->count;
  pairs = kan_vals(m, 2u * count + 2u);
  out = kan_vals(m, 2u * count + 2u);
  if (pairs == NULL || out == NULL)
    return NULL;
  /* psi -> unglue (u i):  under fun i: i 0, sys 1, face 2, u 3 */
  v[0] = u;
  v[1] = a->args[1];
  v[2] = sys;
  pairs[0] = psi;
  pairs[1] = run(m, tube(m, op(m, OP_UNGLUE, 3u, app(m, var(m, 3u), var(m, 0)), var(m, 2u), var(m, 1u))), v, 3u);
  e = 0;
  for (k = 0; k < sys->inst; k++)
    for (c = 0; c < sys->args[2u * k]->ivl->count; c++, e++) {
      /* env [p, psi, u, u0];  under fun i: i 0, u0 1, u 2, psi 3, p 4;  under fun j: j 0, i 1, u0 2, u 3, psi 4, p 5 */
      const Core *inner = tube(m, sys2(m, var(m, 4u), app(m, var(m, 3u), ivl(m, IVL_MEET, var(m, 0), var(m, 1u))), ivl(m, IVL_REV, var(m, 1u), NULL), var(m, 2u)));
      const Core *fill = hcomp4(m, proj(m, OP_WITNESS, var(m, 4u)), ivl(m, IVL_JOIN, var(m, 3u), ivl(m, IVL_REV, var(m, 0), NULL)), inner, var(m, 1u));
      f = sys->args[2u * k]->ivl;
      v[0] = value_on_face(m, bound + 1u, sys->args[2u * k + 1u], f, c);
      v[1] = value_on_face(m, bound + 1u, psi, f, c);
      v[2] = value_on_face(m, bound + 1u, u, f, c);
      v[3] = value_on_face(m, bound + 1u, u0, f, c);
      f = face_conj(&h, f, c);
      pairs[2u + 2u * e] = f == NULL ? NULL : val_ivl(m, f);
      pairs[3u + 2u * e] = run(m, tube(m, app(m, proj(m, OP_WITNESS, proj(m, OP_PAYLOAD, var(m, 4u))), fill)), v, 4u);
      out[2u + 2u * e] = pairs[2u + 2u * e];
      out[3u + 2u * e] = run(m, hcomp4(m, proj(m, OP_WITNESS, var(m, 3u)), var(m, 2u), var(m, 1u), var(m, 0)), v, 4u);
    }
  /* a1 = hcomp B (psi \/ phi) tube (unglue u0) */
  f = face_join(&h, psi->ivl, a->args[1]->ivl);
  v[0] = u0;
  v[1] = a->args[1];
  v[2] = sys;
  out[0] = f == NULL ? NULL : kan_hcomp(m, a->args[0], val_ivl(m, f), tube_of(m, pairs, count + 1u), run(m, op(m, OP_UNGLUE, 3u, var(m, 2u), var(m, 1u), var(m, 0)), v, 3u));
  out[1] = a->args[1];
  return out[0] == NULL ? NULL : glue_elem(m, OP_GLUE_ELEM, out, count);
}

/* Closed templates for lineToEquiv:  isContr = fun T => Sigma (c : T)
   ((z : T) -> Path T c z);  fiber = fun X Y f y => Sigma (x : X)
   (Path Y (f x) y);  isEquiv = fun X Y f => (y : Y) -> isContr (fiber X Y f y). */
static const Core *is_contr_fn(Machine *m) {
  const Core *path = op(m, OP_PATHP, 3u, binder(m, CORE_PLAM, "_", var(m, 3u)), var(m, 1u), var(m, 0));
  return binder(m, CORE_LAM, "T", sigma(m, "c", var(m, 0), dep(m, CORE_PI, "z", var(m, 1u), path)));
}

static const Core *fiber_fn(Machine *m) {
  const Core *path = op(m, OP_PATHP, 3u, binder(m, CORE_PLAM, "_", var(m, 4u)), app(m, var(m, 2u), var(m, 0)), var(m, 1u));
  return binder(m, CORE_LAM, "X", binder(m, CORE_LAM, "Y", binder(m, CORE_LAM, "f", binder(m, CORE_LAM, "y", sigma(m, "x", var(m, 3u), path)))));
}

static const Core *is_equiv_fn(Machine *m) {
  /* under y: y 0, f 1, Y 2, X 3 */
  const Core *fib = app(m, app(m, app(m, app(m, fiber_fn(m), var(m, 3u)), var(m, 2u)), var(m, 1u)), var(m, 0));
  return binder(m, CORE_LAM, "X", binder(m, CORE_LAM, "Y", binder(m, CORE_LAM, "f", dep(m, CORE_PI, "y", var(m, 1u), app(m, is_contr_fn(m), fib)))));
}

/* second (idEquiv X) = fun y => pack (pack y (<_> y)) (fun z => <i> pack
   (payload z @ ~i) (<j> payload z @ (~i \/ j))), as check.c core_id_equiv;
   payload z has the ends witness z and y. */
static const Core *id_contr(Machine *m) {
  const Core *z_ij = papp(m, proj(m, OP_PAYLOAD, var(m, 2u)), ivl(m, IVL_JOIN, ivl(m, IVL_REV, var(m, 1u), NULL), var(m, 0)), proj(m, OP_WITNESS, var(m, 2u)), var(m, 3u));
  const Core *z_i = papp(m, proj(m, OP_PAYLOAD, var(m, 1u)), ivl(m, IVL_REV, var(m, 0), NULL), proj(m, OP_WITNESS, var(m, 1u)), var(m, 2u));
  const Core *contract = binder(m, CORE_LAM, "z", binder(m, CORE_PLAM, "i", op(m, OP_PACK, 2u, z_i, binder(m, CORE_PLAM, "j", z_ij), NULL)));
  const Core *center = op(m, OP_PACK, 2u, var(m, 0), binder(m, CORE_PLAM, "_", var(m, 1u)), NULL);
  return binder(m, CORE_LAM, "y", op(m, OP_PACK, 2u, center, contract, NULL));
}

/* M3 s5 (Cubical Agda lineToEquiv):  lineToEquiv L = pack (fun x => transp L i0 x)
   (transp (<k> isEquiv L(i0) L(k) (fun x => transp (<j> L(k /\ j)) ~k x)) i0
   (second (idEquiv L(i0)))).  At k = i0 the function is the identity.
   Environment [L];  under <k>: k 0, L 1;  under fun x: x 0, k 1, L 2;
   under <j>: j 0, x 1, k 2, L 3. */
static const Core *line_to_equiv(Machine *m) {
  const Core *fk = binder(m, CORE_LAM, "x", op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "j", papp(m, var(m, 3u), ivl(m, IVL_MEET, var(m, 2u), var(m, 0)), NULL, NULL)),
                                               ivl(m, IVL_REV, var(m, 1u), NULL), var(m, 0)));
  const Core *ty = app(m, app(m, app(m, is_equiv_fn(m), papp(m, var(m, 1u), zero(m), NULL, NULL)), papp(m, var(m, 1u), var(m, 0), NULL, NULL)), fk);
  const Core *f = binder(m, CORE_LAM, "x", op(m, OP_TRANSP, 3u, var(m, 1u), zero(m), var(m, 0)));
  return op(m, OP_PACK, 2u, f, op(m, OP_TRANSP, 3u, binder(m, CORE_PLAM, "k", ty), zero(m), id_contr(m)), NULL);
}

/* A line with a closed type head is constant and transp along it is the
   identity, so idEquiv L(i0) = pack (fun x => x) (second (idEquiv L(i0)))
   is a lineToEquiv of it. The general unfold transports the isEquiv nest,
   and each hcomp in it evaluates its tube again per face:  that grows
   exponentially (1 GB for a transp in Glue Nat [ k -> .. ], M3 s5-f). */
const Value *kan_l2e(Machine *m, const Value *line) {
  const Value *t;
  if (line == NULL)
    return NULL;
  if (line->kind != VAL_PLAM)
    return kan_fail(m, "a lineToEquiv of a value that is not a path lambda");
  t = probe_head(m, line);
  if (t == NULL)
    return NULL;
  if (closed_type(t))
    return run1(m, op(m, OP_PACK, 2u, binder(m, CORE_LAM, "x", var(m, 0)), id_contr(m), NULL), line);
  return run1(m, line_to_equiv(m), line);
}

/* M3 s5 (Cubical Agda hcomp in the universe):  hcomp (Type n) phi E A0 =
   Glue A0 [ phi -> pack (E i1) (lineToEquiv (<i> E (~i))) ], one branch per
   conjunction c of phi with E put on c (eager systems). NULL without a
   diagnostic when the rule does not apply. */
static const Value *hcomp_univ(Machine *m, const Value *phi, const Value *u, const Value *u0) {
  IvlHeap h = {m->arena, m->diag, m->def};
  const Value **out;
  const Value *ec, *lc;
  const Value *vals[2];
  const Ivl *f;
  uint32_t bound = 0;
  uint32_t c, count;
  if (!value_bound(m, phi, &bound) || !value_bound(m, u, &bound) || !value_bound(m, u0, &bound) || bound >= KAN_PROBE)
    return NULL;
  count = phi->ivl->count;
  out = kan_vals(m, 2u * count + 2u);
  if (out == NULL)
    return NULL;
  out[0] = u0;
  out[1] = phi;
  for (c = 0; c < count; c++) {
    f = face_conj(&h, phi->ivl, c);
    ec = value_on_face(m, bound + 1u, u, phi->ivl, c);
    /* <i> E (~i):  env [E];  under <i>: i 0, E 1 */
    lc = run1(m, binder(m, CORE_PLAM, "i", app(m, var(m, 1u), ivl(m, IVL_REV, var(m, 0), NULL))), ec);
    out[2u + 2u * c] = f == NULL ? NULL : val_ivl(m, f);
    /* pack (E i1) (lineToEquiv L), lineToEquiv lazy (OP_KAN_L2E):  env [E, L];  L 0, E 1 */
    vals[0] = ec;
    vals[1] = lc;
    out[3u + 2u * c] = run(m, op(m, OP_PACK, 2u, app(m, var(m, 1u), one(m)), op(m, OP_KAN_L2E, 1u, var(m, 0), NULL, NULL), NULL), vals, 2u);
  }
  return glue_elem(m, OP_GLUE, out, count);
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
  args[0] = line;
  args[1] = phi;
  args[2] = a;
  if (m->probe > 0)
    return val_op(m, VAL_STUCK, OP_TRANSP, 0, 0, args, 3u);
  /* A Pi head at a fresh i is a Pi head at each r. */
  head = probe_head(m, line);
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
  if (val_is(head, OP_GLUE)) {
    const Value *v = transp_glue(m, line, phi, a);
    if (v != NULL || m->diag->set)
      return v;
  }
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
  if (val_is(a, OP_GLUE)) {
    const Value *v = hcomp_glue(m, a, phi, u, u0);
    if (v != NULL || m->diag->set)
      return v;
  }
  if (a->kind == VAL_UNIV) {
    const Value *v = hcomp_univ(m, phi, u, u0);
    if (v != NULL || m->diag->set)
      return v;
  }
  args[0] = a;
  args[1] = phi;
  args[2] = u;
  args[3] = u0;
  return val_op(m, VAL_STUCK, OP_HCOMP, 0, 0, args, 4u);
}
