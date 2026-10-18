/* BN254 Fr arithmetic (docs/IR.md section 1). glue-lang only.

   r = 21888242871839275222246405745257275088548364400416034343698204186575808495617

   An element is 8 limbs of 32 bits, least significant first, with a value in
   [0, r). The arithmetic uses uint64_t intermediates only (no __int128), so
   that TinyCC builds it. */
#ifndef LANG_FRONT_FR_H
#define LANG_FRONT_FR_H

#include <stddef.h>
#include <stdint.h>

#define FR_LIMBS 8u
/* r has 77 decimal digits. A buffer of FR_TEXT_MAX bytes holds any element
   and the terminating zero. */
#define FR_TEXT_MAX 80u

typedef struct {
  uint32_t w[FR_LIMBS];
} Fr;

/* Parses a canonical decimal (no sign, no leading zero except "0"). Returns 1,
   or 0 when TEXT is not canonical or its value is r or more. */
int fr_parse(const char *text, size_t len, Fr *out);
/* Writes the canonical decimal of X into BUF (FR_TEXT_MAX bytes). */
void fr_print(const Fr *x, char *buf);
void fr_from_u64(Fr *out, uint64_t n);
void fr_add(Fr *out, const Fr *a, const Fr *b);
void fr_sub(Fr *out, const Fr *a, const Fr *b);
void fr_neg(Fr *out, const Fr *a);
void fr_mul(Fr *out, const Fr *a, const Fr *b);
/* The inverse of A:  A^(r - 2) (Fermat), thus 0 at 0 (glue-lang `inv`). */
void fr_inv(Fr *out, const Fr *a);
int fr_eq(const Fr *a, const Fr *b);
/* 1 when X is less than BOUND. */
int fr_lt_u64(const Fr *x, uint64_t bound);

#endif
