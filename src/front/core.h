/* Core terms, values and the evaluator. SHARED by the tcc kits.

   The checker (front/check.c) writes core terms. Evaluation is normalization
   by evaluation: each value is a normal form. A neutral value (VAL_VAR,
   VAL_APP, VAL_STUCK) is stuck on a variable. A trap (a Nat overflow) is a
   value too. It goes up through each strict position (a Nat operation, the
   scrutinee of an eliminator, the function of an application) and nowhere
   else. So the evaluator and the residual program agree: a trap that no
   strict position reads does not stop the program. */
#ifndef LANG_FRONT_CORE_H
#define LANG_FRONT_CORE_H

#include "front/ast.h"
#include "front/base.h"
#include "front/interval.h"

#define EVAL_DEPTH_LIMIT 2000u
#define EVAL_FUEL_STEPS 20000000u

/* The core names: type formers, constructors and operations. */
typedef enum {
  OP_NAT,
  OP_FLAG,
  OP_UNIT,
  OP_PROD,
  OP_SUM,
  OP_OPTION,
  OP_LIST,
  OP_FAMILY,
  OP_UNIT_VAL,
  OP_FLAG_YES,
  OP_FLAG_NO,
  OP_PAIR,
  OP_INL,
  OP_INR,
  OP_NONE,
  OP_SOME,
  OP_NIL,
  OP_CONS,
  OP_PACK,
  OP_CTOR,
  OP_FIRST,
  OP_SECOND,
  OP_EITHER,
  OP_OPTION_ELIM,
  OP_PURE,
  OP_MAP,
  OP_BIND,
  OP_FOLD_NAT,
  OP_FOLD_LIST,
  OP_FOLD_FAMILY,
  OP_UNFOLD,
  OP_FILTER,
  OP_WITNESS,
  OP_PAYLOAD,
  OP_NAT_ADD,
  OP_NAT_SUB,
  OP_NAT_MUL,
  OP_NAT_EQ,
  OP_NAT_LE,
  OP_FLAG_IF,
  OP_PROJ,
  OP_FIELD,     /* glue-lang: the BN254 Fr type */
  OP_FIELD_LIT, /* name: the canonical decimal */
  OP_FIELD_ADD,
  OP_FIELD_SUB,
  OP_FIELD_MUL,
  OP_PATHP,    /* M2: the line (a PLAM closure), then the two ends */
  OP_INTERVAL, /* M2 s4: I as the domain of a Pi. No term has this type */
  OP_SSET,     /* M2 s4: the sort of I, Partial and each Pi over them */
  OP_PARTIAL,  /* M2 s4: Partial phi A. args: the face, then A */
  OP_SYS,      /* M2 s4: [ phi -> t, ... ]. inst: the branch count. args: face, body, ... */
  OP_TRANSP,   /* M3 s1: transp L phi a. args: the line (a PLAM closure), the face, a */
  OP_KAN_DOM,  /* M3 s1: kan.c only. args: L, r. The domain of L @ r */
  OP_KAN_COD,  /* M3 s1: kan.c only. args: L, r, x. The codomain of L @ r at x */
  OP_HCOMP,    /* M3 s2: hcomp A phi u u0. args: A, the face, u, u0 */
  OP_KAN_FIELD, /* M3 s3: kan.c only. args: x. inst: the field index of the constructor x */
  OP_GLUE,      /* M3 s4: Glue A [ ... ]. args: A, the face, the system of pack T e */
  OP_GLUE_ELEM, /* M3 s4: glue [ ... ] a. args: a, the face, the system of t */
  OP_UNGLUE,    /* M3 s4: unglue g. args: g, the face, the system of the Glue type */
  OP_KAN_GLUE,  /* M3 s5: kan.c only. args: L, r. inst: 0 = A, 1 + k = the pack of branch k of the Glue line L at r */
  OP_KAN_L2E    /* M3 s5: kan.c only. args: L. lineToEquiv L, canonical;  a projection unfolds it to a pack */
} Op;

/* The instance of pure, map, bind and filter (formers F5 and F8). */
typedef enum {
  CARRIER_OPTION,
  CARRIER_LIST,
  CARRIER_SUM
} Carrier;

typedef enum {
  CORE_VAR,    /* index: de Bruijn index */
  CORE_GLOBAL, /* index: definition number */
  CORE_NAT,    /* nat */
  CORE_TRAP,   /* an overflow; only quote writes it */
  CORE_UNIV,   /* Type nat */
  CORE_LAM,    /* fun name => b */
  CORE_PI,     /* (name : a) -> b */
  CORE_SIGMA,  /* Sigma (name : a) b */
  CORE_APP,    /* a b */
  CORE_OP,     /* a core name with all of its arguments */
  CORE_IVL,    /* M2: an interval term. nat: the IvlOp. a, b: the operands */
  CORE_PLAM,   /* M2: <name> b */
  CORE_PAPP    /* M2: a @ b. args[0], args[1]: the ends of the type of a */
} CoreKind;

typedef struct Core Core;
struct Core {
  CoreKind kind;
  uint64_t sort_level; /* checked type in Type (sort_level - 1), or 0 if unknown */
  uint32_t index;
  uint64_t nat;
  Op op;
  /* OP_FAMILY, OP_FOLD_FAMILY: the family. OP_CTOR, OP_PROJ: the
     constructor. OP_PURE, OP_MAP, OP_BIND, OP_FILTER: the Carrier. */
  uint32_t inst;
  uint32_t field; /* OP_PROJ: the field number */
  const char *name;
  const Core *a;
  const Core *b;
  const Core *const *args;
  uint32_t argc;
};

typedef enum {
  VAL_NAT,
  VAL_TRAP,
  VAL_UNIV,
  VAL_LAM,
  VAL_PI,
  VAL_SIGMA,
  VAL_OP,   /* a canonical value or a type */
  VAL_VAR,  /* neutral: a variable at a de Bruijn level */
  VAL_APP,  /* neutral: a neutral function applied to a value */
  VAL_STUCK, /* neutral: an operation with a neutral scrutinee */
  VAL_IVL,   /* M2: an interval normal form in ivl */
  VAL_PLAM,  /* M2: <name> body, a closure */
  VAL_PAPP   /* M2: neutral: dom @ arg. args: the two ends (or NULL) */
} ValKind;

typedef struct Value Value;
typedef struct Env Env;

/* The head is de Bruijn index 0. */
struct Env {
  const Value *value;
  const Env *next;
};

struct Value {
  ValKind kind;
  uint64_t sort_level; /* checked type in Type (sort_level - 1), or 0 if unknown */
  uint64_t nat; /* NAT: the number. UNIV: the level. VAR: the level. */
  Op op;        /* OP, STUCK */
  uint32_t inst;
  uint32_t field;
  const Value *const *args;
  uint32_t argc;
  const char *name; /* LAM, PI, SIGMA: the binder */
  const Value *dom; /* PI, SIGMA: the domain. APP: the function. PAPP: the path. */
  const Value *arg; /* APP, PAPP: the argument. A PI with no body: the codomain. */
  const Ivl *ivl;   /* IVL: the normal form */
  const Env *env;   /* LAM, PI, SIGMA: the closure */
  const Core *body;
};

typedef struct {
  const char *name;
  const Core *type; /* in the context of the family parameters */
  int recursive;    /* the field type is the family itself */
} FieldInfo;

typedef struct {
  const char *name;
  uint32_t family;
  const FieldInfo *fields;
  uint32_t field_count;
} CtorInfo;

/* A domain family. Its constructors are ctors[first_ctor ...]. A family
   with one constructor has one projection for each field. */
typedef struct {
  const char *name;
  const Core *const *param_types;
  uint32_t param_count;
  uint32_t first_ctor;
  uint32_t ctor_count;
  uint64_t universe; /* the largest universe of a constructor field */
} FamilyInfo;

typedef struct {
  const char *name;
  Origin origin;
  const Core *body;
  const Value *type;
  const Value *value; /* computed when first used */
} DefInfo;

/* glue-lang: a checked circuit. Binder i is de Bruijn level i: the public
   binders first, then the private binders. Each constraint core is in the
   context of all binders. */
typedef struct {
  ConstraintKind kind;
  /* EQ: the type Path Field a b (M2 ruling b); the lowering reads its two
     ends. RANGE8: the operand. */
  const Core *term;
  int line;
  int col;
} ConstraintInfo;

typedef struct {
  const char *name;
  const char *const *binders;
  uint32_t public_count;
  uint32_t private_count;
  const ConstraintInfo *constraints;
  uint32_t constraint_count;
} CircuitInfo;

/* M3 s5-i: one opening of a closure at a variable (closure_apply). */
typedef struct OpenMemo {
  const Value *binder;
  uint64_t key;
  const Value *out;
  const struct OpenMemo *next;
} OpenMemo;

/* The checked program and the evaluation state. No global state: every
   function gets this context. */
typedef struct {
  Arena *arena;
  Diag *diag;
  const char *def; /* the current definition, for diagnostics */
  DefInfo *defs;
  uint32_t def_count;
  uint32_t def_cap;
  FamilyInfo *families;
  uint32_t family_count;
  uint32_t family_cap;
  CtorInfo *ctors;
  uint32_t ctor_count;
  uint32_t ctor_cap;
  CircuitInfo *circuits;
  uint32_t circuit_count;
  uint32_t circuit_cap;
  unsigned depth;
  unsigned probe; /* M3 s5: > 0 while a Kan rule reads the head of a line */
  const OpenMemo **opens; /* M3 s5-i: closure_apply results, OPEN_BUCKETS lists */
  uint64_t fuel;
  int overflowed;
} Machine;

/* Each function returns NULL (or 0) after a diagnostic. A NULL input gives a
   NULL result, so a caller can test once at the end. */
const Value *val_nat(Machine *m, uint64_t n);
const Value *val_var(Machine *m, uint32_t level);
const Value *val_univ(Machine *m, uint64_t level);
const Value *val_op(Machine *m, ValKind kind, Op op, uint32_t inst, uint32_t field, const Value *const *args, uint32_t argc);
const Value *val_make(Machine *m, Op op, uint32_t inst, uint32_t argc, const Value *a, const Value *b, const Value *c);
/* A non-dependent function type. */
const Value *val_arrow(Machine *m, const Value *dom, const Value *cod);
/* M2: a fresh interval variable at LEVEL, and the end i0 (ONE zero) or i1. */
const Value *val_ivl_var(Machine *m, uint32_t level);
const Value *val_ivl_end(Machine *m, int one);
/* M2 s4: the interval value with the normal form IVL. */
const Value *val_ivl(Machine *m, const Ivl *ivl);
const Value *val_closure(Machine *m, ValKind kind, const char *name, const Value *dom, const Env *env, const Core *body);
int val_is(const Value *v, Op op);
const Env *env_push(Machine *m, const Env *env, const Value *value);
const Value *def_value(Machine *m, uint32_t index);
const Value *eval_core(Machine *m, const Env *env, const Core *core);
const Value *apply_value(Machine *m, const Value *fn, const Value *arg);
/* The body of a LAM, PI or SIGMA value at ARG. */
const Value *closure_apply(Machine *m, const Value *binder, const Value *arg);
/* 1 when A and B have the same normal form. LEVEL is the next free level. */
int conv_values(Machine *m, uint32_t level, const Value *a, const Value *b);
const Core *quote_value(Machine *m, uint32_t level, const Value *v);
/* M3 s2: value_bound opens closures at VALUE_BOUND_OPEN plus the current
   depth. These levels are above the kan.c probe levels and below 2^31. */
#define VALUE_BOUND_OPEN (3u << 29)
/* M3 s2: raise *BOUND above each free level of V below this scan's open
   level, including the temporary levels of active outer scans. */
int value_bound(Machine *m, const Value *v, uint32_t *bound);
/* V on conjunction K of the face normal form FACE. LEVEL is above each free
   level of V. */
const Value *value_on_face(Machine *m, uint32_t level, const Value *v, const Ivl *face, uint32_t k);
/* Writes V in the surface syntax. NAMES gives the names of the levels below
   NAME_COUNT. Returns 1 for a complete rendering, 0 on truncation or error. */
int value_print(Machine *m, const char *const *names, uint32_t name_count, const Value *v, char *buf, size_t cap);
const char *op_name(const Machine *m, Op op, uint32_t inst, uint32_t field);

#endif
