# M10 plan: the sender binding (O22), the `kycNullifier` name, the Poseidon estimates

Status:  RULED 2026-10-20 (the USER took each recommendation of section 9,
the message `(a) .. (j) ..` of the s1 session);  s1 BUILT 2026-10-20 (the
circuits, the goldens, the gate, the tests, the verifiers and the fixtures;
docs/STATUS.md `### s1`).  docs/PLAN.md has the M10 row (PLANNED
2026-10-20, the scope of ruling (a)).  The plan file goes with the M10 s1
commit.  The kickoff probes ran on
scratch copies of the examples with the real build/langc (the runner
~/Documents/glue-lang-M10-kickoff-probe/run.sh, the log run.out);  no
repository file changed.

## 1. Sources

- docs/STATUS.md `## M9` `### s7: close (2026-10-19)`:  the open items at
  the M9 close (O22, O23, the point name `kycNullifier`, the Poseidon forms
  (ii) and (iii), O3, O12 (c), O13, O14, O15, O16, O19).
- SPEC.md 9:  O22 (the sender binding) and O23 (the deployment target);
  SPEC.md 4:  the M9 bullet.
- docs/M9-PLAN.md D8 and ruling (h):  the Poseidon forms are M10 or later.
- contracts/src/KycGate.sol `admit` (lines 88 to 100):  the instance order
  anchor, nf, scope, min;  the count check `instances.length != 4`;  the
  event `Admitted(nf, anchor, msg.sender)`.
- contracts/src/verifiers/kyc.sol line 220:  `num_instances` 4 in the vk
  block;  line 227:  the count check;  lines 232 to 242:  each instance is
  loaded and absorbed into the transcript.  kycMerkle.sol has the same shape (988 lines each).
- test/circuit/expect.txt rows 8 and 9 (the kyc and kycMerkle circuits
  rows, also the e2e rows:  Makefile `E2E_TABLE`);  test/circuit/refuse.txt
  rows 19 and 20 (WITNESS_FAIL).
- contracts/test/Verifiers.t.sol (KycVerifierTest line 261,
  KycMerkleVerifierTest line 287:  the `instances[3] == 18` checks);
  contracts/test/KycGate.t.sol (11 tests, `fixture()` parses
  `.instances`);  contracts/script/check.sh (lines 66 to 71:  `cast call
  admit` with the fixture instances);  contracts/script/rehearsal.sh line
  74 and contracts/test/deployment.sh line 56 (the anchor from
  `.instances[0]`);  contracts/README.md lines 20, 39, 43 and 126 (the
  four public inputs).
- domain/domain.lang line 68:  `family KycSpec := credential |
  kycNullifier | predicate | ..`;  the comment lines 64 and 66.
- The kickoff probe (section 2, D1 and D2).

## 2. Design calls

D1.  The sender binder (questions (b) and (j)).  O22:  `KycGate.admit`
records the nullifier and emits `msg.sender`, but the proof binds no
sender, thus a relayer that sees a proof first can spend the nullifier.
The fix has two parts.  The circuit gets a fifth public binder `sender :
Field` after `min`, so the instance order is anchor, nf, scope, min,
sender.  The gate checks `instances[4]` against `msg.sender`.  The value
is the 20-byte address as an unsigned integer:  160 bits, always below r
(254 bits), thus no range row and no hash.  The gate passes
`uint256(uint160(msg.sender))`.  The proof binds the value:  the halo2
transcript absorbs each instance value (kyc.sol line 232), thus a proof
made for one sender fails the verifier with another sender in
`instances[4]`.  The constraint row (D2) satisfies the language rule that
each public binder is in a constraint (CIRCUIT_UNUSED_PUBLIC, probe kycU
below);  it does not restrict the sender value (a witness with `sender +
1` passes:  the gate restricts it).  The name `sender` is free:  `langc
check` of the probe copies gave `ok`, no REFUSE_NAME.  `KycSpec` gets no
sender point:  the binding is transport, not the credential
specification;  SPEC.md 2.10 gets one sentence.  Recommendation:  the raw
address as the fifth public binder, no hash, no spec point.

The kickoff probe (2026-10-19, scratch copies, the real build/langc;  the
header edit `(min : Field) (sender : Field) private`, the binding row
appended after `range8 d` on the circuit line;  the sender value
1390849295786071768276380950238675083608645509734 = the anvil account 0
address 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266):

| Probe | Circuit | Binding row | rows | copies | public | k | check | witness | satcheck | witness `sender + 1` | witness without `sender` |
|---|---|---|---|---|---|---|---|---|---|---|---|
| golden | kyc | none | 7290 | 8709 | 4 | 13 | ok | ok | ok | n/a | n/a |
| kycA | kyc | `sender * 1 == sender` | 7293 | 8713 | 5 | 13 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |
| kycB | kyc | `sender == sender` | 7291 | 8710 | 5 | 13 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |
| kycC | kyc | `sender + 0 == sender` | 7293 | 8713 | 5 | 13 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |
| kycE | kyc | `0 * sender == 0` | 7294 | 8713 | 5 | 13 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |
| kycU | kyc | none (the binder only) | refused | | | | CIRCUIT_UNUSED_PUBLIC (rc 1) at check, circuit and witness | | | | |
| golden | kycMerkle | none | 21898 | 26177 | 4 | 15 | ok | ok | ok | n/a | n/a |
| kmA | kycMerkle | `sender * 1 == sender` | 21901 | 26181 | 5 | 15 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |
| kmB | kycMerkle | `sender == sender` | 21899 | 26178 | 5 | 15 | ok | ok | ok | ok (rc 0) | WITNESS_ARGS (rc 2) |

The other IR fields stay (`advice` 3, `fixed` 7, `instance` 1, `gates` 1,
`lookups` 1, `fixed_values` 7).  The WITNESS_ARGS text:  `kyc: the binder
sender has no value`.  Times:  kyc check 0.12 to 0.17 s, circuit 0.16 to
0.19 s, witness 0.26 to 0.41 s, satcheck 1.0 to 1.3 s;  kycMerkle check
0.67 to 1.07 s, circuit 0.69 to 0.97 s, witness 1.2 to 1.3 s, satcheck 3.1
to 3.2 s.  k stays 13 and 15:  the ceremony file (k 15, O23) needs no pin
change.

D2.  The binding row (question (c)).  Form B `sender == sender` lowers to
one arith row and one copy (the instance cell of `sender` into the advice
cell of that row).  Forms A and C lower to three rows and four copies
(the constant and the product or sum take rows);  form E to four rows.
The checker accepts each form:  the CIRCUIT_UNUSED_PUBLIC check is
syntactic (a binder in any constraint passes).  The IR golden pins the
row and the s1 mutant m1 (the row removed) pins the refusal.
Recommendation:  form B, appended last on the circuit line (`range8 d;
sender == sender`).

D3.  The circuits that get the binder (question (d)).  Option (i):  `kyc`
and `kycMerkle` in place.  The IR and the verifier of two current
circuits change (rows 7290 to 7291 and 21898 to 21899, `public` 4 to 5,
`num_instances` 4 to 5 in each verifier):  the IR rule of COMMON needs
this ruling.  The six M8 circuits (square, poly, range, nonzero, poseidon,
transfer) stay byte-identical, and the IR check after e2e covers them.
Option (ii):  a third circuit `kycBound` (or two, one for each) keeps the
current two.  Each extra kycMerkle e2e row costs about 48 s of proof time
for each e2e run, and an unbound verifier stays in contracts/src for a
gate deploy with the O22 gap.  Recommendation:  option (i).  No gate is
deployed (O23:  the five fields are the literal word VALUE), thus no
migration.  If a gate were deployed, the fix is a new verifier and a new
gate (the verifier is immutable).

D4.  The gate (question (e)).  `KycGate.admit`:  the count check becomes
`instances.length != 5`;  a new check `if (instances[4] !=
uint256(uint160(msg.sender))) revert WrongSender(instances[4]);` after
`WrongMin` and before the nullifier check (the cheap checks first, the
verifier call last);  a new error `WrongSender(uint256 sender)`;  the
doc comments (lines 15 to 16 and 86 to 87) name five public inputs.  The
event stays.  The tests:  KycGate.t.sol `fixture()` parses five
instances;  each admit of the fixture runs under `vm.prank` of the
fixture sender (the anvil account 0 address);  new tests
`testRefusesAnotherSender` (no prank:  the test contract is the sender,
WrongSender) and `testRefusesAWrongInstanceCount` keeps `new
uint256[](3)`;  Verifiers.t.sol adds `instances[4] == SENDER` to the two
kyc checks.  The scripts:  check.sh `cast call .. --from <the fixture
sender>` (a call from the zero address gets WrongSender);  rehearsal.sh
and deployment.sh send from the anvil account 0 (the fixture sender) and
need a check that they do (s2 reads the `cast send` lines).  The runbook
and contracts/README.md:  five public inputs;  the runtime table row of
each verifier if the size changes.  Recommendation:  as stated;  the
fixture sender is the anvil account 0 address, thus the rehearsal keeps
its proofs.

D5.  The rows and the goldens.  test/circuit/expect.txt rows 8 and 9 get
`sender=1390849295786071768276380950238675083608645509734`;  the goldens
test/circuit/kyc.circuit.json, kyc.witness.json, kycMerkle.circuit.json
and kycMerkle.witness.json regenerate (`langc circuit .. -o`, `langc
witness .. -o`);  refuse.txt rows 19 and 20 (WITNESS_FAIL) get the sender
argument (else WITNESS_ARGS, a different code);  a new refuse row `2
WITNESS_ARGS witness kyc kyc <the row 8 arguments without sender>` pins
the binder.  `make e2e` regenerates contracts/src/verifiers/kyc.sol and
kycMerkle.sol (num_instances 5) and the fixtures kyc.proof.json and
kycMerkle.proof.json (five instances);  `make e2e-ceremony SRS=..`
regenerates the ceremony verifiers under build/ (not committed).  The IR
check:  the kyc and kycMerkle goldens and verifiers differ from HEAD
before e2e (staged in s1) and the diff is the same after e2e;  the six
other circuits stay byte-identical.

D6.  The point name `kycNullifier` (question (f)).  The s3 deviation from
the D3 text of M9:  the constructor namespace is flat (src/front/check.c
name_owner, rule R4) and `nullifier` is a point of `TransferSpec`.  The
name sites:  domain/domain.lang lines 64, 66 and 68 (the family line and
two comment lines) and SPEC.md (2 sites).  No hit in examples/, test/,
docs/M9-PLAN.md or README.md:  the fold cases of `KycSat` are positional.
Option (i):  keep `kycNullifier` (no edit, no rebuild).  Option (ii):  a
USER name that is unique over all families:  domain.lang 3 lines, SPEC.md
2 lines, `make -B check` (domain.lang is compiled into langc).
Recommendation:  option (i), recorded as a ruling;  SPEC.md 2.10 states
the flat namespace rule in one sentence.

D7.  The Poseidon forms (ii) and (iii) (question (g)).  Form (ii), a
custom degree-5 gate, replaces the rows of each S-box by one row;  form
(iii), a sharing pass, adds a memo table to the lowering (src/circuit.c
has none).  Each changes the IR of each current Poseidon circuit or the
verifier generator (M9 ruling (h)).  Recommendation:  an estimate only in
M10 (slice s4):  count the rows of one full round and one partial round
of form (i) (M8 s3 measured 72 and 54), count the S-box rows a degree-5
gate saves, and count the repeated subterms of one round that form (iii)
would share (form (i) already names each S-box output and state word
with a hint, thus the repeated subterms are binder uses, not rows).  Then
state the k each form gives for `kyc` (7291 rows at k 13, 8192 rows) and
`kycMerkle` (21899 rows at k 15, 32768 rows).  The hypothesis:  neither
form changes k, thus neither needs a milestone;  the estimate confirms or
refutes it, and the forms stay open or close on the count.  No code.

D8.  The open items O3, O12 (c), O13, O14, O15, O16 and O19 (question
(h)).  Recommendation:  they stay open (as M9 ruling (i));  the USER
picks one or none.

## 3. Core changes

None recommended.  No conv rule, no new IR form, no verifier generator
change, no src/ change.  The binder is a surface edit in two .glue
examples;  the IR of `kyc` and `kycMerkle` changes under ruling (d).  The
gate change is Solidity.  D6 option (ii) edits domain/domain.lang only.

## 4. Derived operations and their laws

None new.  `KycSpec`, `KycSat`, `kycLaw`, `discLaw` and `kycLawH` stay.

## 5. Effect on examples

- examples/kyc.glue line 221 and examples/kycMerkle.glue line 225:  the
  header gets `(sender : Field)` after `(min : Field)`;  the constraint
  list gets `; sender == sender` last.  The header comments get one
  sentence on the sender binder (O22).
- examples/kycSpec.lang:  unchanged (D1).
- No other example changes.

## 6. Circuits

| Circuit | rows (M9) | rows (M10, form B) | k | public | Verifier | Golden |
|---|---|---|---|---|---|---|
| kyc | 7290 | 7291 | 13 | 4 to 5 | contracts/src/verifiers/kyc.sol regenerates | test/circuit/kyc.*.json regenerate |
| kycMerkle | 21898 | 21899 | 15 | 4 to 5 | kycMerkle.sol regenerates | kycMerkle.*.json regenerate |
| square, poly, range, nonzero, poseidon, transfer | as M8 | unchanged | | | byte-identical | byte-identical |

The e2e table keeps 9 rows.  The ceremony file (k 15) covers both
circuits.

## 7. New check codes

None in langc.  KycGate:  the error `WrongSender(uint256 sender)`.

## 8. Guest tests per slice

- s1:  the four goldens regenerate;  expect.txt rows 8 and 9 with
  `sender=`;  refuse.txt rows 19 and 20 with `sender=`;  the new
  WITNESS_ARGS refuse row (circuit refusals 20 to 21);  Verifiers.t.sol
  two `instances[4]` checks;  KycGate.t.sol the five-instance fixture, the
  pranks and `testRefusesAnotherSender` (forge test 99 to 100).  Mutants:
  m1 the binding row removed (CIRCUIT_UNUSED_PUBLIC at the examples gate);
  m2 the header binder removed with the row kept (an unknown name at
  check);  m3 the expect row without `sender=` (WITNESS_ARGS at the
  circuits row);  m4 the gate check `instances[4]` removed
  (testRefusesAnotherSender);  m5 the count check kept at 4 (the fixture
  tests).
- s2:  contracts/test/deployment.sh and `make rehearsal` run GREEN with
  the five-instance fixture;  check.sh with `--from`;  no new guest row.
- s3 (D6 option (ii) only):  `make -B check` after the domain edit;  the
  eval rows of kycSpec stay (positional cases).
- s4:  no guest test (a count table in docs/STATUS.md).
- s5:  none (the close).

## 9. Questions for the USER

- (a) The scope.  Recommendation:  O22 (s1 the circuits and the gate, s2
  the scripts and the runbook), the `kycNullifier` ruling (s3, a ruling
  and at most one domain edit), the Poseidon estimate (s4), the close
  (s5).  The open items of D8 stay.  Reason:  the deployed gate design has
  the O22 gap, the other items are rulings or counts.
- (b) The binder:  the raw address as a Field value, the fifth public
  binder `sender`, no hash, no `KycSpec` point.  Recommendation:  yes
  (D1):  160 bits is below r, the gate compares one word.
- (c) The binding row:  form B `sender == sender` (1 row) against form A
  `sender * 1 == sender` (3 rows).  Recommendation:  form B (D2).
- (d) The circuits:  `kyc` and `kycMerkle` in place (the IR of two current
  circuits changes:  this ruling is the IR rule ruling) against a third
  circuit.  Recommendation:  in place (D3).
- (e) The gate:  the count 5, `WrongSender` after `WrongMin`, the fixture
  sender = the anvil account 0 address.  Recommendation:  yes (D4).
- (f) The name `kycNullifier`:  keep, or a USER name.  Recommendation:
  keep (D6).
- (g) The Poseidon forms:  an estimate in s4, no build in M10.
  Recommendation:  yes (D7).
- (h) The open items O3, O12 (c), O13, O14, O15, O16, O19:  stay open.
  Recommendation:  yes (D8).
- (i) The slices of section 10.  Recommendation:  s1 to s5 as listed.
- (j) SPEC.md 2.10 gets one sentence on the sender binder and one on the
  flat constructor namespace.  Recommendation:  yes.

## 10. Slices (one session each)

Each slice:  the gates of COMMON (`make -B check`, `make check-clang`,
`forge build --root contracts --sizes`, `make e2e` last, the IR check,
`forge test --root contracts`), the mutants, the close steps.  The gate
counts at the M9 close:  native circuit 9, path 46, face 35, HIT 44;
examples 67, parse refusals 18, check refusals 157, evals 290, json 42
builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 20,
satcheck refusals 6;  node regressions 6, printer 2;  forge test 99;  e2e
9 rows.

- s1:  O22, the circuits and the gate.  examples/kyc.glue and
  kycMerkle.glue (D1, D2);  the four goldens;  expect.txt rows 8 and 9;
  refuse.txt rows 19 and 20 and the new WITNESS_ARGS row;  KycGate.sol
  (D4);  KycGate.t.sol and Verifiers.t.sol;  `make e2e` (the verifiers
  and the fixtures);  `make e2e-ceremony SRS=<the pinned file>` (build/
  only);  SPEC.md 2.10 (the sender sentence) and 9 (O22:  the circuit and
  the gate side done, the runbook in s2);  docs/PLAN.md gets the M10 row
  (PLANNED, the scope of ruling (a));  docs/M10-PLAN.md goes with this
  commit.  Gate delta:  circuit refusals 20 to 21;  forge test 99 to 100;
  the IR diff holds the kyc and kycMerkle goldens and verifiers before and
  after e2e, nothing else.  Two sessions as M9 s2 (the build and the
  mutants, then the gates and the close).
- s2:  O22, the operations.  check.sh (`--from`), rehearsal.sh and
  deployment.sh (the sender account check), contracts/README.md (five
  public inputs, the runbook, the runtime table);  `make rehearsal` and
  `make deployment-test` GREEN (sandbox off, anvil 31337, no key, no
  broadcast);  SPEC.md 9:  O22 CLOSED.  Gate delta:  none in `make check`;
  forge test unchanged.
- s3:  the `kycNullifier` ruling (f).  Option (i):  the ruling recorded in
  docs/STATUS.md and SPEC.md 2.10 (the namespace sentence), no code.
  Option (ii):  domain/domain.lang lines 64, 66 and 68, SPEC.md 2 sites,
  `make -B check`.  Gate delta:  none.
- s4:  the Poseidon estimate (D7):  the counts of one full and one partial
  round of form (i), the saving of each form, the k of each circuit under
  each form, in docs/STATUS.md `### s4`;  SPEC.md 9 gets the result (the
  forms stay open or close).  No code, no gate delta.
- s5:  close.  docs/STATUS.md `### s5: close`;  SPEC.md 4 (the M10 bullet)
  and 9;  docs/PLAN.md M10 row DONE;  ~/Documents/glue-lang-M10-COMMON.md
  traps.  Gate delta:  none.
