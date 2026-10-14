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
  {"cong rejects a wrong domain",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Flag -> Type 0) -> (f : (x : Flag) -> B x) -> Path (B flagYes) (f flagYes) (f flagYes) := fun B f => cong f r\n", "TYPE_MISMATCH"},
  {"cong rejects wrong expected endpoints",
   "def r : Path Nat 0 0 := refl\n"
   "def test : (B : Nat -> Type 0) -> (f g : (x : Nat) -> B x) -> Path (B 0) (f 0) (g 0) := fun B f g => cong f r\n", "TYPE_MISMATCH"},
  {"cong rejects a varying input line",
   "def test : (p : Path Nat 0 1) -> (q : PathP (<i> Path Nat 0 (p @ i)) refl p) -> Path (Path Nat 0 0) refl refl := fun p q => cong (fun (r : Path Nat 0 0) => r) q\n", "TYPE_MISMATCH"}
};

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
  int failed = 0;
  for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++)
    failed += check_case(&CASES[i]);
  printf("native path regressions: %lu checked, %d failed\n", (unsigned long)i, failed);
  return failed != 0;
}
