# M14 plan:  the pow5 gate (O24 form (ii))

## Status

IN PROGRESS 2026-10-20.  The USER accepted all recommendations in section 9, (a) to (k), on 2026-10-20;  this includes the explicit IR ruling in (b).  The selected variant is (ii-b), emitted per circuit.  s1 is complete;  s1-b gates and s2 close remain.  The kickoff commit is 122b00f, on parent 5bf112c.  Probe:  ~/Documents/glue-lang-M14-kickoff-probe/ (run.sh, run.out, rows.jq, model.py, REPORT.md).

## 1. Sources

- SPEC.md section 9 O24 (lines 1611 to 1715:  the form (ii) estimate of M10, the depth tables, the table `Measured, M13`) and section 7.1 (lines 1326 to 1425, the lowering rules).
- docs/IR.md section 4 (lines 78 to 87, the columns and the gates).
- docs/M8-PLAN.md D4 (the forms (i) to (iii)) and docs/M10-PLAN.md (the form (ii) estimate).
- src/circuit.c (the lowering, the memo, the fold, the emitter, the witness writer) and halo2/src/circuit.rs, ir.rs, backend.rs (the generic back end).
- halo2_proofs v0.3.0 (halo2/Cargo.lock lines 873 to 875, rev 73408a1):  the degree rules of the gates, the permutation and the lookup.
- The probe REPORT.md, sections (a) to (f).

## 2. Design calls

- D1 (the form).  Question (a).  The recommendation is variant (ii-b):  a gate `pow5` with 2 polys on the current advice columns, `q5 * (b - (a + c5)^2)` (degree 3) and `q5 * (c - b^2 * (a + c5))` (degree 4).  One row for each S-box:  a = s (the state cell), c5 = the round constant, b = (s + c5)^2 (a witness value, no copy), c = (s + c5)^5.  The current S-box is 4 arith rows (x^2, x^3, x^4, x^5) and 9 copies;  a pow5 row is 1 row and 2 copies (a and the `==` copy).  The other variants:  (ii-a) one poly `q5 * ((a + c5)^5 - c)`, degree 6, the same rows;  (ii-c) `q5 * (a^5 - c)`, degree 6, the `s + rc` arith row stays (2 rows for each S-box).
- D2 (the IR rule).  Question (b).  M14 adds one IR form:  the gate `pow5`, the row kind `ROW_POW5` and the fixed columns q5 and c5 (columns 7 and 8;  FIXED_COUNT 9 in a circuit with a pow5 row).  The gate `arith`, the lookup `range8`, the 3 advice columns, the instance column and the other IR fields do not change.  No conv rule changes:  the pattern is a lowering rule (D3).  No halo2/ code change and no verifier generator code change (REPORT (c), (d)).
- D3 (the lowering).  Question (e).  lower_value lowers the normal form `x * x * x * x * x` of one operand cell x = s + rc (an affine term of one cell) to one pow5 row with c5 = rc;  no other term lowers to pow5.  The memo key of a pow5 row holds c5 and the cell of s.  The affine fold T2 does not fold into a pow5 row:  pow5 pins its operand, as a range row does (src/circuit.c line 530).  fill_advice writes b and c.
- D4 (the emission).  Question (c).  The gate pow5 and the columns q5 and c5 go only into a circuit that has a pow5 row (as the lookup, src/circuit.c line 685).  Thus square, nonzero, poly and range keep their goldens, VKs, verifiers and fixtures byte for byte.  Shared emission changes all 8 verifiers (REPORT (c)).
- D5 (the degree and the proof).  Under (ii-b) the max degree of poseidon and transfer goes from 3 to 4 (extended_k - k 1 to 2, quotient chunks 2 to 3);  kyc and kycMerkle stay at 5 (the lookup;  extended_k - k 2, 4 chunks).  Proof (MODEL;  the model gives each of the 8 fixtures now exactly):  poseidon and transfer 1,536 to 1,344 B (-192);  kyc and kycMerkle 1,760 to 1,824 B (+64:  2 more fixed evaluations).  A fixture changes by 4 x the proof delta (the hex is in `proof` and in `calldata`).
- D6 (the contracts).  Question (d).  4 verifiers change (poseidon, transfer, kyc, kycMerkle):  the VK constants, the quotient and permutation code and the gate expression.  The generator (halo2_solidity_verifier 9e99730) takes the degree from cs.degree():  no code change, only a new VK and a new text.  The 5 public inputs of kyc and kycMerkle and their order do not change;  KycGate.sol, DeployVerifier.s.sol and rehearsal.sh do not change.  contracts/README.md:  the rows of the 4 circuits from one `forge test --root contracts --gas-report` (sandbox off).
- D7 (the docs).  Question (g).  docs/IR.md section 4 (the gate pow5, q5, c5, `ROW_POW5`);  SPEC.md 7.1 (a new lowering item:  the S-box pattern);  SPEC.md 9 O24 (form (ii) DONE, a table `Measured, M14`, the status of O24 at the close:  question (h)).
- D8 (the cost).  The model:  kycMerkle 21,922 to 16,642 rows (-24 %), copies 37,662 to 25,342 (-33 %), k 15 (no change;  k 14 needs 16,368 rows or fewer, 274 over).  The goldens of the 4 circuits get smaller.  Under (ii-b) kyc and kycMerkle keep the extended domain:  no growth of keygen (139 MB, 19.7 s) or prove (273 MB, 17.0 s) is expected (MODEL);  s1-b measures the RSS with the sandbox off (the 4 GB rule).

## 3. Core changes

None in the core (the types, the conv rules, the checker).  src/circuit.c (the lowering and the emitter of D2 to D4) and docs/IR.md change.  No change in halo2/, domain/ or formers/.

## 4. Derived operations and their laws

- The law of a pow5 row (ii-b):  b = (a + c5)^2 and c = b^2 * (a + c5), thus c = (a + c5)^5.  The arith poly of the row is 0 (ql, qr, qm, qc and qo are 0 on it), and the pow5 polys are 0 on each row where q5 = 0.
- The S-box law:  sbox5 (s + rc) = (s + rc)^5 = the c cell of the pow5 row with a = s and c5 = rc.  The `==` copy of the check `sbox5 (s + rc) == tRkJ` goes to the c cell.

## 5. Effect on examples

None.  examples/poseidon.glue (line 213, `sbox5 x = x * x * x * x * x`) and the other examples do not change:  the change is in the lowering.  The examples count stays 69.

## 6. Circuits

| Circuit | rows, k, copies (now) | (ii-b), MODEL | degree | proof B | change |
|---|---|---|---|---|---|
| poseidon | 990, 10, 1,698 | 750, 10, 1,138 | 3 to 4 | 1,536 to 1,344 | golden, witness, verifier, fixture |
| transfer | 1,979, 11, 3,396 | 1,499, 11, 2,276 | 3 to 4 | 1,536 to 1,344 | golden, witness, verifier, fixture |
| kyc | 1,982, 11, 3,402 | 1,502, 11, 2,282 | 5 | 1,760 to 1,824 | golden, witness, verifier, fixture |
| kycMerkle (depth 20) | 21,922, 15, 37,662 | 16,642, 15, 25,342 | 5 | 1,760 to 1,824 | golden, witness, verifier, fixture |

square, nonzero, poly and range do not change (D4;  `git restore` their fixtures after e2e).

## 7. New check codes

None.  A bad pow5 row fails the gate check of the satcheck (the current code;  s1 names it with `rg -n 'SAT_' src/circuit.c`).

## 8. Guest tests per slice

- s1:  the goldens of the 4 circuits (same-ir and the circuit check);  test/circuit/refuse.txt, 2 new rows (question (f)), with the existing test/gate.sh route extended (USER approved 2026-10-20):  a pow5 output c + 1 and, for (ii-b), a bad b (each fails the gate check).  expect.txt does not change (the values stay).
- s1 mutants (on a scratch copy of src/circuit.c, restored after each;  `make -B` for each rebuild;  each must give FAIL lines in `make -B check`, count them before KILLED):
  - m1:  c5 not written (c5 = 0 on each pow5 row;  expected:  killed by the goldens).
  - m2:  c5 removed from the memo key (two S-boxes with a different rc share one row;  expected:  killed by the goldens).
  - m3:  fill_advice does not write b (expected:  killed by the satcheck, the first poly fails, and by the golden witness).
  - m4:  the shared emission (pow5, q5 and c5 in each circuit;  expected:  killed by the square, nonzero, poly and range goldens).
- s1-b:  `make e2e` (the 4 verifiers and fixtures), `forge test --root contracts` (100 passed), `make e2e-ceremony`, `make rehearsal`, `make deployment-test`, the RSS of keygen and prove (sandbox off), the contracts/README.md rows.

## 9. Questions for the USER

- (a) RULED 2026-10-20:  accepted as recommended.  The variant is (ii-b), the 2-poly pow5 gate (D1).  Recommendation:  (ii-b).  Reasons:  the row and copy gain of (ii-a) (kycMerkle 16,642 rows, 25,342 copies);  the max degree stays 5, so kyc and kycMerkle keep their extended domain and quotient count, and poseidon and transfer go only to degree 4;  no new advice column.  (ii-a):  one poly, degree 6 in the 4 circuits, extended_k - k 3, keygen and prove can about double (MODEL).  (ii-c):  degree 6 and 1,760 rows more than (ii-b).
- (b) RULED 2026-10-20:  accepted as recommended.  The IR ruling:  M14 adds the IR form of D2 (the gate pow5, `ROW_POW5`, the fixed columns q5 and c5).  Recommendation:  yes.  Reasons:  O24 form (ii) is a new IR form (M9 ruling (h), the M13 COMMON rule);  the gate arith, the lookup and the columns stay;  the halo2 back end and the verifier generator are generic (no code change).
- (c) RULED 2026-10-20:  accepted as recommended.  The emission is per circuit (D4).  Recommendation:  yes.  Reasons:  4 verifiers change, not 8;  square, nonzero, poly and range stay byte for byte.
- (d) RULED 2026-10-20:  accepted as recommended.  The 4 new verifiers come from the current generator (D6):  only the VK constants, the quotient and permutation code and the gate expression change.  Recommendation:  yes.  Reasons:  no generator code change;  s1-b STOPs if a diff has other lines or a Forge test fails.
- (e) RULED 2026-10-20:  accepted as recommended.  The lowering pattern (D3):  only `x * x * x * x * x` of one affine operand cell lowers to pow5;  the memo key holds c5 and the cell;  T2 does not fold into pow5.  Recommendation:  yes.  Reasons:  the pattern is a lowering rule, not a conv rule (no proof irrelevance);  the other mul chains (the bit checks, the selects) stay arith.
- (f) RULED 2026-10-20:  accepted as recommended.  2 new refuse rows (a bad pow5 output;  a bad b).  Recommendation:  yes.  Reasons:  no current row refuses a bad S-box cell;  circuit refusals 24 to 26.
- (g) RULED 2026-10-20:  accepted as recommended.  The doc sites (D7):  docs/IR.md section 4, SPEC.md 7.1, SPEC.md 9 O24.  Recommendation:  yes.
- (h) RULED 2026-10-20:  accepted as recommended.  At the M14 close O24 is CLOSED:  the forms (i) to (iv) are DONE.  Recommendation:  yes.  Reasons:  each form of docs/M8-PLAN.md D4 and of M11 is built;  the hint-row note of REPORT (c) (kycMerkle 14,882 rows, k 14) is a new input-row form and goes to a new open item only if the USER asks for it.
- (i) RULED 2026-10-20:  accepted as recommended.  The fixtures and the README:  `make e2e` writes all 8 fixtures;  stage the 4 fixtures of the changed circuits and `git restore` the other 4;  the README rows of the 4 circuits come from one gas report.  Recommendation:  yes.
- (j) RULED 2026-10-20:  accepted as recommended.  The open items:  O23 (the USER target fields, the PPoT check of the ceremony file) stays a USER item;  a k 14 goal for kycMerkle is not in M14 (no variant of form (ii) gets it).  Recommendation:  yes.
- (k) RULED 2026-10-20:  accepted as recommended.  The slices of section 10.  Recommendation:  as listed.

## 10. Slices

- s1 (build):  src/circuit.c (D2 to D4), docs/IR.md section 4, SPEC.md 7.1;  the goldens of the 4 circuits;  refuse.txt (if (f));  the mutants m1 to m4.  Gate delta:  `make -B check` rc 0;  examples 69, circuits 9, same-ir 19 (no change);  circuit refusals 24 to 26 (if (f));  the other counts as the M13 close.  The rows, k and copies of the 4 goldens = section 6 exactly, else STOP.  The USER approved an extension of the existing test/gate.sh refusal route on 2026-10-20;  it adds no new harness.  The index against HEAD names only test/gate.sh, src/circuit.c (and src/circuit.h if its line-5 comment changes), the 8 goldens of the 4 circuits, refuse.txt, docs/IR.md, SPEC.md and the 3 M14 docs.
- s1-b (gates):  `make -B check`, `make check-clang`, `forge build --root contracts --sizes`, `make e2e` LAST (sandbox off);  the IR check:  after e2e only the 4 verifiers and the 4 fixtures of section 6 change, and the proof sizes = D5;  `git restore` the other 4 fixtures;  `forge test --root contracts` 100 passed;  `make e2e-ceremony` 9 rows;  `make rehearsal`;  `make deployment-test`;  the RSS of keygen and prove (sandbox off);  the contracts/README.md rows of the 4 circuits.  Gate delta:  forge test 100;  e2e 9 rows;  e2e-ceremony 9 rows.
- s2 (close):  SPEC.md 9 O24 (form (ii) DONE, the measured table, the O24 status of (h));  docs/PLAN.md M14 DONE;  docs/STATUS.md `### s2: close`.  Gate delta:  `make -B check` rc 0 (the control), no count change.
