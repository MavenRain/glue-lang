/* De Morgan interval normal forms. See interval.h. Arena memory only. */
#include "front/interval.h"

/* One conjunction: LEN sorted, unique literals. */
typedef struct {
  const uint32_t *lits;
  uint32_t len;
} Conj;

static const Ivl *ivl_oom(IvlHeap *h) {
  diag_fail(h->diag, "OOM", h->def, "the arena limit is reached");
  return NULL;
}

static Conj conj_of(const Ivl *a, uint32_t k) {
  Conj c;
  c.lits = a->lits + a->starts[k];
  c.len = a->starts[k + 1u] - a->starts[k];
  return c;
}

/* 1 when each literal of A is in B. */
static int conj_subset(Conj a, Conj b) {
  uint32_t j = 0;
  for (uint32_t i = 0; i < a.len; i++) {
    while (j < b.len && b.lits[j] < a.lits[i]) j++;
    if (j == b.len || b.lits[j] != a.lits[i]) return 0;
  }
  return 1;
}

/* Length first, then lexicographic. */
static int conj_less(Conj a, Conj b) {
  if (a.len != b.len) return a.len < b.len;
  for (uint32_t i = 0; i < a.len; i++)
    if (a.lits[i] != b.lits[i]) return a.lits[i] < b.lits[i];
  return 0;
}

/* The sorted union of two conjunctions, in OUT. Returns 0 on OOM. */
static int conj_union(IvlHeap *h, Conj a, Conj b, Conj *out) {
  uint32_t *lits = arena_alloc(h->arena, (a.len + b.len + 1u) * sizeof *lits);
  if (lits == NULL) return 0;
  uint32_t i = 0, j = 0, n = 0;
  while (i < a.len || j < b.len) {
    uint32_t next = j == b.len || (i < a.len && a.lits[i] <= b.lits[j]) ? a.lits[i] : b.lits[j];
    i += i < a.len && a.lits[i] == next;
    j += j < b.len && b.lits[j] == next;
    lits[n++] = next;
  }
  out->lits = lits;
  out->len = n;
  return 1;
}

/* Drops each conjunction that holds another one (of two equal ones the
   first stays), sorts the rest and copies them into one Ivl. */
static const Ivl *normalize(IvlHeap *h, const Conj *items, uint32_t n) {
  Conj *kept = arena_alloc(h->arena, (n + 1u) * sizeof *kept);
  if (kept == NULL) return ivl_oom(h);
  uint32_t count = 0, total = 0;
  for (uint32_t i = 0; i < n; i++) {
    int drop = 0;
    for (uint32_t j = 0; j < n && !drop; j++)
      drop = j != i && (items[j].len < items[i].len || j < i) && conj_subset(items[j], items[i]);
    if (drop) continue;
    if (count == IVL_MAX_CONJ) {
      diag_fail(h->diag, "INTERVAL_LIMIT", h->def,
                "an interval term has more than %u conjunctions in normal form", IVL_MAX_CONJ);
      return NULL;
    }
    uint32_t at = count;
    while (at > 0 && conj_less(items[i], kept[at - 1u])) {
      kept[at] = kept[at - 1u];
      at--;
    }
    kept[at] = items[i];
    count++;
    total += items[i].len;
  }
  Ivl *out = arena_alloc(h->arena, sizeof *out);
  uint32_t *starts = arena_alloc(h->arena, (count + 1u) * sizeof *starts);
  uint32_t *lits = arena_alloc(h->arena, (total + 1u) * sizeof *lits);
  if (out == NULL || starts == NULL || lits == NULL) return ivl_oom(h);
  uint32_t at = 0;
  for (uint32_t k = 0; k < count; k++) {
    starts[k] = at;
    for (uint32_t i = 0; i < kept[k].len; i++) lits[at + i] = kept[k].lits[i];
    at += kept[k].len;
  }
  starts[count] = at;
  out->count = count;
  out->starts = starts;
  out->lits = lits;
  return out;
}

const Ivl *ivl_const(IvlHeap *h, int one) {
  uint32_t none = 0;
  Conj c;
  c.lits = &none;
  c.len = 0;
  return normalize(h, &c, one ? 1u : 0u);
}

const Ivl *ivl_lit(IvlHeap *h, uint32_t level, int rev) {
  uint32_t lit = level * 2u + (rev ? 1u : 0u);
  Conj c;
  c.lits = &lit;
  c.len = 1;
  return normalize(h, &c, 1u);
}

const Ivl *ivl_join(IvlHeap *h, const Ivl *a, const Ivl *b) {
  uint32_t n = a->count + b->count;
  Conj *items = arena_alloc(h->arena, (n + 1u) * sizeof *items);
  if (items == NULL) return ivl_oom(h);
  for (uint32_t k = 0; k < a->count; k++) items[k] = conj_of(a, k);
  for (uint32_t k = 0; k < b->count; k++) items[a->count + k] = conj_of(b, k);
  return normalize(h, items, n);
}

const Ivl *ivl_meet(IvlHeap *h, const Ivl *a, const Ivl *b) {
  uint32_t n = a->count * b->count;
  Conj *items = arena_alloc(h->arena, (n + 1u) * sizeof *items);
  if (items == NULL) return ivl_oom(h);
  for (uint32_t i = 0; i < a->count; i++)
    for (uint32_t j = 0; j < b->count; j++)
      if (!conj_union(h, conj_of(a, i), conj_of(b, j), &items[i * b->count + j])) return ivl_oom(h);
  return normalize(h, items, n);
}

/* ~(c1 \/ ... \/ cn) = ~c1 /\ ... /\ ~cn, and ~(l1 /\ ... /\ lm) = ~l1 \/ ... \/ ~lm. */
const Ivl *ivl_rev(IvlHeap *h, const Ivl *a) {
  const Ivl *acc = ivl_const(h, 1);
  for (uint32_t k = 0; acc != NULL && k < a->count; k++) {
    Conj c = conj_of(a, k);
    Conj *items = arena_alloc(h->arena, (c.len + 1u) * sizeof *items);
    uint32_t *neg = arena_alloc(h->arena, (c.len + 1u) * sizeof *neg);
    if (items == NULL || neg == NULL) return ivl_oom(h);
    for (uint32_t i = 0; i < c.len; i++) {
      neg[i] = c.lits[i] ^ 1u;
      items[i].lits = &neg[i];
      items[i].len = 1;
    }
    const Ivl *dual = normalize(h, items, c.len);
    acc = dual == NULL ? NULL : ivl_meet(h, acc, dual);
  }
  return acc;
}

int ivl_equal(const Ivl *a, const Ivl *b) {
  if (a->count != b->count) return 0;
  for (uint32_t k = 0; k <= a->count; k++)
    if (a->starts[k] != b->starts[k]) return 0;
  for (uint32_t i = 0; i < a->starts[a->count]; i++)
    if (a->lits[i] != b->lits[i]) return 0;
  return 1;
}

int ivl_is_const(const Ivl *a, int one) {
  return one ? a->count == 1u && a->starts[1] == a->starts[0] : a->count == 0u;
}
