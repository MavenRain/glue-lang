# glue-lang plan

glue-lang is a cubical type theory for zero-knowledge circuits. The design
is `design/DESIGN.md`. The compiler is C, built with TinyCC. The output is
a Plonkish circuit (`docs/IR.md`), and the downstream makes a halo2
circuit, a Solidity verifier contract and the deployment scripts.

## Rulings

| Date | Ruling (USER) |
|---|---|
| 2026-10-14 | Host language: TinyCC. |
| 2026-10-14 | Output: option 1, a Plonkish constraint system. The pipeline continues to a halo2 circuit, the verifier contract and the deployment scripts. |
| 2026-10-14 | Proof irrelevance is prohibited: no `Prop`, no `SProp`, no definitional proof irrelevance. Truncations are HITs with path constructors. |
| 2026-10-14 | M2 interval: the De Morgan interval (CCHM), with `~`, `/\` and `\/`. |
| 2026-10-14 | M2 circuits: `==` stays sugar for `Path Field a b`. The IR does not change. |
| 2026-10-14 | M2: delete F12 and F13 (slice s3). |
| 2026-10-14 | M2: the face lattice is in M2, as slice s4. |

## Layout

| Path | Contents | Language |
|---|---|---|
| `src/` | Compiler `build/langc`: lexer, parser, checker, evaluator, lowering, IR writer | C (TinyCC) |
| `halo2/` | `glue-halo2`: reads the IR, builds the halo2 circuit, makes keys, proofs and `Halo2Verifier.sol` | Rust |
| `contracts/` | Foundry project: the generated verifier, `script/DeployVerifier.s.sol`, on-chain tests | Solidity |
| `examples/` | glue-lang programs | glue-lang |
| `test/` | Guest tests (glue-lang programs with expected results) and the host runner | glue-lang |

Tests are written in the development language of each part: glue-lang
programs for the compiler, Rust `#[test]` for `halo2/`, Forge tests for
`contracts/`.

## Field and proving system

Field: BN254 Fr. Proving system: halo2 (PSE fork), KZG commitments, SHPLONK
(BDFG21) openings, Keccak transcript, so that the EVM verifies the proof
with the BN254 precompiles (0x06, 0x07, 0x08). The development key set uses
a local SRS (insecure, refused without `--dev-srs`). A production key set
reads a ceremony SRS file.

## Milestones

| M | Scope | Status |
|---|---|---|
| M0 | Seed: template (tcc-json host), design, plan, IR contract | DONE |
| M1 | Vertical slice. `Field` base type with `+ - *` and literals. `circuit` declarations with public and witness binders. Constraints: Field paths `a == b` (equality) and `range8 x` (lookup). Lowering to IR v1, witness generation. `glue-halo2` keygen, prove, verify, Solidity verifier. Foundry deploy script and Forge test. `make e2e` runs the whole chain. | DONE 2026-10-14 |
| M2 | Path types: interval `I`, `Path A a b` replaces the template equality formers F12 and F13. `refl`, `sym`, `cong`, `funExt` by computation. The checker gets interval variables and the face lattice. | DONE 2026-10-14 |
| M3 | Kan operations `transp` and `hcomp` for Pi, Sigma, Path, universes, coproducts. Glue types, `ua`, uaBeta by computation. Application of a partial element. Tests that come back (docs/STATUS.md, M2 s3): `round : Path Nat price price := trans same back` and `moved := fun P u => transport P same u` (eval row `equality moved => fun P => fun u => u`) in examples/equality.lang. | DONE 2026-10-15 (s1 8317494, s2 fff2c06, s3 50772e6, s4 ed6d9a4, s5 f5a5dca, s6 close; the `moved` row is `transp (<i> P 1200) i0 u`, no regularity) |
| M4 | HITs: point and path constructors, higher constructors, `hcomp` values. Smoke: circle, propositional truncation, set quotient `A / R` with `[_]`, `eq/`, `squash/`, `rec`. | DONE 2026-10-16 (s1 c14bbbe, s2 e795436, s3 2a2f1f6, s4 9350b6a, s5 c7090b1, s6 close; the Direction 4 law holds only for the identity iso at OTPSpec, SPEC.md 9 O12 (e)) |
| M5 | QITs (quotient inductive types, with inductive-inductive definitions). | DONE 2026-10-16 (s1 f783cff, s2 de04000, s3 7f4a8cd, s4 5a223ef, s5 017d6ad, s6 fb93222, s7 close; the full quotient inductive-inductive form is SPEC.md 9 O13) |
| M6 | Monads (map, bind), algebras (fold) and coalgebras (unfold) over the cubical core. Two monads at lowering: the constrained monad and the hint monad (advice values with no constraint). | DONE 2026-10-18 (plan ac8625c, s1 f521e4e, s2 df0d1e2 and 36595d0, s3 fcb1902, s4 1b9f817, s5 and s6 4f0e083, s7 close; F6 uniqueness at a HIT is SPEC.md 9 O14) |
| M7 | Direction 2: circuits quotiented by relation equivalence. The lowering of `rec` runs on representatives; `eq/` and `squash/` cases are checker obligations with no circuit cost. | DONE 2026-10-18 (s1 efd430b with the plan, s2 aa2d240, s3 4aefb85, s4 0ddc6bd, s5 b21aaf7, s6 close; the gap of `CEqv` is SPEC.md 9 O19, the indexed binder form of `Circ` is O20) |
| M8 | Direction 4: HIT specification, equivalence to the circuit relation, transport by `ua`. Poseidon gadget. Telcoin private-transfer circuit. | DONE 2026-10-18 (s1 0b82a04 with the plan, s2 c6d952f, s3 5062dda, s4 cded21d, s5 0224aee, s6 dcbac52, s7 close; the hash of open inputs is SPEC.md 9 O21; Merkle membership and the Poseidon forms (ii) and (iii) are M9 or later) |
| M9 | ZK-KYC credential circuit. Ceremony SRS ingestion. Deployment to a Telcoin network (USER runs the broadcast). | DONE 2026-10-19 (s1 674c32f with the plan, s2 be5a3f6, s3 d409eab, s4 0208087, s5 becd33c, s6 0b0c136, s7 close; the sender binding is SPEC.md 9 O22, the deployment target and the ceremony file check are O23; the Poseidon forms (ii) and (iii) are M10 or later) |
| M10 | The sender binding (O22): the fifth public binder `sender` of kyc and kycMerkle, the gate check, the operations scripts and the runbook. The `kycNullifier` name ruling. The Poseidon forms (ii) and (iii) as an estimate. | DONE 2026-10-20 (s1 54cc43b with the plan, s2 6a17d6f, s3 and s4 c4b79d1 (one docs commit), s5 close; O22 CLOSED; the deployment target and the ceremony file check stay O23; the Poseidon forms (ii) and (iii) are O24, open, no milestone named) |
| M11 | The Poseidon forms (O24):  form (iii), a memo in the lowering, if the USER rules it;  forms (ii) and (iv) stay open. | DONE 2026-10-20 (s1 d0b78f1 with the plan, s2 close;  form (iii) DONE;  forms (ii) and (iv) stay O24, open, no milestone named) |
| M12 | The literal fold (O24 form (iv)):  T1, a literal operand into the coefficients ql, qr, qm, qc of its row, and T2, the affine fold, if the USER rules them;  form (ii) stays open. | DONE 2026-10-20 (RULED 2026-10-20, (a) to (h) = the recommendations of docs/M12-PLAN.md section 9;  s1 (T1) 152c6d4 with the plan, s2 (T2) 54cda59, s3 close;  form (iv) DONE;  form (ii) stays O24, open, no milestone named) |
| M13 | The kycMerkle depth (topic (ii), RULED 2026-10-20):  depth 4 to depth 20 (21,922 rows, k 15, the pinned SRS);  reverses M12 ruling (e);  no IR change. | DONE 2026-10-20 (RULED 2026-10-20, (a) to (k) = the recommendations of docs/M13-PLAN.md section 9;  s1 (depth 20) 79fe06d with the plan, the gates GREEN in s1-b, s2 close;  kycMerkle depth 20, 21,922 rows, k 15;  O24 form (ii) and O23 stay open, no milestone named) |
| M14 | The pow5 gate (O24 form (ii-b)):  one IR form, the gate pow5 with the fixed columns q5 and c5, for the S-box of poseidon, transfer, kyc and kycMerkle;  all section 9 recommendations accepted by the USER. | DONE 2026-10-21 (RULED 2026-10-20, (a) to (k) = the recommendations of docs/M14-PLAN.md section 9, including the IR ruling (b);  kickoff 122b00f, s1 (the pow5 gate, (ii-b), per circuit) 083b93d, s1-b (the 4 verifiers and fixtures, the gates GREEN) 00155c7, s2 close;  kycMerkle 16,642 rows, k 15;  O24 CLOSED;  O23 stays open, no milestone named) |
| M15 | k 14 for kycMerkle (USER pick 2026-10-21):  the hint merge (H1), a pow5-checked S-box hint has no input row;  kycMerkle depth 20, 16,642 to 14,882 rows (MODEL), k 15 to 14;  a ruling on the lowering rule D14, no IR form. | DONE 2026-10-21 (RULED 2026-10-21, (a) to (k) = the recommendations of docs/M15-PLAN.md section 9, including the lowering-rule ruling (b);  kickoff ae82f95, s1 (the hint merge H1) e23bc92, s1-b (the 4 verifiers and fixtures, the gates GREEN) 8192f9d, s2 close;  kycMerkle 14,882 rows, k 14;  no IR form, no open item;  O23 stays open, no milestone named) |
| M16 | A deeper Merkle tree for kycMerkle at k 14 (USER pick 2026-10-21):  depth 20 to 22 (the max at k 14, MODEL), 14,882 to 16,236 rows, k 14 stays;  a new circuit text, no IR form, no src/ change. | DONE 2026-10-21 (RULED 2026-10-21, (a) to (k) = the recommendations of docs/M16-PLAN.md section 9;  kickoff 0e7efcb, s1 (depth 22, the circuit text and the 2 goldens) 3a1676c, s1-b (the kycMerkle verifier and fixture, the gates GREEN) 0625a5d, s2 close;  kycMerkle depth 22, 16,236 rows, k 14;  no IR form, no src/ change, no open item;  O23 stays open, no milestone named) |
| M17 | Depth 23 for kycMerkle at k 15 (USER pick 2026-10-21):  depth 22 to 23, 16,236 to 16,913 rows (MODEL), k 14 to 15 (the pinned k 15 SRS);  a new circuit text, no IR form, no src/ change. | DONE 2026-10-22 (RULED 2026-10-21, (a) to (k) = the recommendations of docs/M17-PLAN.md section 9;  kickoff e2e1389, s1 (depth 23, the circuit text and the 2 goldens) 6c333a7, s1-b (the kycMerkle verifier and fixture, the gates GREEN) 1e8a0da, s2 close;  kycMerkle depth 23, 16,913 rows, k 15;  no IR form, no src/ change, no open item;  O23 stays open, no milestone named) |
| M18 | O3 and O23, then the close (USER pick 2026-10-22, "Let's do O23 and O3 and then conclude"):  O3, CIRCUIT_UNCONSTRAINED on the final rows (the copy classes, the cancellation, the elimination);  O23, the PPoT check of the ceremony file (`glue-halo2 srs-check`) and the deployment (the USER facts, the USER broadcast);  the last milestone. | DRAFT 2026-10-22 (docs/M18-PLAN.md;  section 9, (a) to (l), for the USER) |

## Lowering rules (all milestones)

| Former | Lowering |
|---|---|
| Product, Sigma over data | A tuple of wires |
| Coproduct | A boolean tag wire; both arms are constrained under a selector |
| Pi over a finite domain, fold | Unrolled; dependent types carry the static sizes |
| unfold | Needs a static depth bound |
| Path in a set (Field) | An equality constraint (a copy or an `arith` row) |
| `transp` along `ua e` | The forward map of `e`, at compile time |
| Quotient eliminator | Runs on representatives |

A proof stays a term. It is not erased because of its sort; the lowering
has nothing to emit for a path in a type with no wires.
