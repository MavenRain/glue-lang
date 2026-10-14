/* Native regressions for compiler and satcheck boundary cases. */
#include <stdio.h>
#include <string.h>

#include "circuit.h"
#include "front/check.h"
#include "front/front.h"

static int check_source(const char *label, const char *text, const char *code) {
  Arena arena;
  Diag diag;
  DeclList decls;
  Machine machine;
  int ok;
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  ok = front_load(&arena, label, text, strlen(text), &decls, &diag)
    && check_program(&arena, &decls, &machine, &diag)
    && circuit_check_all(&machine);
  if (code == NULL ? !ok : ok || diag.code == NULL || strcmp(diag.code, code) != 0) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", label,
      code == NULL ? "ok" : code, ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code);
    arena_release(&arena);
    return 1;
  }
  arena_release(&arena);
  return 0;
}

static int check_json(const char *label, const char *version, const char *suffix, const char *lookups, const char *code) {
  const char *witness = "{\"glue-lang-witness\":1,\"name\":\"c\",\"advice\":[[\"0\"]],\"instance\":[]}";
  char circuit[1024];
  Arena arena;
  Diag diag;
  int ok;
  snprintf(circuit, sizeof circuit,
    "{\"glue-lang-plonkish\":%s,\"name\":\"c\",\"field\":\"bn254-fr\","
    "\"k\":5,\"rows\":1,\"advice\":1,\"fixed\":1,\"instance\":0,"
    "\"gates\":[{\"name\":\"zero\",\"polys\":[{\"*\":[{\"f\":[0,0]},{\"a\":[0,0]}]}]}],"
    "\"lookups\":%s,\"copies\":[],\"fixed_values\":[[\"1\"]],\"public\":0%s}", version, lookups, suffix);
  arena_init(&arena, (size_t)64 << 20);
  diag_init(&diag);
  ok = circuit_satcheck(&arena, &diag, circuit, strlen(circuit), witness, strlen(witness));
  if (code == NULL ? !ok : ok || diag.code == NULL || strcmp(diag.code, code) != 0) {
    fprintf(stderr, "FAIL %s: expected %s, got %s\n", label,
      code == NULL ? "ok" : code, ok ? "ok" : diag.code == NULL ? "no diagnostic" : diag.code);
    arena_release(&arena);
    return 1;
  }
  arena_release(&arena);
  return 0;
}

int main(void) {
  int failed = 0;
  failed += check_source("large Field", "def x : Field := 18446744073709551616\n", NULL);
  failed += check_source("overflowing universe", "def U : Type 18446744073709551616 := Nat\n", "TYPE_UNIVERSE");
  failed += check_source("circuit then definition", "circuit same private (x : Field) := range8 x\ndef same : Nat := 0\n", "REFUSE_NAME");
  failed += check_source("definition then circuit", "def same : Nat := 0\ncircuit same private (x : Field) := range8 x\n", "REFUSE_NAME");
  failed += check_json("valid IR", "1", "", "[]", NULL);
  failed += check_json("duplicate JSON key", "1", ",\"fixed_values\":[[\"2\"]]", "[]", "SAT_JSON");
  failed += check_json("leading zero JSON number", "01", "", "[]", "SAT_JSON");
  failed += check_json("unknown IR key", "1", ",\"extra\":1", "[]", "SAT_IR");
  failed += check_json("advice lookup table", "1", "", "[{\"name\":\"bad\",\"inputs\":[{\"a\":[0,0]}],\"tables\":[{\"a\":[0,0]}]}]", "SAT_IR");
  printf("native circuit regressions: 9 checked, %d failed\n", failed);
  return failed != 0;
}
