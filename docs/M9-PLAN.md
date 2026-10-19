# M9 plan:  ZK-KYC credential circuit, ceremony SRS, Telcoin deployment

Status:  RULED 2026-10-18 (USER):  each recommendation of section 9, (a)
to (j).  Open USER facts:  the ceremony file and its source (question
(c), before s5);  the network, the chain id, the RPC URL, the explorer
and the keystore account name (question (e), before s6).  This file goes
into the M9 s1 commit.

The M9 row of docs/PLAN.md:  a ZK-KYC credential circuit;  ceremony SRS
ingestion;  deployment to a Telcoin network (the USER runs the broadcast).
M8 gives M9 these items:  O21 (SPEC.md 9), Merkle membership (ruling (f)
of M8) and the Poseidon forms (ii) and (iii) (ruling (d) of M8).

## 1. Sources

- docs/PLAN.md:  the M9 row.
- docs/STATUS.md:  `### s7: close (2026-10-18)` of `## M8` (the open items)
  and `## M9` `### kickoff` (the probes of this plan).
- SPEC.md 9:  O21 (new in M8), O3, O12 (c), O13, O14, O15, O16, O19.
- docs/M8-PLAN.md:  D4 (Poseidon form (i)), D7 (the transfer circuit), D8
  (the law transport);  rulings (a) to (h).
- design/DESIGN.md:  the ZK-KYC text (KycPublic, KycWitness, KycRelation,
  KycSpec, lines 280 to 354).  It is a design sketch, not a format.
- halo2/src/backend.rs (Srs::dev, Srs::read, SolidityGenerator::new),
  halo2/src/main.rs (`keygen --srs FILE | --dev-srs`), Makefile (e2e).
- contracts/script/DeployVerifier.s.sol (getCode and CREATE, s4 ruling of
  M8), contracts/foundry.toml (evm_version paris).
- The kickoff probes:  ~/Documents/glue-lang-M9-kickoff-probe/run.sh and
  run.out (the real build/langc;  no new family, thus no scratch copy).

## 2. Design calls

D1.  The credential (question (a)).  The recommended minimum has the
shape of the transfer circuit:  two Poseidon permutations (form (i)) and
one attribute check.

- Commitment:  `cm = pos2 attr sk`.  The issuer publishes `cm`.  `sk` is
  the holder key, thus only the holder can prove for `cm`.
- Nullifier:  `nf = pos2 sk scope`.  `scope` is public (an application or
  an epoch).  One `nf` for each holder and scope:  the consumer contract
  refuses a second use.
- Attribute check:  `range8 d` with `d + min == attr`.  This implies the
  integer bound `min <= attr <= min + 255` only when `min <= r - 256`,
  where `r` is the BN254 scalar modulus (src/front/fr.h).  The consumer
  must enforce this bound when its public policy is configured (D7).
  Without it, `min = r - 1`, `attr = 0`, `d = 1` satisfies both field
  constraints and violates the claimed integer lower bound.  The raw
  circuit enforces a modular relation, not this policy precondition.
  The alternative is the equality `attr == want`.
- Public:  `cm`, `nf`, `scope`, `min`.  Private:  `attr`, `sk`, `d`.

Probe (a):  kyc2 (two permutations and `v == want`) is 7289 rows, k 13;
kyc2r (two permutations, `d + want == v` and `range8 d`) is 7291 rows, k
13, 1 lookup.  One permutation and one equality (kyc1) is 3645 rows, k 12.
The attribute check adds 1 row (equality) or 3 rows (range).

D2.  Merkle membership (question (b), ruling (f) of M8).  A public issuer
root hides `cm`:  without it, `cm` is public and links each proof of one
holder.  One level is a bit `b` (`b * b == b`), two selects (`l == cur + b
* (sib - cur)`, `r == sib + b * (cur - sib)`) and one permutation.

Probe (b):  merkle1 (one level) is 3654 rows, k 12;  merkle2 (two levels)
is 7307 rows, k 13.  One level is 3653 rows.  The estimate of the
credential circuit with depth d is 7291 + 3653 d rows:

| Depth d | Rows (estimate) | k | Holders |
|---|---|---|---|
| 0 | 7291 | 13 | (cm public) |
| 2 | 14,597 | 14 | 4 |
| 4 | 21,903 | 15 | 16 |
| 8 | 36,515 | 16 | 256 |
| 16 | 65,739 | 17 | 65,536 |
| 20 | 80,351 | 17 | 1,048,576 |

These are row estimates, not supported end-to-end depths.  The current
satchecker accepts only k <= 16 (SAT_K_MAX in src/circuit.c, SPEC.md 7).
Depths 16 and 20 therefore fail `langc satcheck` and `make e2e` at k 17,
even when the source and JSON files fit.  Among the listed depths, 8 is
the largest within that gate limit.  Depth 4 stays the M9 choice;  k 17
needs a separate satchecker change and validation before use.

Sizes:  examples/transfer.glue (2 permutations) is 73,039 B, its circuit
JSON 1,080,055 B and its witness 1,251,199 B.  Thus one permutation is
about 36 KB of source and 0.54 MB of circuit JSON.  Depth 20 (22
permutations) is about 0.8 MB of source (SOURCE_MAX_BYTES is 1 MiB) and
about 12 MB of circuit JSON and 14 MB of witness (JSON_MAX_BYTES is 16
MiB).  A depth above about 20 also needs a new file limit or form (ii) or
(iii), in addition to the satchecker limit above.

D3.  The specification HIT (question (g)).  A family `KycSpec` in
domain/domain.lang with the form of `TransferSpec` (ruling (a) of M8):
points (`credential`, `nullifier`, `predicate`) and path constructors
(`noReplay`, `disclosure`).  Its fold into `Type 0` uses `ua` of an
`Equiv` of the `Sat` types (ruling (b) of M8).  The hash is an opaque
argument from the start (D4).

D4.  O21 (question (f)).  The law for all inputs takes the hash as an
argument `h : Field -> Field -> Field`, and the instance gives `pos2`.

Probe (c):  `TSatH h nf cm v sk rho rho2 = Sat2 (h sk rho) (h v rho2) nf
cm` and `transferSatH` (for all `h` and all inputs, through `transferLaw`)
check:  0.41 s for the file (examples/transferSpec.lang alone:  0.23 s).
`transferD7H` (h := pos2, the D7 inputs) gives the pair of `transferD7` in
0.42 s (transferD7:  0.47 s).  The normal form of `transferSatH` is `fun h
=> .. => fun p => pair (<j> (first p) @ ~j) (<j> (second p) @ j)` (0.38
s).  No EVAL_FUEL:  a neutral `h` stops the hash in the Glue transport.
The transport of `balanceCons`:  `BalLine a b nf cm = <i> TransferSat a b
nf cm (balanceCons @ i)` from `Sat2` to `CommSat`, `balLaw` and `balD7`
check (0.36 s) and `balD7` gives the pair `nf`, `cm` (0.43 s).  The other
orientation gives PATH_BOUNDARY.  Thus both parts of O21 have a form.

D5.  The ceremony SRS (question (c)).  The back end reads a halo2 KZG
params file now (`keygen --srs FILE`, ParamsKZG::read, then downsize to
the circuit k;  a smaller k is refused).  e2e uses `--dev-srs` (a fixed
seed, INSECURE) for each of its 7 rows;  the largest k is 13 (transfer).
prove, verify and solidity read `srs.bin` from the keys dir.  The Solidity
verifier holds G2 points of the SRS (SolidityGenerator::new takes the
SRS), thus a ceremony SRS changes each verifier file, but not the IR and
not the verifier generator.  Recommendation:  keep `make e2e` on the
development SRS (the gates and the IR check stay byte-identical), and add
a separate rule `make e2e-ceremony SRS=FILE` that pins the file by
SHA-256 (a checksum file in the repository) and writes the verifiers to a
separate directory.

D6.  Deployment (question (e)).  Claude writes the scripts, the dry runs
and the checks.  The USER runs the broadcast with the USER key.  Claude
never reads or uses a key and never calls a live RPC that sends a
transaction.  The steps:  a rehearsal on a local anvil with the chain id
of the target network (no live RPC);  a runbook with the exact `forge
script .. --broadcast --account NAME` command for the USER;  a check
script (`cast code`, the runtime hash against the build artifact, and one
`cast call` of the verifier with a fixture proof).  The check is read only.

D7.  The consumer contract (question (d)).  A verifier does not stop a
replay:  the same proof verifies twice.  A small contract `KycGate`
stores each used `nf` and calls the verifier.  Verification alone also
does not authenticate the issuer or choose the application's policy.
Configure the trusted verifier, an issuer-controlled allowlist of `cm`
for kyc or an approved issuer root for kycMerkle, and fixed `scope` and
`min` values.  Reject a configuration with `min > r - 256` (D1).
Before accepting a proof, compare its public inputs in the circuit's
declared order with that configuration, and check the commitment
allowlist or root.  A caller must not choose the trust anchor, scope or
threshold.  Otherwise a holder can prove membership in a self-created
tree, choose an easier threshold, or vary `scope` to obtain a fresh
`nf` and bypass the intended replay policy.  Only record `nf` after
successful verification under the approved policy.  Any later policy
updates require issuer or application authorization.

D8.  The Poseidon forms (ii) and (iii) (question (h)).  Form (ii) (a
custom degree-5 gate) and form (iii) (a sharing pass) each change the IR
or the verifier generator.  Recommendation:  not in M9.  D2 shows that
form (i) fits the file limits to about depth 20, but the current
satchecker blocks k 17.  Depth 4 works within the existing gate limits.

## 3. Core changes

None recommended.  No conv rule, no new IR form, no verifier generator
change.  D4 needs no core change (probe (c) used the real build/langc).
D3 adds one family to domain/domain.lang (rule R1).  D5 changes
halo2/src (the checksum pin) and the Makefile.  D6 and D7 change
contracts/ only.

## 4. Derived operations and their laws

- `transferSatH` (D4):  the law of `noDoubleSpend` for all hashes and all
  inputs;  `transferD7H` is its instance at `pos2`.
- `balLaw` (D4):  the transport of `balanceCons` from `Sat2` to `CommSat`.
- `KycSat` and `kycLaw` (D3):  the fold of `KycSpec` into `Type 0` and
  the transport of one law to the satisfaction type of the credential
  circuit, with the hash as an argument.

## 5. Effect on examples

- examples/transferSpec.lang:  the defs of D4 (s1).  Its JSON gets the
  closed defs of a Prod of Path types as instances (transferD7H, balD7).
- examples/kyc.glue:  the credential circuit (s2).
- examples/kycSpec.lang:  the HIT of D3 and its law (s3).
- examples/kycMerkle.glue:  the credential circuit with membership (s4).
- No change to a current circuit:  the IR and the verifier of each M8
  circuit stay byte-identical.

## 6. Circuits

| Circuit | Slice | Rows | k | Public |
|---|---|---|---|---|
| kyc | s2 | about 7291 | 13 | cm, nf, scope, min |
| kycMerkle | s4 | about 7291 + 3653 d | question (b) | root, nf, scope, min |

Each adds a golden, an e2e row, a verifier and Forge tests (as poseidon
and transfer in M8:  11 tests each).

## 7. New check codes

None recommended.  The rows use the current codes:  WITNESS_FAIL (a wrong
attribute or a wrong hash input), PATH_BOUNDARY (the wrong orientation of
`BalLine`), REFUSE_NAME (a program that uses a new domain name).

## 8. Guest tests per slice

- s1:  eval rows `transferSpec transferD7H` and `transferSpec balD7`;  a
  check refusal row with the wrong `BalLine` orientation (PATH_BOUNDARY).
- s2:  examples/kyc.glue;  the circuits row and the golden;  the e2e row;
  a WITNESS_FAIL row (an attribute below a valid `min`).  Document the
  modular counterexample `min = r - 1`, `attr = 0`, `d = 1` separately
  from the integer predicate and its consumer precondition (D1).
- s3:  examples/kycSpec.lang;  its eval rows;  the REFUSE_NAME row.
- s4:  examples/kycMerkle.glue;  the circuits row, the golden, the e2e
  row;  a WITNESS_FAIL row (a wrong sibling).
- s5:  a Rust test in halo2/tests for the checksum refusal (the back end
  is Rust:  its tests stay Rust).
- s6:  Forge tests for KycGate:  accept an approved proof once, then
  reject replay;  reject otherwise valid proofs under an unapproved
  commitment or root, a different scope, or a different threshold.
  Reject policy configurations with `min > r - 256`, including the D1
  counterexample, and accept the boundary `min = r - 256`.

## 9. Questions for the USER

Each question has a recommendation.  The USER rules each one before s1.

- (a) The credential format and the attribute check.  Recommendation:
  D1 (`cm = pos2 attr sk`, `nf = pos2 sk scope`, `range8 d` with `d + min
  == attr`, with the D1 bound on `min` enforced by D7).  Reasons:  probe
  (a) measured this shape (7291 rows, k 13);
  it needs no new IR;  an age predicate (attr >= min) is the first
  example of design/DESIGN.md.  The issuer signature, the expiry and the
  revocation are not in the minimum:  each needs another hash or a
  signature gadget.  Alternative:  the equality `attr == want` (7289
  rows).
- (b) Merkle membership and its depth.  Recommendation:  a separate
  circuit kycMerkle in s4 with depth 4 (about 21,903 rows, k 15, 16
  holders).  Reasons:  without it `cm` is public and links the proofs of
  one holder;  depth 4 keeps e2e and the 4 GB limit of each run safe;  a
  larger depth is the same text with more levels.  D2 gives the file
  estimates to depth 20, but k 17 exceeds the current satchecker limit.
- (c) The ceremony and its file.  Recommendation:  a halo2 KZG params file
  that comes from the Perpetual Powers of Tau ceremony, with k >= 15 (the
  largest k of question (b)), pinned by SHA-256.  The USER names the file
  and its source.  Reasons:  `keygen --srs FILE` reads that format now, so
  no new reader;  a public ceremony with many contributors.  Before s5
  reads it, the slice records the source URL and the checksum (as ruling
  (e) of M8 for the Poseidon constants).  Alternative:  a reader of the
  snarkjs `.ptau` format (new Rust code in halo2/src).
- (d) The consumer contract.  Recommendation:  KycGate (D7) in s6.
  Reasons:  without it a proof can be used again;  a mapping of used `nf`
  values and checks against the trusted issuer and fixed policy are
  testable in Forge.  The verifier alone checks a caller's statement;
  it provides neither issuer authorization nor replay protection.
- (e) The deployment target.  USER facts:  the network (testnet or
  mainnet), its chain id, its RPC URL and its explorer;  the account name
  of the USER keystore (Claude does not read it);  the circuits to deploy.
  Recommendation:  the testnet first, the kyc and kycMerkle verifiers with
  the ceremony SRS, and KycGate.  Claude runs the anvil rehearsal only;  a
  read-only call to the live RPC (the chain id, the BN254 precompiles 0x06
  to 0x08, the check after the broadcast) only after a USER ruling.
- (f) O21.  Recommendation:  close O21 in s1 with D4 (the hash as an
  argument;  `balLaw` for `balanceCons`).  Reasons:  probe (c) checks and
  evaluates in under 0.5 s;  the law holds for each hash, and `pos2` is
  one instance.
- (g) The specification HIT `KycSpec`.  Recommendation:  D3 in s3 (one new
  family, the form of `TransferSpec`, the hash as an argument).  Reasons:
  Direction 4 for the credential;  the forms of rulings (a) and (b) of M8
  and of D4 hold.
- (h) The Poseidon forms (ii) and (iii).  Recommendation:  not in M9 (D8).
- (i) The open items O3, O12 (c), O13, O14, O15, O16 and O19.
  Recommendation:  they stay open;  no M9 slice changes them.
- (j) The slices of section 10.  Recommendation:  s1 to s7 as below.

## 10. Slices (one session each)

| Slice | Content | Gate delta |
|---|---|---|
| s1 | This plan (the commit);  O21 closed (D4):  the defs in examples/transferSpec.lang;  SPEC.md 9 O21 closed | evals +2;  check refusals +1;  json:  test/json/transferSpec.json +2 instances, builds +0 |
| s2 | The credential circuit kyc (D1) | examples +1;  circuits +1 (8);  circuit refusals +1;  e2e 8 rows;  1 verifier;  forge test +11 |
| s3 | `KycSpec` (D3), examples/kycSpec.lang, its law | examples +1;  json +1;  evals +2 or more;  check refusals +1 (REFUSE_NAME);  native HIT count:  check at the slice start |
| s4 | kycMerkle (D2, depth of question (b)) | examples +1;  circuits +1 (9);  circuit refusals +1;  e2e 9 rows;  1 verifier;  forge test +11 |
| s5 | The ceremony SRS (D5):  the checksum pin, `make e2e-ceremony` | cargo test +1 or more;  `make e2e` unchanged;  `make e2e-ceremony` exit 0 (sandbox off) |
| s6 | Deployment (D6, D7):  KycGate, the anvil rehearsal, the runbook, the check script | forge test +3 or more;  the rehearsal exit 0;  the USER broadcast is after s6 |
| s7 | Close:  STATUS, SPEC.md 4 and 9, PLAN M9 row, the mutant roll-up | none |

Each slice keeps the M8 gates (docs/STATUS.md M8 `### s7`):  `make -B
check`, `make check-clang`, `forge build --root contracts --sizes`, `make
e2e`, the IR diff of test/circuit and contracts/src/verifiers before and
after e2e, `forge test --root contracts`.
