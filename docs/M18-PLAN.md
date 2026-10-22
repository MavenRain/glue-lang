# M18 plan:  O3 and O23, then the close

Status:  DRAFT 2026-10-22.  The USER rules section 9, (a) to (l).

Topic (USER pick, 2026-10-22):  "Let's do O23 and O3 and then conclude."  M18 addresses the open items O3 and O23 of SPEC.md section 9.  O3 closes after s1;  the PPoT part of O23 closes after s2, and its deployment target closes only after s3-b succeeds.  If the USER defers deployment, s3 does not run and the target part of O23 stays open at the close.  M18 is the last milestone:  after the M18 close, docs/PLAN.md names no milestone.

## 1. Sources

- SPEC.md section 9:  O3 (lines 1583 to 1586) and O23 (lines 1741 to 1750);  SPEC.md 2.3 line 100 (the CIRCUIT_UNCONSTRAINED row);  7.1 item 4 (lines 1363 to 1364).
- docs/M7-PLAN.md lines 116 to 117 ("a semantic check needs a rank or solver argument");  docs/M8-PLAN.md lines 126 to 128;  docs/M9-PLAN.md ruling (c) (lines 237 to 244) and D6 (Claude writes the scripts, the dry runs and the checks;  the USER broadcasts).
- contracts/README.md lines 88 to 91 (the ceremony file) and the runbook (lines 146 to 235).
- The probes:  ~/Documents/glue-lang-M18-kickoff-probe/o3/ (REPORT.md, probe.sh, probe.out, the probe circuits) and o23/ (REPORT.md, run.sh, run.out);  read only, no build in the repository.

## 2. The facts at HEAD (d1d2f22, the probes)

O3:
- src/circuit.c lower_circuit (lines 736 to 766) gives CIRCUIT_UNCONSTRAINED (lines 759 to 762) for a private binder or a hint whose flag `used` is 0.  Only lower_value sets the flag, at VAL_VAR (lines 372 to 377).  Thus a binder counts as constrained when the lowering of a constraint reaches it, also when its terms cancel or its row has a free output.  The rule runs in the lowering, before merge_hints and fold_affine;  `langc check` runs it too (src/main.c line 185, circuit_check_all).
- The probe circuits (one public p with `p == 1` and one private w;  check, circuit, witness and satcheck each give exit 0):  (a) `w == w`:  the compiler writes no copy (put_copies, lines 840 to 843), and w is in no row.  (b) `w - w == 0`:  one ARITH row ql = 1, qr = -1, with both operands copies of w;  the terms cancel.  (c) `w * 0 == 0`:  an ARITH row with all q 0 except qo;  w is no operand.  (d) `w + 1 == w + 1`:  the row memo gives the two sides one cell, thus no check copy;  the row c = w + 1 has a free output.  (f) `hint v := w := v == v`:  v has an INPUT row and no copy.  The control (e) `w * w == p` has the qm row and the copies.
- satcheck (circuit.c lines 1471 to 1508) has no binder concept and accepts each probe circuit.
- The native test check_reflexive_copy (test/circuit-regressions.c lines 88 to 108) expects `x == x` to lower with no copy:  it is an O3 instance that the tests accept.  check_self_hint (lines 55 to 86, `sbox5 t == t`) is not an O3 instance:  the pow5 row reads t.
- The polynomial form of T2:  Mono and Form (circuit.c lines 458 to 472, degree at most 2 over cells);  form_add keeps a merged monomial with q 0 (lines 502 to 506).
- The CIRCUIT_UNCONSTRAINED rows:  test/check/expect.txt lines 31 and 145 (the format `FILE CODE`);  test/circuit/refuse.txt has none.

O23:
- SPEC.md O23:  the five USER facts (the network, the chain id, the RPC URL, the explorer with its verify endpoint, the keystore account name) came as the word VALUE (2026-10-19);  no value is in a repository file;  the PPoT check of the Axiom copy is a USER item.
- No code, Makefile or script variable holds VALUE.  The runbook (contracts/README.md lines 152 to 156) names the facts as the shell placeholders NETWORK, CHAIN_ID, RPC_URL, EXPLORER, VERIFY_URL and ACCOUNT.  The Makefile has RPC_URL (no default) and CHAIN_ID ?= 31337.
- The runbook commands:  `make e2e-ceremony SRS=FILE` (line 161), `make rehearsal` (170), `forge script` of DeployVerifier.s.sol for kycMerkle without and then with `--broadcast` (180 to 183), the same for DeployKycGate.s.sol (193 to 198), `make deploy-check` (205), `forge verify-contract` (219 to 223), the admission by `cast call` and `cast send` (232 to 235).  DeployKycGate `run(verifier, issuer, scope, min, anchor)` (DeployKycGate.s.sol lines 18 to 26):  the issuer, the scope, the min and the anchor are USER values too.
- contracts/script/check.sh (`make deploy-check`) makes read calls only:  `cast chain-id`, `cast code` (the keccak = the ceremony artifact) and `cast call`.
- The ceremony file:  https://axiom-crypto.s3.amazonaws.com/challenge_0085/kzg_bn254_15.srs, the Axiom copy of the perpetual powers of tau (PPoT) challenge 0085 in the halo2 KZG layout (PSE halo2 v0.3.0);  4,194,564 B;  the pin halo2/ceremony.sha256 (4cdb1688..0ab67b).  The Axiom conversion method is not recorded.  halo2/src/backend.rs read_pinned (lines 113 to 130) checks the SHA-256 and k only.
- 4,194,564 B = 4 (k) + 2 x 2^15 x 64 (g and g_lagrange) + 2 x 128 (g2 and s_g2).

## 3. Decisions (MODEL)

- D1 (the O3 rule).  The rule moves from the term walk to the final rows (after merge_hints and fold_affine), because the rows are what the proof checks.  (1) The copy classes:  a union-find over the copies, with the instance cells.  Preserve the final cell of each private binder and hint through merging and folding, and mark every class that contains a binder or an instance cell.  (2) The row forms:  each gate polynomial gives a separate equation over the class of each cell, with the selector values of docs/IR.md section 4;  a pow5 row gives two equations.  Equal monomials merge, and a monomial with coefficient 0 goes (the cancellation).  A lookup row (range8) gives an occurrence of its cell class.  (3) The elimination:  a class that holds no binder cell, no instance cell and no lookup occurrence, and that has an occurrence in exactly one remaining equation, linear with a constant non-zero coefficient, goes with that equation (the class takes the value that solves it).  Repeat until no class goes.  Other equations of the same row stay.  (4) For each private binder and hint, resolve its final copy class:  CIRCUIT_UNCONSTRAINED applies only if that class holds no instance cell and has no occurrence in any remaining equation or lookup.  A copy to an instance cell constrains the binder even with no gate occurrence.
- D2 (the soundness of D1).  Each elimination keeps the set of the values of the other classes that satisfy the equations (the projection).  A refused class has no instance cell, so changing its value does not change the public values.  Given a satisfying assignment, every value of that class extends to a satisfying assignment of the original equations;  binders in the same class vary together, and eliminated cells take their solving values.  D1 does not refuse a constrained binder (MODEL).  In particular, `w == p` and a hint `h == p`, for public p, stay accepted even when the copy class has no gate occurrence.  D1 is not complete:  `w * h == 0` with a free hint h passes (h = 0), and a binder that only a hash ties to a free hint can pass.  A complete check needs a solver (docs/M7-PLAN.md lines 116 to 117).
- D3 (the probe forms under D1).  (a) The class of w has no occurrence:  refused.  (b) The terms of w cancel:  refused.  (c) w is in no row:  refused.  (d) The class of c = w + 1 has one linear occurrence and no binder, thus it goes with its row, and then w has no occurrence:  refused.  (f) v has no occurrence:  refused.  (g) `hint c := w := c == w` puts c and w in one class with no gate occurrence:  refused.  (e) The qm row of w stays:  accepted.
- D4 (the scope).  Public binders stay out of D1:  the instance column binds them, and CIRCUIT_UNUSED_PUBLIC stays the syntactic rule (kyc and kycMerkle have `sender == sender`, SPEC.md lines 1147 to 1148).  The code stays CIRCUIT_UNCONSTRAINED;  the message names the binder and says that no row constrains it after the cancellation.  No IR form, no new row kind, no conv rule.  The goldens do not change (D1 only refuses).
- D5 (the O3 tests).  Six new rows in test/check/expect.txt, with the files test/check/*.glue for (a), (b), (c), (d), (f) and (g):  check refusals 157 to 163.  check_reflexive_copy expects CIRCUIT_UNCONSTRAINED.  Four new native check_source acceptance cases use `circuit copyBound public (p : Field) private (w : Field) := w == p`, `circuit hintBound public (p : Field) hint h := p := h == p`, `circuit constantBound private (w : Field) := w == 1`, and `circuit linearBound public (p : Field) private (w : Field) := w + 1 == p` (native circuit 11 to 15).  These cover instance binding without a gate, hint instance binding, copy propagation to a constant row, and binder protection during elimination.  If an example of examples/ or a golden gives a new refusal, STOP:  it is an O3 instance in a USER program (a new USER ruling).
- D6 (the PPoT check).  A new subcommand `glue-halo2 srs-check` (Rust, halo2/) compares the ceremony file with 2 byte ranges of the PPoT challenge 0085:  the first 2^15 G1 powers of tau (offset 64, 2,097,152 B) and the first 2 G2 powers (256 B, at the G2 offset of the challenge layout).  It converts each point to the halo2 form and compares g, g2 and s_g2;  it computes g_lagrange from g and compares;  it checks the powers with one batched pairing equation (a random linear combination:  e(sum r_i g_(i+1), g2) = e(sum r_i g_i, s_g2)).  A Makefile target `make srs-check SRS=FILE PPOT=DIR`.  Rust `#[test]`s on the dev SRS:  a matched extract passes, a changed byte refuses.  The full-file BLAKE2b hash of the challenge (a file of about 100 GB) is out of scope:  the check trusts the PPoT host for the bytes that it serves.
- D7 (the fetch).  2 HTTP range requests (about 2 MiB) to the PPoT host, read only;  the extract stays outside the repository (~/Documents/glue-lang-M18-s2-probe/ppot/), and docs/STATUS.md records its SHA-256.  s2 confirms the host URL and the G2 offset from the PPoT documents before the fetch.
- D8 (the deployment).  The USER facts:  the network, the chain id, the RPC URL, the explorer and its verify URL, the keystore account name, and the KycGate values issuer, scope, min and anchor.  Claude makes read calls only:  `cast chain-id` against the RPC URL, `make e2e-ceremony`, `make rehearsal CHAIN_ID=...` (anvil), `forge script` without `--broadcast` (`--sender` the USER address, no key), and after the broadcast `make deploy-check`.  The USER runs the 2 broadcasts and `forge verify-contract`.  contracts/README.md gets a table `Deployments` (the network, the chain id, the verifier and the gate addresses, the 2 transaction hashes, the ceremony pin);  the RPC URL and the account name stay out of the repository.
- D9 (the close).  SPEC.md section 9:  O3 closed after s1 (D1, with the D2 limit as a stated limit);  the PPoT part of O23 closed after s2 (D6).  Mark O23 fully closed only after s3-b has checked and recorded the live deployment (D8).  If the USER defers deployment, close M18 with the target part of O23 explicitly open;  do not claim that O23 is closed.  docs/PLAN.md:  the M18 row DONE with the actual O23 status, and the line "No milestone after M18 (USER, 2026-10-22)".

## 4. Soundness

D1 is a refusal rule, not a conv rule:  no value is equal to another because of its type (proof irrelevance stays prohibited).  D2 makes each refusal a true free binder.  The check stays the source of truth for a merged cell (D1 reads the merged rows).  D6 adds a check of the SRS file and does not change keygen, prove or the verifier generator.

## 5. The code and the files

- s1:  src/circuit.c (the D1 pass after fold_affine, in lower_circuit;  no new global, an exhaustive switch over the row kinds, NULL on a diagnostic);  test/check/ (6 files and 6 rows);  test/circuit-regressions.c (check_reflexive_copy and the four D5 acceptance cases);  SPEC.md 2.3 line 100, a D1 text in 2.2 or 7.1, and O3 in section 9.
- s2:  halo2/src/ (the srs-check subcommand and its tests), the Makefile (the target srs-check), contracts/README.md (the ceremony paragraph), SPEC.md O23 (the PPoT part).
- s3:  no code;  contracts/README.md `Deployments`, SPEC.md O23.

## 6. The expected changes (MODEL)

| Slice | files | gate delta |
|---|---|---|
| s1 | src/circuit.c, test/check/ (6 new files), test/check/expect.txt, test/circuit-regressions.c, SPEC.md | check refusals 157 to 163;  native circuit 11 to 15;  the rest no change |
| s2 | halo2/src/, Makefile, contracts/README.md, SPEC.md | the halo2 Rust tests (s2 counts them);  `make srs-check` exit 0 on the pinned file;  the rest no change |
| s3 | contracts/README.md, SPEC.md | none |

No golden, verifier or fixture changes in M18.  If one changes, STOP.

## 7. Gates and the expected delta

As in M17 (docs/M17-PLAN.md section 7):  `make -B check`, `make check-clang`, `forge build --root contracts --sizes`, `make e2e` LAST, `forge test --root contracts` (100 passed).  s1 and s2 run each gate;  s2 also runs the halo2 Rust tests and `make e2e-ceremony`;  s3 runs `make e2e-ceremony`, `make rehearsal` and `make deployment-test`.

The s1 mutants (each must give FAIL lines):  u1, no cancellation (a monomial with q 0 stays):  (b) passes;  u2, no elimination:  (d) passes;  u3, no copy classes:  copyBound, hintBound and constantBound give false refusals;  u4, a binder class can go during elimination:  linearBound gives a false refusal.  Form (g) has no gate occurrence even before elimination or class merging, so it does not distinguish u2 or u3.  Keep (g) as a refusal regression.

The s2 mutants:  p1, the compare of g skips one point;  p2, the pairing equation uses g2 in place of s_g2.  For p1, change just that point in the PPoT extract to another valid subgroup point, leaving the candidate SRS unchanged:  the ordinary checker must refuse the mismatch, and p1 must miss it (the pairing and g_lagrange checks use the unchanged candidate SRS).  A malformed encoding that already fails decoding does not kill p1.  For p2, the matched dev extract and SRS must pass the ordinary checker and fail p2 (use a non-unit dev tau).  Also test a candidate with two unequal non-generator G1 powers swapped, its g_lagrange recomputed, and a matching extract:  all comparisons pass but the ordinary pairing check must refuse.  Record each mutant's failing test and exit status.

## 8. Risks

- An example or a golden gives a new refusal under D1 (an O3 instance in a USER program, or a D1 defect):  STOP and report.
- The cost of D1 at kycMerkle (16,913 rows, 26,801 copies):  linear in the rows and the copies (MODEL);  s1 measures `langc check` (M17 s1:  36 s).
- The PPoT host, its URL or the G2 offset is not as D6 says, or the host refuses a range request:  STOP (a USER fact).
- The halo2 point layout (the Montgomery form, the byte order) is not as D6 says:  s2 reads the halo2curves source in the cargo registry before the compare.
- The USER does not give the deployment facts:  s3 waits, and O23 stays open for the target only.

## 9. Questions for the USER

- (a) The O3 rule (D1).  Recommendation:  yes, the row form with the copy classes, the cancellation and the elimination.  Reasons:  the rows are what the proof checks;  D1 refuses each probe form (a) to (d), (f) and (g) (D3);  D2 gives no false refusal.
- (b) The code and the message (D4).  Recommendation:  keep CIRCUIT_UNCONSTRAINED, with a new message.  Reasons:  the same meaning;  no new code in SPEC.md 2.3.
- (c) Public binders (D4).  Recommendation:  out of D1;  CIRCUIT_UNUSED_PUBLIC stays.  Reasons:  the instance column binds a public value;  `sender == sender` (O22) stays legal.
- (d) O3 closes with the D2 limit as a stated limit (a sound check, not a complete check).  Recommendation:  yes.  Reasons:  a complete check needs a solver (M7);  the limit names the residue.
- (e) The O3 tests (D5).  Recommendation:  six check refusal rows (157 to 163), the changed check_reflexive_copy, and the four native acceptance cases (11 to 15).  Reasons:  tests in the guest language;  the native test is an O3 instance today;  the acceptance cases protect constrained binders from false refusals.
- (f) The PPoT check scope (D6).  Recommendation:  the 2 ranges, the conversion, the g_lagrange compare and the batched pairing;  no full-file hash.  Reasons:  about 2 MiB in place of about 100 GB;  the pairing equation shows that the file holds the powers of one tau.
- (g) The PPoT check code (D6).  Recommendation:  the Rust subcommand srs-check, its Rust tests and the Makefile target.  Reasons:  the development language of halo2/;  no new shell, node or Python harness.
- (h) The fetch (D7).  Recommendation:  s2 makes the 2 range requests with the host allowed (read only), and the extract stays outside the repository.  Alternative:  the USER runs the 2 curl commands that s2 prints.
- (i) The deployment facts (D8).  Recommendation:  the USER gives them at the start of s3, not in this plan:  the network, the chain id, the RPC URL, the explorer and the verify URL, the account name, the issuer, the scope, the min and the anchor.
- (j) The live RPC use (D8).  Recommendation:  read calls only by Claude (`cast chain-id`, `cast code`, `cast call`, `forge script` without `--broadcast`);  the USER runs the broadcasts and `forge verify-contract`.  Reasons:  M9 D6;  the verify call publishes to an external service.
- (k) The deployment record (D8).  Recommendation:  contracts/README.md `Deployments` with the network, the chain id, the 2 addresses, the 2 transaction hashes and the pin;  no RPC URL and no account name.
- (l) The slices and the close (section 10, D9).  Recommendation:  s1 (O3), s2 (the PPoT check), s3 (the deployment:  the read checks, the USER broadcast, then s3-b with the deploy-check and the record), s4 (the close of M18 and of glue-lang).  If the USER does not deploy, s3 does not run, and O23 stays open for the target only.

## 10. Slices

- s1 (O3).  The D1 pass in src/circuit.c;  the 6 probe files in test/check/ with 6 rows;  check_reflexive_copy and the four D5 acceptance cases;  SPEC.md 2.3, the D1 text and O3;  the mutants u1 to u4;  the gates (section 7);  the `langc check` time at kycMerkle.  One commit.
- s2 (the PPoT check).  The host URL and the layout from the PPoT documents;  the 2 range requests;  srs-check, its tests and the Makefile target;  `make srs-check` on the pinned file;  the mutants p1 and p2;  the gates.  One commit.
- s3 (the deployment).  With the USER facts:  `cast chain-id`, `make e2e-ceremony`, `make rehearsal CHAIN_ID=...`, `forge script` without `--broadcast`.  Then the USER broadcasts the verifier and the gate.  s3-b:  `make deploy-check`, the `Deployments` table, SPEC.md O23.  One commit (s3-b).
- s4 (close).  After s1 and s2, and either successful s3-b or the USER's decision to defer deployment:  SPEC.md section 9 (O3 closed;  O23 closed only after s3-b, otherwise the target part explicitly stays open);  docs/PLAN.md M18 DONE with the actual O23 status and "No milestone after M18";  this Status line;  docs/STATUS.md `### s4: close`;  the control `make -B check`.  Docs only, one commit.
