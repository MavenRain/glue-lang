# M15 plan:  k 14 for kycMerkle (the hint merge)

## Status

DONE 2026-10-21 (s1 e23bc92, s1-b 8192f9d, s2 close).  RULED 2026-10-21:  the USER accepts the recommendations (a) to (k) of section 9.  The USER picked the topic on 2026-10-21:  k 14 for kycMerkle.  The Merkle depth stays 20 (M13).  The USER rules section 9, (a) to (k);  question (b) is the ruling on the lowering rule.  Kickoff:  this plan, the docs/PLAN.md M15 row and docs/STATUS.md M15 `### kickoff`, on parent 8a09680.  Probe:  ~/Documents/glue-lang-M15-kickoff-probe/ (run.sh, run.out, census.py, census.json, gluecensus.py, rows.jq, model.py, REPORT.md).

## 1. Sources

- SPEC.md section 7.1 (lines 1326 to 1438:  item 1, the input rows;  item 5, `range8` on a binder;  item 6, the pow5 pattern;  the memo;  the affine fold T2) and the hint paragraph D14 (line 74).
- docs/IR.md sections 2 and 4 (the document has no row kind and no input-row text;  the gate set).
- docs/M14-PLAN.md (D1 to D8;  rulings (h) and (j)) and the M14 probe note on the hint-row form (~/Documents/glue-lang-M14-kickoff-probe/REPORT.md line 51).
- src/circuit.c:  lower_circuit (636 to 668), lower_check (418 to 450), the pow5 pattern and lower_value (316 to 394), memo_key (183 to 211), fold_affine (584 to 634), put_copies (729 to 748), pick_k and circuit_document (689 to 783), fill_advice (822 to 861), witness_checks (863 to 878), witness_hints (883 to 902), circuit_satcheck (1360 to 1397).
- halo2/src/backend.rs (lines 95 to 158:  the SRS read, `sized`, `downsized`) and halo2_proofs v0.3.0, rev 73408a1 (plonk/circuit.rs lines 2348 to 2381:  the blinding factors and the minimum rows).
- The probe REPORT.md, sections (a) to (g) and the main-loop checks.

## 2. Design calls

- D1 (the form).  Question (a).  The recommendation is H1, the hint merge:  a hint h whose one check is `sbox5 (s + rc) == h`, with a pow5 row on the other side, has no input row.  The cell of h is the c cell of the pow5 row.  The check then has the same cell on its two sides, so put_copies gives no copy (src/circuit.c line 744).  Each S-box saves 1 row and 1 copy:  kycMerkle 16,642 to 14,882 rows, k 15 to 14, copies 25,342 to 23,582 (MODEL).  The other forms (MODEL;  kycMerkle rows and k):  H2, the pow5 gate on the hint row (the M14 note:  b = s, a = the hint), 14,882, k 14, a new gate form;  H1g, the merge of each hint (S-box, MDS, constant and select hints), 10,530, k 14, and poseidon (10 to 9), transfer and kyc (11 to 10) change k too;  H1-mds, the merge of the MDS hints only, 12,352, k 14.
- D2 (the IR rule).  Question (b).  H1 adds no IR form.  docs/IR.md has no row kind and no text on input rows (`rg -n 'binder|hint|input row' docs/IR.md` gives no line).  The gates arith and pow5, the lookup range8, the 3 advice columns, the instance column and the fixed columns do not change.  H1 changes a lowering rule:  SPEC.md 7.1 item 1 and D14 of M6 s5 (src/circuit.c lines 636 to 637:  "a hint row is an input row too").  The kickoff text called H1 a new input-row form;  the probe shows that the IR document has no input-row form, so the ruling is on the lowering rule.  No conv rule changes:  the merge is a lowering rule (proof irrelevance stays prohibited).  No halo2/ code change and no verifier generator code change (as M14 D6).  H2 is a new gate form:  the permuted pow5 gate in place of the M14 gate, or a second gate with a new selector (10 fixed columns, +32 B of proof, MODEL).
- D3 (the merge conditions).  Question (d).  lower_circuit merges a hint h only when each of these is true:  (1) h is a hint, not a public or a private binder;  (2) exactly one check has h as one full side, and it is an `==` check;  (3) the other side of that check is the c cell of a ROW_POW5 row;  (4) h has no `range8` (its input row has qrange 0, src/circuit.c lines 439 to 441);  (5) no row at or before that pow5 row has the cell of h as an active operand, including the input of that pow5 row itself.  Else h keeps its input row (the current rule).  For example, `hint t := 1` checked by `sbox5 t == t` must keep its input row:  the pow5 row reads t itself, and merging it would make fill_advice read its own output before writing it.  The pass runs after the checks and before fold_affine.  It uses the row remap of fold_affine (the `at` map, lines 620 to 631) for the input rows that go and for the cells of the rows and the checks.  The merged check still pins its cell (lines 603 to 606), so T2 makes no new fold.  The check is the source of truth for the merged cell, not the hint term e.  At HEAD, 80, 160, 160 and 1,760 hints merge (poseidon, transfer, kyc, kycMerkle).  The other hints keep their rows:  the MDS hints, the 1, 2, 2 and 22 constant S-box hints `sbox5 (0 + rc0)` (their check is a shared const row) and the 40 select hints.  No hint is used before its check, and no S-box hint has 2 checks (probe (d)).
- D4 (the witness).  Question (c).  fill_advice writes the merged cell from the pow5 row (c = (a + c5)^5).  witness_hints still computes h from e (lines 883 to 902), and witness_checks compares that value with the merged cell:  WITNESS_FAIL when they differ, as now (lines 863 to 878).  Without this comparison, a hint term that is not equal to its check gives a witness and no refusal (probe risk 2).  In the satcheck, a wrong value in a merged cell fails the pow5 gate (SAT_GATE;  circuit_satcheck runs check_gates before check_copies, lines 1360 to 1397);  at HEAD the copy fails (SAT_COPY).
- D5 (k, the SRS and the proof).  kycMerkle goes from k 15 to k 14:  14,882 rows, 1,486 under the IR limit 16,368 (2^14 - 16, BLINDING_ROWS 16 at src/circuit.c line 27;  pick_k at line 693).  The halo2_proofs limit is larger (16,378;  probe (c)).  poseidon, transfer and kyc keep k (10, 11, 11).  The max Merkle depth (MODEL, (2^k - 16 - 1,342) / 677):  22 at k 14, 46 at k 15 (now 19 and 41).  The pinned SRS (k 15) serves k 14:  halo2/src/backend.rs `sized` refuses only a file with a k below the circuit k, and `downsized` calls `params.downsize(k)` (lines 132 to 150).  halo2/ceremony.sha256 does not change.  The degree does not change (poseidon and transfer 4, kyc and kycMerkle 5).  The proof sizes do not change (MODEL, model.py:  1,344, 1,344, 1,824 and 1,824 B);  the bytes of the 4 fixtures change.
- D6 (the contracts).  Question (f).  4 verifiers change (poseidon, transfer, kyc, kycMerkle):  the VK constants, and for kycMerkle also the k 14 domain constants.  The generator (halo2_solidity_verifier 9e99730) has no code change.  The 5 public inputs of kyc and kycMerkle and their order do not change (the input rows 0 to 4 and the instance copies stay);  KycGate.sol, DeployVerifier.s.sol and rehearsal.sh do not change.  contracts/README.md:  the rows of the 4 circuits from one `forge test --root contracts --gas-report` (sandbox off).  The `verifyProof` gas stays about the same (MODEL;  the verifier cost does not depend on the rows, M14 s1-b).
- D7 (the docs).  Question (g).  SPEC.md 7.1:  item 1 (a merged hint has no input row) and a new item (the hint merge, D3 and D4), with a table `Measured, M15` at the close.  The D14 paragraph of SPEC.md (line 74) gets one sentence:  the lowering can put h in the output cell of its check.  src/circuit.c lines 636 to 637 (the D14 comment).  docs/IR.md does not change (D2).  No new open item;  O23 stays open.
- D8 (the cost, MODEL).  kycMerkle at k 14 has 2^14 rows and an extended domain of 2^16 (half of now):  the keygen and prove RSS and time go down by about half (M14 s1-b, measured:  keygen 188 MB, 20.1 s;  prove 27.5 s).  s1-b measures them with the sandbox off (the 4 GB rule).  The goldens of the 4 circuits get smaller (fewer rows and copies).

## 3. Core changes

None in the core (the types, the conv rules, the checker).  src/circuit.c changes (the merge pass of D3, fill_advice and witness_checks of D4, the D14 comment);  src/circuit.h does not change (its header comment does not name the input rows).  No change in halo2/, domain/, formers/ or the Makefile.

## 4. Derived operations and their laws

- The merge law:  for a hint h with the one check `sbox5 (s + rc) == h`, the satisfying assignments on (s, h) are the same before and after the merge:  h = (s + rc)^5.  Before:  the pow5 row gives c = (s + rc)^5 and a copy gives h = c.  After:  h is c.
- The witness law:  the merged cell holds (s + rc)^5 (the pow5 row).  The value of e (witness_hints) must be equal to it, else WITNESS_FAIL (D4).
- The row law:  rows (after) = rows (now) - the merged hints;  copies (after) = copies (now) - the merged hints.  kycMerkle(d) = 1,342 + 677 d (MODEL;  now 1,502 + 757 d).

## 5. Effect on examples

One new example for the refusal of question (e):  a circuit with one hint whose term is not its S-box check (for example `hint t := s * s` and the check `sbox5 (s + 1) == t`).  s1 names the file (as examples/memoKey.glue and examples/foldKey.glue for refuse.txt rows 22 and 23).  Examples 69 to 70 (MODEL;  s1 records each count that the new file changes).  The other examples do not change:  the change is in the lowering.

## 6. Circuits

| Circuit | rows, k, copies (now) | H1, MODEL | degree | proof B | change |
|---|---|---|---|---|---|
| poseidon | 750, 10, 1,138 | 670, 10, 1,058 | 4 | 1,344 | golden, witness, verifier, fixture |
| transfer | 1,499, 11, 2,276 | 1,339, 11, 2,116 | 4 | 1,344 | golden, witness, verifier, fixture |
| kyc | 1,502, 11, 2,282 | 1,342, 11, 2,122 | 5 | 1,824 | golden, witness, verifier, fixture |
| kycMerkle (depth 20) | 16,642, 15, 25,342 | 14,882, 14, 23,582 | 5 | 1,824 | golden, witness, verifier, fixture |

The merged hints:  80, 160, 160 and 1,760.  square, nonzero, poly and range do not change (no pow5 row, no merged hint;  `git restore` their fixtures after e2e).  The new example of section 5 has no golden (a refusal only).

## 7. New check codes

None.  A hint term that is not equal to its check gives WITNESS_FAIL (D4);  a wrong merged cell in a witness document gives SAT_GATE (the pow5 gate).

## 8. Guest tests per slice

- s1:  the goldens of the 4 circuits (circuit and witness;  same-ir and the circuit check).  test/circuit/refuse.txt:  rows 24 and 25 (`satcheck poseidon c 280` and `b 280`) go to row 200, the first pow5 row after the merge (each still gives SAT_GATE);  one new WITNESS_FAIL row on the example of section 5 (circuit refusals 26 to 27).  expect.txt, same-ir.txt and the route test/gate.sh do not change.
- s1:  extend the existing test/circuit-regressions.c with one native regression using `def sbox5 : Field -> Field := fun x => x * x * x * x * x` and `circuit selfHint hint t := 1 := sbox5 t == t`.  Require a two-row circuit that retains the hint input row, successful witness generation and a passing satcheck (t = 1).  Native circuit regressions 9 to 10;  no new circuit-table entry.
- s1 mutants (on a scratch copy of src/circuit.c, restored after each;  `make -B` for each rebuild;  each must give FAIL lines in `make -B check`, count them before KILLED):
  - m1:  the remap is off by one for the rows after a removed input row (expected:  killed by the 4 goldens and the satcheck).
  - m2:  no ROW_POW5 test in D3 (3), so the MDS hints merge too (expected:  killed by the 4 goldens;  poseidon about 475 rows, not 670).
  - m3:  no comparison of the hint value in witness_checks (expected:  killed by the new WITNESS_FAIL row).
  - m4:  no `same` test in put_copies (line 744;  a copy of a cell to itself;  expected:  killed by the goldens, poseidon copies 1,058 to 1,138).
  - m5:  change "at or before" in D3 (5) back to "before", allowing the producing pow5 row to read its own merged c cell (expected:  killed by the selfHint native regression).
- s1-b:  `make e2e` (the 4 verifiers and fixtures), `forge test --root contracts` (100 passed), `make e2e-ceremony` (max k 14), `make rehearsal` (kycMerkle at k 14), `make deployment-test`, the RSS and time of keygen and prove (sandbox off), the contracts/README.md rows.

## 9. Questions for the USER

RULED 2026-10-21:  the USER accepts the recommendations (a) to (k):  H1, the hint merge of the pow5-checked S-box hints;  the lowering rule D14 changes (SPEC.md 7.1 items 1 and 7);  no IR form, no gate, no column and no halo2/ change.

- (a) The form (D1).  Recommendation:  H1, the hint merge for the pow5-checked S-box hints only.  Reasons:  it gets k 14 (14,882 rows, 1,486 under the limit);  it changes only the lowering, no gate and no column;  the constraint set is the same (section 4);  only kycMerkle changes k.  H2:  the same rows, but a new gate form and a new gate text in 4 verifiers.  H1g:  more rows go (kycMerkle 10,530, max depth 32 at k 14), but 4 circuits change k, and the MDS and select checks have more cases (a hint on the lhs, 2 checks);  a later milestone can take it if the USER asks.
- (b) The ruling on the lowering rule (D2).  Recommendation:  yes, M15 changes the lowering rule D14 (SPEC.md 7.1 item 1:  each hint has its own input row) for the hints of D3;  no IR form changes.  Reasons:  the IR document has no input-row form;  the halo2 back end and the verifier generator are generic;  the merge is a lowering rule, not a conv rule.
- (c) The witness rule (D4).  Recommendation:  yes.  Reasons:  it keeps WITNESS_FAIL for a hint term that is not equal to its check;  the check is the source of truth for the cell.
- (d) The merge conditions (D3), (1) to (5).  Recommendation:  yes.  Reasons:  each condition holds for each S-box hint of the 4 circuits (probe (d));  a hint that does not satisfy them keeps its input row, so a future program with an early use, a use in the producing pow5 row itself, or a `range8` hint stays correct.
- (e) The tests (section 8).  Recommendation:  refuse.txt rows 24 and 25 to row 200;  one new WITNESS_FAIL row with one new example (section 5), plus the selfHint native regression and m5 (section 8;  native circuit regressions 9 to 10).  Reasons:  no current row has a hint term that is not equal to its S-box check, so without the new row m3 is not killed.  The selfHint regression prevents a valid t = 1 witness from being refused by a self-read merge.  Circuit refusals 26 to 27, examples 69 to 70.
- (f) The contracts (D6).  Recommendation:  yes:  the 4 new verifiers from the current generator;  stage the 4 fixtures of the changed circuits and `git restore` the other 4;  the README rows of the 4 circuits from one gas report.  Reasons:  no generator code change;  s1-b STOPs if a verifier diff has lines other than the VK and the domain constants, or if a Forge test fails.
- (g) The doc sites (D7).  Recommendation:  SPEC.md 7.1 (item 1, the new item, the table `Measured, M15` at the close), one sentence in the SPEC.md D14 paragraph, src/circuit.c lines 636 to 637;  docs/IR.md does not change.
- (h) The SRS (D5).  Recommendation:  keep the pinned k 15 file and its pin;  e2e-ceremony and the rehearsal downsize it to k 14.  Reasons:  backend.rs `sized` and `downsized`;  no new USER fact.
- (i) The cost facts (D8).  Recommendation:  s1-b measures the kycMerkle keygen and prove RSS and time (sandbox off, one sample each);  s2 records them as measured facts.
- (j) The open items.  Recommendation:  O23 stays a USER item (the target fields, the PPoT check);  H1g, H1-mds and H2 are not open items (each needs a USER request);  M15 adds no open item.
- (k) The slices of section 10.  Recommendation:  as listed.

## 10. Slices

- s1 (build):  src/circuit.c (D3, D4, the D14 comment);  the goldens of the 4 circuits;  refuse.txt (rows 24 and 25, the new row);  the new example;  test/circuit-regressions.c (the selfHint regression);  SPEC.md 7.1 and the D14 paragraph;  the mutants m1 to m5.  Gate delta:  `make -B check` rc 0;  circuits 9, same-ir 19 (no change);  native circuit regressions 9 to 10;  examples 69 to 70, circuit refusals 26 to 27 (if (e));  the other counts as the M14 close, and s1 records each count that the new example changes.  The rows, k and copies of the 4 goldens = section 6 exactly, else STOP.  The index against HEAD names only src/circuit.c, the 8 goldens of the 4 circuits, refuse.txt, the new example, test/circuit-regressions.c, SPEC.md, docs/M15-PLAN.md and docs/STATUS.md.
- s1-b (gates):  `make -B check`, `make check-clang`, `forge build --root contracts --sizes`, `make e2e` LAST (sandbox off);  the IR check:  after e2e only the 4 verifiers and the 4 fixtures of section 6 change, the proof sizes = D5, and the kycMerkle verifier is at k 14;  `git restore` the other 4 fixtures;  `forge test --root contracts` 100 passed;  `make e2e-ceremony` 9 rows (max k 14);  `make rehearsal`;  `make deployment-test`;  the RSS and time of keygen and prove (sandbox off);  the contracts/README.md rows of the 4 circuits.  Gate delta:  forge test 100;  e2e 9 rows;  e2e-ceremony 9 rows.
- s2 (close):  SPEC.md 7.1 (the table `Measured, M15`);  docs/PLAN.md M15 DONE;  docs/M15-PLAN.md Status DONE;  docs/STATUS.md `### s2: close`.  Gate delta:  `make -B check` rc 0 (the control), no count change.
