# glue-lang M8 plan: HIT specifications, the Poseidon gadget and the private transfer

Status: RULED (2026-10-18). The USER ruled questions (a) to (h) of section
9: each recommendation. M7 is the s6 close commit 1e12d86. The kickoff makes no code
change. The s1 commit includes this plan, as in M2 to M7.

M8 is Direction 4 of design/DESIGN.md: a specification is a HIT whose path
constructors are the laws, an equivalence connects the specification to
the circuit relation of M7, and `ua` transports a law. The docs/PLAN.md M8
row also gives the Poseidon gadget over BN254 Fr and the Telcoin private
transfer circuit. M9 has ZK-KYC, the ceremony SRS and the deployment.

## 1. Sources

| Source | What M8 takes |
|---|---|
| docs/PLAN.md rulings, `## Field and proving system`, M8 and M9 rows, lowering rules | BN254 Fr, halo2 PSE, KZG, SHPLONK, Keccak transcript; `transp` along `ua e` is the forward map of `e`; the quotient eliminator runs on representatives; a proof stays a term |
| design/DESIGN.md `### Direction 4` (lines 118 to 178) | OTPSpec: points, a path constructor for the law, `interp`, `isoToEquiv`, `ua`, transport of a law |
| design/DESIGN.md lines 212 to 266 | `PrivateTransferCircuit`, `TransferRelation`, `TransferSpec` with `noDoubleSpend`, `balanceCons`, `ownership` |
| SPEC.md 2.5, 2.6 | `ua`, uaBeta, `isoToEquiv`; HITs, `fold B`, D3 |
| SPEC.md 2.9, 4 | `Circ`, `ev`, `CRel`, `evQ`, `Sat`, `SatP`, `CEqv`, `SatQ`, `opt`, `moveSat`; circuits keep GQuot (D7) |
| SPEC.md 7.1, docs/IR.md 2 and 3 | The rows of the lowering: one input row for each binder and hint, one constant row for each literal, one arith row for each `+ - *`; the k rule; `gates` can hold any poly |
| SPEC.md 2.2 (line 53) | `circuit NAME [public ..] [private ..] (hint NAME := TERM)* := C (; C)*` |
| SPEC.md 9 | O3, O12 (c), O13, O14, O15, O19 and O20 (RULED: M8 work) |
| docs/STATUS.md `### s6: close` | The M7 gate counts and open items |

Code anchors: src/circuit.c (lower_value, lower_check, lower_circuit; no
memo or sharing table); src/front/fr.h (the decimal of r); halo2/src/circuit.rs
line 112 (an IR `Expr` maps to a halo2 `Expression` term by term);
halo2/src/backend.rs lines 20 and 349 (`SolidityGenerator::new` with
`BatchOpenScheme::Bdfg21`; line 331 checks its asserts first).

Probes (2026-10-18, files under $TMPDIR; the three families in a scratch
copy of domain/domain.lang; the repository tree did not change):

| P | Probe | Result |
|---|---|---|
| P1 | `family SqSpec := smul \| spow \| slaw <i> [ ~i -> smul, i -> spow ]`; `interpS : SqSpec -> GQuot Circ CRel` is a fold with the point cases `gcls Circ CRel (cmul cpriv cpriv)` and `gcls Circ CRel (cpow cpriv 1)` and the path case `lawQ` (the def of `<i> geqc .. opt @ i`) | check ok |
| P2 | `moveSpec y x p = transport (<i> SatP y x (interpS (slaw @ i))) p`; `m3s = moveSpec 9 3 refl` | eval `<j> 9` |
| P3 | `SpecSat y x` is the fold of SqSpec into `Type 0` with the path case `ua (idSat y x)` (`isoToEquiv` of `Sat (cmul cpriv cpriv) y x` and `Sat (cpow cpriv 1) y x`, both maps `fun p => p`); `uaFwd`: `transport (UaLine y x) p` converts with `witness (idSat y x) p` by refl | check ok; `m4s` eval `<j> 9` |
| P4 | circuit `square` with `evQ y x (interpS (slaw @ i1)) == y` | rows 3, IR SAME as square |
| P5 | `family TSpec := tnote (tnv : Field) \| tnull (tnk : Field) \| tlaw (tlk : Field) <i> [ ~i -> tnote tlk, i -> tnull tlk ]`; a fold into Field and the line `<i> tOk (tlaw 5 @ i)` | check ok |
| P6 | `family CircV := vvar (vvn : Nat) \| vlit .. \| vadd .. \| vmul ..`; `evV : (Nat -> Field) -> CircV -> Field` (an `elim`); the environment is an `elim` at Nat; `CRelV`, `evQV` | check ok; `evV (envSq 9 3) (vmul (vvar 1) (vvar 1))` eval 9 |
| P7 | circuit `square` with `evQV (envSq y x) (gcls CircV CRelV (vmul (vvar 1) (vvar 1))) == y` | IR SAME as square |
| P8 | circuit `poly` (public a, y; private x) with `cubic (evV en (vvar 0)) (evV en (vvar 2)) == evV en (vvar 1)` | rows 9, IR SAME as poly |
| P9 | `(x * x) * (x * x) == y`; the same through `def sqr` | 5 rows each: the lowering does not share a subterm |
| P10 | `x * x * x * x * x == y` | 6 rows (4 arith rows) |
| P11 | A 2-word round `rd` (an S-box on each word, a mix) composed 1 to 4 times in one constraint | rows 13, 73, 433, 2593 (k 5, 7, 9, 12): 6 times for each round |
| P12 | One full round at t = 3 (3 S-boxes, 3 MDS rows) as one constraint | 156 rows, k 8 |
| P13 | A 74-digit literal; the literal 2^256 | eval ok; 2^256 gives FIELD_RANGE ("not a decimal below r") |
| P14 | `x * L == y` with a 74-digit literal L | rows 4; L is in `fixed_values` |
| P15 | Files that fold over `Circ` with `ev` (`elim (fun d => Field) y x`) | 7: examples/circRel.lang, circEval.lang, circSyntax.glue, circOpt.glue, circTransport.lang, circPow.glue, test/check/ring-refl.lang |

## 2. Design calls

- D1. A specification HIT is a domain family (rule R1) in the OTPSpec
  form: points, and path constructors between points (P1). Its fold into
  `GQuot Circ CRel` (or `GQuot CircV CRelV`, D3) sends each point to the
  `gcls` of a representative and each law to the `geqc` path of a relation
  proof. A circuit through the specification reduces before the lowering:
  the boundary rule at `@ i1`, the point rule, then `ev`. Thus it has the
  IR of the representative (P4). The lowering does not change (M7 D5).
- D2. The equivalence target is the satisfaction type at a representative,
  not a fold into Field (O19). Two forms check (P2, P3): (a) the line `<i>
  SatP y x (interpS (slaw @ i))`, through the lowering quotient; (b) the
  fold of the specification into `Type 0` whose law case is `ua e`, with
  `e` an `Equiv` of the two satisfaction types (`isoToEquiv`). In (b),
  transport along the law is the forward map of `e` by uaBeta (uaFwd by
  refl). (b) is the DESIGN shape: a law of the HIT becomes an equivalence
  of circuit satisfaction types, and `ua` transports a proof of `Sat`. An
  `Equiv` of the whole HIT and `GQuot Circ CRel` is not the target: the
  quotient has a point for each circuit, and the HIT has a point for each
  named representative. The OTP form of a whole-type `Equiv` holds already
  for an automorphism of the HIT (O12 (e), examples/otp.lang).
- D3. O20: a new domain family `CircV` next to `Circ`, with `vvar (vvn :
  Nat)`, `vlit`, `vadd`, `vmul` and `vpow` (the `cpow` form, so that `opt`
  has a `CircV` form). The environment is a function `Nat -> Field` by an
  `elim` at Nat; its value at a closed index reduces (P6). `evV`, `CRelV`
  and `evQV` are the `CircV` forms of `ev`, `CRel` and `evQ`. The probes
  give the IR of square and of poly (two public binders) (P7, P8). A
  `cvar` constructor in `Circ` changes the 7 files of P15 (each `elim` gets
  one more case); the new family changes no M7 file.
- D4. The lowering has no subterm sharing (P9). A Poseidon round in one
  constraint grows 6 times for each round (P11), thus the permutation (65
  rounds) cannot be one constraint. Form (i) uses hints (SPEC.md 2.2): one
  hint for each S-box output and each state word of each round, and one
  `==` constraint for each hint. Each constraint is small. Estimate at t =
  3: a full round 48 rows (3 hints `t_j == (s_j + c_j)^5` at 7 rows, 3
  hints `s'_i == M_i . t` at 9 rows), a partial round 46 rows, thus 8 x 48
  + 57 x 46 = 3006 rows, k 12. A circuit with 276 hints and 276
  constraints is long text; the round terms are guest defs, and the hint
  lines are regular. No IR form changes, and the verifier generator does
  not change. s3 measures the rows.
- D5. A round constant is a Field literal (a decimal below r; P13). Each
  literal in a constraint is one constant row (P14). The constant table is
  a set of defs in an example file, not domain/domain.lang (R1 is for
  families). A literal is not a fold count, thus O15 does not apply. r is
  in src/front/fr.h; s3 adds an eval row for `(r - 1) + 1` (0).
- D6. The reference parameters and the test vector are a USER fact
  (question (e)). Default: circomlib Poseidon over BN254 Fr, t = 3, 8 full
  rounds, 57 partial rounds, S-box x^5, with the circomlib constants and
  MDS matrix; the vector `poseidon([1, 2])` =
  7853200120776062878684798364095072458815029376092732009249414926327459813530
  (circomlibjs). s3 checks the vector with an eval row, s4 with a witness
  and `satcheck` row.
- D7. The transfer circuit in M8 (question (f)): one input note and one
  output note on `CircV`-style binders. Public: the nullifier `nf` and
  the output commitment `cm`. Private: the value `v`, the spend key `sk`,
  the input randomness `rho` and the output randomness `rho2`.
  Constraints: `nf == H(sk, rho)` and `cm == H(v, rho2)` (value
  conservation for one input and one output is the shared `v`). Two
  permutations give about 6000 rows, k 13. Merkle membership and
  deployment are not in M8.
- D8. `TransferSpec` is a domain family in the D1 form. Its points name
  the circuit representatives (the note, the nullifier, the commitment),
  and its laws are paths between them. Its fold into `Type 0` (D2 (b))
  sends each law to `ua` of an `Equiv` of satisfaction types. s6 moves one
  law (for example `noDoubleSpend`) along `ua` to the transfer circuit. A
  law between two representatives with different normal forms needs a
  proof term (M7 D8: no ring normal form).
- D9. Keep D3 of M4, D8 of M3, D13 of M6 and the M7 calls (GQuot for
  circuits, no core isSet Field, the lowering on representatives). No conv
  rule reads a type. The IR and the verifier of each current circuit stay
  byte-identical.
- D10. Open items: O20 closes in s1 (D3). O19 stays open: D2 needs no fold
  of `GQuot Circ CEqv` into Field. O3 stays open: each Poseidon hint is in
  one `==` constraint, so the syntactic check holds and the constraint
  fixes the hint. O12 (c) stays open: the M8 laws have no Path field at a
  path constructor (P5 has a Field field). O13, O14 and O15 stay open (no
  M8 work needs them; D5 for O15).

## 3. Core changes

- Syntax: none.
- Typing rules: none.
- Evaluator rules: none.
- Kan rules: none.
- Conversion: none.
- Printer and JSON: none.
- domain/domain.lang: `CircV` (s1), the specification family (s2) and
  `TransferSpec` (s5). Each constructor and field name is a domain name
  (REFUSE_NAME).
- Lowering: none for form (i) (D4). Form (ii) or a sharing pass needs a
  USER ruling first (question (d)).
- halo2/ and contracts/: no back end change. Each new circuit adds a
  generated verifier file.

## 4. Derived operations and their laws

| Law | Statement | Proof |
|---|---|---|
| evVPoint | `evQV en (gcls CircV CRelV c)` converts with `evV en c` | refl |
| evVSq | `evV (envSq y x) (vmul (vvar 1) (vvar 1))` converts with `x * x` | refl |
| optV | `CRelV (vmul (vvar 1) (vvar 1)) (vpow (vvar 1) 1)` | refl |
| interpEnd0, interpEnd1 | `interpS (slaw @ i0)` and `@ i1` convert with the two `gcls` points | refl |
| specMove | `moveSpec 9 3 refl` | eval `<j> 9` |
| uaFwd | `transport (UaLine y x) p` converts with `witness (idSat y x) p` | refl (uaBeta) |
| wrap | `(r - 1) + 1` | eval 0 |
| poseidonVec | the permutation at the test vector input | eval row (D6) |
| transferLaw | one `TransferSpec` law moved to a `Sat` proof of the transfer circuit | refl or a proof term (D8) |

## 5. Effect on examples

No current example changes (D3). New examples: circVar.lang, circVarSq.glue,
circVarPoly.glue (s1); spec.lang, specSq.glue (s2); poseidon.lang (s3);
poseidon.glue (s4); transfer.glue (s5); transferSpec.lang (s6).

## 6. Circuits

The IR of each current circuit does not change. circVarSq.glue and
specSq.glue get same-ir rows against square, circVarPoly.glue against
poly. poseidon.glue (circuit `poseidon`, s4) and transfer.glue (circuit
`transfer`, s5) are new circuits: each adds a golden in test/circuit, an
e2e row, a verifier file in contracts/src/verifiers and its Forge tests
(gate deltas in section 10). e2e goes from 5 to 7 rows.

## 7. New check codes

None. The REFUSE_NAME rows for the new domain names and the WITNESS_FAIL
rows for the new circuits use the current codes.

| Code | Exit | Refusal | Slice |
|---|---|---|---|
| (none) | | | |

## 8. Guest tests per slice

- s1: circVar.lang (evVPoint, evVSq, optV, eval rows); circVarSq.glue and
  circVarPoly.glue (same-ir); test/check/domain-vvar.lang (REFUSE_NAME).
- s2: spec.lang (interpEnd0, interpEnd1, specMove, uaFwd, eval rows);
  specSq.glue (same-ir); a REFUSE_NAME row for a specification name.
- s3: poseidon.lang (the constants, the round defs, wrap, poseidonVec).
- s4: poseidon.glue (golden, e2e, verifier); a WITNESS_FAIL row with a
  wrong hash.
- s5: transfer.glue (golden, e2e, verifier); a WITNESS_FAIL row with a
  wrong nullifier.
- s6: transferSpec.lang (transferLaw, eval row).

## 9. Questions for the USER

The USER ruled each question on 2026-10-18: each recommendation. The D
calls already use the recommendations, so no D call changes.

Each recommendation was checked against the sources and the probes of
section 1.

- (a) The form of the specification HIT. (i) The OTPSpec form: points,
  and path constructors between points. (ii) Path constructors whose ends
  are terms with fields (`nullifier = deriveNullifier note`). Recommended:
  (i); (ii) only where a law needs it. Reasons: P1 to P4 show that a law
  in form (i) transports along `ua` to a satisfaction type and that a
  circuit through it has the IR of square; (ii) with a Field field checks
  (P5), but a Path field meets O12 (c).
  RULED 2026-10-18 (USER):  the recommendation (the OTPSpec form;  fields at a path constructor only where a law needs them).
- (b) The equivalence target. Recommended: the satisfaction type at a
  representative, in form D2 (b) (a fold of the specification into `Type
  0` with `ua` of an `Equiv` of satisfaction types at each law), with D2
  (a) (`SatP` through `GQuot Circ CRel`) for the circuit side; not a fold
  into Field. Reasons: O19; P2 and P3 compute to `<j> 9`; uaFwd holds by
  refl; the lowering takes `GQuot Circ CRel` at a representative (M7).
  RULED 2026-10-18 (USER):  the recommendation (the satisfaction type at a representative:  D2 (b) with `ua`, D2 (a) for the circuit side).
- (c) The O20 binder form. Recommended: the new family `CircV` (D3), with
  the environment `Nat -> Field` by an `elim` at Nat. Reasons: P7 and P8
  give the IR of square and poly; `cvar` in `Circ` changes 7 files (P15);
  each M7 same-ir row stays.
  RULED 2026-10-18 (USER):  the recommendation (the new family `CircV`).
- (d) The Poseidon form. (i) A guest round function with one hint for
  each S-box output and state word (D4: about 3006 rows, k 12, no IR
  change, no verifier generator change). (ii) A custom degree-5 gate (a
  new gate shape in the back end and the verifier). (iii) A sharing pass
  in the lowering (no new IR form, but the IR of a current circuit with a
  repeated subterm changes: P9 gives 4 rows, not 5). Recommended: (i).
  Reasons: P9 to P12 (no sharing, 6 times growth for each round); the
  COMMON IR rule; (ii) and (iii) need a USER ruling and an e2e probe,
  thus they are M9 or later.
  RULED 2026-10-18 (USER):  the recommendation (form (i), hints on the current IR forms;  (ii) and (iii) are not in M8).
- (e) The Poseidon parameters. They must equal the hash that Telcoin uses
  for notes (a USER fact). Recommended: the USER names the reference; else
  circomlib BN254 Poseidon (t = 3, 8 full rounds, 57 partial rounds) with
  the vector of D6. Reason: Solidity implementations of it exist for the
  EVM side.
  RULED 2026-10-18 (USER):  the recommendation (no other reference named:  circomlib BN254 Poseidon, t = 3, 8 full and 57 partial rounds, the D6 vector;  s3 checks the constants and the vector against the circomlib source).
- (f) The scope of the transfer circuit. Recommended: D7 (one input note,
  one output note, the nullifier and the commitment as Poseidon hashes)
  in M8; Merkle membership in M9 or later. Reasons: a Merkle path of depth
  d adds d permutations (about 3000 rows each), and the depth is a static
  bound.
  RULED 2026-10-18 (USER):  the recommendation (D7:  one input note, one output note, `nf` and `cm`;  Merkle membership in M9 or later).
- (g) The open items in M8. Recommended: D10 (O20 closes in s1; O19, O3,
  O12 (c), O13, O14 and O15 stay open). Reasons: D2 needs no Field fold of
  `GQuot Circ CEqv`; form (i) constrains each hint; no M8 law has a Path
  field.
  RULED 2026-10-18 (USER):  the recommendation (D10:  O20 closes in s1;  O19, O3, O12 (c), O13, O14 and O15 stay open).
- (h) Slices. Recommended: s1 to s7 of section 10. Reason: each slice is
  one session and has its own gate rows.
  RULED 2026-10-18 (USER):  the recommendation (slices s1 to s7).

## 10. Slices (one session each)

| Slice | Content | Gate delta (from the M7 close) |
|---|---|---|
| s1 | O20: `CircV` in domain/domain.lang; circVar.lang, circVarSq.glue, circVarPoly.glue; the REFUSE_NAME row; SPEC.md 2.10 (new section: Direction 4, M8) starts; O20 closes | examples +3, same-ir +2, evals +2, json +1, check refusals +1 |
| s2 | The specification HIT (SqSpec form) in domain/domain.lang; spec.lang, specSq.glue; the D2 laws | examples +2, same-ir +1, evals +2, json +1, check refusals +1 |
| s3 | poseidon.lang: the constants, the round defs, wrap, the test vector; the row count of form (i) | examples +1, evals +3, json +1 |
| s4 | poseidon.glue: the circuit `poseidon` with hints; golden, e2e row, verifier file, Forge tests; the WITNESS_FAIL row | examples +1, circuits +1, circuit refusals +1, e2e +1 row, forge tests +N (s4 measures) |
| s5 | `TransferSpec` in domain/domain.lang; transfer.glue: the circuit `transfer`; golden, e2e row, verifier file, Forge tests; the WITNESS_FAIL row | examples +1, circuits +1, circuit refusals +1, check refusals +1, e2e +1 row, forge tests +N |
| s6 | transferSpec.lang: the transport of one transfer law; the s1 to s5 mutant review | examples +1, evals +1, json +1 |
| s7 | Close: STATUS `### s7: close`, the PLAN M8 row DONE, SPEC.md 4 and 9, the mutant roll-up, the M9 kickoff prompt | none |

Each slice runs the gates of ~/Documents/glue-lang-M8-COMMON.md and the IR
check after `make e2e`. In s4 and s5 the IR check diff holds only the new
golden and the new verifier file.
