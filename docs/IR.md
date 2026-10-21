# Plonkish IR, version 1

This file is the contract between the glue-lang compiler (`build/langc`, C,
TinyCC) and the halo2 back end (`halo2/`, Rust). The compiler writes two
JSON documents. The back end reads them, builds a halo2 circuit, and makes
the keys, the proofs and the Solidity verifier.

## 1. Field

All values are elements of the BN254 scalar field Fr:

    r = 21888242871839275222246405745257275088548364400416034343698204186575808495617

A value is a JSON string that holds a decimal integer in `[0, r)`. A reader
refuses any other form (sign, leading zeros, hex, a JSON number).

## 2. The circuit document (`NAME.circuit.json`)

```json
{
  "glue-lang-plonkish": 1,
  "name": "square",
  "field": "bn254-fr",
  "k": 9,
  "rows": 3,
  "advice": 3,
  "fixed": 7,
  "instance": 1,
  "gates": [{"name": "arith", "polys": [EXPR]}],
  "lookups": [{"name": "range8", "inputs": [EXPR], "tables": [EXPR]}],
  "copies": [[CELL, CELL]],
  "fixed_values": [["0", "1", "0"]],
  "public": 1
}
```

| Key | Meaning |
|---|---|
| `k` | The domain has `2^k` rows. |
| `rows` | The number of rows that the circuit uses, from row 0. The back end refuses the document when `rows` is more than the usable rows of `2^k` (halo2 keeps the last rows for blinding). The compiler selects the smallest `k` with `max(rows, L) + 16 <= 2^k`, where `L` is the length of the longest lookup table (0 when there is no lookup). A table column must also fit in the usable rows, so `2^k >= L` is not sufficient. |
| `advice`, `fixed`, `instance` | The number of columns of each kind. |
| `gates` | Each poly must be zero on every row of the domain. A gate uses fixed columns as selectors, so a poly is zero on rows that the gate does not use. |
| `lookups` | For each row, the tuple of `inputs` must be equal to the tuple of `tables` on some row. `inputs` and `tables` have the same length. Tables use fixed columns only. Each table must contain the tuple of zeros, so that rows with a zero selector pass. |
| `copies` | Each pair of cells must hold equal values (the permutation argument). The back end enables equality on each column that a copy names. |
| `fixed_values` | One array for each fixed column, of length `rows`. The back end fills the other rows with zero. A lookup table column can be longer than `rows`, up to the usable rows. |
| `public` | The number of public inputs. They are rows `0` to `public - 1` of instance column 0. |

`EXPR` is one of:

| Form | Meaning |
|---|---|
| `{"c": "5"}` | A constant |
| `{"a": [COL, ROT]}` | Advice column `COL` at rotation `ROT` (an integer, can be negative) |
| `{"f": [COL, ROT]}` | Fixed column `COL` at rotation `ROT` |
| `{"i": [COL, ROT]}` | Instance column `COL` at rotation `ROT` |
| `{"+": [EXPR, EXPR]}` | Sum |
| `{"*": [EXPR, EXPR]}` | Product |
| `{"-": EXPR}` | Negation |

`CELL` is `{"col": "advice" | "fixed" | "instance", "index": N, "row": N}`.

## 3. The witness document (`NAME.witness.json`)

```json
{
  "glue-lang-witness": 1,
  "name": "square",
  "advice": [["3", "3", "9"]],
  "instance": [["9"]]
}
```

`advice` has one array for each advice column, of length `rows`. `instance`
has one array for each instance column; column 0 has `public` values. The
back end fills the other rows with zero. The names in the two documents
must be equal.

## 4. The gate set

The compiler emits the gate arith and emits the lookup range8 when a
circuit uses it.  M14 adds the gate pow5 when a circuit has a pow5 row;
the back end builds any document that section 2 allows.

Advice columns are a, b and c (0, 1 and 2).  The instance column is 0.
Fixed columns are ql, qr, qo, qm, qc, qrange and t (0 to 6).
A circuit with a pow5 row also has q5 and c5 (7 and 8).

    arith:  ql*a + qr*b + qm*a*b + qc - qo*c = 0
    range8: (qrange * a) in t,  t = 0, 1, ..., 255
    pow5:   q5 * (b - (a + c5)^2) = 0
            q5 * (c - b^2 * (a + c5)) = 0

The lowering row kind ROW_POW5 has a = s, c5 = the affine constant,
b = (s + c5)^2 and c = (s + c5)^5.  q5 is 1 on this row;
ql, qr, qo, qm, qc and qrange are 0.  A copy ties a to s.
The b cell is an intermediate witness value and has no operand copy.
The result wire is c.  q5 and c5 are 0 on other rows.

The two pow5 polynomials have degree 3 and 4.  The arith gate and the
range8 lookup keep their forms;  no advice column or rotation is added.
Circuits without a pow5 row keep 7 fixed columns and omit the pow5 gate.

## 5. Solidity interface

The verifier contract that the back end writes has this function:

    function verifyProof(bytes calldata proof, uint256[] calldata instances)
        public view returns (bool);

`instances` is instance column 0, in row order, of length `public`. The
function returns true or reverts. The selector and the ABI encoding of the
arguments are those of `verifyProof(bytes,uint256[])`, so a caller can use
an `external view` interface.

The verifier requires the canonical ABI layout emitted by
`abi.encodeWithSelector` and the CLI. It rejects redirected proof or
instance offsets, so the assembly checks the arguments Solidity decoded.
