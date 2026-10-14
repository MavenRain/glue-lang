# glue-lang

glue-lang is a domain-specific language. It comes from lang-template with
the tcc-json host.

- `SPEC.md`: the specification. Fill each section.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `formers/FORMERS.md`: the type formers. `formers/tcc-json.md` gives their
  status on the tcc-json host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.

## Commands

Build with `make`. The compiler is `build/langc`. Exit 0 is ok, exit 1 is a
refusal of the program or the document, and exit 2 is a usage or IO error.

| Command | What it does |
|---|---|
| `langc check PROG` | Checks the program and lowers each circuit. Prints `ok`. |
| `langc eval PROG NAME [ARGS...]` | Evaluates the definition NAME. |
| `langc build PROG [-o OUT]` | Writes the JSON target. |
| `langc circuit PROG NAME [-o OUT]` | Writes the Plonkish IR of the circuit NAME to NAME.circuit.json (or OUT). |
| `langc witness PROG NAME BINDER=VALUE... [-o OUT]` | Computes the witness of the circuit NAME and writes NAME.witness.json (or OUT). Refuses when a constraint fails. |
| `langc satcheck CIRCUIT.json WITNESS.json` | Checks each gate, lookup and copy of the two documents. Prints `ok`. |

Example:

```
build/langc circuit examples/square.glue square
build/langc witness examples/square.glue square y=9 x=3
build/langc satcheck square.circuit.json square.witness.json
```

`make check` runs the gate (test/gate.sh) and `make check-clang` runs clang
with -Wall -Wextra -Wswitch-enum -Werror. SPEC.md section 2.2 gives the
circuit syntax and section 7.1 gives the lowering.

## Pipeline

A circuit goes from the compiler to an EVM verifier in these steps.
`docs/IR.md` is the contract between `langc` and the halo2 back end
(`halo2/`, `glue-halo2`). The verifiers are a Foundry project in
`contracts/`.

```
build/langc circuit examples/square.glue square -o build/square.circuit.json
build/langc witness examples/square.glue square y=9 x=3 -o build/square.witness.json
build/langc satcheck build/square.circuit.json build/square.witness.json
cargo build --manifest-path halo2/Cargo.toml -j 2
halo2/target/debug/glue-halo2 keygen --circuit build/square.circuit.json --dev-srs --keys build/square-keys
halo2/target/debug/glue-halo2 prove --circuit build/square.circuit.json --witness build/square.witness.json --keys build/square-keys --out build/square-proof
halo2/target/debug/glue-halo2 verify --circuit build/square.circuit.json --keys build/square-keys --proof build/square-proof/proof.json
halo2/target/debug/glue-halo2 solidity --circuit build/square.circuit.json --keys build/square-keys --out contracts/src/verifiers/square.sol
cp build/square-proof/proof.json contracts/test/fixtures/square.proof.json
forge test --root contracts
```

`make e2e` runs these steps for each example circuit in
`test/circuit/expect.txt` (square, range, poly). Then it runs
`forge test` and a simulation of the deploy script for each circuit (no
RPC, no broadcast). It needs `cargo` and `forge` on the PATH.

`--dev-srs` is INSECURE. It is for local tests only. For a real deploy,
use `--srs FILE` with a KZG params file from a trusted setup, and run
`keygen`, `prove` and `solidity` with the same keys.

## Deploy on Telcoin

The deploy script reads the chain from the RPC and the key from a keystore.
Import the deploy key one time (`cast wallet import <keystore>
--interactive`). Simulate first, without `--broadcast`:

```
forge script contracts/script/DeployVerifier.s.sol --root contracts \
    --sig 'run(string)' square --rpc-url $TELCOIN_RPC_URL --account <keystore>
```

Then add `--broadcast` to deploy. Replace `square` with `range` or `poly`
for the other verifiers. `contracts/README.md` gives the local anvil
deploy.
