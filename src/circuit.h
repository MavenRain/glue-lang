/* The Plonkish IR target (docs/IR.md). glue-lang only.

   Lowering evaluates each constraint of a circuit with one neutral variable
   for each binder. The normal form is a tree of Field literals, binders and
   stuck + - * operations. Each operation gives one arith row, each literal
   gives one qc row, and copies tie the wires. A helper definition inlines
   because normalization unfolds it. */
#ifndef LANG_CIRCUIT_H
#define LANG_CIRCUIT_H

#include <stddef.h>

#include "front/core.h"

/* Lowers each circuit of M, so that `langc check` gives the lowering
   refusals too. Returns 1, or 0 after a diagnostic. */
int circuit_check_all(Machine *m);
/* Builds the circuit document of NAME. Returns 1 and sets TEXT and LEN, or 0
   after a diagnostic. */
int circuit_document(Machine *m, const char *name, const char **text, size_t *len);
/* Builds the witness document of NAME from ARGS (`binder=value`). Returns 0
   and sets TEXT and LEN, 1 after a refusal, or 2 after a WITNESS_ARGS
   refusal. */
int circuit_witness(Machine *m, const char *name, char *const *args, int arg_count, const char **text, size_t *len);
/* Reads a circuit document and a witness document and checks every gate
   poly on every row, every lookup and every copy. Returns 1, or 0 after a
   diagnostic. */
int circuit_satcheck(Arena *arena, Diag *diag, const char *circuit, size_t circuit_len, const char *witness, size_t witness_len);

#endif
