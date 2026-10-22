# M17 plan:  depth 23 for kycMerkle at k 15

Status:  RULED 2026-10-21 (the USER accepts the recommendations (a) to (k) of section 9).  s1 staged 2026-10-21 (claude4):  depth 23, 16,913 rows, k 15, 26,801 copies (= D1).

Topic (USER pick, 2026-10-21, the answer to a question of the kickoff session):  depth 23 for the circuit `kycMerkle` at k 15 (16,913 rows, the MODEL of docs/M16-PLAN.md D5).  At the M16 close (1210e0a) `kycMerkle` has depth 22 at k 14 with 16,236 rows.  M16 ruling (k) says "a depth above 22 needs k 15 and a new USER request";  this pick is that request.

## 1. Sources

- docs/M16-PLAN.md D1 (the linear model), D5 (k 15 for depth 23;  the max depth at k 15 is 46) and D8 (the cost);  the rulings (a) to (k);  the mutants n1 to n3.  M17 uses its form.
- docs/M13-PLAN.md (the depth change 4 to 20 at k 15, the pinned SRS) and SPEC.md section 9 `Measured, M14` (kycMerkle at k 15:  16,642 rows;  keygen 188 MB, 20.1 s;  prove 202 MB, 27.5 s;  `verifyProof` 330,359 gas).
- docs/STATUS.md M16 `### s2: close` (the counts at the M16 close = the M15 close counts) and SPEC.md 7.1 `Measured, M16`.
- The probe:  ~/Documents/glue-lang-M17-kickoff-probe/ (run.sh, run.out, REPORT.md;  read only, no build).
- The generator:  line 21 of examples/kycMerkle.glue names ~/Documents/glue-lang-M16-s1-probe/run.sh (outside the repository;  `run.sh gen D OUT`).

## 2. The facts at HEAD (1210e0a, the probe)

- examples/kycMerkle.glue:  228 lines, 962,418 B;  line 228 (the circuit) has 940,525 chars.  The header lines 1, 2, 4, 9, 10, 14 and 21 name depth 22, 2^22 leaves, b0 to b21, s0 to s21, "the hash of level 21" and the M16 generator.  Line 228 ends with `m21s64k0 == root; d + min == attr; range8 d; sender == sender`.
- The circuit:  5 public binders (root, nf, scope, min, sender) and 3 + 2 x 22 = 47 private binders (attr, sk, d, b0 to b21, s0 to s21).
- The golden test/circuit/kycMerkle.circuit.json:  16,236 rows, k 14, 25,728 copies, 3 advice columns, 9 fixed columns, 1 instance column (= SPEC.md 7.1 `Measured, M16`).  The goldens have 4,438,959 B (circuit) and 3,122,072 B (witness).
- contracts/src/verifiers/kycMerkle.sol line 321:  the `// k` word 0x00..0e (k 14).  The fixture proof:  1,824 B.  contracts/README.md line 58:  kycMerkle 6,612 | 1,481,232 | 330,294.
- The depth sites (as M16):  the example (the header and line 228);  the 2 goldens;  test/circuit/expect.txt row 9 (609 chars);  test/circuit/refuse.txt rows 20 and 26 (633 chars each);  contracts/test/Verifiers.t.sol lines 289 to 291 (the path bits b0 to b21, the siblings s0=11 to s21=242, "depth 22") and line 312 (the message "the depth 22 tree");  SPEC.md section 4 line 1240 (the M16 sentence) and 7.1 lines 1432 to 1445 (`Measured, M16`;  "depth 23 needs k 15").
- No k site outside the docs, other than the verifier:  `rg -i 'max k|k 14|k=14'` over the code, the tests and the contracts (not docs/, SPEC.md, the example, the goldens and the fixtures) finds only the `// k` line of kycMerkle.sol.  The Makefile, src/, halo2/ and the scripts have no k parameter for kycMerkle;  k comes from the rows (the IR rule:  the least k with rows <= 2^k - 16).
- The SRS:  the SHA-256 of ~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs = the pin halo2/ceremony.sha256 (4cdb1688..0ab67b).  halo2/src/backend.rs `sized` (line 133) refuses a file with a k below the circuit k (line 134);  `downsized` (line 144) calls `params.downsize(k)` only when the file k is above the circuit k (line 148).  Thus at k 15 the file serves as it is.
- The k 15 facts at e23bc92 (M15 s1:  kycMerkle depth 20 at k 15):  the fixture proof has 1,824 B.  The M15 s1-b diff of kycMerkle.sol (e23bc92 to 8192f9d, k 15 to 14) has 5 domain lines (`// k` 0x00..0f to 0x00..0e, n_inv, omega, omega_inv, omega_inv_to_l) and 28 VK lines on each side.

## 3. Decisions (MODEL)

- D1 (the depth).  The depth of `kycMerkle` goes from 22 to 23 (question (a)):  2^23 = 8,388,608 leaves.  The model of docs/M16-PLAN.md D1 (exact at the M13 and M15 probes and at M16 s1):  rows(d) = 1,342 + 677 x d, copies(d) = 2,122 + 1,073 x d, merged hints(d) = 160 + 80 x d.  At depth 23:  16,913 rows, 26,801 copies, 2,000 merged hints.  The limit at k 14 is 16,368 rows (545 rows over);  the limit at k 15 is 32,752 rows (15,839 rows of margin).  Thus k 15.
- D2 (the IR rule).  A depth change is a new circuit text, not a new IR form (M13 D2, M16 D2).  k goes from 14 to 15 by the IR rule (the least k with rows <= 2^k - 16);  no k parameter changes.  The gate arith, the gate pow5, the lookup range8, the 3 advice columns, the fixed columns and the instance column stay.  Only the rows, k, the copies and the fixed values change.  No change in src/, in halo2/ or in the verifier generator.
- D3 (the circuit text).  The level 22 has the same form as the levels 0 to 21:  the bit b22, the sibling s22, the bit constraint b22 * b22 == b22, the two selects and the hash.  The hash of level 22 equals root (`m22s64k0 == root`).  The private binders go from 47 to 49.  The 5 public binders and their order do not change.  Line 228 grows by about 40,305 chars (the M16 step for one level) to about 980,830 chars (+4.3 %).
- D4 (the generator).  Questions (d) and (e).  The generator stays outside the repository (M13 ruling (c), M16 ruling (c)).  s1 ports ~/Documents/glue-lang-M16-s1-probe/run.sh to ~/Documents/glue-lang-M17-s1-probe/ against the depth-22 text at HEAD.  The M16 driver still assumes depth-20 input:  its tail guard (line 45) and root substitution (line 66) both name `m19s64k0`, and its control (lines 116 to 143) uses depth 20.  Change both the guard and substitution to `m21s64k0`, and all control generation, root evaluation, argument lists and row comparisons to depth 22.  Updating only the guard lets the depth-22 control pass but leaves the depth-23 circuit checking `m21s64k0 == root`, so the final level is not bound to root.  The control comes first:  call the internal raw `gen 22` before the M17 header rewrite, with the M16 header from HEAD, and require examples/kycMerkle.glue byte for byte;  regenerate the 2 goldens and the rows expect 9, refuse 20 and refuse 26 from that control.  The command-line `gen` branch applies `withheader` and needs header.txt, so it is not the raw control.  A difference in line 228 is a STOP.  Then change the target depth to 23, the header text (M17, depth 23, the M17 generator path) and the install values (the refusal rows, the root guards, the shape checks).  Before installing, require the depth-23 circuit to check `m22s64k0 == root` and not `m21s64k0 == root`;  otherwise STOP.  The bit vector of M13 repeats with period 20:  b22 = 1 (entry 2);  sibling i = 11 x (i + 1):  s22 = 253.
- D5 (k, the SRS and the proof).  k goes from 14 to 15 (MODEL).  The pinned k 15 SRS serves the k 15 circuit as it is (no downsize;  halo2/src/backend.rs lines 133 to 148);  halo2/ceremony.sha256 does not change.  The proof size stays 1,824 B:  it depends on the columns and the degree, not on the rows or k (measured:  1,824 B at k 15 at e23bc92 and at k 14 at HEAD).  The verifier changes in its VK constants (vk_digest, the fixed and the permutation commitments) and in the 5 domain lines (`// k` 0x0e to 0x0f, n_inv, omega, omega_inv, omega_inv_to_l).  The 5 new domain lines are the lines of e23bc92 (MODEL:  they depend on k only).  `make e2e-ceremony` gives max k 15 (was 14):  the k of the file.  A later kycMerkle above 32,752 rows needs k 16 and a new ceremony file (a USER fact).
- D6 (the contracts).  The 5 public inputs and their order do not change.  KycGate.sol, KycGate.t.sol, DeployVerifier.s.sol and rehearsal.sh do not change.  contracts/test/Verifiers.t.sol takes the new root, the text "depth 23" (lines 291 and 312) and the path comment b0 to b22, s0=11 to s22=253 (lines 289 and 290) (question (g)).
- D7 (the docs).  examples/kycMerkle.glue header lines 1, 2, 4, 9, 10, 14 and 21 (depth 23, 2^23 leaves, b0 to b22, s0 to s22, "the hash of level 22", the M17 generator path);  SPEC.md section 4 (one M17 sentence after the M16 text at line 1240);  SPEC.md 7.1 (a table `Measured, M17` for kycMerkle after the table `Measured, M16`, the M16 values in parentheses);  contracts/README.md (the kycMerkle gas row).  The M13 to M16 tables and the M16 sentence "Thus 22 is the max depth at k 14" stay as records.
- D8 (the cost, MODEL).  The domain and the extended domain double (k 14 to 15).  The M14 measures of kycMerkle at k 15 (depth 20, 16,642 rows) are the model:  keygen 188 MB, 20.1 s;  prove 202 MB, 27.5 s (M16 at k 14:  127 MB, 5.5 s;  150 MB, 8.4 s).  Thus the keygen time goes up about 3.7 times and the prove time about 3.3 times;  the RSS goes up about 1.5 and 1.35 times.  All values stay far below the 4 GB limit.  The `verifyProof` gas does not depend on the rows;  from M14 to M15 (k 15 to 14) it went down by 65 (330,359 to 330,294):  MODEL at k 15 330,359.  The deployment gas changes by a few hundred gas (the VK and the domain constants).  `make -B check` can take longer (M13 and M14 at k 15:  259 s to 463 s;  M16 at k 14:  97 s to 138 s).  s1-b measures each value (one sample, the sandbox off, the 4 GB limit).

## 4. Soundness

- The constraints of each level are the same as at depth 22.  One more level adds one bit constraint, two selects and one hash.  The root is the hash of level 22.  A depth-22 proof does not verify with the depth-23 verifier:  the VK and the domain change.
- No conv rule changes.  No rule makes two values equal because of their type:  proof irrelevance stays prohibited.  The hint merge of M15 (D14) applies to the S-box hints of the new level as to the old levels.
- The soundness of a level does not depend on the depth or on k.  The leaf count doubles (2^22 to 2^23).  The k 15 SRS is the pinned ceremony file (no new USER fact).

## 5. The code and the files

- No staged change:  src/, gen/, halo2/, docs/IR.md, contracts/src/KycGate.sol, contracts/script/, the Makefile, and the goldens, verifiers and fixtures of the other 7 circuits.  e2e regenerates their randomized proof fixtures temporarily (section 6).
- s1:  examples/kycMerkle.glue (the generator output), test/circuit/kycMerkle.circuit.json, test/circuit/kycMerkle.witness.json, test/circuit/expect.txt row 9, test/circuit/refuse.txt rows 20 and 26, SPEC.md section 4, docs/STATUS.md.
- s1-b:  contracts/src/verifiers/kycMerkle.sol and contracts/test/fixtures/kycMerkle.proof.json (both from `make e2e`), contracts/test/Verifiers.t.sol, contracts/README.md (the kycMerkle gas row), docs/STATUS.md.
- s2:  SPEC.md 7.1 (`Measured, M17`), docs/PLAN.md, this file (the Status line), docs/STATUS.md.

## 6. The expected changes (MODEL)

| Circuit | rows, k, copies (HEAD) | rows, k, copies (MODEL) | max degree | proof (B) | files |
|---|---|---|---|---|---|
| kycMerkle (depth 22 to 23) | 16,236, 14, 25,728 | 16,913, 15, 26,801 | 5 | 1,824 | the example, the golden, the witness, expect row 9, refuse rows 20 and 26, the verifier, the fixture, Verifiers.t.sol |
| square, nonzero, poly, range, poseidon, transfer, kyc | no change | no change | no change | no change | none |

If a golden or a verifier of another circuit changes, STOP.  `make e2e` uses OsRng for proofs, so the other 7 fixtures change in their `proof` and derived `calldata` fields.  Before restoring those 7 fixture paths, compare their `columns` and `instances` with HEAD and STOP on any difference outside `proof` and `calldata`.  After restoration, the staged scope must contain only the kycMerkle artifacts of section 5.

The goldens grow by about 186,481 B (circuit) and 130,121 B (witness) for one level (the M16 step):  MODEL at depth 23 about 4,625,440 B and 3,252,193 B.

The depth table (MODEL;  the limit at k 15 is 32,752 rows):

| depth | leaves | rows | k | copies | margin at k 15 (rows) |
|---|---|---|---|---|---|
| 22 (HEAD) | 4,194,304 | 16,236 | 14 | 25,728 | 16,516 |
| 23 | 8,388,608 | 16,913 | 15 | 26,801 | 15,839 |
| 24 | 16,777,216 | 17,590 | 15 | 27,874 | 15,162 |
| 32 | 2^32 | 23,006 | 15 | 36,458 | 9,746 |
| 46 | 2^46 | 32,484 | 15 | 51,480 | 268 |
| 47 | 2^47 | 33,161 | 16 | 52,553 | none (k 16) |

## 7. Gates and the expected delta

- s1:  `make -B check` exit 0 and `make check-clang` exit 0.  The counts do not change, because the same files change:  native circuit 11, path 46, face 35, HIT 44;  examples 70, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 27, satcheck refusals 6;  node regressions 6, printer 2.  The time can be longer at k 15 (D8).
- s1-b:  `forge build --root contracts --sizes`, then `make e2e` LAST (9 rows).  The IR check (question (i)):  only the kycMerkle verifier changes;  its non-VK lines are the 5 domain lines, and each new domain line equals the line of e23bc92;  the `// k` line goes to 0x00..0f.  Check and restore the other 7 fixtures as in section 6, then confirm that only the kycMerkle verifier and fixture remain changed.  `forge test --root contracts` 100 passed, `make e2e-ceremony SRS=FILE` 9 rows (max k 15), `make rehearsal`, `make deployment-test`, the gas report, the RSS.  FILE is the k 15 ceremony file of halo2/ceremony.sha256;  pass it explicitly, since the target has no SRS default.
- s2:  the control `make -B check` exit 0;  docs only.

## 8. Risks

- The cost at k 15 (D8):  the keygen and prove times go up about 3.3 to 3.7 times (MODEL from M14).  Question (b) asks the USER to accept this cost.
- The pinned SRS has k 15, the max k of the file.  After M17, a kycMerkle change above 32,752 rows needs a new ceremony file (k 16, a USER fact).  At depth 23 the margin is 15,839 rows.
- The gate times:  `make -B check` and `make e2e` can take longer at k 15 (M13 and M14:  259 s to 463 s for `make -B check`).  Run them in the background;  the poll cap is 240 s.
- The model was exact at the M13 and M15 probes and at M16 s1;  s1 measures the rows and k before it writes a golden.  If k is not 15, STOP.
- The copied generator may not give the HEAD text (the D4 control).  A difference in line 228 is a STOP.  The depth-22 control must pass before generating depth 23 or installing files.

## 9. Questions for the USER

- (a) The depth (D1).  Recommendation:  23.  Reasons:  the USER pick;  8,388,608 leaves;  the smallest step (one level, +677 rows);  the largest margin at k 15 (15,839 rows) for a later lowering change;  the smallest growth of the example and the goldens.  Other values at the same k:  24 to 46 (each level adds 677 rows, about 40 kB of text and about 317 kB of goldens);  32 (2^32 leaves, 23,006 rows, 9,746 rows of margin);  46 (the max, 268 rows of margin).
- (b) k 15 for kycMerkle (D2, D8):  the k 14 gain of M15 goes for kycMerkle.  M16 ruling (k) asks for a new USER request for a depth above 22;  this pick is that request.  Recommendation:  yes;  SPEC.md section 4 records it.  Reasons:  the USER pick names k 15;  the pinned SRS covers k 15;  the cost stays far below the 4 GB limit (D8).
- (c) The name stays `kycMerkle`, and the depth-22 circuit goes (no `kycMerkle22` next to it).  Recommendation:  yes.  Reasons:  M13 ruling (d) and M16 ruling (b);  KycGate, the deploy scripts, the rehearsal and the README name kycMerkle;  e2e stays 9 rows, with 8 verifiers and 8 fixtures.
- (d) The generator stays outside the repository (D4):  s1 ports the M16 s1 run.sh for depth-22 input, including both the tail guard and root substitution and all depth-22 control calls, before changing the target depth, header and install values.  Recommendation:  yes.  Reasons:  M13 ruling (c) and M16 ruling (c);  the rule "no new node/Python/shell harness";  the goldens, same-ir and the e2e row check the output;  the depth-23 root guard of D4 checks that the final level is bound to root.
- (e) The new private values (D4).  Recommendation:  b22 = 1 (the 20-entry bit vector of M13 repeats:  22 mod 20 = 2, entry 2 is 1) and s22 = 253 (sibling i = 11 x (i + 1)).  Reasons:  M16 ruling (d);  the hashes of levels 0 to 21 stay and only the root changes (MODEL).
- (f) The refusal rows.  Recommendation:  refuse.txt rows 20 (root + 1) and 26 (b7 = 2) take the new root and the new binders;  no new refusal row;  the code stays WITNESS_FAIL.  Reasons:  M16 ruling (e);  the circuit refusals stay 27.
- (g) contracts/test/Verifiers.t.sol (D6).  Recommendation:  it changes in s1-b, with the verifier and the fixture.  Reasons:  M16 ruling (g);  each commit then passes `forge test`.
- (h) The SRS (D5).  Recommendation:  keep the pinned k 15 file and its pin.  Reasons:  `sized` admits k 15;  no new USER fact;  the proof size at k 15 is measured (1,824 B at e23bc92).
- (i) The IR check of s1-b (D5).  Recommendation:  the phase1.sh scope check accepts the VK lines and exactly the 5 domain lines, and compares each new domain line with the line of e23bc92;  any other line is a STOP.  Reasons:  M16 had only VK lines (k stayed);  M15 had the 5 domain lines (k 15 to 14);  the comparison with e23bc92 tests the k 15 domain without a new harness.
- (j) The cost (D8).  Recommendation:  s1-b measures the keygen and prove RSS and time of kycMerkle (one sample each, the sandbox off), and one gas report gives the README row;  s2 puts them in `Measured, M17` with the M16 values in parentheses.  Reasons:  the 4 GB rule;  the form of M15 and M16.
- (k) The docs sites and the open items.  Recommendation:  as D7;  the M16 texts stay as records;  no new open item;  O23 stays a USER item;  a depth above 46 or a kycMerkle above 32,752 rows needs k 16 and a new ceremony file (a USER fact).  Reasons:  M17 adds no IR form and no rule.

RULED 2026-10-21:  the USER accepts the recommendation of each question, (a) to (k).  Thus the depth is 23, kycMerkle has k 15, the name stays `kycMerkle`, the generator stays outside the repository, b22 = 1 and s22 = 253, and Verifiers.t.sol changes in s1-b.

## 10. Slices

- s1 (build).  (1) The generator copy and the depth-22 control (D4).  (2) The depth-23 output in the s1 probe directory, then `langc circuit` and `langc witness` there:  the rows, k and copies against D1;  STOP if k is not 15;  record the measured tuple.  (3) The install:  examples/kycMerkle.glue, the 2 goldens, expect.txt row 9, refuse.txt rows 20 and 26, SPEC.md section 4.  (4) The mutants (each must give FAIL lines in `make -B check`;  count them before KILLED):  n1, the generator stops at depth 22 (the binders b22 and s22 are not known);  n2, the children of level 22 swap (the root differs:  WITNESS_FAIL);  n3, the bit constraint of level 22 goes (the golden differs).  (5) The gates `make -B check` and `make check-clang`.  Gate delta:  none (section 7).  One commit.
- s1-b (gates).  `forge build --root contracts --sizes` (the sandbox off);  Verifiers.t.sol (D6) before e2e;  `make e2e` LAST (the sandbox off);  the IR check (section 7, question (i));  check and restore the other 7 fixtures (section 6), then inspect the final scope;  `forge test --root contracts` (100 passed);  `make e2e-ceremony SRS=FILE` (9 rows, max k 15);  `make rehearsal`;  `make deployment-test`;  the gas report (the contracts/README.md kycMerkle row);  the RSS (D8).  Two phases:  phase1.sh to the scope report, a manual look at the scope, then phase2.sh.  One commit.
- s2 (close).  SPEC.md 7.1 `Measured, M17` (kycMerkle only;  the M16 values in parentheses);  docs/PLAN.md M17 DONE;  the Status line of this file;  docs/STATUS.md `### s2: close`;  the control `make -B check`.  Docs only, one commit.
