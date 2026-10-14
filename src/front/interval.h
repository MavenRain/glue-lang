/* The De Morgan interval (CCHM). SHARED by the tcc kits.
   An element is a normal form: a join of conjunctions, and each conjunction
   is a meet of literals. A literal is level * 2 + sign, and sign 1 is the
   reversal ~i. 0 is the empty join. 1 is the join of one empty conjunction.
   The algebra is De Morgan, not Boolean: i /\ ~i stays and is not 0.
   No conjunction holds another, the literals of a conjunction are sorted
   and unique, and the conjunctions are sorted by length, then
   lexicographically. So each element has one normal form. */
#ifndef LANG_FRONT_INTERVAL_H
#define LANG_FRONT_INTERVAL_H

#include <stdint.h>

#include "front/base.h"

/* A normal form with more conjunctions fails with INTERVAL_LIMIT. */
#define IVL_MAX_CONJ 64u

typedef enum { IVL_ZERO, IVL_ONE, IVL_REV, IVL_MEET, IVL_JOIN } IvlOp;

/* Conjunction k is lits[starts[k]] .. lits[starts[k + 1] - 1].
   starts has count + 1 entries. */
typedef struct {
  uint32_t count;
  const uint32_t *starts;
  const uint32_t *lits;
} Ivl;

/* DEF names the definition in a diagnostic. */
typedef struct {
  Arena *arena;
  Diag *diag;
  const char *def;
} IvlHeap;

/* Each constructor returns NULL after a diagnostic (OOM or INTERVAL_LIMIT). */
const Ivl *ivl_const(IvlHeap *h, int one);
const Ivl *ivl_lit(IvlHeap *h, uint32_t level, int rev);
const Ivl *ivl_meet(IvlHeap *h, const Ivl *a, const Ivl *b);
const Ivl *ivl_join(IvlHeap *h, const Ivl *a, const Ivl *b);
const Ivl *ivl_rev(IvlHeap *h, const Ivl *a);
/* Normal forms are unique, so equality is equality of the arrays. */
int ivl_equal(const Ivl *a, const Ivl *b);
/* 1 when A is the constant 1 (ONE nonzero) or the constant 0 (ONE zero). */
int ivl_is_const(const Ivl *a, int one);

#endif
