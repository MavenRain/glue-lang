/* BN254 Fr arithmetic (see front/fr.h). glue-lang only. */
#include <string.h>

#include "front/fr.h"

#define FR_DIGITS_MAX 77u

static const uint32_t R[FR_LIMBS] = {
  0xf0000001u, 0x43e1f593u, 0x79b97091u, 0x2833e848u,
  0x8181585du, 0xb85045b6u, 0xe131a029u, 0x30644e72u,
};

/* -1, 0 or 1 as A is less than, equal to or more than B. */
static int cmp_limbs(const uint32_t *a, const uint32_t *b) {
  unsigned i;
  for (i = FR_LIMBS; i > 0; i--) {
    if (a[i - 1] != b[i - 1])
      return a[i - 1] < b[i - 1] ? -1 : 1;
  }
  return 0;
}

/* A = A - B modulo 2^256. Returns the borrow. */
static uint32_t sub_limbs(uint32_t *a, const uint32_t *b) {
  uint64_t borrow = 0;
  unsigned i;
  for (i = 0; i < FR_LIMBS; i++) {
    uint64_t d = (uint64_t)a[i] - b[i] - borrow;
    a[i] = (uint32_t)d;
    borrow = d >> 63;
  }
  return (uint32_t)borrow;
}

/* A = A + B modulo 2^256. Returns the carry. */
static uint32_t add_limbs(uint32_t *a, const uint32_t *b) {
  uint64_t carry = 0;
  unsigned i;
  for (i = 0; i < FR_LIMBS; i++) {
    uint64_t s = (uint64_t)a[i] + b[i] + carry;
    a[i] = (uint32_t)s;
    carry = s >> 32;
  }
  return (uint32_t)carry;
}

static int is_zero(const Fr *x) {
  unsigned i;
  for (i = 0; i < FR_LIMBS; i++) {
    if (x->w[i] != 0)
      return 0;
  }
  return 1;
}

/* X = X * 10 + DIGIT. Returns the carry out of the top limb. */
static uint32_t mul10_add(uint32_t *x, uint32_t digit) {
  uint64_t carry = digit;
  unsigned i;
  for (i = 0; i < FR_LIMBS; i++) {
    uint64_t t = (uint64_t)x[i] * 10u + carry;
    x[i] = (uint32_t)t;
    carry = t >> 32;
  }
  return (uint32_t)carry;
}

/* X = X / 10. Returns the remainder. */
static uint32_t div10(uint32_t *x) {
  uint64_t rem = 0;
  unsigned i;
  for (i = FR_LIMBS; i > 0; i--) {
    uint64_t cur = (rem << 32) | x[i - 1];
    x[i - 1] = (uint32_t)(cur / 10u);
    rem = cur % 10u;
  }
  return (uint32_t)rem;
}

int fr_parse(const char *text, size_t len, Fr *out) {
  Fr x;
  size_t i;
  memset(&x, 0, sizeof x);
  if (len == 0 || len > FR_DIGITS_MAX || (len > 1 && text[0] == '0'))
    return 0;
  for (i = 0; i < len; i++) {
    if (text[i] < '0' || text[i] > '9')
      return 0;
    if (mul10_add(x.w, (uint32_t)(text[i] - '0')) != 0)
      return 0;
  }
  if (cmp_limbs(x.w, R) >= 0)
    return 0;
  *out = x;
  return 1;
}

void fr_print(const Fr *x, char *buf) {
  char rev[FR_TEXT_MAX];
  Fr t = *x;
  size_t n = 0;
  size_t i;
  do {
    rev[n] = (char)('0' + div10(t.w));
    n++;
  } while (!is_zero(&t) && n < FR_TEXT_MAX - 1);
  for (i = 0; i < n; i++)
    buf[i] = rev[n - 1 - i];
  buf[n] = '\0';
}

void fr_from_u64(Fr *out, uint64_t n) {
  memset(out, 0, sizeof *out);
  out->w[0] = (uint32_t)n;
  out->w[1] = (uint32_t)(n >> 32);
}

void fr_add(Fr *out, const Fr *a, const Fr *b) {
  /* a + b < 2r < 2^255, so the sum has no carry out of the top limb. */
  Fr t = *a;
  add_limbs(t.w, b->w);
  if (cmp_limbs(t.w, R) >= 0)
    sub_limbs(t.w, R);
  *out = t;
}

void fr_sub(Fr *out, const Fr *a, const Fr *b) {
  Fr t = *a;
  if (sub_limbs(t.w, b->w) != 0)
    add_limbs(t.w, R);
  *out = t;
}

void fr_neg(Fr *out, const Fr *a) {
  Fr zero;
  memset(&zero, 0, sizeof zero);
  fr_sub(out, &zero, a);
}

/* The product of A and B, then the remainder modulo r by shift and subtract,
   from the top bit of the 512-bit product. The remainder stays below r, so
   2 * rem + 1 < 2^255 fits in the limbs. */
void fr_mul(Fr *out, const Fr *a, const Fr *b) {
  uint32_t p[2u * FR_LIMBS];
  Fr rem;
  unsigned i;
  unsigned j;
  memset(p, 0, sizeof p);
  memset(&rem, 0, sizeof rem);
  for (i = 0; i < FR_LIMBS; i++) {
    uint64_t carry = 0;
    for (j = 0; j < FR_LIMBS; j++) {
      uint64_t t = (uint64_t)a->w[i] * b->w[j] + p[i + j] + carry;
      p[i + j] = (uint32_t)t;
      carry = t >> 32;
    }
    p[i + FR_LIMBS] = (uint32_t)carry;
  }
  for (i = 2u * FR_LIMBS * 32u; i > 0; i--) {
    uint32_t bit = (p[(i - 1) / 32u] >> ((i - 1) % 32u)) & 1u;
    for (j = FR_LIMBS - 1; j > 0; j--)
      rem.w[j] = (rem.w[j] << 1) | (rem.w[j - 1] >> 31);
    rem.w[0] = (rem.w[0] << 1) | bit;
    if (cmp_limbs(rem.w, R) >= 0)
      sub_limbs(rem.w, R);
  }
  *out = rem;
}

int fr_eq(const Fr *a, const Fr *b) {
  return cmp_limbs(a->w, b->w) == 0;
}

int fr_lt_u64(const Fr *x, uint64_t bound) {
  unsigned i;
  for (i = 2; i < FR_LIMBS; i++) {
    if (x->w[i] != 0)
      return 0;
  }
  return (((uint64_t)x->w[1] << 32) | x->w[0]) < bound;
}
