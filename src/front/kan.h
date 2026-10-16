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
   (OP_KAN_COD), for a Pi, Sigma or Prod line. M4 s2: for a family line,
   OP_KAN_DOM gives parameter INST of L @ R. */
const Value *kan_dom(Machine *m, const Value *line, const Value *r, uint32_t inst);
const Value *kan_cod(Machine *m, const Value *line, const Value *r, const Value *x);
/* hcomp A phi u u0. PHI is in face normal form, U is a function of an
   interval variable. M3-PLAN 3.4 (s2). */
const Value *kan_hcomp(Machine *m, const Value *a, const Value *phi, const Value *u, const Value *u0);
/* M4 s3 (D11): fold RES CASES H for an hcomp value H at the HIT FAMILY:
   hcomp RES phi (fun i => [ phi -> fold RES CASES (u i) ]) (fold RES CASES u0). */
const Value *kan_fold_hcomp(Machine *m, uint32_t family, const Value *res, const Value *const *cases, uint32_t ncases, const Value *h);
/* M3 s5 (OP_KAN_GLUE): part SEL of the Glue line L at R:  0 = A, 1 + k =
   the pack of branch k. At an end where the Glue face is i1 the part is
   read at a fresh level and then put on that end. */
const Value *kan_glue(Machine *m, const Value *line, const Value *r, uint32_t sel);
/* M3 s5 (OP_KAN_L2E): the pack of lineToEquiv L (Cubical Agda), for a
   projection or a conversion against a value that is not OP_KAN_L2E. */
const Value *kan_l2e(Machine *m, const Value *line);

#endif
