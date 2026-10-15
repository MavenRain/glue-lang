/* Native regressions for dependent path elaboration. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/front.h"

typedef struct {
  const char *label;
  const char *source;
  const char *code;
} Case;

static const Case CASES[] = {
  {"dependent cong at refl",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Nat -> Type 0) -> (f : (x : Nat) -> B x) -> Path (B 0) (f 0) (f 0) := fun B f => cong f r\n", NULL},
  {"dependent lambda cong at refl",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Nat -> Type 0) -> (f : (x : Nat) -> B x) -> Path (B 0) (f 0) (f 0) := fun B f => cong (fun (x : Nat) => f x) r\n", NULL},
  {"dependent cong over a varying line",
   "def test : (B : Nat -> Type 0) -> (f : (x : Nat) -> B x) -> (p : Path Nat 0 1) -> PathP (<i> B (p @ i)) (f 0) (f 1) := fun B f p => cong f p\n", NULL},
  {"dependent cong beta",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Nat -> Type 0) -> (f : (x : Nat) -> B x) -> Path (Path (B 0) (f 0) (f 0)) (cong f r) refl := fun B f => refl\n", NULL},
  {"dependent funExt check",
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> (h : (x : Nat) -> Path (B x) (f x) (g x)) -> Path ((x : Nat) -> B x) f g := fun B f g h => funExt h\n", NULL},
  {"dependent funExt inference",
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> (h : (x : Nat) -> Path (B x) (f x) (g x)) -> (x : Nat) -> Path (B x) (f x) (g x) := fun B f g h x => happly (funExt h) x\n", NULL},
  {"dependent funExt endpoint",
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> (h : (x : Nat) -> Path (B x) (f x) (g x)) -> (x : Nat) -> Path (B x) ((funExt h @ i0) x) (f x) := fun B f g h x => refl\n", NULL},
  {"dependent happly endpoint",
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> (p : Path ((x : Nat) -> B x) f g) -> (x : Nat) -> Path (B x) ((happly p x) @ i0) (f x) := fun B f g p x => refl\n", NULL},
  {"dependent sym involution",
   "def test : (p : Path Nat 0 1) -> (q : PathP (<i> Path Nat 0 (p @ i)) refl p) -> Path (PathP (<i> Path Nat 0 (p @ i)) refl p) (sym (sym q)) q := fun p q => refl\n", NULL},
  {"nested hcomp normalization",
   "def nested : (n : Nat) -> (q : Path Nat n n) -> (k l : I) -> Nat := fun n q k l => hcomp Nat k (fun i => [ k -> hcomp Nat l (fun j => [ l -> q @ (i /\\ j) ]) n ]) n\n"
   "def test : (n : Nat) -> (q : Path Nat n n) -> (k l : I) -> Path Nat (nested n q k l) (nested n q k l) := fun n q k l => refl\n", NULL},
  {"dependent Sigma hcomp normalization",
   "def dep : (p : Sigma (n : Nat) (Path Nat n n)) -> (s : Path (Sigma (n : Nat) (Path Nat n n)) p p) -> (k : I) -> Sigma (n : Nat) (Path Nat n n) := fun p s k => hcomp (Sigma (n : Nat) (Path Nat n n)) k (fun i => [ k -> s @ i ]) p\n"
   "def test : (p : Sigma (n : Nat) (Path Nat n n)) -> (s : Path (Sigma (n : Nat) (Path Nat n n)) p p) -> (k : I) -> Path (Sigma (n : Nat) (Path Nat n n)) (dep p s k) (dep p s k) := fun p s k => refl\n", NULL},
  {"nested hcomp preserves a nonconstant tube",
   "def nested : (n : Nat) -> (q : Path Nat n n) -> (k l : I) -> Nat := fun n q k l => hcomp Nat k (fun i => [ k -> hcomp Nat l (fun j => [ l -> q @ (i /\\ j) ]) n ]) n\n"
   "def test : (n : Nat) -> (q : Path Nat n n) -> (k l : I) -> Path Nat (nested n q k l) n := fun n q k l => refl\n", "TYPE_REFL"},
  {"nested PathP transport normalization",
   "def pathVar : (q : Path Nat 0 1) -> PathP (<j> Path Nat 0 1) q q := fun q => transp (<i> PathP (<j> Path Nat 0 (q @ i)) (<j> q @ (i /\\ j)) (<j> q @ (i /\\ j))) i0 (<j> <k> 0)\n"
   "def test : (q : Path Nat 0 1) -> Path (PathP (<j> Path Nat 0 1) q q) (pathVar q) (pathVar q) := fun q => refl\n", NULL},
  {"cong rejects a wrong domain",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Flag -> Type 0) -> (f : (x : Flag) -> B x) -> Path (B flagYes) (f flagYes) (f flagYes) := fun B f => cong f r\n", "TYPE_MISMATCH"},
  {"cong rejects wrong expected endpoints",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> Path (B 0) (f 0) (g 0) := fun B f g => cong f r\n", "TYPE_MISMATCH"},
  {"ua accepts an applied type family",
   "def test : (P : Nat -> Type 0) -> (x : Nat) -> Path (Type 0) (ua (idEquiv (P x)) @ i0) (P x) := fun P x => refl\n", NULL},
  {"ua accepts a domain family",
   "def test : Path (Type 0) (ua (idEquiv Color) @ i0) Color := refl\n", NULL},
  {"ua accepts a type computed by fold",
   "def T : Nat -> Type 0 := fun n => fold (fun A => Option A) Nat n\n"
   "def test : (n : Nat) -> Path (Type 0) (ua (idEquiv (T n)) @ i0) (T n) := fun n => refl\n", NULL},
  {"isEquiv accepts a type computed by fold",
   "def T : Nat -> Type 0 := fun n => fold (fun A => Option A) Nat n\n"
   "def test : (n : Nat) -> (f : T n -> T n) -> Type 0 := fun n f => isEquiv f\n", NULL},
  {"ua accepts an applied function computed by fold",
   "def T : Nat -> Nat -> Type 0 := fun n => fold (fun f => f) (fun x => Nat) n\n"
   "def test : (n : Nat) -> (e : Equiv (T n 0) (T n 0)) -> Path (Type 0) (ua e @ i0) (T n 0) := fun n e => refl\n", NULL},
  {"fiber and isEquiv accept applied type families",
   "def eq : (P : Nat -> Type 0) -> (x : Nat) -> isEquiv (first (idEquiv (P x))) := fun P x => second (idEquiv (P x))\n"
   "def test : (P : Nat -> Type 0) -> (x : Nat) -> (y : P x) -> isContr (fiber (first (idEquiv (P x))) y) := fun P x y => second (idEquiv (P x)) y\n", NULL},
  {"ua accepts a neutral path of types",
   "def test : (p : Path (Type 0) Nat Nat) -> (j : I) -> Path (Type 0) (ua (idEquiv (p @ j)) @ i0) (p @ j) := fun p j => refl\n", NULL},
  {"ua accepts a projected type",
   "def test : (s : Sigma (A : Type 0) A) -> Path (Type 0) (ua (idEquiv (witness s)) @ i0) (witness s) := fun s => refl\n", NULL},
  {"ua respects higher family universes",
   "def test : (P : Nat -> Type 1) -> (x : Nat) -> Path (Type 1) (ua (idEquiv (P x)) @ i1) (P x) := fun P x => refl\n", NULL},
  {"isEquiv does not lower a family universe",
   "def test : (P : Nat -> Type 1) -> (x : Nat) -> Type 0 := fun P x => isEquiv (first (idEquiv (P x)))\n", "TYPE_MISMATCH"},
  {"ua preserves checked universes under restriction",
   "def test : (P : I -> Type 0) -> (j k : I) -> (e : Equiv (P j) (P j)) -> Partial k (Path (Type 0) (ua e @ i0) (P j)) := fun P j k e => [ k -> refl ]\n", NULL},
  {"isEquiv still rejects a dependent function",
   "def test : (P : Nat -> Type 0) -> (f : (x : Nat) -> P x) -> Type 0 := fun P f => isEquiv f\n", "TYPE_NOT_FUNCTION"},
  {"unglue distributes through a partial system",
   "def g : (j k : I) -> Partial k (Glue Nat [ j -> pack Nat (idEquiv Nat) ]) := fun j k => [ k -> glue [ j -> 5 ] 5 ]\n"
   "def test : (j k : I) -> (f : Partial k Nat -> Nat) -> Path Nat (f [ k -> unglue (g j k) ]) (f [ k -> 5 ]) := fun j k f => refl\n", NULL},
  {"glue with an empty face still checks its base",
   "def test : Glue Nat [ i0 -> pack Nat (idEquiv Nat) ] := glue [ i0 -> 5 ] flagYes\n", "TYPE_MISMATCH"},
  {"an empty glue system must cover its declared face",
   "def test : (j : I) -> Glue Nat [ j -> pack Nat (idEquiv Nat) ] := fun j => glue [ i0 -> 5 ] 5\n", "FACE_COVER"},
  {"cong rejects a varying input line",
   "def test : (p : Path Nat 0 1) -> (q : PathP (<i> Path Nat 0 (p @ i)) refl p) -> Path (Path Nat 0 0) refl refl := fun p q => cong (fun (r : Path Nat 0 0) => r) q\n", "TYPE_MISMATCH"}
};

typedef struct {
  const char *label;
  const char *source;
  const char *expected;
} EvalCase;

static const EvalCase EVAL_CASES[] = {
  {"glue accepts an empty face",
   "def g : Glue Nat [ i0 -> pack Nat (idEquiv Nat) ] := glue [ i0 -> 5 ] 5\n"
   "def test : Nat := unglue g\n", "5"},
  {"glue ignores an empty branch beside a live branch",
   "def g : (j : I) -> Glue Nat [ j -> pack Nat (idEquiv Nat) ] := fun j => glue [ i0 -> nil, j -> 5 ] 5\n"
   "def test : (j : I) -> Nat := fun j => unglue (g j)\n", "fun j => 5"},
  {"Option hcomp ignores an empty branch",
   "def test : Option Nat := hcomp (Option Nat) i0 (fun i => [ i0 -> none ]) (some 0)\n",
   "some 0"},
  {"List hcomp ignores an empty branch",
   "def test : List Nat := hcomp (List Nat) i0 (fun i => [ i0 -> nil ]) (cons 0 nil)\n",
   "cons 0 nil"},
  {"Sum hcomp projects on the branch face",
   "def test : (A : Type 0) -> (x : A) -> (p : Path (Sum A A) (inl x) (inl x)) -> (k : I) -> Sum A A := fun A x p k => hcomp (Sum A A) k (fun i => [ k -> p @ k ]) (inl x)\n",
   "fun A => fun x => fun p => fun k => inl (hcomp A k (fun i => [ k -> x ]) x)"},
  {"Option hcomp projects on the branch face",
   "def test : (A : Type 0) -> (x : A) -> (p : Path (Option A) (some x) (some x)) -> (k : I) -> Option A := fun A x p k => hcomp (Option A) k (fun i => [ k -> p @ k ]) (some x)\n",
   "fun A => fun x => fun p => fun k => some (hcomp A k (fun i => [ k -> x ]) x)"},
  {"List hcomp projects both fields on the branch face",
   "def test : (A : Type 0) -> (x : A) -> (p : Path (List A) (cons x nil) (cons x nil)) -> (k : I) -> List A := fun A x p k => hcomp (List A) k (fun i => [ k -> p @ k ]) (cons x nil)\n",
   "fun A => fun x => fun p => fun k => cons (hcomp A k (fun i => [ k -> x ]) x) nil"},
  {"Sum hcomp projects each conjunction of a branch face",
   "def test : (A : Type 0) -> (x : A) -> (p : Path (Sum A A) (inl x) (inl x)) -> (k : I) -> Sum A A := fun A x p k => hcomp (Sum A A) (k \\/ ~k) (fun i => [ k \\/ ~k -> p @ k ]) (inl x)\n",
   "fun A => fun x => fun p => fun k => inl (hcomp A (k \\/ ~k) (fun i => [ k -> x, ~k -> x ]) x)"},
  {"disjunctive hcomp preserves distinct varying field tubes",
   "def test : (A : Type 0) -> (x : A) -> (s t : Path A x x) -> (q : (i : I) -> Path (Option A) (some (s @ i)) (some (t @ i))) -> (k : I) -> Option A := fun A x s t q k => hcomp (Option A) (k \\/ ~k) (fun i => [ k \\/ ~k -> q i @ k ]) (some x)\n",
   "fun A => fun x => fun s => fun t => fun q => fun k => some (hcomp A (k \\/ ~k) (fun i => [ k -> t @ i, ~k -> s @ i ]) x)"}
};

static int eval_case(const EvalCase *test) {
  Arena arena;
  Diag diag;
  DeclList decls;
  Machine machine;
  const Value *value = NULL;
  char output[2048] = "";
  int failed;
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  if (front_load(&arena, test->label, test->source, strlen(test->source), &decls, &diag)
      && check_program(&arena, &decls, &machine, &diag))
    value = def_value(&machine, machine.def_count - 1u);
  failed = value == NULL || !value_print(&machine, NULL, 0, value, output, sizeof output)
    || strcmp(output, test->expected) != 0;
  if (failed) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", test->label, test->expected, output);
    if (diag.set)
      diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return failed;
}

static int check_case(const Case *test) {
  Arena arena;
  Diag diag;
  DeclList decls;
  Machine machine;
  int ok;
  int failed;
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  ok = front_load(&arena, test->label, test->source, strlen(test->source), &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  failed = test->code == NULL ? !ok : ok || diag.code == NULL || strcmp(diag.code, test->code) != 0;
  if (failed) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", test->label,
      test->code == NULL ? "ok" : test->code, ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code);
    if (!ok)
      diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return failed;
}

int main(void) {
  size_t i;
  size_t j;
  int failed = 0;
  for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++)
    failed += check_case(&CASES[i]);
  for (j = 0; j < sizeof EVAL_CASES / sizeof EVAL_CASES[0]; j++)
    failed += eval_case(&EVAL_CASES[j]);
  printf("native path regressions: %lu checked, %d failed\n", (unsigned long)(i + j), failed);
  return failed != 0;
}
