/* Native regressions for higher inductive constructor boundaries. */
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/front.h"
#include "front/face.h"

typedef struct {
  const char *label;
  const char *domain;
  const char *source;
  const char *code;
  const char *normal;
} Case;

static const char WALK[] =
  "family Walk := go (step : Walk) <i> [ ~i -> step, i -> step ]\n";

static const char NESTED[] =
  "family Nested := seed | next (prior : Nested)"
  " | join (branch : Nested) <i> [ ~i -> next branch, i -> next branch ]\n";

static const char SHAPE[] =
  "family Shape := point | edge <i> [ ~i -> point, i -> point ]"
  " | square <i j> [ ~i -> point, i -> edge @ j, ~j -> point, j -> point ]"
  " | cube <i j k> [ ~i -> point, i -> square @ j @ k, ~j -> point,"
  " j -> square @ i @ k, ~k -> point, k -> point ]\n";

static const Case FACE_CASES[] = {
  {"fold preserves endpoints after applying recursive field cases", "",
   "def test : (B : Type 0) -> (b : B) -> (pB : isProp B) -> (unused : Nat) -> Path B b b"
   " := fun B b pB unused => <j> fold B (fun n => b) (fun x y => pB x y)"
   " (squash Nat (inc Nat 0) (inc Nat 1) @ j)\n", NULL, NULL},
  {"fold preserves the endpoints of a neutral path case", "",
   "def test : (B : Type 0) -> (b : B) -> (l : Path B b b) -> (unused : Nat) -> Path B b b"
   " := fun B b l unused => <j> fold B b l (loop @ j)\n",
   NULL, NULL},
  {"transport respects a path constructor application boundary",
   "family FunH (A B : Type 0) := funStart (startFn : A -> B) (startArg : A)"
   " | funEnd (endVal : B)"
   " | funEdge (linkFn : A -> B) (linkArg : A) <i>"
   " [ ~i -> funStart A B linkFn linkArg, i -> funEnd A B (linkFn linkArg) ]\n",
   "def test : (L M : I -> Type 0) -> (f : L i0 -> M i0) -> (a : L i0) ->"
   " Path (FunH (L i1) (M i1))"
   " (transp (<i> FunH (L i) (M i)) i0 (funStart (L i0) (M i0) f a))"
   " (transp (<i> FunH (L i) (M i)) i0 (funEnd (L i0) (M i0) (f a)))"
   " := fun L M f a => <j> transp (<i> FunH (L i) (M i)) i0 (funEdge (L i0) (M i0) f a @ j)\n",
   NULL, NULL}
};

static const Case CASES[] = {
  {"dependent non-recursive constructor fields and projections compute",
   "family Dep := dep (depType : Type 0) (depValue : depType)\n",
   "def test : Nat := depValue (dep Nat 7)\n", NULL, "7"},
  {"plain fields cannot depend on folded recursive fields",
   "family Evidence := evidenceZero"
   " | evidenceNext (evidencePrior : Evidence)"
   " (evidenceProof : Path Nat (fold 0 0 evidencePrior) 0)\n",
   "def test : Evidence := evidenceNext (evidenceNext evidenceZero refl) refl\n",
   "DOMAIN_FAMILY", NULL},
  {"fold maps a boundary path lambda without folding its images again",
   "family PathField := pathPoint"
   " | pathEdge (edgePath : Path PathField pathPoint pathPoint) <i>"
   " [ ~i -> pathPoint, i -> pathPoint ]"
   " | pathSquare (squarePath : Path PathField pathPoint pathPoint) <i j>"
   " [ ~i -> pathEdge (<k> squarePath @ ~k) @ j,"
   " i -> pathEdge (<k> squarePath @ ~k) @ j,"
   " ~j -> pathPoint, j -> pathPoint ]\n",
   "def test : (B : Type 0) -> (b : B)"
   " -> (e : Path B b b -> Path B b b) -> PathField -> B"
   " := fun B b e x => fold B b e (fun p => <i> <j> e (<k> p @ ~k) @ j) x\n", NULL, NULL},
  {"later function fields remain independent despite captured fields",
   "family Duo (A : Type 0) := duo (duoLeft : A) (duoRight : A -> A)\n",
   "def test : (L : I -> Type 0) -> (a : L i0) -> (f : L i0 -> L i0)"
   " -> Path (L i1 -> L i1)"
   " (duoRight (transp (<i> Duo (L i)) i0 (duo (L i0) a f)))"
   " (transp (<i> L i -> L i) i0 f) := fun L a f => refl\n", NULL, NULL},
  {"fold preserves path field images inside boundary constructors",
   "family PathField := pathPoint"
   " | pathEdge (edgePath : Path PathField pathPoint pathPoint) <i>"
   " [ ~i -> pathPoint, i -> pathPoint ]"
   " | pathSquare (squarePath : Path PathField pathPoint pathPoint) <i j>"
   " [ ~i -> pathEdge squarePath @ j, i -> pathEdge squarePath @ j,"
   " ~j -> pathPoint, j -> pathPoint ]\n",
   "def test : (B : Type 0) -> (b : B)"
   " -> (e : Path B b b -> Path B b b) -> PathField -> B"
   " := fun B b e x => fold B b e (fun p => <i> <j> e p @ j) x\n", NULL, NULL},
  {"transport keeps the parameter scope of later independent fields",
   "family Duo (A : Type 0) := duo (duoLeft : A) (duoRight : A)\n",
   "def test : (L : I -> Type 0) -> (a b : L i0) -> Path (L i1)"
   " (duoRight (transp (<i> Duo (L i)) i0 (duo (L i0) a b)))"
   " (transp (<i> L i) i0 b) := fun L a b => refl\n", NULL, NULL},
  {"composition keeps the parameter scope of later independent fields",
   "family Duo (A : Type 0) := duo (duoLeft : A) (duoRight : A)\n",
   "def test : (A : Type 0) -> (a b : A) -> (j : I) -> Path A"
   " (duoRight (hcomp (Duo A) j (fun i => [ j -> duo A a b ]) (duo A a b)))"
   " (hcomp A j (fun i => [ j -> b ]) b) := fun A a b j => refl\n", NULL, NULL},
  {"fold erases an empty neutral hcomp tube", "",
   "def test : (B : Type 0) -> (b : B) -> (l : Path B b b)"
   " -> (u : (i : I) -> Partial i0 Circle)"
   " -> Path B (fold B b l (hcomp Circle i0 u base))"
   " (hcomp B i0 (fun i => [ i0 -> b ]) b) := fun B b l u => refl\n",
   NULL, NULL},
  {"parameterized recursive plain family fold computes",
   "family Chain (A : Type 0) := chainEnd | chainLink (entry : A) (suffix : Chain A)\n",
   "def test : Nat := fold 0 (fun n rest => natAdd n rest)"
   " (chainLink Nat 2 (chainLink Nat 3 (chainEnd Nat)))\n", NULL, "5"},
  {"fold maps a constructor around a recursive boundary field", NESTED,
   "def nestedSize : Nested -> Nat := fun x => fold Nat 0 (fun n => natAdd n 1) (fun n => refl) x\n"
   "def test : Nat := nestedSize (next seed)\n", NULL, "1"},
  {"fold checks mapped recursive constructor boundaries", NESTED,
   "def test : Nested -> Nat := fun x => fold Nat 0 (fun n => natAdd n 1) (fun n => <i> 0) x\n",
   "HIT_CASE", NULL},
  {"fold computes on higher path constructors", SHAPE,
   "def test : (i j k : I) -> Path Shape"
   " (fold Shape point edge square cube (cube @ i @ j @ k)) (cube @ i @ j @ k)"
   " := fun i j k => refl\n", NULL, NULL},
  {"HIT fold cannot distinguish connected points", "",
   "def f : Susp Nat -> Nat := fun (x : Susp Nat) => fold Nat 0 1 (fun (a : Nat) => refl) x\n"
   "def bad : Path Nat 0 1 := cong f (merid Nat 0)\n", "HIT_CASE", NULL},
  {"HIT fold computes on a path constructor interior", "",
   "def test : Path (Path Nat 0 0) (<i> fold Nat 0 refl (loop @ i)) refl := refl\n", NULL, NULL},
  {"HIT fold needs the result type and one case per constructor", "",
   "def bad : Nat := fold 0 0 base\n", "HIT_CASES", NULL},
  {"HIT boundaries refuse the plain family fold shape",
   "family Sneak := aa | bb | pp (item : Sneak) <i> [ ~i -> fold aa bb aa item, i -> aa ]\n",
   "def test : Sneak := aa\n", "HIT_CASES", NULL},
  {"a recursive field is the family at its own parameters",
   "family Twist (A : Type 0) := tw (inner : Twist Nat)\n",
   "def test : Nat := 0\n", "DOMAIN_FAMILY", NULL},
  {"a path constructor field is not a projection", WALK,
   "def bad : Walk -> Walk := fun (x : Walk) => step x\n", "TYPE_MISMATCH", NULL},
  {"a path constructor interior has no record projection", WALK,
   "def bad : (x : Walk) -> Path Walk (step x) (step x) := fun (x : Walk) => <i> step (go x @ i)\n",
   "TYPE_MISMATCH", NULL},
  {"plain family fold still computes", "",
   "def test : Nat := fold 1 2 3 red\n", NULL, "1"},
  {"closed plain family transport computes before folding", "",
   "def test : Nat := fold 1 2 3 (transp (<_> Color) i0 red)\n", NULL, "1"},
  {"plain family projection still computes", "",
   "def test : Nat := itemCount (makeItem red 7 3)\n", NULL, "3"},
  {"higher constructor faces preserve the remaining intervals", SHAPE,
   "def squarePath : Path (Path Shape point point) refl edge := square\n"
   "def start : Path Shape (cube @ i0 @ i0 @ i0) point := refl\n"
   "def end : Path Shape (cube @ i1 @ i1 @ i1) point := refl\n"
   "def test : (j k : I) -> Path Shape (cube @ i1 @ j @ k) (square @ j @ k) := fun j k => refl\n",
   NULL, "fun j => fun k => <i> square @ j @ k"},
  {"constructor parameters and fields remain in scope under path binders",
   "family Segment (A : Type 0) := segLeft (segLVal : A) | segRight (segRVal : A)"
   " | segLink (segFrom segTo : A) <i> [ ~i -> segLeft A segFrom, i -> segRight A segTo ]\n",
   "def test : (A : Type 0) -> (x y : A) -> Path (Segment A) (segLeft A x) (segRight A y) := fun A x y => segLink A x y\n",
   NULL, "fun A => fun x => fun y => <i> segLink A x y @ i"}
};

static int load(Arena *arena, const char *name, Origin origin, const char *text, size_t len, DeclList *decls, Diag *diag) {
  TokenList tokens;
  return lex_source(arena, name, text, len, &tokens, diag)
    && parse_source(arena, name, origin, &tokens, decls, diag);
}

static int path_faces_agree(Machine *m, const Value *p, const char *label) {
  const char *names[] = {"L", "M", "f", "a", "j"};
  IvlHeap heap = {m->arena, m->diag, label};
  const Value *open;
  uint32_t j;
  int one;
  for (j = 0; j < 4u && p != NULL; j++)
    p = apply_value(m, p, val_var(m, j));
  open = p == NULL ? NULL : closure_apply(m, p, val_ivl_var(m, 4u));
  for (one = 0; one < 2; one++) {
    const Value *direct = p == NULL ? NULL : closure_apply(m, p, val_ivl_end(m, one));
    const Ivl *face = ivl_lit(&heap, 4u, !one);
    const Value *restricted = open == NULL || face == NULL ? NULL : value_on_face(m, 5u, open, face, 0);
    if (direct == NULL || restricted == NULL || !conv_values(m, 5u, direct, restricted)) {
      char normal[2048];
      fprintf(stderr, "FAIL %s: normalization changed endpoint i%d\n", label, one);
      if (direct != NULL && value_print(m, names, 5u, direct, normal, sizeof normal))
        fprintf(stderr, "direct endpoint: %s\n", normal);
      if (restricted != NULL && value_print(m, names, 5u, restricted, normal, sizeof normal))
        fprintf(stderr, "restricted endpoint: %s\n", normal);
      return 0;
    }
  }
  return 1;
}

static int run_case(const Case *test, int check_faces) {
  Arena arena;
  Diag diag;
  DeclList decls = {0};
  Machine machine;
  char normal[2048] = "";
  int ok;
  int failed;
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  ok = load(&arena, "domain/domain.lang", ORIGIN_DOMAIN, (const char *)domain_source, domain_source_len, &decls, &diag)
    && load(&arena, test->label, ORIGIN_DOMAIN, test->domain, strlen(test->domain), &decls, &diag)
    && load(&arena, test->label, ORIGIN_PROGRAM, test->source, strlen(test->source), &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag);
  failed = test->code == NULL ? !ok : ok || diag.code == NULL || strcmp(diag.code, test->code) != 0;
  if (!failed && test->normal != NULL) {
    const Value *value = def_value(&machine, machine.def_count - 1u);
    failed = value == NULL || !value_print(&machine, NULL, 0, value, normal, sizeof normal)
      || strcmp(normal, test->normal) != 0;
  }
  if (failed) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", test->label,
      test->normal != NULL ? test->normal : test->code != NULL ? test->code : "ok",
      test->normal != NULL && ok ? normal : ok ? "ok" : diag.code != NULL ? diag.code : "no diagnostic");
    if (diag.set)
      diag_print(&diag, stderr);
  }
  if (!failed && check_faces)
    failed = !path_faces_agree(&machine, def_value(&machine, machine.def_count - 1u), test->label);
  arena_release(&arena);
  return failed;
}

int main(void) {
  size_t i;
  size_t checked = 0;
  int failed = 0;
  for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++, checked++)
    failed += run_case(&CASES[i], 0);
  for (i = 0; i < sizeof FACE_CASES / sizeof FACE_CASES[0]; i++, checked++)
    failed += run_case(&FACE_CASES[i], 1);
  printf("native HIT regressions: %lu checked, %d failed\n", (unsigned long)checked, failed);
  return failed != 0;
}
