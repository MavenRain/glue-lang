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
| M2 | Path types: interval `I`, `Path A a b` replaces the template equality formers F12 and F13. `refl`, `sym`, `cong`, `funExt` by computation. The checker gets interval variables and the face lattice. | PLANNED |
| M3 | Kan operations `transp` and `hcomp` for Pi, Sigma, Path, universes, coproducts. Glue types, `ua`, uaBeta by computation. | PLANNED |
| M4 | HITs: point and path constructors, higher constructors, `hcomp` values. Smoke: circle, propositional truncation, set quotient `A / R` with `[_]`, `eq/`, `squash/`, `rec`. | PLANNED |
| M5 | QITs (quotient inductive types, with inductive-inductive definitions). | PLANNED |
| M6 | Monads (map, bind), algebras (fold) and coalgebras (unfold) over the cubical core. Two monads at lowering: the constrained monad and the hint monad (advice values with no constraint). | PLANNED |
| M7 | Direction 2: circuits quotiented by relation equivalence. The lowering of `rec` runs on representatives; `eq/` and `squash/` cases are checker obligations with no circuit cost. | PLANNED |
| M8 | Direction 4: HIT specification, equivalence to the circuit relation, transport by `ua`. Poseidon gadget. Telcoin private-transfer circuit. | PLANNED |
| M9 | ZK-KYC credential circuit. Ceremony SRS ingestion. Deployment to a Telcoin network (USER runs the broadcast). | PLANNED |

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
