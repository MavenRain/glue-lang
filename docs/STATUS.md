# glue-lang status

The build implements milestone M1 of docs/PLAN.md: the vertical slice from
a `circuit` declaration to a Solidity verifier. It has the lang-template
core of M0 (the sample domain, the former checker, the JSON target). It
does not have the cubical part: Path types and the interval are M2.

## Implemented

- `Field`: the BN254 scalar field Fr, literals and infix `+ - *`
  (src/front/fr.c, 32-bit limbs).
- `circuit NAME public (...) private (...) := C; C; ...`: constraints
  `a == b` on Field terms and `range8 e`. Helper definitions inline at
  lowering (examples/poly.glue).
- Lowering to Plonkish IR v1 (docs/IR.md): one `arith` gate, one `range8`
  lookup, copies. `langc circuit`, `langc witness`, `langc satcheck`.
- `glue-halo2` (halo2/): PSE halo2 KZG on BN254 with SHPLONK. `check`,
  `keygen`, `prove`, `verify`, `solidity`.
- `contracts/`: one Solidity verifier for each example circuit, the Forge
  tests and the deploy script.
- `make e2e`: for each example circuit (square, range, poly), the whole
  chain from `langc circuit` to `forge test` and a simulation of the
  deploy script.

## Remaining work

1. M2: Path types and the interval (docs/PLAN.md). It needs the M1 commit
   first.

## Known limits

- `make e2e` uses the INSECURE development SRS (`--dev-srs`). A real
  deploy needs `--srs FILE` from a trusted setup.
- The prover is randomized. Each `make e2e` writes new proofs to
  `contracts/test/fixtures/`.
- CIRCUIT_UNCONSTRAINED is a syntactic check (SPEC.md O3). A private
  binder that is only in `x == x` passes it.
- No refusal test for CIRCUIT_LOWER (SPEC.md O4).
- The Solidity generator takes one instance column, no rotated instance
  query and at least one advice column (halo2/README.md).

## Internal boundaries

The C compiler (`src/`, TinyCC) holds the front end, the lowering and the
witness. `test/gate.sh` checks the examples, the refusal tables, the
circuit goldens (`test/circuit/`) and the satcheck refusals
(`test/satcheck/`). docs/IR.md is the only interface between the compiler
and the back end.

The halo2 back end (`halo2/`, Rust) reads IR v1 and refuses each document
that IR.md does not allow. `cargo test --manifest-path halo2/Cargo.toml`
checks the hand fixtures and the compiler goldens.

The Foundry project (`contracts/`) holds the generated verifiers. Forge
tests check each verifier against its proof fixture. The deploy script
selects a verifier by name and reads the key and the chain from the forge
flags.

## M1 integration fixes (session 4)

- docs/IR.md section 2: the k rule said `2^k` at least each table length.
  That gives k 8 for `range`, and the back end refuses that document (the
  256-row table does not fit in the 250 usable rows). IR.md now has the
  compiler rule: `max(rows, L) + 16 <= 2^k`.
- docs/IR.md section 5: the generated verifier has
  `public view returns (bool)`, not `external returns (bool)`. The
  selector and the ABI are the same, so IR.md now gives the generated
  signature.
- contracts/: the Forge test and the deploy script covered `square` only.
  They now cover each example circuit. The verifiers moved from
  `src/Halo2Verifier.sol` to `src/verifiers/NAME.sol`.
