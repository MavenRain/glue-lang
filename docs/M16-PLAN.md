# M16 plan:  a deeper Merkle tree for kycMerkle at k 14

Status:  DRAFT 2026-10-21 (the M16 kickoff, claude4).  The USER rules section 9, (a) to (k).

Topic (USER pick, 2026-10-21):  a deeper Merkle tree for the circuit `kycMerkle` at k 14 (the max depth 22, the MODEL of docs/M15-PLAN.md D5).  At the M15 close (81af21c) `kycMerkle` has depth 20 at k 14 with 14,882 rows.

## 1. Sources

- docs/M15-PLAN.md D5 (the max depth at k 14:  (2^k - 16 - 1,342) / 677 = 22) and SPEC.md 7.1 (the table `Measured, M15`).
- docs/M13-PLAN.md (the depth change 4 to 20:  D1 the depth, D2 the IR rule, D4 the generator outside the repository, D6 the contracts, D7 the docs, the rulings (a) to (k), the mutants n1 to n4).  M16 uses its form.
- docs/STATUS.md M15 `### s2: close` (the counts at the M15 close) and SPEC.md section 9 (O23 stays a USER item;  no open item names the depth).
- The probe:  ~/Documents/glue-lang-M16-kickoff-probe/ (run.sh, run.out, REPORT.md;  read only, no build).
- The generator:  line 21 of examples/kycMerkle.glue names ~/Documents/glue-lang-M13-s1-probe/run.sh (outside the repository, M13 ruling (c)).

## 2. The facts at HEAD (81af21c, the probe)

- examples/kycMerkle.glue:  228 lines, 881,808 B;  line 228 (the circuit) has 859,915 chars.  Lines 1 to 21 are the header (depth 20, 2^20 leaves, the bits b0 to b19, the siblings s0 to s19, "the hash of level 19", the generator path).
- The circuit:  5 public binders (root, nf, scope, min, sender) and 3 + 2 x 20 = 43 private binders (attr, sk, d, b0 to b19, s0 to s19).  Each level i has the bit constraint bi * bi == bi, the select of the two children and the hash poseidon([li, ri]).  The hash of level 19 equals root.
- The golden test/circuit/kycMerkle.circuit.json:  14,882 rows, k 14, 23,582 copies (= SPEC.md 7.1 `Measured, M15`).  The IR limit at k 14 is 16,368 rows (2^14 - 16).
- The Merkle depth is not a parameter of src/, halo2/ or the Makefile.  `rg -i depth` over src/ finds only the nest limits:  PARSE_MAX_DEPTH 1000 (src/front/parser.c line 22), CHECK_DEPTH_LIMIT 2000 (src/front/check.c line 13), EVAL_DEPTH_LIMIT 2000 (src/front/core.h line 17) and JSON_DEPTH_LIMIT (src/json.c line 341).  The depth is in the circuit text only.
- The sites that name the depth or the root of the tree:  examples/kycMerkle.glue (the header and line 228);  the goldens test/circuit/kycMerkle.circuit.json and kycMerkle.witness.json;  test/circuit/expect.txt row 9 (580 chars:  root, nf, scope, min, sender, attr, sk, d, b0 to b19, s0 to s19);  test/circuit/refuse.txt rows 20 (root + 1) and 26 (the true root and one bad value);  contracts/test/Verifiers.t.sol lines 288 to 312 (the comment, the check of the public inputs and the message "the root of the depth 20 tree");  SPEC.md section 4 line 1239 (`depth 20; k 15`, the M13 text) and the table headers `kycMerkle (depth 20)` in 7.1 and 9.
- No change:  contracts/src/KycGate.sol (the root is an anchor that the issuer approves), contracts/test/KycGate.t.sol (the root comes from the fixture), contracts/script/DeployVerifier.s.sol, contracts/script/rehearsal.sh and the Makefile (`NAME ?= kycMerkle` only).  contracts/README.md changes in the kycMerkle gas row only.

## 3. Decisions (MODEL)

- D1 (the depth).  The depth of `kycMerkle` goes from 20 to 22 (question (a)):  2^22 = 4,194,304 leaves.  The model is linear in the depth, from the goldens at HEAD ((kycMerkle - kyc) / 20;  a linear model of this form was exact at the M13 and M15 probes):  rows(d) = 1,342 + 677 x d, copies(d) = 2,122 + 1,073 x d, merged hints(d) = 160 + 80 x d.  At depth 22:  16,236 rows, k 14, 25,728 copies, 1,920 merged hints;  132 rows under the limit 16,368.
- D2 (the IR rule).  A depth change is a new circuit text, not a new IR form (M13 D2).  The gate arith, the gate pow5, the lookup range8, the 3 advice columns, the fixed columns and the instance column stay.  Only the rows, the copies and the fixed values change.  k stays 14.  No change in src/, in halo2/ or in the verifier generator.
- D3 (the circuit text).  The levels 20 and 21 have the same form as the levels 0 to 19:  the bits b20 and b21, the siblings s20 and s21, the bit constraints, the selects and the hashes.  The hash of level 21 equals root.  The private binders go from 43 to 47.  The 5 public binders and their order do not change.
- D4 (the generator).  Questions (c) and (d).  The generator stays outside the repository (M13 ruling (c)).  s1 ports the generation functions of ~/Documents/glue-lang-M13-s1-probe/run.sh into a fresh driver in ~/Documents/glue-lang-M16-s1-probe/, based on the depth-20 text at HEAD.  Bound the level-3 hint template by `hint l4`, and the level-3 constraint template by `b4 * b4 == b4`:  the M13 extraction runs to the end of the depth-4 text and would duplicate levels 4 to 19 at HEAD.  Rewrite the root reference from level 19 to the requested final level.  Replace the old depth-4 control, root guards, refusal-row numbers and shape checks with the M16 values before any install;  the full M13 driver writes repository files and is not reusable unchanged.  The control comes first:  raw output at depth 20 must give examples/kycMerkle.glue of HEAD byte for byte.  Record any intended header change separately;  a difference in line 228 is a STOP.  Extend the 20-entry witness-bit vector explicitly as in question (d) before root or witness generation;  otherwise b20 and b21 are empty.
- D5 (k, the SRS and the proof).  k stays 14 (MODEL).  The pinned k 15 SRS serves the k 14 circuit (halo2/src/backend.rs `sized` and `downsized`);  halo2/ceremony.sha256 does not change.  The proof size stays 1,824 B:  it depends on the columns and the degree, not on the rows.  The verifier changes in its VK constants only (vk_digest, the fixed and the permutation commitments);  the domain lines (`// k` 0x0e, n_inv, omega) do not change.  Depth 23 gives 16,913 rows and k 15 (the max depth at k 15 is 46, MODEL):  not in M16.
- D6 (the contracts).  The 5 public inputs and their order do not change.  KycGate.sol, KycGate.t.sol, DeployVerifier.s.sol and rehearsal.sh do not change.  contracts/test/Verifiers.t.sol takes the new root and the text "depth 22" (question (g)).
- D7 (the docs).  examples/kycMerkle.glue lines 1 to 21 (depth 22, 2^22 leaves, b0 to b21, s0 to s21, "the hash of level 21", the M16 generator path);  SPEC.md section 4 (one M16 sentence after the M13 text at line 1239);  SPEC.md 7.1 (a table `Measured, M16` for kycMerkle after the table `Measured, M15`);  contracts/README.md (the kycMerkle gas row).  The M13, M14 and M15 tables stay as records.
- D8 (the cost, MODEL).  k and the extended domain do not change.  Thus the keygen and prove RSS and time of kycMerkle stay near the M15 values (127 MB, 4.9 s;  167 MB, 6.6 s).  The `verifyProof` gas does not depend on the rows (M15):  it stays near 330,294.  The deployment gas changes by a few hundred gas (the VK constants).  s1-b measures each value (one sample, the sandbox off, the 4 GB limit).

## 4. Soundness

- The constraints of each level are the same as at depth 20.  Two more levels add two bit constraints, two selects and two hashes.  The root is the hash of level 21.  A depth-20 proof does not verify with the depth-22 verifier:  the VK changes.
- No conv rule changes.  No rule makes two values equal because of their type:  proof irrelevance stays prohibited.  The hint merge of M15 (D14) applies to the S-box hints of the new levels as to the old levels.
- The soundness of a level does not depend on the depth.  The leaf count goes up by a factor of 4 (2^20 to 2^22).

## 5. The code and the files

- No staged change:  src/, gen/, halo2/, docs/IR.md, contracts/src/KycGate.sol, contracts/script/, the Makefile, and the goldens, verifiers and fixtures of the other 7 circuits.  e2e regenerates their randomized proof fixtures temporarily (section 6).
- s1:  examples/kycMerkle.glue (the generator output), test/circuit/kycMerkle.circuit.json, test/circuit/kycMerkle.witness.json, test/circuit/expect.txt row 9, test/circuit/refuse.txt rows 20 and 26, SPEC.md section 4, docs/STATUS.md.
- s1-b:  contracts/src/verifiers/kycMerkle.sol and contracts/test/fixtures/kycMerkle.proof.json (both from `make e2e`), contracts/test/Verifiers.t.sol, contracts/README.md (the kycMerkle gas row), docs/STATUS.md.
- s2:  SPEC.md 7.1 (`Measured, M16`), docs/PLAN.md, this file (the Status line), docs/STATUS.md.

## 6. The expected changes (MODEL)

| Circuit | rows, k, copies (HEAD) | rows, k, copies (MODEL) | max degree | proof (B) | files |
|---|---|---|---|---|---|
| kycMerkle (depth 20 to 22) | 14,882, 14, 23,582 | 16,236, 14, 25,728 | 5 | 1,824 | the example, the golden, the witness, expect row 9, refuse rows 20 and 26, the verifier, the fixture, Verifiers.t.sol |
| square, nonzero, poly, range, poseidon, transfer, kyc | no change | no change | no change | no change | none |

If a golden or a verifier of another circuit changes, STOP.  `make e2e` uses OsRng for proofs, so the other 7 fixtures change in their `proof` and derived `calldata` fields.  Before restoring those 7 fixture paths, compare their `columns` and `instances` with HEAD and STOP on any difference outside `proof` and `calldata`.  After restoration, the staged scope must contain only the kycMerkle artifacts of section 5.

The depth table (MODEL;  the limit at k 14 is 16,368 rows):

| depth | leaves | rows | k | copies | margin at k 14 (rows) |
|---|---|---|---|---|---|
| 20 (HEAD) | 1,048,576 | 14,882 | 14 | 23,582 | 1,486 |
| 21 | 2,097,152 | 15,559 | 14 | 24,655 | 809 |
| 22 | 4,194,304 | 16,236 | 14 | 25,728 | 132 |
| 23 | 8,388,608 | 16,913 | 15 | 26,801 | none (k 15) |

## 7. Gates and the expected delta

- s1:  `make -B check` exit 0 (114 s to 135 s at k 14) and `make check-clang` exit 0.  The counts do not change, because the same files change:  native circuit 11, path 46, face 35, HIT 44;  examples 70, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 27, satcheck refusals 6;  node regressions 6, printer 2.
- s1-b:  `forge build --root contracts --sizes`, then `make e2e` LAST (9 rows).  The IR check:  only the kycMerkle verifier changes;  its diff is VK lines only;  the `// k` line stays 0x0e.  Check and restore the other 7 fixtures as in section 6, then confirm that only the kycMerkle verifier and fixture remain changed.  `forge test --root contracts` 100 passed, `make e2e-ceremony SRS=FILE` 9 rows (max k 14), `make rehearsal`, `make deployment-test`, the gas report, the RSS.  FILE is the existing k 15 ceremony SRS matching halo2/ceremony.sha256;  pass it explicitly, since the target has no SRS default.
- s2:  the control `make -B check` exit 0;  docs only.

## 8. Risks

- The margin at depth 22 is 132 rows.  The model was exact at the M13 and M15 probes, and s1 measures the rows before it writes a golden.  If the rows are above 16,368 (k 15), STOP:  the USER rules depth 21 or k 15.
- A later lowering change that adds 133 rows or more to kycMerkle moves it to k 15.  The pinned SRS covers k 15 (the max depth at k 15 is 46, MODEL).
- Line 228 grows by less than 10 % (2 more levels).  The nest limits do not apply:  the constraints are a sequence, not a nest (the depth-20 text checks at HEAD).  The parse and check times grow with the text (MODEL:  about linear).
- The ported generator may not give the HEAD text (the D4 control).  Record intended header changes separately;  a difference in line 228 is a STOP.  The depth-20 control must pass before generating depth 22 or installing files.

## 9. Questions for the USER

- (a) The depth (D1).  Recommendation:  22.  Reasons:  the USER pick names the max depth 22;  4,194,304 leaves;  k 14 stays;  the model was exact at M13 and M15;  s1 measures before it writes a golden.  Depth 21:  809 rows of margin and 2,097,152 leaves.  Depth 23 or more:  k 15 (the pinned SRS covers it, but the k 14 gain of M15 goes).
- (b) The name stays `kycMerkle`, and the depth-20 circuit goes (no `kycMerkle20` next to it).  Recommendation:  yes.  Reasons:  KycGate, the deploy scripts, the rehearsal and the README name kycMerkle;  e2e stays 9 rows, with 8 verifiers and 8 fixtures (M13 ruling (d)).
- (c) The generator stays outside the repository (D4).  Recommendation:  yes.  Reasons:  M13 ruling (c);  the rule "no new node/Python/shell harness";  the goldens, same-ir and the e2e row check the output.
- (d) The new private values (the bits b20 and b21, the siblings s20 and s21).  Recommendation:  retain the M13 values at levels 0 to 19 and repeat its 20-entry bit vector periodically, giving b20 = 0 and b21 = 1;  keep sibling i = 11 x (i + 1), giving s20 = 231 and s21 = 242.  Reasons:  the M13 generator has only 20 literal bits and no extension rule, so the new bits need an explicit rule.  The hashes of levels 0 to 19 stay and only the root changes (MODEL).
- (e) The refusal rows.  Recommendation:  refuse.txt rows 20 (root + 1) and 26 take the new root and the new binders;  no new refusal row;  the code stays WITNESS_FAIL.  Reasons:  the rows test the same faults at the new depth;  the circuit refusals stay 27.
- (f) A test for k.  Recommendation:  no new test.  Reasons:  the golden holds k 14;  e2e-ceremony prints the max k (14);  the s1-b IR check reads the `// k` line of the verifier.
- (g) contracts/test/Verifiers.t.sol (D6).  Recommendation:  it changes in s1-b, with the verifier and the fixture (M13 changed it in s1).  Reasons:  each commit then passes `forge test`:  at s1 the depth-20 verifier, fixture and test stay together.
- (h) The SRS (D5).  Recommendation:  keep the pinned k 15 file and its pin.  Reasons:  `downsized` serves k 14;  no new USER fact.
- (i) The docs sites (D7).  Recommendation:  as D7;  the M13 text of SPEC.md section 4 and the tables `Measured, M14` and `Measured, M15` stay as records.  Reasons:  the form of M13 and M15.
- (j) The cost (D8).  Recommendation:  s1-b measures the keygen and prove RSS and time of kycMerkle (one sample each, the sandbox off), and one gas report gives the README row.  Reasons:  the 4 GB rule;  the form of M15.
- (k) The open items.  Recommendation:  no new open item;  O23 stays a USER item;  a depth above 22 needs k 15 and a new USER request.  Reasons:  M16 adds no IR form and no rule.

## 10. Slices

- s1 (build).  (1) The generator port and the depth-20 control (D4), then the explicit witness-bit extension of question (d).  (2) The depth-22 output in the s1 probe directory, then `langc circuit` and `langc witness` there:  the rows, k and copies against D1;  STOP if k is not 14;  record the measured tuple.  (3) The install:  examples/kycMerkle.glue, the 2 goldens, expect.txt row 9, refuse.txt rows 20 and 26, SPEC.md section 4.  (4) The mutants (each must give FAIL lines in `make -B check`;  count them before KILLED):  n1, the generator stops at depth 21 (the binders b21 and s21 are not known);  n2, the children of level 21 swap (the root differs:  WITNESS_FAIL);  n3, the bit constraint of level 21 goes (the golden differs).  (5) The gates `make -B check` and `make check-clang`.  Gate delta:  none (section 7).  One commit.
- s1-b (gates).  `forge build --root contracts --sizes` (the sandbox off);  Verifiers.t.sol (D6) before e2e;  `make e2e` LAST (the sandbox off);  the IR check (section 7);  check and restore the other 7 fixtures (section 6), then inspect the final scope;  `forge test --root contracts` (100 passed);  `make e2e-ceremony SRS=FILE` (section 7;  9 rows, max k 14);  `make rehearsal`;  `make deployment-test`;  the gas report (the contracts/README.md kycMerkle row);  the RSS (D8).  Two phases:  phase1.sh to the scope report, a manual look at the scope, then phase2.sh.  One commit.
- s2 (close).  SPEC.md 7.1 `Measured, M16` (kycMerkle only;  the M15 values in parentheses);  docs/PLAN.md M16 DONE;  the Status line of this file;  docs/STATUS.md `### s2: close`;  the control `make -B check`.  Docs only, one commit.
