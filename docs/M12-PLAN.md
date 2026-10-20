# M12 plan: the literal fold (O24 form (iv))

Status:  CLOSED 2026-10-20 (s3);  s1 152c6d4 (USER);  s2 54cda59 (USER);
s3 the close, DONE 2026-10-20 (docs only;  docs/STATUS.md `### s3:
close`).
RULED 2026-10-20:  the USER took each recommendation (DRAFT at the kickoff).  The USER ruled on section 9,
(a) to (h), before s1.  The plan file, the docs/PLAN.md M12 row and the
docs/STATUS.md `### kickoff` section of `## M12` go with the M12 s1
commit.  The topic slot of the kickoff prompt named form (iv) ("Topic:
form (iv)"), the first option.  The kickoff probe ran on scratch copies
with the real build/langc and the real halo2/target/debug/glue-halo2 (the
runner ~/Documents/glue-lang-M12-kickoff-probe/run.sh, model.awk,
flatten.jq, fold-circuit.jq and fold-witness.jq, the log run.out, all
output under scratch/);  no repository file changed.

## 1. Sources

- SPEC.md (the repository root) 9 O24:  forms (ii), (iii) and (iv), the
  M11 measured rows and k, the depth table.
- docs/M11-PLAN.md D1 (the measure and the depth model), D2 (the
  structural memo), D5 (form (iv):  "needs no new IR form and no back-end
  change"), D6 (the IR rule).
- ~/Documents/glue-lang-M11-COMMON.md line 5 ("a new row kind (form
  (iv)) ... needs a new USER ruling first").
- docs/IR.md sections 2 and 4:  `fixed_values` holds one Fr for each row
  of each fixed column;  the arith gate `ql*a + qr*b + qm*a*b + qc - qo*c
  = 0`.
- src/circuit.c (read only):  `RowKind` and `Row` 41 to 56, `add_row`
  131, the memo 146 to 227, `lower_value` 234 to 273, `pick_k` 362,
  `fixed_at` 370 to 396 (qr = -1 for a SUB row, qc = k for a CONST row),
  `put_copies` 408 to 427, `fill_advice` 488 to 510.
- The kickoff probe:  the model on the 8 goldens and one folded copy of
  the poseidon golden (docs/STATUS.md M12 `### kickoff`).

## 2. Design calls

D1.  The question of the kickoff:  a new IR form or a new row kind?  The
answer:  no new IR form, and a lowering change in src/circuit.c.  The IR
(docs/IR.md) has no row kinds:  a row is the values of the 7 fixed
columns and the copies, and `fixed_values` holds any Fr (the current
lowering already writes qr = -1 and qc = k).  The probe folded three rows
of a copy of the poseidon golden by hand:  row 293 (a MUL by a literal:
qr = m, qm = 0, the copy of the literal operand removed), row 281 (an ADD
of a literal:  qr = 0, qc = rc, the copy removed) and row 296 (an ADD of
two MUL-by-literal rows:  ql = m0, qr = m1, a and b copied from the two
state cells).  satcheck, check, keygen, prove, verify and solidity gave
rc 0 (the verifier 46893 bytes, as the golden:  the gate does not
change).  The bad witness (c of row 296 changed) is refused by satcheck
(`SAT_GATE ... on row 296`), check and prove (`Constraint 0 in gate 0
('arith') is not satisfied ... at offset 296`).  Thus D5 of M11 is true
for the IR, the back end and the verifier generator.  The "new row kind"
of COMMON line 5 is true for the lowering only:  `Row` must hold the
coefficients (question (c)).  The fold needs the USER ruling that COMMON
line 5 asks for (question (b)), as the memo did (M11 ruling (c)).

D2.  The two scopes of the fold (question (a)).  T1, the literal fold:  a
literal operand of an ARITH row goes into ql, qr, qm or qc of that row;
a CONST row goes when each of its uses is an ARITH operand.  T2, the
affine fold:  T1, and also an ARITH row whose uses are all ARITH operands
goes into each consumer row when the form of the consumer still fits the
gate (at most two cells, degree 2 at most, no square of one cell when two
cells occur);  the row goes when each consumer takes it.  Examples:
`m0 * t0 + m1 * t1` is one row (ql = m0, qr = m1);  `(s + rc) * (s + rc)`
is one row (a = b = s, qm = 1, ql = 2 rc, qc = rc^2);  `x4 * (s + rc)`
is one row (qm = 1, ql = rc).

D3.  The model (the probe, on top of the memo;  rows, k, copies).

| Circuit | memo (M11) | T1 | T2 |
|---|---|---|---|
| poseidon | 1972, 11, 3256 | 1769, 11, 2477 | 990, 10, 1698 |
| transfer | 3740, 12, 6512 | 3537, 12, 4954 | 1979, 11, 3396 |
| kyc | 3743, 12, 6518 | 3540, 12, 4960 | 1982, 11, 3402 |
| kycMerkle | 10847, 14, 19602 | 10644, 14, 14928 | 5970, 13, 10254 |
| poly | 9, 5, 13 | 8, 5, 12 | 7, 5, 11 |
| range | 4, 9, 4 | 3, 9, 3 | 3, 9, 3 |
| square, nonzero | no change | no change | no change |

The counts in each Poseidon golden:  204 CONST rows (the memo shares each
literal over all hashes;  203 have ARITH operand uses only, and 1 is also
a constraint side);  the ARITH rows with a literal operand:  poseidon ADD
194 and MUL 585, transfer and kyc ADD 388 and MUL 1170, kycMerkle ADD 1164
and MUL 3510;  no ARITH row has two literal operands.  T1 removes the 203
CONST rows only, so k does not change.  T2 also removes each `s + rc` row
and each MUL-by-literal row (poseidon:  194 and 585), so k drops by 1 for
each Poseidon circuit.  The model was checked on the goldens (instance +
check + operand copies = the copies of each golden;  build/langc
reproduces the poseidon golden).

D4.  The depth (the D1 model of M11:  level rows = (kycMerkle - kyc) / 4;
k 15 has 32752 usable rows).

| Form | rows a level | kycMerkle (depth 4) | max depth at k 15 | depth 20 |
|---|---|---|---|---|
| structural memo (M11) | 1776 | 10847, k 14 | 16 | 39263, k 16 |
| memo + T1 | 1776 | 10644, k 14 | 16 | 39060, k 16 |
| memo + T2 | 997 | 5970, k 13 | 30 | 21922, k 15 |

Under T2 a depth-20 tree fits the pinned SRS (k 15).  M12 keeps the depth
4 (question (e)).

D5.  The lowering (question (c)).  T1 is at the lowering site:  in the
`VAL_STUCK` case of `lower_value`, a Field literal argument is not
lowered to a CONST row;  its value goes into the coefficients of the
row.  A literal that is a constraint side keeps its CONST row.  T2 is a
pass after `lower_circuit`:  count the uses of each row, compute the form
of each ARITH row bottom-up, take each operand row that fits, then remove
the rows that each consumer took and renumber the rows above the binder
rows (the binder rows 0 to B - 1 do not move;  the cells of the rows and
of the checks are renumbered).  The memo key of an ARITH row becomes the
4 coefficients and the 2 operand cells (no op):  two rows with the same
key hold the same value in each satisfying witness.  Size (an estimate):
T1 40 to 70 lines, T2 150 to 250 lines, in src/circuit.c only.

D6.  Soundness.  A folded row states the same polynomial identity as the
rows it replaces:  the removed cell had no use other than the consumers
that took its form, so the satisfying assignments on the remaining cells
do not change.  The fold is a lowering change, not a conv rule:  no two
values become equal because of their type, so the prohibition of proof
irrelevance does not apply.  satcheck checks each regenerated golden.

D7.  The IR rule of M12 (question (b)).  As M11 D6:  no new IR form (the
gate, the lookup and the IR fields stay;  docs/IR.md does not change).
The fold changes the rows, the copies, the fixed values and maybe k of
each circuit with a literal operand:  T1 poseidon, transfer, kyc,
kycMerkle, poly and range;  T2 the same 6.  Each changed golden
regenerates with its verifier and its fixture;  the slice lists them
before e2e.  If another path changes, STOP.

D8.  The open items (question (g)).  Form (ii) (the degree-5 gate) stays
open in O24 (not modeled on top of T2).  O23 (USER items), O3, O12 (c),
O13, O14, O15, O16 and O19 do not change.

## 3. Core changes

src/circuit.c only:  `Row` holds the coefficients ql, qr, qm, qc (D5,
question (c));  `lower_value` folds a literal argument (T1);  the memo key
holds the coefficients;  `fixed_at`, `fill_advice` and `put_copies` read
the coefficients;  a fold pass after `lower_circuit` (T2).  No conv rule,
no new IR form, no back-end change, no verifier generator change, no
satcheck change.

## 4. Derived operations and their laws

None new.

## 5. Effect on examples

None for the current examples (the fold is in the lowering).  Question
(d):  one new guest example for the key mutant (examples/foldKey.glue,
a refusal row only).

## 6. Circuits

The rows, k and copies of section D3, T1 at s1 and T2 at s2.  The
verifiers and the fixtures of the 6 changed circuits regenerate;  the
e2e table keeps 9 rows;  the ceremony file (k 15) covers each k.

## 7. New check codes

None.

## 8. Guest tests per slice

- s1 (T1):  the 6 changed goldens regenerate (the counts of `make check`
  stay, except the new refusal row);  mutants:  f1 the fold removed (the
  rows go back to the memo:  the golden diff);  f2 the memo key without
  the coefficients (`x + 5` and `x + 7` on one cell share a row:  the
  refusal row of examples/foldKey.glue, `x + 5 == p;  x + 7 == q` with
  the bad witness q = x + 5;  no Poseidon golden is named for f2 until s1
  shows a case);  f3 the qc of an ADD fold set to 0 (satcheck on each
  golden);  `forge test --root contracts` 100 passed.
- s2 (T2):  the 6 goldens regenerate again;  mutants:  f4 the use check
  removed (a row with a consumer that did not take it goes:  satcheck or
  a refusal);  f5 the fit check removed (a form with three cells:  the
  golden diff or satcheck);  f6 the renumbering of the check cells
  removed (satcheck).
- s3:  none (the close).

## 9. Questions for the USER (RULED 2026-10-20:  "Take all recommendations for (a) through (h).")

- (a) The scope:  T1 and T2 in M12, T2 after T1.  Recommendation:  yes.
  Reasons:  T1 alone saves 203 rows and no k (D3);  T2 gives k 10, 11,
  11 and 13 for the 4 Poseidon circuits and the depth 30 at k 15 (D4);
  T1 first gives a small slice with its own gates, and T2 builds on the
  coefficient rows of T1.
- (b) The IR rule ruling of D7 (COMMON line 5 asks for a ruling for "a
  new row kind").  Recommendation:  yes.  Reasons:  D1 (the probe:  the
  back end and the verifier generator accept the folded rows unchanged;
  the IR has no row kinds);  the rule is the M11 rule.
- (c) The row in src/circuit.c:  (r1) the 4 coefficients on `Row` (the
  4 values of `RowKind` stay;  an ARITH row holds its coefficients, not
  an op) against (r2) a new `RowKind` value for a folded row.
  Recommendation:  (r1).  Reasons:  T2 makes any form of the gate, so a
  row with its coefficients is the general case;  (r2) adds a case to
  each exhaustive switch and keeps two encodings of one gate row.
- (d) The key mutant f2:  a refusal row for the coefficients in the memo
  key (examples/foldKey.glue, test/circuit/refuse.txt row 23;  no e2e
  row, no verifier, no fixture).  Recommendation:  yes.  Reason:  the M11
  s1 trap (m2 survived on the goldens;  a key mutant needs a circuit with
  the case).
- (e) The Merkle depth stays 4 in M12;  the depth table of D4 goes into
  SPEC.md 9 O24.  Recommendation:  yes.  Reasons:  a depth change changes
  the kycMerkle circuit, its e2e row and SPEC.md section 8;  under T2 a
  depth-20 tree fits k 15, a candidate topic after M12.
- (f) docs/IR.md:  no change.  Recommendation:  yes.  Reason:  section 4
  gives the gate with coefficient columns and does not limit their values.
- (g) The open items of D8 stay open.  Recommendation:  yes.
- (h) The slices of section 10.  Recommendation:  as listed.

## 10. Slices

Each slice:  the gates of COMMON (`make -B check`, `make check-clang`,
`forge build --root contracts --sizes`, `make e2e` last, the IR check,
`forge test --root contracts`), the mutants, the close steps.  The gate
counts at the M11 close:  native circuit 9, path 46, face 35, HIT 44;
examples 68, parse refusals 18, check refusals 157, evals 290, json 42
builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 22,
satcheck refusals 6;  node regressions 6, printer 2;  forge test 100;
e2e 9 rows.

- s1:  T1, two sessions as M11 s1.  s1 (the build):  the coefficient
  rows (ruling (c)), the literal fold in `lower_value`, the memo key with
  the coefficients, examples/foldKey.glue and refuse.txt row 23 (ruling
  (d)), the 6 goldens regenerated and staged before e2e (the D3 T1
  column), the mutants f1 to f3.  s1-b (the gates, the sandbox off):  the
  gates of COMMON, `make e2e-ceremony`, `make rehearsal`, `make
  deployment-test`;  SPEC.md 7.1 (the fold rule) and 9 O24 (T1 done);
  docs/PLAN.md and docs/M12-PLAN.md go with this commit.  Gate delta:
  examples 68 to 69, circuit refusals 22 to 23;  forge test 100.
- s2:  T2, two sessions (s2 the build, s2-b the gates):  the fold pass
  after `lower_circuit`, the 6 goldens regenerated (the D3 T2 column),
  the mutants f4 to f6.  Gate delta:  none (the rows and k of the
  goldens change).
- s3:  the close.  SPEC.md 4 (the M12 bullet) and 9 O24 (form (iv) DONE,
  the depth table of D4, form (ii) open);  docs/PLAN.md M12 row DONE;
  docs/STATUS.md `### s3: close`;  COMMON traps.  Gate delta:  none.
