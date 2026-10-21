# M13 plan:  the kycMerkle depth

## Status

DRAFT 2026-10-20 (the M13 kickoff, claude4);  RULED 2026-10-20 (USER:  each question of section 9, (a) to (k), takes its recommendation).  Topic (ii) of the M13 kickoff (USER, RULED 2026-10-20):  a Merkle depth change for the circuit `kycMerkle`.  The topic text:  "under T2 depth 20 = 21922 rows, k 15, fits the pinned SRS;  the circuit, its e2e row and SPEC.md 8 change;  reverses M12 ruling (e)".  The USER ruled section 9, (a) to (k), on 2026-10-20:  the recommendations.  This file, the docs/PLAN.md M13 row and the docs/STATUS.md M13 `### kickoff` section go with the s1 commit.  Probe:  ~/Documents/glue-lang-M13-kickoff-probe/ (run.sh, the log run.out, REPORT.md, scratch/).

## 1. Sources

- docs/M12-PLAN.md D4 (the depth table) and ruling (e) (line 202:  "The Merkle depth stays 4 in M12").
- docs/M9-PLAN.md D2 (the Merkle credential circuit, depth 4) and docs/STATUS.md M9 `### s4` (the circuit text, the values, the goldens).
- SPEC.md section 4 (line 1238) and section 9 O24 (lines 1636 to 1700, the two depth tables).
- The M9 s4 generator ~/Documents/glue-lang-M9-s4-probe/probe.sh and the M13 probe generator ~/Documents/glue-lang-M13-kickoff-probe/run.sh (`gen`, `ren`, `rootof`, `args`).
- The ceremony file ~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs (k 15, the pin halo2/ceremony.sha256).

## 2. Design calls

- D1 (the depth).  The depth of `kycMerkle` goes from 4 to 20 (question (a)):  2^20 = 1,048,576 leaves.  The probe numbers:  rows 21,922, k 15, copies 37,662, public 5, fixed 7, lookups 1 (the model of M12 s3 exactly:  rows(d) = 5970 + (d - 4) x 997, copies(d) = 10254 + (d - 4) x 1713).  k 15 fits the ceremony SRS (k <= 15).  The max depth at k 15 is 30 (31,892 rows;  depth 31 needs 32,889 rows > 32,752).  Depth 8 (the check of the model):  9,958 rows, k 14, copies 17,106.
- D2 (the IR rule).  A depth change is a new circuit text, not a new IR form.  The probe:  the gate set, the 3 advice columns, the 7 fixed columns, the 1 instance column, the lookup and the IR fields stay;  only the rows, k, the copies and the fixed values change.  No new `RowKind`, no verifier generator change, no src/ change, no halo2/ change.  docs/IR.md does not change.
- D3 (the circuit text).  The level i (i = 4 to 19) is the level-3 text with the renames m2s64k0 to m(i-1)s64k0, m3tNkJ and m3sNkJ to mitNkJ and misNkJ, l3, r3, b3, s3 to li, ri, bi, si.  The binders b4 to b19 and s4 to s19 go after b3 and s3.  The root compare is `m19s64k0 == root`.  Lines 1 to 227 do not change, except the header (D7).  The probe:  `gen 4` gives examples/kycMerkle.glue byte for byte, and the rename of level 3 to level 2 gives the level-2 text byte for byte.
- D4 (the generator).  Question (c).  The recommendation:  no generator file in the repository.  The s1 slice runs the probe generator (a copy in the s1 probe dir) and STATUS records the command and the anchors.  The goldens, same-ir and the e2e row check the output.
- D5 (the values).  The path bits b0 to b3 = 0 1 1 0 (as the golden), b4 to b19 = 1 0 0 1 1 0 1 0 1 1 0 0 1 0 1 0;  the sibling si = 11 (i + 1);  the leaf cm = pos2 30 5;  nf, scope 9, min 18, sender, attr 30, sk 5, d 12 as the golden.  The root comes from `langc eval` (a `pos2` chain over the M9 s2 kycvals.lang);  76 digits, the probe row scratch/d20.expect-row.txt.  test/circuit/refuse.txt row 20 is the same row at root + 1.
- D6 (the contracts).  The 5 public inputs (root, nf, scope, min, sender) and their order do not change.  KycGate.sol, KycGate.t.sol (the root from the fixture), DeployVerifier.s.sol and rehearsal.sh do not change.  The depth-20 verifier has 49,890 B and 988 lines (the size of the depth-4 verifier at k 13);  58 lines change, all `mstore` constants.  The proof has 8,466 B (the fixture size).  contracts/test/Verifiers.t.sol lines 289 to 311 (the path comment, the root literal at 306, the message at 311) change.
- D7 (the docs).  examples/kycMerkle.glue lines 1 to 21 (depth 20, 2^20 leaves, b0 to b19, s0 to s19, "the hash of level 19");  SPEC.md section 4 line 1238 (`depth 20; k 15`);  SPEC.md 9 O24 (the reversal of M12 ruling (e) at line 1698;  the depth tables keep the model, a row "measured, M13" goes after them);  contracts/README.md line 58 (the kycMerkle numbers row).  "SPEC.md section 8" of M12 ruling (e) names no depth text:  section 8 (line 1425) is the empty `Host and target` template, and 2.10 (M8) has no depth text (question (e)).
- D8 (the cost).  The goldens grow:  kycMerkle.circuit.json 1,565,250 B to 5,792,505 B, kycMerkle.witness.json 1,162,441 B to 4,268,021 B, examples/kycMerkle.glue 248,297 B to 881,730 B (about +7.9 MB).  The probe times (sandbox on):  langc check 44.4 s, circuit 40.6 s, witness 34.8 s, satcheck 3.8 s, halo2 check 5.7 s, keygen 12.2 s, prove 17.8 s, verify 4.5 s.  `make -B check` and `make e2e` take about 60 s more each (an estimate).  RSS:  not measured in the probe (the sandbox `ps` poll gave 0);  M9 s4 measured 274 MB at k 15.  s1-b measures it with the sandbox off.

## 3. Core changes

None.  No change in src/, halo2/, domain/, formers/ or docs/IR.md (D2).

## 4. Derived operations and their laws

None new.  The level of D3 is the M9 s4 level:  `bi * bi == bi`, `li == cur + bi * (si - cur)`, `ri == si + bi * (cur - si)`, the hash poseidon([li, ri]).  The law of the root:  root = h19, h(-1) = cm, hi = pos2 h(i-1) si if bi = 0, pos2 si h(i-1) if bi = 1 (the `rootof` chain).

## 5. Effect on examples

- examples/kycMerkle.glue:  depth 20 (D3), the header (D7).  228 lines;  line 228 has 859,915 chars and 6,112 hints.
- No other example changes.  The examples count stays 69.

## 6. Circuits

| Circuit | rows | k | copies | verifier B | change |
|---|---|---|---|---|---|
| kycMerkle (HEAD, depth 4) | 5,970 | 13 | 10,254 | 49,890 | |
| kycMerkle (M13, depth 20) | 21,922 | 15 | 37,662 | 49,890 | the golden, the witness, the verifier constants, the fixture |

The 8 other circuits do not change (their goldens and verifiers stay;  `git restore` their fixtures after e2e).

## 7. New check codes

None.  The refusals of the depth-20 circuit use the current codes (WITNESS_FAIL, SAT_COPY).

## 8. Guest tests per slice

- s1:  the goldens test/circuit/kycMerkle.circuit.json and kycMerkle.witness.json at depth 20 (same-ir and the circuit check);  test/circuit/expect.txt row 9 (the D5 values);  test/circuit/refuse.txt row 20 (root + 1);  a new refuse row (question (g)):  b7 = 2 with the D5 root, WITNESS_FAIL (the bit check of level 7).  contracts/test/Verifiers.t.sol:  the root literal of D5.
- s1 mutants (on a scratch copy of the generator output;  each must give FAIL lines in `make -B check`, count them before KILLED):
  - n1:  the bit check `b12 * b12 == b12` removed (killed by the golden;  the refuse row of (g) does not catch a level other than 7).
  - n2:  the select of level 10 swapped (`l10 == s10 + b10 * (m9s64k0 - s10)` and the hint to match;  killed by the expect row:  the root changes).
  - n3:  the root compare on level 18 (`m18s64k0 == root`;  killed by the expect row and the golden).
  - n4:  the generator stops at depth 19 (rows 20,925;  killed by the golden and the expect row:  the binders b19 and s19 are not known).
- s1-b:  `make e2e` (the verifier and the fixture of kycMerkle), `forge test --root contracts` (100 passed, KycGate.t.sol with the new fixture), `make e2e-ceremony`, `make rehearsal`, `make deployment-test`.

## 9. Questions for the USER

- (a) The depth value is 20.  Recommendation:  20.  Reasons:  2^20 leaves;  k 15 fits the pinned ceremony SRS;  the probe ran the whole chain at depth 20 on the development SRS and on the ceremony file.  Depth 30 (the max at k 15) leaves no rows for a later change;  depth 16 (17,934 rows, k 15) has no cost gain in k.  RULED 2026-10-20:  the recommendation.
- (b) M13 reverses M12 ruling (e) ("The Merkle depth stays 4 in M12").  Recommendation:  yes.  Reasons:  the topic (ii) ruling;  SPEC.md 9 O24 line 1698 records the reversal.  RULED 2026-10-20:  the recommendation.
- (c) The generator stays outside the repository (D4).  Recommendation:  yes, (r1).  Reasons:  the rule "no new node/Python/shell harness";  gen/ holds embed.c only;  the goldens check the output.  (r2):  a script in the repository (a new shell tool).  (r3):  a surface form for a repeated level (a new IR or surface form:  a new USER ruling, not M13).  RULED 2026-10-20:  the recommendation.
- (d) The circuit name stays `kycMerkle` and the depth-4 circuit goes (no `kycMerkle4` beside it).  Recommendation:  yes.  Reasons:  KycGate, the deploy scripts, the rehearsal and the README name kycMerkle;  e2e stays 9 rows.  RULED 2026-10-20:  the recommendation.
- (e) The SPEC.md sites are section 4 (line 1238) and section 9 O24 (lines 1650 to 1698);  no text goes into section 8.  Recommendation:  yes.  Reasons:  section 8 is the empty `Host and target` template;  2.10 has no depth text.  RULED 2026-10-20:  the recommendation.
- (f) The e2e row and the fixture:  expect.txt row 9 and refuse.txt row 20 get the D5 values;  `make e2e` writes kycMerkle.sol and kycMerkle.proof.json;  `git restore` the 8 other fixtures.  Recommendation:  yes.  RULED 2026-10-20:  the recommendation.
- (g) A new refuse row:  b7 = 2 (WITNESS_FAIL).  Recommendation:  yes.  Reasons:  no current row refuses a non-boolean path bit;  circuit refusals 23 to 24.  RULED 2026-10-20:  the recommendation.
- (h) contracts/test/Verifiers.t.sol (the root literal) and contracts/README.md line 58 (the numbers row from the s1-b run) change.  Recommendation:  yes.  RULED 2026-10-20:  the recommendation.
- (i) The header of examples/kycMerkle.glue says depth 20 and names the generator (the s1 probe path).  Recommendation:  yes.  RULED 2026-10-20:  the recommendation.
- (j) The open items stay open:  O24 form (ii) (the degree-5 gate);  O23 (the USER target fields);  the RSS of the depth-20 chain is measured in s1-b (the 4 GB rule).  Recommendation:  yes.  RULED 2026-10-20:  the recommendation.
- (k) The slices of section 10.  Recommendation:  as listed.  RULED 2026-10-20:  the recommendation.

## 10. Slices

- s1 (build):  the depth-20 examples/kycMerkle.glue (the generator of D4 in ~/Documents/glue-lang-M13-s1-probe/), the header;  the goldens;  expect.txt row 9, refuse.txt rows 20 and 24 (if (g));  Verifiers.t.sol;  SPEC.md section 4;  the mutants n1 to n4.  Gate delta:  `make -B check` rc 0;  examples 69, circuits 9, same-ir 19 (no change);  circuit refusals 23 to 24 (if (g));  the other counts as the M12 close.  The index against HEAD names only examples/kycMerkle.glue, the 2 kycMerkle goldens, expect.txt, refuse.txt, Verifiers.t.sol, SPEC.md and the 3 M13 docs.
- s1-b (gates):  `make -B check`, `make check-clang`, `forge build --root contracts --sizes`, `make e2e` LAST (sandbox off);  the IR check:  after e2e only contracts/src/verifiers/kycMerkle.sol (58 constant lines) and contracts/test/fixtures/kycMerkle.proof.json change;  `git restore` the 8 other fixtures;  `forge test --root contracts` 100 passed;  `make e2e-ceremony` 9 rows;  `make rehearsal`;  `make deployment-test`;  the RSS of keygen and prove (sandbox off);  contracts/README.md line 58.  Gate delta:  forge test 100;  e2e 9 rows;  e2e-ceremony 9 rows.
- s2 (close):  SPEC.md 9 O24 (the reversal, the measured row);  docs/PLAN.md M13 DONE;  docs/STATUS.md `### s2: close`.  Gate delta:  `make -B check` rc 0 (the control), no count change.
