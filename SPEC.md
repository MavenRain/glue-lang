# glue-lang specification (draft)

Status: draft before milestone M0. `glue-lang` is a working name.

Fill each section. The quoted line under each heading says what to write
there. Delete the quoted line when the section is complete.

## 1. Purpose

> Write here: what one program describes, what the compiler checks, and what
> it writes. Name the host and the target. Say that the core types and core
> operations are only those of `design/DESIGN.md`.

glue-lang is a language for <the domain>. Its type formers are F1 to F15 of
`formers/FORMERS.md`. Its core data types and core operations are only the
types and operations of `design/DESIGN.md` (the design).

## 2. Programs

> Write here: the form of a program and the refusal list. Copy the host
> forms from `formers/tcc-json.md`. Add each fixed first definition that the
> domain needs (for example a size parameter).

A program is a sequence of definitions:

```
def NAME : TYPE := TERM
```

The compiler refuses these host forms in a program: <the list from
`formers/tcc-json.md`>. It also refuses a definition that uses a core name or
a prelude name. Thus a program cannot add a data type, an unproved fact or
general recursion. Recursion comes only from `fold` and `unfold` (F6, F7).
The compiler writes the target; a program cannot.

### 2.1 Field

`Field` is the scalar field of BN254 (Fr). The modulus r is
21888242871839275222246405745257275088548364400416034343698204186575808495617.
A Field literal is a decimal below r. `+`, `-` and `*` are modulo r. `*` binds
more tightly than `+` and `-`. All three associate to the left.

### 2.2 Circuits (M1)

A program can also declare circuits:

```
circuit NAME [public BINDERS] [private BINDERS] := C (; C)*
BINDERS := (x : Field) (y : Field) ...
C       := SUM == SUM | range8 SUM
```

Example (`examples/square.glue`):

```
circuit square public (y : Field) private (x : Field) := x * x == y
```

The public binders are the public inputs of the proof. The private binders
are the witness. A constraint `a == b` tells the prover to show that the two
Field values are equal. In M1 it is a constraint form. M2 makes it a Path
type. `range8 e` tells the prover to show that 0 <= e < 256. A constraint can
call an ordinary definition of a Field-valued function. The call inlines at
lowering. `circuit`, `public`, `private` and `range8` are keywords. `witness`
is a core name, so the witness binders use the keyword `private`.

### 2.3 Refusal codes for Field and circuits

| Code | Exit | The compiler refuses |
|---|---|---|
| FIELD_RANGE | 1 | a Field literal that is not below r |
| FIELD_ARITH | 1 | `+`, `-` or `*` on a term that is not a Field |
| CIRCUIT_BINDER | 1 | a circuit binder whose type is not Field |
| EQ_FIELD | 1 | a side of `==` that is not a Field |
| RANGE8_FIELD | 1 | a `range8` operand that is not a Field |
| REFUSE_NAME | 1 | two circuits with one name |
| CIRCUIT_LOWER | 1 | a constraint side whose normal form is not a Field literal, a binder or `+ - *` of these |
| CIRCUIT_UNCONSTRAINED | 1 | a private binder that is in no constraint |
| CIRCUIT_UNUSED_PUBLIC | 1 | a public binder that is in no constraint |
| CIRCUIT_ENTRY | 1 | `circuit` or `witness` with a name that is not a circuit |
| WITNESS_FAIL | 1 | a witness that does not satisfy a constraint |
| WITNESS_ARGS | 2 | a witness argument that is not `BINDER=VALUE`, a missing or repeated binder, or a value that is not a decimal below r |
| SAT_JSON | 1 | a satcheck document that is not JSON of the IR subset |
| SAT_IR | 1 | a satcheck document that does not obey docs/IR.md |
| SAT_GATE | 1 | a gate poly that is not zero on a row |
| SAT_LOOKUP | 1 | a lookup input that is not in its table |
| SAT_COPY | 1 | a copy that joins two cells with different values |

`langc check` lowers each circuit, so it also gives the lowering refusals.

## 3. Type formers

> Write here: only the deviations from the formers. Do not copy the forms.
> Give each former whose status in `formers/tcc-json.md` is not DONE, and the
> open item (section 9) that records it.

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-json column of the realization matrix. `formers/tcc-json.md` gives
the host form of each former.

| ID | Status on tcc-json | Effect on this language | Open item |
|---|---|---|---|
| | | | |

## 4. Structures

> Write here: the carriers of Monad (F5), fold and unfold (F6, F7) and
> filter (F8) that the design uses. Name each carrier that the host does not
> give.

- **Monad**: `pure`, `map`, `bind` on <carriers>.
- **Algebra**: `fold` on <carriers>. `unfold` on <carriers>, or the reason
  that there is no `unfold`.
- **Filterable**: `filter` on <carriers>.

## 5. Core types

> Write here (DOMAIN SLOT): only the types of `design/DESIGN.md`. Give each
> type its meaning in the design and its definition in host forms. Mark each
> definition that a probe fact forces with "probe-forced".

| Type | Meaning (design section) | Definition |
|---|---|---|
| | | |

## 6. Core operations

> Write here (DOMAIN SLOT): only the operations of `design/DESIGN.md`. Give
> each operation its type and its meaning in the design.

| Operation | Type | Meaning (design section) |
|---|---|---|
| | | |

## 7. Target and instance encoding

> Write here: what the compiler writes. For a JSON target, give the instance
> rule and the encoding table. For a contract target, give the storage, the
> entries and the guards. Say how each proof erases.

An instance is a top-level definition whose type is a data type (F14).

| Type | Encoding |
|---|---|
| | |

### 7.1 Circuits: lowering to the Plonkish IR v1

`langc circuit PROG NAME` writes NAME.circuit.json. `langc witness PROG NAME
BINDER=VALUE...` writes NAME.witness.json. docs/IR.md is the contract for the
two documents. M1 uses the gate set of docs/IR.md section 4: advice a, b, c;
fixed ql, qr, qo, qm, qc, qrange, t; instance column 0; gate `arith` with
ql*a + qr*b + qm*a*b + qc - qo*c = 0; lookup `range8` with input qrange*a and
table t.

The compiler evaluates each constraint side to a normal form, with the binders
as variables. Then it lowers the normal form:

1. Rows 0 to n-1 are input rows, one for each binder (public binders first).
   The value of the binder is in advice a.
2. A Field literal k is a constant row: qc = k, qo = 1. The value is in c.
3. Each `+`, `-` or `*` is an arith row. Copies tie a and b to the wires of
   the operands. The value is in c. `+` sets ql = qr = qo = 1. `-` sets
   ql = 1, qr = r - 1, qo = 1. `*` sets qm = qo = 1.
4. `a == b` is a copy between the two wires. If the two wires are one cell,
   the compiler writes no copy.
5. `range8 e` on a binder sets qrange = 1 on the input row of the binder.
   On any other wire, it adds a range row: a copy of the wire in a, and
   qrange = 1.

The copies are in this order: instance(0, i) to advice(a, i) for each public
binder i; for each row, a then b; then the `==` copies.

The t column holds 0 to 255 when the circuit uses `range8`. Otherwise t is
zero on each row, and `lookups` is empty.

k rule: k is the smallest k with max(rows, 256 if the circuit uses range8)
+ 16 <= 2^k. The 16 rows are room for the blinding rows of the prover. This
is the rule of docs/IR.md section 2. The halo2 back end refuses a document
whose rows or table columns do not fit in the usable rows of 2^k.

`langc satcheck CIRCUIT.json WITNESS.json` reads the two documents again. It
checks each gate poly on each of the 2^k rows, each lookup and each copy. It
prints `ok` or gives one refusal. It accepts k up to 16.

## 8. Host and target

> Write here: the host, its pinned commit (`PIN`), the facts from
> `probe/CAPABILITY.md` that set the design, and the check that the compiler
> runs on its output (for example the axiom report).

## 9. Open items

> Write here: one item for each open question. Number the items O1, O2 and
> so on. Mark each decision as "RULED YYYY-MM-DD (USER): <the ruling>". Mark
> each fact that a probe found as "Probe-forced".

- O1. <question>.
- O2. Closed in M1. The k rule of section 7.1 was stricter than docs/IR.md
  section 2. The IR.md rule (2^k >= the table length) gives k 8 for
  `range`, and the back end refuses that document: the 256-row table does
  not fit in the usable rows. docs/IR.md now has the rule of section 7.1.
- O3. CIRCUIT_UNCONSTRAINED is a syntactic check. A private binder that is
  only in `x == x` or `x - x == 0` passes the check, but the proof does not
  constrain it.
- O4. No gate test gives CIRCUIT_LOWER yet. M1 has no Field-valued normal
  form other than literals, binders and `+ - *`.

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
