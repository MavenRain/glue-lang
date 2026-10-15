/* M3: the Kan operations (CCHM). SHARED by the tcc kits.
   Each rule is a core term template. kan.c evaluates it in the environment
   extended with the argument values (M3 D7). */
#ifndef LANG_FRONT_KAN_H
#define LANG_FRONT_KAN_H

#include "front/core.h"

/* The fresh variable that reads the head former of a line. Depth makes
   nested probes different. Interval levels must be below 2^31, and the
   value_bound scan levels are above these probes. Face substitution must
   not build an environment at a probe level. */
#define KAN_PROBE (1u << 30)

/* transp L phi a. L is a PLAM closure, PHI is in face normal form.
   M3-PLAN 3.4: the face i1 rule, then the rule of the head former of L at
   a fresh variable, else a stuck value. */
const Value *kan_transp(Machine *m, const Value *line, const Value *phi, const Value *a);
/* The domain of L @ R (OP_KAN_DOM) and the codomain of L @ R at X
   (OP_KAN_COD), for a Pi, Sigma or Prod line. */
const Value *kan_dom(Machine *m, const Value *line, const Value *r);
const Value *kan_cod(Machine *m, const Value *line, const Value *r, const Value *x);
/* hcomp A phi u u0. PHI is in face normal form, U is a function of an
   interval variable. M3-PLAN 3.4 (s2). */
const Value *kan_hcomp(Machine *m, const Value *a, const Value *phi, const Value *u, const Value *u0);

#endif
