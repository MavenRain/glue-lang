/* The face lattice (CCHM). SHARED by the tcc kits.
   A face is an interval term read as "the term is i1" (IsOne in Cubical
   Agda). The face lattice is the De Morgan algebra (interval.h) divided by
   i /\ ~i = 0. A face in normal form is an interval normal form with no
   conjunction that holds both i and ~i. Each conjunction is then an
   assignment: the literal i sets i to i1, the literal ~i sets i to i0.
   0 (no conjunction) is the empty face. 1 (one empty conjunction) is the
   full face. */
#ifndef LANG_FRONT_FACE_H
#define LANG_FRONT_FACE_H

#include <stdint.h>

#include "front/interval.h"

/* One item of a conjunction read as a substitution. */
typedef struct {
  uint32_t level; /* the de Bruijn level of the interval variable */
  int one;        /* 1: the variable is i1;  0: the variable is i0 */
} FaceSet;

/* Each function that returns a face returns NULL after a diagnostic
   (OOM or INTERVAL_LIMIT). The arguments of meet, join, equal and leq are
   in face normal form. */
const Ivl *face_norm(IvlHeap *h, const Ivl *a);
const Ivl *face_meet(IvlHeap *h, const Ivl *a, const Ivl *b);
const Ivl *face_join(IvlHeap *h, const Ivl *a, const Ivl *b);
/* CCHM forall LEVEL. A: the join of the conjunctions of A that do not use
   LEVEL (i0 when there is none). */
const Ivl *face_forall(IvlHeap *h, const Ivl *a, uint32_t level);
/* Normal forms are unique, so equality is equality of the arrays. */
int face_equal(const Ivl *a, const Ivl *b);
/* Entailment: 1 when A <= B, 0 when not, -1 after a diagnostic. */
int face_leq(IvlHeap *h, const Ivl *a, const Ivl *b);

/* Conjunction K of the face A as a substitution. K < a->count. */
const Ivl *face_conj(IvlHeap *h, const Ivl *a, uint32_t k);
uint32_t face_conj_len(const Ivl *a, uint32_t k);
/* Item I of conjunction K. I < face_conj_len(a, k). */
FaceSet face_conj_at(const Ivl *a, uint32_t k, uint32_t i);
/* 1 when conjunction K of A sets LEVEL, with the value in *ONE;  else 0. */
int face_conj_sets(const Ivl *a, uint32_t k, uint32_t level, int *one);

#endif
