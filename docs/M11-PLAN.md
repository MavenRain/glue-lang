# M11 plan: the Poseidon forms (O24)

Status:  CLOSED 2026-10-20 (s2);  s1 d0b78f1 (USER);  s2 the close, DONE
2026-10-20 (docs only;  docs/STATUS.md `### s2: close`).
RULED 2026-10-20 (USER, the first message of the s1 session:
"Take all recommendations for (a) through (h)";  each recommendation of
section 9 taken, no ruling differs);  s1 BUILT 2026-10-20.  The m2 ruling
of s1-b (USER 2026-10-20):  (r1), the recommendation:  a refusal row for
the op in the memo key (examples/memoKey.glue, test/circuit/refuse.txt
row 22);  it adds one guest example to ruling (f), with no e2e row, no
verifier and no fixture.  The plan file, the docs/PLAN.md M11
row and the docs/STATUS.md `### kickoff` section of `## M11` go with the
M11 s1 commit.  The topic slot of the kickoff message came as the literal
placeholder `<O24 the Poseidon forms (ii)/(iii) | another topic, name
it>`;  the kickoff took its first option (O24) and said so.  The kickoff
probes ran on scratch copies with the real build/langc and the real
halo2/target/debug/glue-halo2 (the runner
~/Documents/glue-lang-M11-kickoff-probe/run.sh and rewrite.awk, the log
run.out, all output under scratch/);  no repository file changed.

## 1. Sources

- SPEC.md 9 O24 (lines 1534 to 1561):  the Poseidon forms, the M10 s4
  result and the literal memo formula of the USER (`585 * H - 9 + 195 *
  (H - 1)`).
- docs/STATUS.md M10 `### s4` (line 5518):  the form (i) counts (an S-box
  on a binder 14 rows, a full round 72 rows, a partial round 54 rows) and
  the estimate table;  `### s5: close` (line 5537):  the gate counts at
  the M10 close.
- docs/M8-PLAN.md D4:  the forms (i), (ii) and (iii).
- M9 ruling (h).
- docs/IR.md sections 2 and 4:  the IR fields and the gate set (the arith
  gate with the coefficient columns ql, qr, qm and qc).
- src/circuit.c (the kickoff survey, read only):  `add_row` 128 to 141,
  `lower_value` 148 to 187, `lower_check` 189 to 221 (`eval_core` at
  197), `lower_circuit` 225 to 255, `pick_k` 276 to 282, `fixed_at` 284
  to 310, `witness_hints` 444 to 457, satcheck 753 to 833;  the structs
  Cell 36 to 39, RowKind 41 to 46, Row 48 to 56 and Lower 68 to 79;
  ARITH_POLY 29 to 31, RANGE8_LOOKUP 32 to 33, FIXED_COUNT 7 at line 20.
- The kickoff probe:  P1 (form (iii) by hand in the source, scratch
  copies of the four Poseidon examples) and P2 (form (ii) on the back end,
  a jq copy of test/circuit/square.circuit.json).

## 2. Design calls

D1.  The measure (question (d)).  The cost of a circuit is its row count:
the rows give k (the rule rows + 16 <= 2^k), k gives the prover domain
(2^k rows, and an extended domain set by the gate degree) and the SRS fit
(the pinned ceremony file has k 15:  O23 and the M9 s7 USER rewording).
The forms are compared on the rows and k of the four Poseidon goldens and
on the Merkle depth that fits in k 15.  The depth model:  the rows of one
Merkle level = (kycMerkle - kyc) / 4 (kycMerkle has depth 4:  6 hashes,
the bits b0 to b3);  rows(d) = the rows at depth 4 + (d - 4) x the level
rows;  k 15 has 32752 usable rows.

| Form | rows a level | kycMerkle (depth 4) | max depth at k 15 | depth 20 |
|---|---|---|---|---|
| (i), the current lowering | 3652 | 21899, k 15 | 6 | 80331, k 17 |
| pointer memo (D3) | 3012 | 18059, k 15 | 8 | |
| binder memo (P1, no hint rows) | 2556 | 15323, k 14 | 10 | |
| structural memo (D2, a model) | 1776 | 10847, k 14 | 16 | 39263, k 16 |

A depth-20 tree is above the pinned SRS under each form.  M11 keeps the
depth 4;  the table goes into SPEC.md 9 O24.

D2.  Form (iii) as the structural row memo (questions (a) and (b)).  The
key of an ARITH row is (the op, the cell of the first operand, the cell
of the second operand), in this order:  no commutative normal form, so
`a + b` and `b + a` are two keys.  The key of a CONST row is its Fr
value.  A VAR is a binder cell and has no row, thus no entry.  The table
is part of `Lower` (one table for each circuit):  it shares rows across
the constraints and shares each literal row.  The place:  in
`lower_value`, a probe before each `add_row` (the CONST row and the ARITH
row, near lines 163 and 172;  s1 confirms the sites) returns the cell of
the row in the table, and an insert after `add_row` records a new row.
The operands of a stuck `+`, `-` or `*` are lowered first, so the key
holds their cells.  The size:  about 40 to 60 lines in src/circuit.c.
Soundness:  the arith gate fixes the output cell of a row as a function
of the op and the operand cells, so two rows with the same key hold the
same value in each satisfying witness;  a CONST row is keyed on its
value.  The memo thus removes rows and copies only, never a value or a
constraint on a value that the old IR did not imply.  It is a lowering
change, not a conv rule:  no two values become equal because of their
type, so the prohibition of proof irrelevance does not apply.  satcheck
checks each regenerated golden.  The lookup rows (RANGE8_LOOKUP) do not
change.

D3.  The pointer memo and the source form, rejected (question (b)).
Option (p) of the survey keys the memo on the `const Value *` of a stuck
term (about 20 to 30 lines).  `lower_check` runs `eval_core` once for
each constraint (line 197), so a pointer is shared inside one constraint
only:  the five factors of `sbox5 (s + rc)` are one pointer (inferred
from the code;  tested in s1:  yes, the five factors of each of the 80
sbox5 chains of poseidon are one `const Value *`), and the sums
that three mix constraints repeat are not.  It gives the P1 sbox column:
kycMerkle 18059 rows, k 15.  The source form (P1:  a hint for each sum
in the .glue file) gives kycMerkle 16487 rows, k 15 (the hint rows
stay), and it edits four one-line files of up to 226 KB that nothing
generates.  Neither gives k 14 for kycMerkle.

D4.  Form (ii), the degree-5 gate (question (a)).  P2 shows that the
back end is ready:  a fixed column q5 and the gate `pow5` with the poly
`q5 * (a^5 - c)` (degree 6) on a copy of the square golden pass check,
keygen, prove, verify and solidity of the real glue-halo2 (rc 0), and
the bad witness c = 242 fails check and prove (`Constraint 0 in gate 1
('pow5') is not satisfied`).  The survey finds no degree limit and no
fixed-column limit in halo2/ (the gates are generic, circuit.rs 214 to
221;  MAX_K 28, ir.rs line 13) and in the verifier generator (rev
9e99730a asserts advice > 0 and at most one instance column), and
satcheck evaluates an EXPR of any degree.  The change is a lowering
peephole (a product chain of one cell to the 5th power becomes one pow5
row) and docs/IR.md section 4 (a new gate in the gate set:  a new IR
form, thus a USER ruling).  The gain on top of form (iii):  no k for a
current circuit (the M10 s4 table:  both forms together give kycMerkle
13883 rows, k 14, the k of (iii) alone).  The cost:  a degree-6 gate
grows the extended domain of the prover (not measured on a real
circuit).  On the square golden the verifier and the proof are smaller
with the gate (41815 bytes against 46893, 5690 against 6714:  the
generator sets the permutation chunks and the quotient count from
`cs.degree()`).  Form (ii) stays open in O24.

D5.  Form (iv), the literal fold (question (e)).  New in the kickoff,
not in O24.  The arith gate has the coefficient columns ql, qr, qm and qc
(docs/IR.md section 4), but the lowering gives each literal its own
CONST row and each op its own ARITH row.  A fold of a literal operand
into ql, qr or qc (`x + rc` in one row, `m0 * t0 + m1 * t1` in one row)
needs no new IR form and no back-end change.  Not estimated and not
probed.  It goes into SPEC.md 9 O24;  no build in M11.

D6.  The IR rule of M11 (question (c)).  The memo changes the IR of the
current circuits:  the rows, the copies, the fixed values and maybe k of
each circuit that repeats a literal or an operation (the four Poseidon
circuits;  maybe all 8 goldens of the 9 circuit rows, quotSq runs
square:  s1 lists them).  No new IR form:  the row kinds, the gate, the
lookup and the IR fields stay, and docs/IR.md does not change.  Each
golden that changes regenerates (.circuit.json and .witness.json) with
its verifier (contracts/src/verifiers/) and its fixture.  s1 lists the
changed goldens before e2e;  the diff before e2e (the index against
HEAD) and after e2e (the worktree against the index) holds only the
listed goldens, their verifiers and their fixtures.  If another path
changes, STOP.  The pins that s1 checks:  test/circuit/expect.txt rows 6
to 9 (also the e2e table), refuse.txt rows 17 to 20 (WITNESS_FAIL) and
21 (WITNESS_ARGS kyc), and the 19 rows of same-ir.txt (the memo applies
to both sides of each row;  each row keeps its verdict).

D7.  The open items (question (g)).  O3, O12 (c), O13, O14, O15, O16,
O19 and O23 stay open (O23:  the deployment target and the check of the
Axiom copy of the ceremony file, USER items).  O24 stays open after M11
with forms (ii) and (iv) and the depth table of D1.

## 3. Core changes

src/circuit.c only:  `Lower` gets the memo table;  `lower_value` probes
and inserts (D2).  No conv rule, no new IR form, no back-end change, no
verifier generator change, no satcheck change.

## 4. Derived operations and their laws

None new.

## 5. Effect on examples

None:  the memo is in the lowering, and the .glue files stay.  The m2
ruling (r1) of s1-b adds examples/memoKey.glue (one circuit, a refusal
row only).

## 6. Circuits

| Circuit | rows, k (golden) | rows, k (memo, the model = s1 measured) | Verifier | Golden |
|---|---|---|---|---|
| poseidon | 3644, 12 | 1972, 11 | contracts/src/verifiers/poseidon.sol regenerates | test/circuit/poseidon.*.json regenerate |
| transfer | 7288, 13 | 3740, 12 | transfer.sol regenerates | transfer.*.json regenerate |
| kyc | 7291, 13 | 3743, 12 | kyc.sol regenerates | kyc.*.json regenerate |
| kycMerkle | 21899, 15 | 10847, 14 | kycMerkle.sol regenerates | kycMerkle.*.json regenerate |
| square, poly, range, nonzero (quotSq runs square) | as M10 | as M10 (s1 measured:  no change) | no change | no change |

The model column is the M10 s4 `(iii) + literals` column:  s1 measured
the same rows and k for each circuit (circuit, witness and satcheck rc 0;
the copies 3256, 6512, 6518 and 19602).  P1 confirmed the `(iii) binders` column exactly (2548, 5096,
5099, 15323).  The e2e table keeps 9 rows.  The ceremony file (k 15)
covers each k (M9 s5 ran kyc at k 13 with it).

## 7. New check codes

None.

## 8. Guest tests per slice

- s1:  the changed goldens regenerate (the counts of `make check` stay);
  mutants:  m1 the memo probe removed (the rows go back to form (i):  the
  golden diff at `make check`);  m2 the key without the op (a `+` and a
  `*` on the same cells share one row:  satcheck or the golden;  s1:
  SURVIVED, because no current circuit has the case;  the m2 ruling (r1)
  adds the refusal row of examples/memoKey.glue, and m2 is KILLED there);  m3 the
  key without the second operand (satcheck or the golden);  m4 a CONST key
  without the value (all literals share one row:  satcheck);  `forge test
  --root contracts` 100 passed with the regenerated fixtures.
- s2:  none (the close).

## 9. Questions for the USER

- (a) The scope:  form (iii) as the structural memo in M11;  forms (ii)
  and (iv) stay open in O24.  Recommendation:  yes.  Reasons:  k 14 for
  kycMerkle and k 11, 12 and 12 for poseidon, transfer and kyc (the
  model), with no new IR form and no back-end change;  form (ii) adds no k
  on top of (iii) for a current circuit and needs a new gate (D4).
- (b) The key:  structural (the op and the operand cells, the value of a
  literal) against the pointer key.  Recommendation:  structural.
  Reasons:  the pointer key shares inside one constraint only and leaves
  kycMerkle at k 15 (D3);  the structural key shares across the
  constraints and each literal row (D2).
- (c) The IR rule ruling:  each golden that the memo changes regenerates
  with its verifier and its fixture;  s1 lists them before e2e.
  Recommendation:  yes.  Reason:  D6 (no new IR form, docs/IR.md stays;
  this ruling is the IR rule ruling for the current circuits).
- (d) The Merkle depth stays 4 in M11, and the depth table of D1 goes
  into SPEC.md 9 O24.  Recommendation:  yes.  Reason:  a depth-20 tree is
  above the pinned SRS (k 15) under each form (k 16 with the memo).
- (e) Form (iv), the literal fold:  recorded in O24, no build.
  Recommendation:  yes.  Reason:  not estimated and not probed (D5).
- (f) A small guest example for the memo (`memo.glue`:  a new e2e row and
  a new verifier) against the Poseidon goldens only.  Recommendation:  the
  goldens only.  Reasons:  the mutants m1 to m4 die at the goldens and at
  satcheck;  a new example adds a verifier, a fixture and an e2e row.
- (g) The open items of D7 stay open.  Recommendation:  yes.
- (h) The slices of section 10.  Recommendation:  s1 (two sessions) and
  s2 as listed.

## 10. Slices

Each slice:  the gates of COMMON (`make -B check`, `make check-clang`,
`forge build --root contracts --sizes`, `make e2e` last, the IR check,
`forge test --root contracts`), the mutants, the close steps.  The gate
counts at the M10 close:  native circuit 9, path 46, face 35, HIT 44;
examples 67, parse refusals 18, check refusals 157, evals 290, json 42
builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 21,
satcheck refusals 6;  node regressions 6, printer 2;  forge test 100;
e2e 9 rows.

- s1:  the memo, two sessions as M10 s1.  s1 (the build):  first test
  the D3 inference (the five factors of `sbox5` are one pointer) and
  measure the memo rows of each golden in a scratch copy;  then the memo
  in src/circuit.c (D2), the changed goldens regenerated and staged
  before e2e, the langc mutants m1 to m4.  s1-b (the gates, the sandbox
  off):  `make -B check`, `make check-clang`, `forge build --root
  contracts --sizes`, `make e2e` (the verifiers and the fixtures
  regenerate), the IR check (only the listed goldens, verifiers and
  fixtures change), `forge test --root contracts` 100, `make e2e-ceremony
  SRS=<the pinned file>` (kycMerkle at k 14 with the k 15 file), `make
  rehearsal`, `make deployment-test`;  SPEC.md 7.1 (the memo rule) and 9
  O24 (form (iii) done);  docs/PLAN.md and docs/M11-PLAN.md go with this
  commit.  Gate delta:  the counts stay;  the rows and k of the changed
  goldens change (the section 6 table);  forge test 100.
- s2:  the close.  SPEC.md 4 (the M11 bullet) and 9 O24 (forms (ii) and
  (iv) open, the depth table);  docs/PLAN.md M11 row DONE;  docs/STATUS.md
  `### s2: close`;  ~/Documents/glue-lang-M11-COMMON.md traps.  Gate
  delta:  none.
