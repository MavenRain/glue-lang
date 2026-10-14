# glue-halo2

The halo2 back end of glue-lang. It reads the Plonkish IR v1 documents
(docs/IR.md) and makes keys, proofs and a Solidity verifier.

## Proving system

- PSE `halo2_proofs`, KZG on BN254, SHPLONK openings (BDFG21 in the
  verifier), Keccak256 EVM transcript.
- PSE `halo2-solidity-verifier` for the verifier contract and the calldata.

## Pins

- `halo2_solidity_verifier`: rev `9e99730a12f9203d46d9b056662440de2d305ca8`
  (main HEAD on 2026-10-14).
- `halo2_proofs`: tag `v0.3.0`, which is commit
  `73408a140737d8336490452193b21f5a7a94e7de`. Cargo.lock pins the hash.

The verifier rev declares `halo2_proofs` with `tag = "v0.3.0"`. Cargo sees a
tag and a rev as two sources, even for one commit. So this crate names the
same tag, and the build has ONE `halo2_proofs`. That commit has the
`circuit-params` feature: the circuit shape is `Circuit::Params`, and
`configure_with_params` builds the columns, gates and lookups from data.

## Commands

    glue-halo2 check    --circuit C --witness W
    glue-halo2 keygen   --circuit C (--srs FILE | --dev-srs) --keys DIR
    glue-halo2 prove    --circuit C --witness W --keys DIR --out DIR
    glue-halo2 verify   --circuit C --keys DIR --proof FILE.json
    glue-halo2 solidity --circuit C --keys DIR --out FILE.sol

- `check` runs the mock prover.
- `keygen` writes `srs.bin`, `vk.bin` and `pk.bin` to DIR. `--srs` reads a
  halo2 KZG params file and downsizes it to the circuit `k`. `--dev-srs`
  makes an SRS from a fixed seed. It is INSECURE: use it for local tests
  only.
- `prove` runs the mock prover first, then writes `proof.bin` and
  `proof.json` to DIR. `proof.json` has `proof` (0x hex), `instances`
  (instance column 0, 0x hex uint256), `columns` (every instance column)
  and `calldata` (for `verifyProof(bytes,uint256[])`).
- `verify` checks a proof natively. It takes the instances from `columns`.
  It refuses a `proof.json` whose `instances` or `calldata` does not agree
  with `proof` and `columns`.
- `solidity` writes the verifier contract. The verifying key is in the
  contract, so there is one contract to deploy.

The Solidity generator takes at most one instance column, no rotated
instance query and at least one advice column. Other circuits are
refused with an `Unsupported` error.


Loading keys rebuilds the verifying key from the circuit document and
checks the saved fixed and permutation commitments. Changed coefficients
or copies require new keys. Native verification also checks the document's
declared public input count. The generated verifier requires canonical ABI
argument offsets, matching the CLI calldata.

## Tests

    cargo test --manifest-path halo2/Cargo.toml -j 2

The tests use the hand fixtures in `tests/fixtures/` and the compiler
goldens in `../test/circuit/`.
