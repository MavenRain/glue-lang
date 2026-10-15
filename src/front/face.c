/* The face lattice. See face.h. Arena memory only. */
#include "front/face.h"

/* The literals of a conjunction are sorted and unique, so i (2L) and ~i
   (2L + 1) are next to each other when both are in it. */
static int conj_consistent(const Ivl *a, uint32_t k) {
  for (uint32_t i = a->starts[k]; i + 1u < a->starts[k + 1u]; i++)
    if ((a->lits[i] & 1u) == 0u && a->lits[i + 1u] == a->lits[i] + 1u) return 0;
  return 1;
}

/* A conjunction that holds an inconsistent one is inconsistent too, so the
   consistent conjunctions of an antichain are the face normal form. They
   stay sorted. */
const Ivl *face_norm(IvlHeap *h, const Ivl *a) {
  if (a == NULL) return NULL;
  Ivl *out = arena_alloc(h->arena, sizeof *out);
  uint32_t *starts = arena_alloc(h->arena, (a->count + 1u) * sizeof *starts);
  uint32_t *lits = arena_alloc(h->arena, (a->starts[a->count] + 1u) * sizeof *lits);
  if (out == NULL || starts == NULL || lits == NULL) {
    diag_fail(h->diag, "OOM", h->def, "the arena limit is reached");
    return NULL;
  }
  uint32_t count = 0, at = 0;
  for (uint32_t k = 0; k < a->count; k++) {
    if (!conj_consistent(a, k)) continue;
    starts[count++] = at;
    for (uint32_t i = a->starts[k]; i < a->starts[k + 1u]; i++) lits[at++] = a->lits[i];
  }
  starts[count] = at;
  out->count = count;
  out->starts = starts;
  out->lits = lits;
  return out;
}

const Ivl *face_meet(IvlHeap *h, const Ivl *a, const Ivl *b) {
  return face_norm(h, ivl_meet(h, a, b));
}

const Ivl *face_join(IvlHeap *h, const Ivl *a, const Ivl *b) {
  return face_norm(h, ivl_join(h, a, b));
}

int face_equal(const Ivl *a, const Ivl *b) {
  return ivl_equal(a, b);
}

int face_leq(IvlHeap *h, const Ivl *a, const Ivl *b) {
  const Ivl *m = face_meet(h, a, b);
  return m == NULL ? -1 : face_equal(m, a);
}

const Ivl *face_conj(IvlHeap *h, const Ivl *a, uint32_t k) {
  Ivl *out = arena_alloc(h->arena, sizeof *out);
  uint32_t *starts = arena_alloc(h->arena, 2u * sizeof *starts);
  if (out == NULL || starts == NULL) {
    diag_fail(h->diag, "OOM", h->def, "the arena limit is reached");
    return NULL;
  }
  starts[0] = 0;
  starts[1] = a->starts[k + 1u] - a->starts[k];
  out->count = 1;
  out->starts = starts;
  out->lits = a->lits + a->starts[k];
  return out;
}

uint32_t face_conj_len(const Ivl *a, uint32_t k) {
  return a->starts[k + 1u] - a->starts[k];
}

FaceSet face_conj_at(const Ivl *a, uint32_t k, uint32_t i) {
  uint32_t lit = a->lits[a->starts[k] + i];
  FaceSet s;
  s.level = lit >> 1;
  s.one = (lit & 1u) == 0u;
  return s;
}

int face_conj_sets(const Ivl *a, uint32_t k, uint32_t level, int *one) {
  for (uint32_t i = a->starts[k]; i < a->starts[k + 1u]; i++)
    if ((a->lits[i] >> 1) == level) {
      *one = (a->lits[i] & 1u) == 0u;
      return 1;
    }
  return 0;
}
