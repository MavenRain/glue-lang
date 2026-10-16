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

static const char SHAPE[] =
  "family Shape := point | edge <i> [ ~i -> point, i -> point ]"
  " | square <i j> [ ~i -> point, i -> edge @ j, ~j -> point, j -> point ]"
  " | cube <i j k> [ ~i -> point, i -> square @ j @ k, ~j -> point,"
  " j -> square @ i @ k, ~k -> point, k -> point ]\n";

static const Case TRANSPORT_CASES[] = {
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
  {"HIT fold cannot distinguish connected points", "",
   "def f : Susp Nat -> Nat := fun (x : Susp Nat) => fold 0 1 (fun (a : Nat) => 0) x\n"
   "def bad : Path Nat 0 1 := cong f (merid Nat 0)\n", "TYPE_MISMATCH", NULL},
  {"HIT fold rejects a path constructor interior", "",
   "def bad : Path Nat 0 0 := <i> fold 0 0 (loop @ i)\n", "TYPE_MISMATCH", NULL},
  {"HIT fold rejects point constructors too", "",
   "def bad : Nat := fold 0 0 base\n", "TYPE_MISMATCH", NULL},
  {"HIT boundaries cannot use the plain family fold",
   "family Sneak := aa | bb | pp (item : Sneak) <i> [ ~i -> fold aa bb aa item, i -> aa ]\n",
   "def test : Sneak := aa\n", "TYPE_MISMATCH", NULL},
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
  for (i = 0; i < sizeof TRANSPORT_CASES / sizeof TRANSPORT_CASES[0]; i++, checked++)
    failed += run_case(&TRANSPORT_CASES[i], 1);
  printf("native HIT regressions: %lu checked, %d failed\n", (unsigned long)checked, failed);
  return failed != 0;
}
