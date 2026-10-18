/* The syntax tree. SHARED by the tcc kits. Terms and types share one syntax. */
#ifndef LANG_FRONT_AST_H
#define LANG_FRONT_AST_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  TERM_VAR,   /* name */
  TERM_NAT,   /* nat */
  TERM_APP,   /* left applied to right */
  TERM_LAM,   /* fun (name : left) => right; left is NULL for a bare binder */
  TERM_PI,    /* (name : left) -> right; name is NULL for left -> right */
  TERM_SIGMA, /* Sigma (name : left) right, or (name : left) * right */
  TERM_UNIV,  /* Type nat */
  TERM_ARITH, /* left OP right, OP = nat: 0 +, 1 -, 2 * (Field) */
  TERM_PLAM,  /* <name> right: a path abstraction */
  TERM_PAPP,  /* left @ right: a path applied to an interval term */
  TERM_IVL,   /* nat = IvlOp (interval.h): 2 ~left, 3 left /\ right, 4 left \/ right */
  TERM_SYS    /* M2: one branch `left -> right` of a system [ ... ]; next: the next branch */
} TermKind;

typedef struct Term Term;
struct Term {
  TermKind kind;
  int line;
  int col;
  const char *name;
  uint64_t nat;
  int big; /* NAT: the literal is above 2^64 - 1. name: the digits */
  const Term *left;
  const Term *right;
  const Term *next; /* SYS: the next branch, or NULL */
};

/* A named field of a constructor, or a parameter of a family. */
typedef struct {
  const char *name;
  const Term *type;
  int line;
  int col;
} Field;

typedef struct {
  const char *name;
  const Field *fields;
  size_t field_count;
  /* M4 s1: a path constructor binds interval names and gives a boundary
     system (the TERM_SYS chain). binder_count is 0 for a point constructor
     and boundary is NULL. */
  const char *const *binders;
  size_t binder_count;
  const Term *boundary;
  int line;
  int col;
} Ctor;

typedef enum {
  DECL_DEF,    /* def [rec] name : type := body */
  DECL_AXIOM,  /* axiom name : type */
  DECL_FAMILY, /* family name params := ctor | ... */
  DECL_CIRCUIT /* glue-lang: circuit name public binders private binders := constraints */
} DeclKind;

/* glue-lang: a circuit constraint. EQ: lhs == rhs. RANGE8: range8 lhs. */
typedef enum {
  CONSTRAINT_EQ,
  CONSTRAINT_RANGE8
} ConstraintKind;

typedef struct {
  ConstraintKind kind;
  const Term *lhs;
  const Term *rhs; /* NULL for RANGE8 */
  int line;
  int col;
} Constraint;

/* A family is legal only in the domain file (rule R1). */
typedef enum {
  ORIGIN_DOMAIN,
  ORIGIN_PROGRAM
} Origin;

typedef struct {
  DeclKind kind;
  Origin origin;
  int is_rec;
  int block; /* M5 s4 (D6): 1 and 2 for the first and the second family of a mutual block, else 0 */
  int line;
  int col;
  const char *name;
  const Term *type;
  const Term *body;
  const Field *params;
  size_t param_count;
  const Ctor *ctors;
  size_t ctor_count;
  /* DECL_CIRCUIT: params are the public binders. */
  const Field *privates;
  size_t private_count;
  /* M6 s5: the hint binders. The `type` of each holds the hint term e. */
  const Field *hints;
  size_t hint_count;
  const Constraint *constraints;
  size_t constraint_count;
} Decl;

/* Domain declarations come first, then the program. */
typedef struct {
  Decl *items;
  size_t count;
  size_t cap;
} DeclList;

#endif
