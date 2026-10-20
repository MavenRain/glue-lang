/* The Plonkish IR target. glue-lang only. See circuit.h and docs/IR.md. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "circuit.h"
#include "front/fr.h"

/* docs/IR.md section 4: advice a, b, c; fixed ql, qr, qo, qm, qc, qrange, t. */
#define COL_A 0u
#define COL_B 1u
#define COL_C 2u
#define ADVICE_COUNT 3u
#define FIX_QL 0u
#define FIX_QR 1u
#define FIX_QO 2u
#define FIX_QM 3u
#define FIX_QC 4u
#define FIX_QRANGE 5u
#define FIX_T 6u
#define FIXED_COUNT 7u
#define RANGE8_SIZE 256u
#define BLINDING_ROWS 16u
#define LOWER_DEPTH_LIMIT 2000u
#define SAT_K_MAX 16u
#define SAT_COLUMNS_MAX 64u
#define SAT_DEPTH_LIMIT 200u

static const char ARITH_POLY[] =
  "{\"+\":[{\"+\":[{\"+\":[{\"+\":[{\"*\":[{\"f\":[0,0]},{\"a\":[0,0]}]},{\"*\":[{\"f\":[1,0]},{\"a\":[1,0]}]}]},"
  "{\"*\":[{\"*\":[{\"f\":[3,0]},{\"a\":[0,0]}]},{\"a\":[1,0]}]}]},{\"f\":[4,0]}]},{\"-\":{\"*\":[{\"f\":[2,0]},{\"a\":[2,0]}]}}]}";
static const char RANGE8_LOOKUP[] =
  "{\"name\":\"range8\",\"inputs\":[{\"*\":[{\"f\":[5,0]},{\"a\":[0,0]}]}],\"tables\":[{\"f\":[6,0]}]}";

/* An advice cell. */
typedef struct {
  uint32_t col;
  uint32_t row;
} Cell;

typedef enum {
  ROW_INPUT, /* a = binder */
  ROW_CONST, /* qc = k, qo = 1: c = k */
  ROW_ARITH, /* a = x, b = y, c = a op b */
  ROW_RANGE  /* a = x, qrange = 1 */
} RowKind;

typedef struct {
  RowKind kind;
  uint32_t binder;
  Op op;
  Fr k;
  Cell x;
  Cell y;
  int range; /* qrange is 1 on this row */
} Row;

/* A constraint after lowering: the wires that must be equal, or the wire
   that must be below 256. */
typedef struct {
  ConstraintKind kind;
  Cell lhs;
  Cell rhs;
  int line;
  int col;
} Check;

typedef struct {
  Machine *m;
  const CircuitInfo *ci;
  Row *rows;
  uint32_t row_count;
  uint32_t row_cap;
  Check *checks;
  uint32_t check_count;
  unsigned char *used;
  int ranged;
  unsigned depth;
  uint32_t *memo; /* M11 D2: a row index + 1 in each slot, or 0 */
  uint32_t memo_cap;
  uint32_t memo_count;
} Lower;

typedef struct {
  Arena *arena;
  Diag *diag;
  char *data;
  size_t len;
  size_t cap;
} Buf;

static int oom(Diag *diag) {
  return diag->set ? 0 : diag_fail(diag, "OOM", NULL, "the arena limit is reached");
}

static int putf(Buf *b, const char *fmt, ...) {
  char small[256];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(small, sizeof small, fmt, ap);
  va_end(ap);
  if (n < 0) return diag_fail(b->diag, "INTERNAL", NULL, "the IR writer cannot format");
  if (b->len + (size_t)n + 1 > b->cap) {
    size_t cap = b->cap == 0 ? 4096 : b->cap;
    while (cap < b->len + (size_t)n + 1) cap *= 2;
    char *data = arena_alloc(b->arena, cap);
    if (data == NULL) return oom(b->diag);
    if (b->len > 0) memcpy(data, b->data, b->len);
    b->data = data;
    b->cap = cap;
  }
  if ((size_t)n < sizeof small) {
    memcpy(b->data + b->len, small, (size_t)n + 1);
  } else {
    va_start(ap, fmt);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
  }
  b->len += (size_t)n;
  return 1;
}

static int put_fr(Buf *b, const Fr *x, int first) {
  char text[FR_TEXT_MAX];
  fr_print(x, text);
  return putf(b, "%s\"%s\"", first ? "" : ",", text);
}

/* ---- Lowering ---- */

static int add_row(Lower *l, const Row *row, Cell *out) {
  if (l->row_count == l->row_cap) {
    uint32_t cap = l->row_cap == 0 ? 16u : l->row_cap * 2u;
    Row *rows = arena_alloc(l->m->arena, cap * sizeof *rows);
    if (rows == NULL) return oom(l->m->diag);
    if (l->row_count > 0) memcpy(rows, l->rows, l->row_count * sizeof *rows);
    l->rows = rows;
    l->row_cap = cap;
  }
  l->rows[l->row_count] = *row;
  out->col = row->kind == ROW_CONST || row->kind == ROW_ARITH ? COL_C : COL_A;
  out->row = l->row_count++;
  return 1;
}

/* M11 D2: the structural row memo. The key of a CONST row is its value; the
   key of an ARITH row is the op and the cells of the two operands, in this
   order (no commutative normal form). The arith gate fixes the output cell
   of a row from the op and the operand cells, so two rows with one key hold
   one value in each satisfying witness. INPUT and RANGE rows have no key. */
#define MEMO_KEY_WORDS (FR_LIMBS + 1u)

static void memo_key(const Row *r, uint32_t *key) {
  memset(key, 0, MEMO_KEY_WORDS * sizeof *key);
  key[0] = (uint32_t)r->kind;
  switch (r->kind) {
    case ROW_CONST:
      memcpy(&key[1], r->k.w, sizeof r->k.w);
      break;
    case ROW_ARITH:
      key[1] = (uint32_t)r->op;
      key[2] = r->x.col;
      key[3] = r->x.row;
      key[4] = r->y.col;
      key[5] = r->y.row;
      break;
    case ROW_INPUT:
    case ROW_RANGE:
      break;
  }
}

static uint32_t memo_hash(const uint32_t *key) {
  uint32_t h = 2166136261u;
  uint32_t i;
  for (i = 0; i < MEMO_KEY_WORDS; i++) h = (h ^ key[i]) * 16777619u;
  return h ^ (h >> 16);
}

/* The slot of the row with KEY, else the empty slot where the probe stops.
   The table always has an empty slot (it doubles at half load). */
static uint32_t memo_slot(const Lower *l, const uint32_t *key) {
  uint32_t mask = l->memo_cap - 1u;
  uint32_t i = memo_hash(key) & mask;
  uint32_t have[MEMO_KEY_WORDS];
  while (l->memo[i] != 0) {
    memo_key(&l->rows[l->memo[i] - 1u], have);
    if (memcmp(have, key, sizeof have) == 0) return i;
    i = (i + 1u) & mask;
  }
  return i;
}

/* A CONST or an ARITH row through the memo: OUT gets the cell of the row with
   the key of ROW, else the cell of a new row. */
static int memo_row(Lower *l, const Row *row, Cell *out) {
  uint32_t key[MEMO_KEY_WORDS];
  uint32_t found;
  uint32_t i;
  memo_key(row, key);
  found = l->memo_cap == 0 ? 0 : l->memo[memo_slot(l, key)];
  if (found != 0) {
    out->col = COL_C;
    out->row = found - 1u;
    return 1;
  }
  if (!add_row(l, row, out)) return 0;
  if (2u * (l->memo_count + 1u) > l->memo_cap) {
    const uint32_t *old = l->memo;
    uint32_t old_cap = l->memo_cap;
    uint32_t cap = old_cap == 0 ? 64u : old_cap * 2u;
    uint32_t *slots = arena_alloc(l->m->arena, cap * sizeof *slots);
    if (slots == NULL) return oom(l->m->diag);
    memset(slots, 0, cap * sizeof *slots);
    l->memo = slots;
    l->memo_cap = cap;
    for (i = 0; i < old_cap; i++)
      if (old[i] != 0) {
        uint32_t have[MEMO_KEY_WORDS];
        memo_key(&l->rows[old[i] - 1u], have);
        l->memo[memo_slot(l, have)] = old[i];
      }
  }
  l->memo[memo_slot(l, key)] = out->row + 1u;
  l->memo_count++;
  return 1;
}

static int no_circuit_form(Lower *l, const char *what) {
  return diag_fail(l->m->diag, "CIRCUIT_LOWER", l->ci->name, "a constraint side normalizes to %s, which has no circuit form; M1 lowers Field literals, binders and + - * only", what);
}

/* V is a normal form of type Field. OUT gets the wire that holds its value. */
static int lower_value(Lower *l, const Value *v, Cell *out) {
  Row row;
  memset(&row, 0, sizeof row);
  if (v == NULL) return l->m->diag->set ? 0 : oom(l->m->diag);
  if (l->depth >= LOWER_DEPTH_LIMIT) return diag_fail(l->m->diag, "CIRCUIT_LOWER", l->ci->name, "a constraint is deeper than %u operations", LOWER_DEPTH_LIMIT);
  switch (v->kind) {
    case VAL_VAR:
      if (v->nat >= l->ci->public_count + l->ci->private_count + l->ci->hint_count) return no_circuit_form(l, "a variable that is not a binder");
      l->used[v->nat] = 1;
      out->col = COL_A;
      out->row = (uint32_t)v->nat;
      return 1;
    case VAL_OP:
      if (v->op != OP_FIELD_LIT || v->name == NULL || !fr_parse(v->name, strlen(v->name), &row.k)) return no_circuit_form(l, "a value that is not a Field literal");
      row.kind = ROW_CONST;
      return memo_row(l, &row, out);
    case VAL_STUCK: {
      int arith = v->op == OP_FIELD_ADD || v->op == OP_FIELD_SUB || v->op == OP_FIELD_MUL;
      if (!arith || v->argc != 2) return no_circuit_form(l, "a stuck operation that is not + - *");
      l->depth++;
      int ok = lower_value(l, v->args[0], &row.x) && lower_value(l, v->args[1], &row.y);
      l->depth--;
      row.kind = ROW_ARITH;
      row.op = v->op;
      return ok && memo_row(l, &row, out);
    }
    case VAL_NAT:
    case VAL_TRAP:
    case VAL_UNIV:
    case VAL_LAM:
    case VAL_PI:
    case VAL_SIGMA:
    case VAL_APP:
    case VAL_IVL:
    case VAL_PLAM:
    case VAL_PAPP:
      break;
  }
  return no_circuit_form(l, "a value that is not a Field");
}

static int lower_check(Lower *l, const Env *env, const ConstraintInfo *k) {
  Check *ch = &l->checks[l->check_count];
  const Value *v;
  Row row;
  Cell cell;
  ch->kind = k->kind;
  ch->line = k->line;
  ch->col = k->col;
  v = eval_core(l->m, env, k->term);
  switch (k->kind) {
    case CONSTRAINT_EQ:
      /* M2 ruling b: the term is the type Path Field a b; read its ends. */
      if (!val_is(v, OP_PATHP)) return no_circuit_form(l, "a type that is not Path Field");
      if (!lower_value(l, v->args[1], &ch->lhs)) return 0;
      l->check_count++;
      return lower_value(l, v->args[2], &ch->rhs);
    case CONSTRAINT_RANGE8:
      if (!lower_value(l, v, &ch->lhs)) return 0;
      l->check_count++;
      l->ranged = 1;
      /* A wire in column a is a binder row: set qrange on that row. */
      if (ch->lhs.col == COL_A) {
        l->rows[ch->lhs.row].range = 1;
        return 1;
      }
      memset(&row, 0, sizeof row);
      row.kind = ROW_RANGE;
      row.x = ch->lhs;
      row.range = 1;
      return add_row(l, &row, &cell);
  }
  return 0;
}

/* M6 s5 (D14): a hint row is an input row too (zero selectors, no instance
   copy), after the private rows. */
static int lower_circuit(Machine *m, const CircuitInfo *ci, Lower *l) {
  uint32_t n = ci->public_count + ci->private_count + ci->hint_count;
  const Env *env = NULL;
  uint32_t i;
  memset(l, 0, sizeof *l);
  l->m = m;
  l->ci = ci;
  l->used = arena_alloc(m->arena, n + 1u);
  l->checks = arena_alloc(m->arena, (ci->constraint_count + 1u) * sizeof *l->checks);
  if (l->used == NULL || l->checks == NULL) return oom(m->diag);
  m->def = ci->name;
  for (i = 0; i < n; i++) {
    Row row;
    Cell cell;
    memset(&row, 0, sizeof row);
    row.kind = ROW_INPUT;
    row.binder = i;
    env = env_push(m, env, val_var(m, i));
    if (env == NULL) return oom(m->diag);
    if (!add_row(l, &row, &cell)) return 0;
  }
  for (i = 0; i < ci->constraint_count; i++)
    if (!lower_check(l, env, &ci->constraints[i])) return 0;
  for (i = ci->public_count; i < n; i++)
    if (!l->used[i])
      return diag_fail(m->diag, "CIRCUIT_UNCONSTRAINED", ci->name, "the %s %s is in no constraint, so the proof does not constrain it",
                       i < ci->public_count + ci->private_count ? "private binder" : "hint", ci->binders[i]);
  for (i = 0; i < ci->public_count; i++)
    if (!l->used[i]) return diag_fail(m->diag, "CIRCUIT_UNUSED_PUBLIC", ci->name, "the public binder %s is in no constraint", ci->binders[i]);
  return 1;
}

static const CircuitInfo *find_circuit(Machine *m, const char *name) {
  uint32_t i;
  for (i = 0; i < m->circuit_count; i++)
    if (strcmp(m->circuits[i].name, name) == 0) return &m->circuits[i];
  diag_fail(m->diag, "CIRCUIT_ENTRY", name, "%s is not a circuit of the program", name);
  return NULL;
}

int circuit_check_all(Machine *m) {
  Lower l;
  uint32_t i;
  for (i = 0; i < m->circuit_count; i++)
    if (!lower_circuit(m, &m->circuits[i], &l)) return 0;
  m->def = NULL;
  return 1;
}

/* The smallest k with room for the rows, the range8 table and the blinding
   rows (docs/IR.md section 2). */
static uint32_t pick_k(const Lower *l) {
  uint64_t need = l->row_count;
  uint32_t k = 1;
  if (l->ranged && need < RANGE8_SIZE) need = RANGE8_SIZE;
  while (((uint64_t)1 << k) < need + BLINDING_ROWS) k++;
  return k;
}

static void fixed_at(const Lower *l, uint32_t col, uint32_t row, Fr *out) {
  const Row *r = row < l->row_count ? &l->rows[row] : NULL;
  int arith = r != NULL && r->kind == ROW_ARITH;
  uint64_t one = 0;
  fr_from_u64(out, 0);
  switch (col) {
    case FIX_QL: one = arith && r->op != OP_FIELD_MUL; break;
    case FIX_QR:
      one = arith && r->op != OP_FIELD_MUL;
      if (one && r->op == OP_FIELD_SUB) {
        Fr unit;
        fr_from_u64(&unit, 1);
        fr_neg(out, &unit);
        return;
      }
      break;
    case FIX_QO: one = arith || (r != NULL && r->kind == ROW_CONST); break;
    case FIX_QM: one = arith && r->op == OP_FIELD_MUL; break;
    case FIX_QC:
      if (r != NULL && r->kind == ROW_CONST) *out = r->k;
      return;
    case FIX_QRANGE: one = r != NULL && r->range; break;
    case FIX_T: one = 0; if (l->ranged && row < RANGE8_SIZE) fr_from_u64(out, row); return;
    default: break;
  }
  fr_from_u64(out, one);
}

static int put_cell(Buf *b, const char *col, uint32_t index, uint32_t row) {
  return putf(b, "{\"col\":\"%s\",\"index\":%u,\"row\":%u}", col, index, row);
}

static int put_copy(Buf *b, int *first, const char *col, uint32_t index, uint32_t row, Cell to) {
  int ok = putf(b, "%s[", *first ? "" : ",") && put_cell(b, col, index, row) && putf(b, ",") && put_cell(b, "advice", to.col, to.row) && putf(b, "]");
  *first = 0;
  return ok;
}

static int put_copies(Buf *b, const Lower *l) {
  int first = 1;
  int ok = putf(b, "\"copies\":[");
  uint32_t i;
  for (i = 0; ok && i < l->ci->public_count; i++) {
    Cell cell = {COL_A, i};
    ok = put_copy(b, &first, "instance", 0, i, cell);
  }
  for (i = 0; ok && i < l->row_count; i++) {
    const Row *r = &l->rows[i];
    if (r->kind == ROW_ARITH || r->kind == ROW_RANGE) ok = put_copy(b, &first, "advice", COL_A, i, r->x);
    if (ok && r->kind == ROW_ARITH) ok = put_copy(b, &first, "advice", COL_B, i, r->y);
  }
  for (i = 0; ok && i < l->check_count; i++) {
    const Check *ch = &l->checks[i];
    int same = ch->lhs.col == ch->rhs.col && ch->lhs.row == ch->rhs.row;
    if (ch->kind == CONSTRAINT_EQ && !same) ok = put_copy(b, &first, "advice", ch->lhs.col, ch->lhs.row, ch->rhs);
  }
  return ok && putf(b, "],\n");
}

int circuit_document(Machine *m, const char *name, const char **text, size_t *len) {
  Buf b = {m->arena, m->diag, NULL, 0, 0};
  Lower l;
  const CircuitInfo *ci = find_circuit(m, name);
  uint32_t col;
  if (ci == NULL || !lower_circuit(m, ci, &l)) return 0;
  int ok = putf(&b, "{\"glue-lang-plonkish\":1,\n\"name\":\"%s\",\n\"field\":\"bn254-fr\",\n\"k\":%u,\n\"rows\":%u,\n", ci->name, pick_k(&l), l.row_count)
           && putf(&b, "\"advice\":%u,\n\"fixed\":%u,\n\"instance\":1,\n", ADVICE_COUNT, FIXED_COUNT)
           && putf(&b, "\"gates\":[{\"name\":\"arith\",\"polys\":[%s]}],\n", ARITH_POLY)
           && putf(&b, "\"lookups\":[%s],\n", l.ranged ? RANGE8_LOOKUP : "")
           && put_copies(&b, &l) && putf(&b, "\"fixed_values\":[");
  for (col = 0; ok && col < FIXED_COUNT; col++) {
    uint32_t count = col == FIX_T && l.ranged && l.row_count < RANGE8_SIZE ? RANGE8_SIZE : l.row_count;
    uint32_t row;
    ok = putf(&b, "%s\n[", col == 0 ? "" : ",");
    for (row = 0; ok && row < count; row++) {
      Fr x;
      fixed_at(&l, col, row, &x);
      ok = put_fr(&b, &x, row == 0);
    }
    ok = ok && putf(&b, "]");
  }
  ok = ok && putf(&b, "],\n\"public\":%u}\n", ci->public_count);
  m->def = NULL;
  *text = b.data;
  *len = b.len;
  return ok;
}

/* ---- Witness ---- */

static int witness_args(Machine *m, const CircuitInfo *ci, char *const *args, int arg_count, Fr *inputs) {
  uint32_t n = ci->public_count + ci->private_count;
  uint32_t all = n + ci->hint_count;
  unsigned char *seen = arena_alloc(m->arena, n + 1u);
  uint32_t i;
  int a;
  if (seen == NULL) return oom(m->diag);
  for (a = 0; a < arg_count; a++) {
    const char *eq = strchr(args[a], '=');
    size_t name_len = eq == NULL ? 0 : (size_t)(eq - args[a]);
    for (i = 0; i < all; i++)
      if (eq != NULL && strlen(ci->binders[i]) == name_len && strncmp(ci->binders[i], args[a], name_len) == 0) break;
    if (eq == NULL || i == all) return diag_fail(m->diag, "WITNESS_ARGS", ci->name, "%s is not BINDER=VALUE for a binder of the circuit", args[a]);
    if (i >= n) return diag_fail(m->diag, "WITNESS_ARGS", ci->name, "%s is a hint: witness computes its value from its term", ci->binders[i]);
    if (seen[i]) return diag_fail(m->diag, "WITNESS_ARGS", ci->name, "the binder %s has two values", ci->binders[i]);
    if (!fr_parse(eq + 1, strlen(eq + 1), &inputs[i])) return diag_fail(m->diag, "WITNESS_ARGS", ci->name, "the value of %s is not a canonical decimal below r", ci->binders[i]);
    seen[i] = 1;
  }
  for (i = 0; i < n; i++)
    if (!seen[i]) return diag_fail(m->diag, "WITNESS_ARGS", ci->name, "the binder %s has no value", ci->binders[i]);
  return 1;
}

static Fr *cell_at(Fr *cells, uint32_t rows, Cell c) {
  return &cells[c.col * rows + c.row];
}

/* Computes each advice cell in row order. Each source is an earlier row. */
static void fill_advice(const Lower *l, const Fr *inputs, Fr *cells) {
  uint32_t rows = l->row_count;
  uint32_t i;
  for (i = 0; i < rows; i++) {
    const Row *r = &l->rows[i];
    Cell a = {COL_A, i}, bc = {COL_B, i}, c = {COL_C, i};
    switch (r->kind) {
      case ROW_INPUT: *cell_at(cells, rows, a) = inputs[r->binder]; break;
      case ROW_CONST: *cell_at(cells, rows, c) = r->k; break;
      case ROW_RANGE: *cell_at(cells, rows, a) = *cell_at(cells, rows, r->x); break;
      case ROW_ARITH: {
        Fr x = *cell_at(cells, rows, r->x);
        Fr y = *cell_at(cells, rows, r->y);
        *cell_at(cells, rows, a) = x;
        *cell_at(cells, rows, bc) = y;
        if (r->op == OP_FIELD_ADD) fr_add(cell_at(cells, rows, c), &x, &y);
        else if (r->op == OP_FIELD_SUB) fr_sub(cell_at(cells, rows, c), &x, &y);
        else fr_mul(cell_at(cells, rows, c), &x, &y);
        break;
      }
    }
  }
}

static int witness_checks(const Lower *l, Fr *cells) {
  char left[FR_TEXT_MAX];
  char right[FR_TEXT_MAX];
  uint32_t i;
  for (i = 0; i < l->check_count; i++) {
    const Check *ch = &l->checks[i];
    const Fr *x = cell_at(cells, l->row_count, ch->lhs);
    fr_print(x, left);
    if (ch->kind == CONSTRAINT_RANGE8 && !fr_lt_u64(x, RANGE8_SIZE))
      return diag_fail(l->m->diag, "WITNESS_FAIL", l->ci->name, "%d:%d: range8 fails: the value is %s", ch->line, ch->col, left);
    if (ch->kind == CONSTRAINT_EQ && !fr_eq(x, cell_at(cells, l->row_count, ch->rhs))) {
      fr_print(cell_at(cells, l->row_count, ch->rhs), right);
      return diag_fail(l->m->diag, "WITNESS_FAIL", l->ci->name, "%d:%d: the constraint fails: the left side is %s, the right side is %s", ch->line, ch->col, left, right);
    }
  }
  return 1;
}

/* M6 s5 (D14): computes each hint from its term over the values of the
   earlier binders (public, private, then the earlier hints). */
static int witness_hints(Machine *m, const CircuitInfo *ci, Fr *inputs) {
  uint32_t base = ci->public_count + ci->private_count;
  const Env *env = NULL;
  char text[FR_TEXT_MAX];
  uint32_t i;
  for (i = 0; i < base + ci->hint_count; i++) {
    const Value *lit;
    if (i >= base) {
      const Value *v = eval_core(m, env, ci->hints[i - base]);
      if (v == NULL) return m->diag->set ? 0 : oom(m->diag);
      if (!val_is(v, OP_FIELD_LIT) || !fr_parse(v->name, strlen(v->name), &inputs[i]))
        return diag_fail(m->diag, "WITNESS_HINT", ci->name, "the hint %s does not compute to a Field literal", ci->binders[i]);
    }
    fr_print(&inputs[i], text);
    lit = val_field(m, text);
    env = lit == NULL ? NULL : env_push(m, env, lit);
    if (env == NULL) return m->diag->set ? 0 : oom(m->diag);
  }
  return 1;
}

int circuit_witness(Machine *m, const char *name, char *const *args, int arg_count, const char **text, size_t *len) {
  Buf b = {m->arena, m->diag, NULL, 0, 0};
  Lower l;
  const CircuitInfo *ci = find_circuit(m, name);
  if (ci == NULL || !lower_circuit(m, ci, &l)) return 1;
  Fr *inputs = arena_alloc(m->arena, (ci->public_count + ci->private_count + ci->hint_count + 1u) * sizeof *inputs);
  Fr *cells = arena_alloc(m->arena, (ADVICE_COUNT * l.row_count + 1u) * sizeof *cells);
  if (inputs == NULL || cells == NULL) return oom(m->diag), 1;
  if (!witness_args(m, ci, args, arg_count, inputs)) return 2;
  if (!witness_hints(m, ci, inputs)) return 1;
  fill_advice(&l, inputs, cells);
  if (!witness_checks(&l, cells)) return 1;
  int ok = putf(&b, "{\"glue-lang-witness\":1,\n\"name\":\"%s\",\n\"advice\":[", ci->name);
  uint32_t col, row;
  for (col = 0; ok && col < ADVICE_COUNT; col++) {
    ok = putf(&b, "%s\n[", col == 0 ? "" : ",");
    for (row = 0; ok && row < l.row_count; row++) ok = put_fr(&b, &cells[col * l.row_count + row], row == 0);
    ok = ok && putf(&b, "]");
  }
  ok = ok && putf(&b, "],\n\"instance\":[[");
  for (row = 0; ok && row < ci->public_count; row++) ok = put_fr(&b, &inputs[row], row == 0);
  ok = ok && putf(&b, "]]}\n");
  m->def = NULL;
  *text = b.data;
  *len = b.len;
  return ok ? 0 : 1;
}

/* ---- satcheck: a JSON reader for the two documents ---- */

typedef enum {
  J_OBJ,
  J_ARR,
  J_STR,
  J_INT
} JKind;

typedef struct JVal JVal;
struct JVal {
  JKind kind;
  const char *text; /* STR: zero terminated */
  int64_t num;
  const char **keys; /* OBJ */
  const JVal **items;
  uint32_t count;
};

typedef struct {
  Arena *arena;
  Diag *diag;
  const char *what;
  const char *s;
  size_t len;
  size_t pos;
  unsigned depth;
} JRead;

static int jfail(JRead *r, const char *msg) {
  return diag_fail(r->diag, "SAT_JSON", NULL, "%s: byte %zu: %s", r->what, r->pos, msg);
}

static void jspace(JRead *r) {
  while (r->pos < r->len && (r->s[r->pos] == ' ' || r->s[r->pos] == '\n' || r->s[r->pos] == '\r' || r->s[r->pos] == '\t')) r->pos++;
}

static int jeat(JRead *r, char c) {
  jspace(r);
  if (r->pos < r->len && r->s[r->pos] == c) {
    r->pos++;
    return 1;
  }
  return 0;
}

static const JVal *jvalue(JRead *r);

static const char *jstring(JRead *r) {
  size_t start = r->pos + 1;
  size_t end = start;
  while (end < r->len && r->s[end] != '"') {
    unsigned char ch = (unsigned char)r->s[end];
    if (ch == '\\' || ch < 0x20) {
      r->pos = end;
      jfail(r, "a string holds an escape or a control byte");
      return NULL;
    }
    end++;
  }
  if (end == r->len) {
    jfail(r, "a string has no end");
    return NULL;
  }
  r->pos = end + 1;
  char *text = arena_strndup(r->arena, r->s + start, end - start);
  if (text == NULL) oom(r->diag);
  return text;
}

static int jpush(JRead *r, JVal *v, const JVal *item, const char *key, uint32_t *cap) {
  if (v->count == *cap) {
    uint32_t next = *cap == 0 ? 8u : *cap * 2u;
    const JVal **items = arena_alloc(r->arena, next * sizeof *items);
    const char **keys = arena_alloc(r->arena, next * sizeof *keys);
    if (items == NULL || keys == NULL) return oom(r->diag);
    if (v->count > 0) {
      memcpy(items, v->items, v->count * sizeof *items);
      memcpy(keys, v->keys, v->count * sizeof *keys);
    }
    v->items = items;
    v->keys = keys;
    *cap = next;
  }
  v->items[v->count] = item;
  v->keys[v->count] = key;
  v->count++;
  return 1;
}

static const JVal *jcompound(JRead *r, JVal *v, char close) {
  uint32_t cap = 0;
  r->pos++;
  if (jeat(r, close)) return v;
  do {
    const char *key = NULL;
    if (v->kind == J_OBJ) {
      jspace(r);
      if (r->pos >= r->len || r->s[r->pos] != '"') return jfail(r, "expected a key"), NULL;
      key = jstring(r);
      if (key == NULL) return NULL;
      uint32_t i;
      for (i = 0; i < v->count; i++)
        if (strcmp(v->keys[i], key) == 0) return jfail(r, "a key occurs twice"), NULL;
      if (!jeat(r, ':')) return jfail(r, "expected ':'"), NULL;
    }
    const JVal *item = jvalue(r);
    if (item == NULL || !jpush(r, v, item, key, &cap)) return NULL;
  } while (jeat(r, ','));
  return jeat(r, close) ? v : (jfail(r, "expected ',' or the end of a list"), NULL);
}

static const JVal *jvalue(JRead *r) {
  JVal *v = arena_alloc(r->arena, sizeof *v);
  if (v == NULL) return oom(r->diag), NULL;
  if (r->depth >= SAT_DEPTH_LIMIT) return jfail(r, "the document is too deep"), NULL;
  jspace(r);
  if (r->pos >= r->len) return jfail(r, "unexpected end of input"), NULL;
  char c = r->s[r->pos];
  r->depth++;
  const JVal *out = NULL;
  if (c == '{' || c == '[') {
    v->kind = c == '{' ? J_OBJ : J_ARR;
    out = jcompound(r, v, c == '{' ? '}' : ']');
  } else if (c == '"') {
    v->kind = J_STR;
    v->text = jstring(r);
    out = v->text == NULL ? NULL : v;
  } else if (c == '-' || (c >= '0' && c <= '9')) {
    int neg = c == '-';
    size_t digits = 0;
    v->kind = J_INT;
    r->pos += (size_t)neg;
    size_t start = r->pos;
    while (r->pos < r->len && r->s[r->pos] >= '0' && r->s[r->pos] <= '9' && digits < 18) {
      v->num = v->num * 10 + (r->s[r->pos] - '0');
      r->pos++;
      digits++;
    }
    int bad = digits == 0 || (digits > 1 && r->s[start] == '0') || (r->pos < r->len && (r->s[r->pos] == '.' || r->s[r->pos] == 'e' || r->s[r->pos] == 'E' || (r->s[r->pos] >= '0' && r->s[r->pos] <= '9')));
    v->num = neg ? -v->num : v->num;
    out = bad ? (jfail(r, "a number is not a small integer"), NULL) : v;
  } else {
    jfail(r, "expected a value");
  }
  r->depth--;
  return out;
}

static const JVal *jdocument(Arena *arena, Diag *diag, const char *what, const char *s, size_t len) {
  JRead r = {arena, diag, what, s, len, 0, 0};
  const JVal *v = jvalue(&r);
  jspace(&r);
  if (v != NULL && r.pos != r.len) return jfail(&r, "text follows the document"), NULL;
  return v;
}

/* ---- satcheck: the IR ---- */

typedef struct {
  const Fr *v;
  uint32_t len;
} Column;

typedef enum {
  X_CONST,
  X_ADVICE,
  X_FIXED,
  X_INSTANCE,
  X_SUM,
  X_PROD,
  X_NEG
} XKind;

typedef struct Expr Expr;
struct Expr {
  XKind kind;
  Fr c;
  uint32_t col;
  int64_t rot;
  const Expr *a;
  const Expr *b;
};

typedef struct {
  Arena *arena;
  Diag *diag;
  uint32_t n; /* 2^k */
  uint32_t rows;
  uint32_t counts[3]; /* advice, fixed, instance */
  Column *cols[3];
  unsigned char table_columns[SAT_COLUMNS_MAX];
} Sat;

static const char *const COL_KINDS[3] = {"advice", "fixed", "instance"};

static int ir_fail(Sat *s, const char *msg, const char *key) {
  return diag_fail(s->diag, "SAT_IR", NULL, "%s: %s", key, msg);
}

static const JVal *jget(const JVal *o, const char *key) {
  uint32_t i;
  if (o == NULL || o->kind != J_OBJ) return NULL;
  for (i = 0; i < o->count; i++)
    if (strcmp(o->keys[i], key) == 0) return o->items[i];
  return NULL;
}

static int want_keys(Sat *s, const JVal *o, const char *const *keys, size_t count, const char *where) {
  uint32_t i;
  size_t j;
  if (o == NULL || o->kind != J_OBJ || o->count != count) return ir_fail(s, "an object has missing or extra keys", where);
  for (i = 0; i < o->count; i++) {
    for (j = 0; j < count && strcmp(o->keys[i], keys[j]) != 0; j++) {}
    if (j == count) return ir_fail(s, "an object has an unknown key", where);
  }
  return 1;
}

static int want_int(Sat *s, const JVal *o, const char *key, int64_t lo, int64_t hi, uint32_t *out) {
  const JVal *v = jget(o, key);
  if (v == NULL || v->kind != J_INT || v->num < lo || v->num > hi) return ir_fail(s, "missing, or not an integer in range", key);
  *out = (uint32_t)v->num;
  return 1;
}

static const JVal *want_arr(Sat *s, const JVal *v, const char *key) {
  if (v == NULL || v->kind != J_ARR) return ir_fail(s, "missing, or not a list", key), NULL;
  return v;
}

static int want_fr(Sat *s, const JVal *v, Fr *out, const char *key) {
  if (v == NULL || v->kind != J_STR || !fr_parse(v->text, strlen(v->text), out)) return ir_fail(s, "a value is not a canonical decimal below r", key);
  return 1;
}

/* LIST is a list of COUNT columns, each of length MIN to MAX. */
static int read_columns(Sat *s, const JVal *list, const char *key, uint32_t count, uint32_t min, uint32_t max, Column **out) {
  Column *cols = arena_alloc(s->arena, (count + 1u) * sizeof *cols);
  uint32_t i, j;
  if (cols == NULL) return oom(s->diag);
  if (want_arr(s, list, key) == NULL) return 0;
  if (list->count != count) return ir_fail(s, "the number of columns is wrong", key);
  for (i = 0; i < count; i++) {
    const JVal *col = want_arr(s, list->items[i], key);
    if (col == NULL) return 0;
    if (col->count < min || col->count > max) return ir_fail(s, "a column has the wrong length", key);
    Fr *v = arena_alloc(s->arena, (col->count + 1u) * sizeof *v);
    if (v == NULL) return oom(s->diag);
    for (j = 0; j < col->count; j++)
      if (!want_fr(s, col->items[j], &v[j], key)) return 0;
    cols[i].v = v;
    cols[i].len = col->count;
  }
  *out = cols;
  return 1;
}

static const Expr *read_expr(Sat *s, const JVal *v, unsigned depth) {
  Expr *x = arena_alloc(s->arena, sizeof *x);
  static const char *const LEAVES[3] = {"a", "f", "i"};
  uint32_t i;
  if (x == NULL) return oom(s->diag), NULL;
  if (v == NULL || v->kind != J_OBJ || v->count != 1 || depth >= SAT_DEPTH_LIMIT) return ir_fail(s, "an EXPR is not an object with one key", "gates"), NULL;
  const char *key = v->keys[0];
  const JVal *arg = v->items[0];
  if (strcmp(key, "c") == 0) {
    x->kind = X_CONST;
    return want_fr(s, arg, &x->c, "gates") ? x : NULL;
  }
  for (i = 0; i < 3; i++) {
    if (strcmp(key, LEAVES[i]) != 0) continue;
    int ok = arg->kind == J_ARR && arg->count == 2 && arg->items[0]->kind == J_INT && arg->items[1]->kind == J_INT;
    if (!ok || arg->items[0]->num < 0 || arg->items[0]->num >= (int64_t)s->counts[i]) return ir_fail(s, "a column reference is not [COL, ROT] with a known COL", "gates"), NULL;
    if (arg->items[1]->num < INT32_MIN || arg->items[1]->num > INT32_MAX) return ir_fail(s, "a rotation is not a 32-bit integer", "gates"), NULL;
    x->kind = i == 0 ? X_ADVICE : i == 1 ? X_FIXED : X_INSTANCE;
    x->col = (uint32_t)arg->items[0]->num;
    x->rot = arg->items[1]->num;
    return x;
  }
  if (strcmp(key, "-") == 0) {
    x->kind = X_NEG;
    x->a = read_expr(s, arg, depth + 1);
    return x->a == NULL ? NULL : x;
  }
  int sum = strcmp(key, "+") == 0;
  if ((!sum && strcmp(key, "*") != 0) || arg->kind != J_ARR || arg->count != 2) return ir_fail(s, "an EXPR has an unknown form", "gates"), NULL;
  x->kind = sum ? X_SUM : X_PROD;
  x->a = read_expr(s, arg->items[0], depth + 1);
  x->b = x->a == NULL ? NULL : read_expr(s, arg->items[1], depth + 1);
  return x->b == NULL ? NULL : x;
}

static void col_value(const Sat *s, uint32_t kind, uint32_t col, uint32_t row, Fr *out) {
  const Column *c = &s->cols[kind][col];
  if (row < c->len) *out = c->v[row];
  else fr_from_u64(out, 0);
}

static void eval_expr(const Sat *s, const Expr *x, uint32_t row, Fr *out) {
  Fr a, b;
  int64_t at = ((int64_t)row + x->rot % (int64_t)s->n + (int64_t)s->n) % (int64_t)s->n;
  switch (x->kind) {
    case X_CONST: *out = x->c; break;
    case X_ADVICE: col_value(s, 0, x->col, (uint32_t)at, out); break;
    case X_FIXED: col_value(s, 1, x->col, (uint32_t)at, out); break;
    case X_INSTANCE: col_value(s, 2, x->col, (uint32_t)at, out); break;
    case X_NEG: eval_expr(s, x->a, row, &a); fr_neg(out, &a); break;
    case X_SUM: eval_expr(s, x->a, row, &a); eval_expr(s, x->b, row, &b); fr_add(out, &a, &b); break;
    case X_PROD: eval_expr(s, x->a, row, &a); eval_expr(s, x->b, row, &b); fr_mul(out, &a, &b); break;
  }
}

static int check_gates(Sat *s, const JVal *gates) {
  static const char *const KEYS[] = {"name", "polys"};
  uint32_t g, p, row;
  if (want_arr(s, gates, "gates") == NULL) return 0;
  for (g = 0; g < gates->count; g++) {
    if (!want_keys(s, gates->items[g], KEYS, sizeof KEYS / sizeof KEYS[0], "gates")) return 0;
    const JVal *name = jget(gates->items[g], "name");
    const JVal *polys = want_arr(s, jget(gates->items[g], "polys"), "gates");
    if (polys == NULL || name == NULL || name->kind != J_STR) return polys == NULL ? 0 : ir_fail(s, "a gate has no name", "gates");
    if (polys->count == 0) return ir_fail(s, "a gate has no polynomials", "gates");
    for (p = 0; p < polys->count; p++) {
      const Expr *x = read_expr(s, polys->items[p], 0);
      if (x == NULL) return 0;
      for (row = 0; row < s->n; row++) {
        Fr v;
        eval_expr(s, x, row, &v);
        if (!fr_lt_u64(&v, 1)) {
          char text[FR_TEXT_MAX];
          fr_print(&v, text);
          return diag_fail(s->diag, "SAT_GATE", NULL, "gate %s poly %u is %s on row %u", name->text, p, text, row);
        }
      }
    }
  }
  return 1;
}

static int read_exprs(Sat *s, const JVal *list, const Expr ***out) {
  const Expr **xs = arena_alloc(s->arena, (list->count + 1u) * sizeof *xs);
  uint32_t i;
  if (xs == NULL) return oom(s->diag);
  for (i = 0; i < list->count; i++)
    if ((xs[i] = read_expr(s, list->items[i], 0)) == NULL) return 0;
  *out = xs;
  return 1;
}

static int tuple_eq(const Fr *a, const Fr *b, uint32_t width) {
  uint32_t i;
  for (i = 0; i < width; i++)
    if (!fr_eq(&a[i], &b[i])) return 0;
  return 1;
}

static int table_expr(Sat *s, const Expr *x) {
  switch (x->kind) {
    case X_CONST: return 1;
    case X_FIXED: s->table_columns[x->col] = 1; return 1;
    case X_SUM:
    case X_PROD: return table_expr(s, x->a) && table_expr(s, x->b);
    case X_NEG: return table_expr(s, x->a);
    case X_ADVICE:
    case X_INSTANCE: return 0;
  }
  return 0;
}

/* The distinct table tuples, then a scan for each input tuple. */
static int check_lookup(Sat *s, const JVal *lookup) {
  static const char *const KEYS[] = {"name", "inputs", "tables"};
  if (!want_keys(s, lookup, KEYS, sizeof KEYS / sizeof KEYS[0], "lookups")) return 0;
  const JVal *name = jget(lookup, "name");
  const JVal *inputs = want_arr(s, jget(lookup, "inputs"), "lookups");
  const JVal *tables = inputs == NULL ? NULL : want_arr(s, jget(lookup, "tables"), "lookups");
  const Expr **in = NULL;
  const Expr **tab = NULL;
  uint32_t row, i, j, distinct = 0;
  if (tables == NULL) return 0;
  if (name == NULL || name->kind != J_STR || inputs->count == 0 || inputs->count != tables->count) return ir_fail(s, "a lookup needs a name and inputs and tables of one length", "lookups");
  uint32_t w = inputs->count;
  Fr *set = arena_alloc(s->arena, ((size_t)s->n * w + 1u) * sizeof *set);
  Fr *probe = arena_alloc(s->arena, (w + 1u) * sizeof *probe);
  if (set == NULL || probe == NULL) return oom(s->diag);
  if (!read_exprs(s, inputs, &in) || !read_exprs(s, tables, &tab)) return 0;
  for (i = 0; i < w; i++)
    if (!table_expr(s, tab[i])) return ir_fail(s, "tables must use fixed columns only", "lookups");
  for (row = 0; row < s->n; row++) {
    for (i = 0; i < w; i++) eval_expr(s, tab[i], row, &probe[i]);
    for (j = 0; j < distinct && !tuple_eq(&set[j * w], probe, w); j++) {}
    if (j == distinct) memcpy(&set[distinct++ * w], probe, w * sizeof *probe);
  }
  for (row = 0; row < s->n; row++) {
    for (i = 0; i < w; i++) eval_expr(s, in[i], row, &probe[i]);
    for (j = 0; j < distinct && !tuple_eq(&set[j * w], probe, w); j++) {}
    if (j == distinct) return diag_fail(s->diag, "SAT_LOOKUP", NULL, "lookup %s fails on row %u", name->text, row);
  }
  return 1;
}

static int read_cell(Sat *s, const JVal *v, Fr *out) {
  static const char *const KEYS[] = {"col", "index", "row"};
  if (!want_keys(s, v, KEYS, sizeof KEYS / sizeof KEYS[0], "copies")) return 0;
  const JVal *col = jget(v, "col");
  uint32_t kind, index, row;
  for (kind = 0; kind < 3 && (col == NULL || col->kind != J_STR || strcmp(col->text, COL_KINDS[kind]) != 0); kind++) {}
  if (kind == 3) return ir_fail(s, "a cell has no known col", "copies");
  uint32_t limit = kind == 2 ? s->n - BLINDING_ROWS : s->rows;
  if (!want_int(s, v, "index", 0, (int64_t)s->counts[kind] - 1, &index) || !want_int(s, v, "row", 0, (int64_t)limit - 1, &row)) return 0;
  col_value(s, kind, index, row, out);
  return 1;
}

static int check_copies(Sat *s, const JVal *copies) {
  uint32_t i;
  if (want_arr(s, copies, "copies") == NULL) return 0;
  for (i = 0; i < copies->count; i++) {
    const JVal *pair = copies->items[i];
    Fr a, b;
    if (pair->kind != J_ARR || pair->count != 2) return ir_fail(s, "a copy is not a pair of cells", "copies");
    if (!read_cell(s, pair->items[0], &a) || !read_cell(s, pair->items[1], &b)) return 0;
    if (!fr_eq(&a, &b)) return diag_fail(s->diag, "SAT_COPY", NULL, "copy %u joins two cells with different values", i);
  }
  return 1;
}

int circuit_satcheck(Arena *arena, Diag *diag, const char *circuit, size_t circuit_len, const char *witness, size_t witness_len) {
  static const char *const C_KEYS[] = {"glue-lang-plonkish", "name", "field", "k", "rows", "advice", "fixed", "instance", "gates", "lookups", "copies", "fixed_values", "public"};
  static const char *const W_KEYS[] = {"glue-lang-witness", "name", "advice", "instance"};
  Sat s;
  uint32_t version, k, pub, i;
  memset(&s, 0, sizeof s);
  s.arena = arena;
  s.diag = diag;
  const JVal *c = jdocument(arena, diag, "the circuit document", circuit, circuit_len);
  const JVal *w = c == NULL ? NULL : jdocument(arena, diag, "the witness document", witness, witness_len);
  if (w == NULL) return 0;
  if (!want_keys(&s, c, C_KEYS, sizeof C_KEYS / sizeof C_KEYS[0], "circuit") || !want_keys(&s, w, W_KEYS, sizeof W_KEYS / sizeof W_KEYS[0], "witness")) return 0;
  const JVal *field = jget(c, "field");
  const JVal *name = jget(c, "name");
  const JVal *wname = jget(w, "name");
  if (!want_int(&s, c, "glue-lang-plonkish", 1, 1, &version) || !want_int(&s, w, "glue-lang-witness", 1, 1, &version)) return 0;
  if (field == NULL || field->kind != J_STR || strcmp(field->text, "bn254-fr") != 0) return ir_fail(&s, "the field is not bn254-fr", "field");
  if (name == NULL || wname == NULL || name->kind != J_STR || wname->kind != J_STR || name->text[0] == '\0' || strcmp(name->text, wname->text) != 0) return ir_fail(&s, "the two documents do not have one nonempty name", "name");
  if (!want_int(&s, c, "k", 1, SAT_K_MAX, &k)) return 0;
  s.n = (uint32_t)1 << k;
  if (!want_int(&s, c, "rows", 0, (int64_t)s.n - BLINDING_ROWS, &s.rows)) return 0;
  for (i = 0; i < 3; i++)
    if (!want_int(&s, c, COL_KINDS[i], 0, SAT_COLUMNS_MAX, &s.counts[i])) return 0;
  if (!want_int(&s, c, "public", 0, s.n - BLINDING_ROWS, &pub)) return 0;
  if (s.counts[2] == 0 && pub > 0) return ir_fail(&s, "public inputs need instance column 0", "public");
  if (!read_columns(&s, jget(c, "fixed_values"), "fixed_values", s.counts[1], s.rows, s.n - BLINDING_ROWS, &s.cols[1])) return 0;
  if (!read_columns(&s, jget(w, "advice"), "advice", s.counts[0], s.rows, s.rows, &s.cols[0])) return 0;
  if (!read_columns(&s, jget(w, "instance"), "instance", s.counts[2], 0, s.n - BLINDING_ROWS, &s.cols[2])) return 0;
  if (s.counts[2] > 0 && s.cols[2][0].len != pub) return ir_fail(&s, "instance column 0 does not have public values", "instance");
  if (!check_gates(&s, jget(c, "gates"))) return 0;
  const JVal *lookups = want_arr(&s, jget(c, "lookups"), "lookups");
  if (lookups == NULL) return 0;
  for (i = 0; i < lookups->count; i++)
    if (!check_lookup(&s, lookups->items[i])) return 0;
  for (i = 0; i < s.counts[1]; i++)
    if (s.cols[1][i].len != s.rows && !s.table_columns[i]) return ir_fail(&s, "only table columns may exceed rows", "fixed_values");
  return check_copies(&s, jget(c, "copies"));
}
