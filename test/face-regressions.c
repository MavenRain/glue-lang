/* Native regressions for interval binders, restricted systems and transport. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/front.h"

typedef enum { CHECK, PRINT_TYPE, QUOTE_TYPE, PRINT_VALUE } Observation;

typedef struct {
  const char *label;
  const char *source;
  Observation observation;
  const char *want;
  const char *code;
} Case;

#define DEPENDENT_PI \
  "def f : (i : I) -> Partial i Nat := fun i => [ i -> 0 ]\n"

#define DISJUNCTION \
  "def t : (p : Path Nat 0 1) -> (i : I) -> Partial (i \\/ ~i) (Path Nat (p @ i) (p @ i)) := fun p i => [ i \\/ ~i -> refl ]\n"

#define MEETING_DISJUNCTION \
  "def t : (p : Path Nat 0 1) -> (i : I) -> (j : I) -> Partial (i \\/ j) (Path Nat (p @ (i /\\ j)) (p @ (i /\\ j))) := fun p i j => [ i \\/ j -> refl ]\n"

static const Case CASES[] = {
  {"Pi transport with a dependent codomain",
   "def test : (A : I -> Type 0) -> (B : (i : I) -> A i -> Type 0) -> (f : (x : A i0) -> B i0 x) -> "
   "Path ((y : A i1) -> B i1 y) (transp (<i> (x : A i) -> B i x) i0 f) "
   "(fun y => transp (<i> B i (transp (<j> A (~j \\/ i)) i y)) i0 (f (transp (<j> A (~j)) i0 y))) := fun A B f => refl\n",
   CHECK, NULL, NULL},
  {"Sigma transport with a dependent second component",
   "def test : (A : I -> Type 0) -> (B : (i : I) -> A i -> Type 0) -> (p : Sigma (x : A i0) (B i0 x)) -> "
   "Path (Sigma (x : A i1) (B i1 x)) (transp (<i> Sigma (x : A i) (B i x)) i0 p) "
   "(pack (transp (<i> A i) i0 (witness p)) (transp (<i> B i (transp (<j> A (i /\\ j)) ~i (witness p))) i0 (payload p))) := fun A B p => refl\n",
   CHECK, NULL, NULL},
  {"Prod transport with two varying components",
   "def test : (A : I -> Type 0) -> (B : I -> Type 0) -> (p : Prod (A i0) (B i0)) -> "
   "Path (Prod (A i1) (B i1)) (transp (<i> Prod (A i) (B i)) i0 p) "
   "(pair (transp (<i> A i) i0 (first p)) (transp (<i> B i) i0 (second p))) := fun A B p => refl\n",
   CHECK, NULL, NULL},
  {"full-face Partial system keeps checking",
   "def test : Partial i1 Nat := [ i1 -> 2 ]\n",
   PRINT_VALUE, "2", NULL},
  {"full-face Partial system as an argument",
   "def test : (f : Partial i1 Nat -> Nat) -> Nat := fun f => f [ i1 -> 2 ]\n",
   PRINT_VALUE, "fun f => f 2", NULL},
  {"full-face Partial system checks path boundaries",
   "def test : Partial i1 (Path Nat 2 2) := [ i1 -> refl ]\n",
   PRINT_VALUE, "<i> 2", NULL},
  {"full-face system checks at the reduced type",
   "def test : Nat := [ i1 -> 2 ]\n",
   PRINT_VALUE, "2", NULL},
  {"full-face system under a restricted branch",
   "def test : (i : I) -> Partial i Nat := fun i => [ i -> [ i -> 2 ] ]\n",
   CHECK, NULL, NULL},
  {"full-face system still rejects disagreement",
   "def test : Partial i1 Nat := [ i1 -> 0, i1 -> 1 ]\n",
   CHECK, NULL, "FACE_AGREE"},
  {"full-face system still checks branch types",
   "def test : Partial i1 Nat := [ i1 -> unit ]\n",
   CHECK, NULL, "TYPE_MISMATCH"},
  {"empty system needs a Partial type",
   "def test : Nat := [ i0 -> 2 ]\n",
   CHECK, NULL, "PARTIAL_EXPECTED"},
  {"non-total system needs a Partial type",
   "def test : (i : I) -> Nat := fun i => [ i -> 2 ]\n",
   CHECK, NULL, "PARTIAL_EXPECTED"},
  {"dependent interval Pi conversion", DEPENDENT_PI
   "def test : (i : I) -> Partial i Nat := f\n", CHECK, NULL, NULL},
  {"dependent interval Pi printing", DEPENDENT_PI,
   PRINT_TYPE, "(i : I) -> Partial i Nat", NULL},
  {"dependent interval Pi quotation", DEPENDENT_PI,
   QUOTE_TYPE, "(i : I) -> Partial i Nat", NULL},
  {"inferred lambda returning a dependent interval function", DEPENDENT_PI
   "def test : (i : I) -> Partial i Nat := (fun (g : (i : I) -> Partial i Nat) => g) f\n",
   CHECK, NULL, NULL},
  {"restriction quotes an interval function local",
   "def test : (f : (j : I) -> Partial j Nat) -> (i : I) -> Partial i Nat := fun f i => [ i -> 0 ]\n",
   CHECK, NULL, NULL},
  {"disjunction keeps the start elaboration", DISJUNCTION
   "def test : (p : Path Nat 0 1) -> Partial i1 (Path Nat 0 0) := fun p => t p i0\n",
   PRINT_VALUE, "fun p => <i> 0", NULL},
  {"disjunction keeps the end elaboration", DISJUNCTION
   "def test : (p : Path Nat 0 1) -> Partial i1 (Path Nat 1 1) := fun p => t p i1\n",
   PRINT_VALUE, "fun p => <i> 1", NULL},
  {"disjunction agrees with an overlapping branch",
   "def test : (p : Path Nat 0 1) -> (i : I) -> Partial (i \\/ ~i) (Path Nat (p @ i) (p @ i)) := fun p i => [ i \\/ ~i -> refl, i -> refl ]\n",
   CHECK, NULL, NULL},
  {"meeting conjunctions keep their own elaborations", MEETING_DISJUNCTION
   "def test : (p : Path Nat 0 1) -> Partial i1 (Path Nat 0 0) := fun p => t p i1 i0\n",
   PRINT_VALUE, "fun p => <i> 0", NULL},
  {"meeting conjunctions agree at their intersection", MEETING_DISJUNCTION
   "def test : (p : Path Nat 0 1) -> Partial i1 (Path Nat 1 1) := fun p => t p i1 i1\n",
   PRINT_VALUE, "fun p => <i> 1", NULL},
  {"disjunction still rejects disagreement",
   "def test : (i : I) -> (j : I) -> Partial (i \\/ j) Nat := fun i j => [ i \\/ j -> 0, i -> 1 ]\n",
   CHECK, NULL, "FACE_AGREE"},
  {"duplicate branches give the same dependent index",
   "def test : (i : I) -> (F : Partial i Nat -> Type 0) -> F [ i -> 0, i -> 0 ] -> F [ i -> 0 ] := fun i F x => x\n",
   CHECK, NULL, NULL},
  {"reordered branches give the same dependent index",
   "def test : (i : I) -> (F : Partial (i \\/ ~i) Nat -> Type 0) -> F [ i -> 1, ~i -> 0 ] -> F [ ~i -> 0, i -> 1 ] := fun i F x => x\n",
   CHECK, NULL, NULL},
  {"face decomposition gives the same dependent index",
   "def test : (i : I) -> (j : I) -> (F : Partial (i \\/ j) Nat -> Type 0) -> F [ i \\/ j -> 0 ] -> F [ j -> 0, i -> 0 ] := fun i j F x => x\n",
   CHECK, NULL, NULL},
  {"system conversion restricts path applications",
   "def test : (i : I) -> (p : Path Nat 0 1) -> (F : Partial (~i) Nat -> Type 0) -> F [ ~i -> p @ i ] -> F [ ~i -> 0 ] := fun i p F x => x\n",
   CHECK, NULL, NULL},
  {"system conversion preserves a free interval under a closure",
   "def test : (i : I) -> (j : I) -> (p : Path Nat 0 1) -> (F : Partial i (Path Nat (p @ j) (p @ j)) -> Type 0) -> F [ i -> <k> p @ j, i -> <k> p @ j ] -> F [ i -> refl ] := fun i j p F x => x\n",
   CHECK, NULL, NULL},
  {"system conversion preserves ordinary and interval variables",
   "def test : (i : I) -> (j : I) -> (p : Path Nat 0 1) -> (h : Nat -> Nat) -> (F : Partial i (Nat -> Nat) -> Type 0) -> F [ i -> fun n => h (p @ j), i -> fun n => h (p @ j) ] -> F [ i -> fun n => h (p @ j) ] := fun i j p h F x => x\n",
   CHECK, NULL, NULL},
  {"system conversion ignores an empty face",
   "def test : (F : Partial i0 Nat -> Type 0) -> F [ i0 -> 0 ] -> F [ i0 -> 1 ] := fun F x => x\n",
   CHECK, NULL, NULL},
  {"system conversion keeps distinct values distinct",
   "def test : (i : I) -> (F : Partial i Nat -> Type 0) -> F [ i -> 0 ] -> F [ i -> 1 ] := fun i F x => x\n",
   CHECK, NULL, "TYPE_MISMATCH"},
  {"system conversion leaves an unrestricted interval free",
   "def test : (i : I) -> (j : I) -> (p : Path Nat 0 1) -> (F : Partial i Nat -> Type 0) -> F [ i -> p @ j ] -> F [ i -> 0 ] := fun i j p F x => x\n",
   CHECK, NULL, "TYPE_MISMATCH"}
};

static int observe(Machine *m, Observation observation, char *buf, size_t cap) {
  const Value *value = NULL;
  switch (observation) {
  case CHECK:
    return 1;
  case PRINT_TYPE:
    value = m->defs[m->def_count - 1u].type;
    break;
  case QUOTE_TYPE:
    value = eval_core(m, NULL, quote_value(m, 0, m->defs[m->def_count - 1u].type));
    break;
  case PRINT_VALUE:
    value = def_value(m, m->def_count - 1u);
    break;
  }
  return value != NULL && value_print(m, NULL, 0, value, buf, cap);
}

static int check_case(const Case *test) {
  Arena arena;
  Diag diag;
  DeclList decls;
  Machine machine;
  char actual[2048] = "";
  int ok;
  int failed;
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  ok = front_load(&arena, test->label, test->source, strlen(test->source), &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  if (ok && test->code == NULL)
    ok = observe(&machine, test->observation, actual, sizeof actual) && !diag.set;
  failed = test->code != NULL ? ok || diag.code == NULL || strcmp(diag.code, test->code) != 0
    : !ok || (test->want != NULL && strcmp(actual, test->want) != 0);
  if (failed) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", test->label,
      test->code != NULL ? test->code : test->want != NULL ? test->want : "ok",
      !ok ? diag.code == NULL ? "no diagnostic" : diag.code : test->want != NULL ? actual : "ok");
    if (diag.set)
      diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return failed;
}

int main(void) {
  size_t i;
  int failed = 0;
  for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++)
    failed += check_case(&CASES[i]);
  printf("native face regressions: %lu checked, %d failed\n", (unsigned long)i, failed);
  return failed != 0;
}
