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
Field values are equal. `a == b` is the type `Path Field a b` (section 2.4):
for each constraint, the checker forms that type, and the lowering reads its
two ends (section 7.1). The constraint form and the IR did not change in M2
(ruling 2026-10-14). `range8 e` tells the prover to show that 0 <= e < 256. A constraint can
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

### 2.4 Paths and the interval (M2)

The interval `I` is the De Morgan interval of CCHM (ruling 2026-10-14).
`I` is not a type. It is not in `Type n`, and `Path I i0 i1` is refused
(PATH_TYPE). The plan and the design calls are in docs/M2-PLAN.md.

| Form | Meaning | Precedence |
|---|---|---|
| `I` | The interval. Only in a `PathP` line and in an interval binder. | |
| `i0`, `i1` | The end points of the interval. Core names. | atom |
| `~r` | Reversal. | tightest, prefix |
| `r /\ s` | Meet (min). | above `\/`, left |
| `r \/ s` | Join (max). | lowest, left |
| `Path A a b` | Paths in `A` from `a` to `b`. | application |
| `PathP (<i> A) a b` | Paths over the line `<i> A`. | application |
| `<i> t` | Path abstraction. The body extends as far as possible (as `fun`). | binder |
| `p @ r` | Path application. Left associative, at the application level: `f x @ i` is `(f x) @ i`. The right operand is an interval atom: `i0`, `i1`, a variable, `~` and an atom, or an interval term in parentheses. | application |
| `fun (i : I) => t`, `(i : I) -> T` | An interval binder. The type is in the sort `SSet`. | binder |
| `Partial phi A` | The partial elements of `A` on the face `phi`. | application |
| `[ phi1 -> t1, phi2 -> t2 ]` | A system: a partial element, one branch for each face. | atom |

The M2 tokens are `<`, `>`, `@`, `~`, `/\`, `\/`, `[`, `]` and `,`. The
parser refuses `<i` with no `>`, `p @` with no interval atom, `/\` or `\/`
with no left operand, a system branch with no body and a system with no `]`.

**Interval values.** An interval term evaluates to a normal form in the
free De Morgan algebra on the interval variables: a disjunction of
conjunctions of literals (`i` or `~i`), where no conjunction contains
another. `i0` is the empty disjunction. `i1` is the disjunction of the empty
conjunction. Two interval terms are equal when their normal forms are equal.
`i /\ ~i` is not `i0`, and `i \/ ~i` is not `i1`: the interval is a De
Morgan algebra, not a Boolean algebra. A normal form with more than 64
conjunctions is refused (INTERVAL_LIMIT).

**Typing rules.**

| Rule | Premises | Conclusion |
|---|---|---|
| Path former | `L` is a line `<i> A` with `A : Type n` for a fresh interval variable `i`; `a : A[i := i0]`; `b : A[i := i1]` | `PathP L a b : Type n` |
| Path sugar | `A : Type n`, `a : A`, `b : A` | `Path A a b` is `PathP (<_> A) a b` |
| Abstraction (check) | Against `PathP L a b`: `t : L @ i` under the fresh interval variable `i`; `t[i := i0]` converts to `a`; `t[i := i1]` converts to `b` | `<i> t : PathP L a b` |
| Application (infer) | `p : PathP L a b`; `r` is an interval term | `p @ r : L @ r` |

`<i> t` with no expected type is refused (PATH_EXPECTED). Thus `sym (<i> 1)`
is refused. Write `def reflOne : Path Nat 1 1 := refl`, then `sym reflOne`.
An interval variable where a term of a type is necessary, or a term of a
type where an interval term is necessary, gives INTERVAL_TERM. An unknown
name in an interval position gives NAME_UNKNOWN or REFUSE_REC from the name
lookup, not INTERVAL_TERM.

**Evaluation and conversion.**

- `(<i> t) @ r` evaluates to `t[i := r]`.
- When `p : PathP L a b` is neutral, `p @ i0` evaluates to `a` and `p @ i1`
  evaluates to `b`. The checker copies the two ends from the type of `p`
  into the core term. Any other `p @ r` with a neutral `p` is neutral.
- Conversion compares interval values by their normal forms. Path eta:
  when one side is a path abstraction, conversion compares `p @ i` and
  `q @ i` for a fresh interval variable `i`. Functions have eta too.
- No rule makes two values equal because of their type. Two different
  variables of one path type do not convert (check row
  no-proof-irrelevance.lang). Proof irrelevance is prohibited (docs/PLAN.md,
  Rulings).

**Derived operations.** Each operation is a checker rule. It writes an
ordinary core path term, so each law below holds by computation (beta for
`<i>`, the end point rule, the De Morgan normal form and path eta).
`refl`, `sym`, `cong`, `funExt` and `happly` are core names, so a definition
with one of these names is refused (REFUSE_NAME).

| Operation | Type | Core term |
|---|---|---|
| `refl` (check) | `PathP L a b` with a constant `L`, where `a` converts to `b` | `<i> a` |
| `sym p` | `p : PathP L a b` gives `PathP (<i> L @ ~i) b a` | `<i> p @ ~i` |
| `cong f p` | `f : (x : A) -> B x`, `p : Path A a b` gives `PathP (<i> B (p @ i)) (f a) (f b)` | `<i> f (p @ i)` |
| `funExt h` | `h : (x : A) -> PathP (<i> B x) (f x) (g x)` gives `Path ((x : A) -> B x) f g` | `<i> fun x => h x @ i` |
| `happly p x` | `p : Path ((x : A) -> B x) f g` gives `Path (B x) (f x) (g x)` | `<i> (p @ i) x` |

`happly` takes two arguments. `funExt` checks against
`Path ((x : A) -> B x) f g`. With no expected type, it reads `f` and `g`
from the ends of the codomain of `h`. examples/paths.lang states each law
as a `refl`: `sym (sym p) = p`, `sym refl = refl`, `cong (fun x => x) p =
p`, `cong g (cong f p) = cong (fun x => g (f x)) p`, `cong f refl = refl`,
`funExt (fun x => refl) = refl`, `happly (funExt h) = h`, `funExt (happly
p) = p`, the end points of a variable, and the connection square:

```
def connection : (p : Path Nat 1 2) -> PathP (<i> Path Nat 1 (p @ i)) refl p
  := fun p => <i> <j> p @ (i /\ j)
```

The template formers F12 and F13 (`Eq`, `symm`, `trans`, `transport`) are
not in glue-lang (ruling 2026-10-14, section 3). `Eq` is an unknown name
(NAME_UNKNOWN). `trans`, `transport`, `transp`, `hcomp`, `Glue` and `ua`
are reserved names (REFUSE_NAME). M3 defines them with the Kan operations.

**Faces, partial elements and systems.** A face is an interval term read as
"the term is `i1`". The face lattice is the De Morgan algebra divided by
`i /\ ~i = i0`. A face in normal form is an antichain of consistent
conjunctions. Each conjunction is an assignment: `i` means `i = 1`, and
`~i` means `i = 0`.

- `Partial phi A` with `A : Type n` is in the sort `SSet`.
- A system checks only against a declared type `Partial phi A`
  (PARTIAL_EXPECTED). For each conjunction of each branch face, the checker
  sets the interval variables of the assignment to `i0` or `i1` in the
  context, then checks the body against `A`. Where two branch faces meet,
  the two bodies must convert (FACE_AGREE). The join of the branch faces
  must equal `phi` (FACE_COVER). The checker checks FACE_COVER first.
- An interval binder `fun (i : I) => t` and an interval Pi `(i : I) -> T`
  are in `SSet`. A Pi with a domain or a codomain in `SSet` is in `SSet`.
  In an application `f r` where the domain of `f` is `I`, `r` is an
  interval term.
- `SSet` is a sort, not a binder type. These are refused (SORT_FIBRANT): a
  type in `SSet` as the type of a path, as a Sigma component, under a type
  former or as the type of an instance; `SSet` as a binder type.
- M2 has no application of a partial element (docs/PLAN.md, M3 row).

```
def sysEnds : (i : I) -> Partial (~i \/ i) Nat
  := fun (i : I) => [ ~i -> 0, i -> 1 ]
```

**JSON.** A definition of a Path type is a proof, not an instance. A Path
component of a data value writes `null`: the ends are in the type (O5). An
instance whose type is `Partial phi A` is refused (O6).

**Refusal codes (M2).**

| Code | Exit | The compiler refuses |
|---|---|---|
| PATH_EXPECTED | 1 | `<i> t` against a type that is not a Path, or with no expected type; `p @ r` where `p` is not a path; `cong` with an argument that is not a path |
| PATH_BOUNDARY | 1 | `<i> t` whose end at `i0` or `i1` does not convert to the end of the type. The message gives the end and the two normal forms |
| PATH_TYPE | 1 | `Path A a b` or `PathP (<i> A) a b` where `A` is not in `Type n` (for example `I`) |
| INTERVAL_TERM | 1 | an interval term where a term of a type is necessary, or the opposite |
| INTERVAL_LIMIT | 1 | an interval normal form with more than 64 conjunctions |
| PARTIAL_EXPECTED | 1 | a system with no declared type `Partial phi A` |
| FACE_AGREE | 1 | two branches of a system that do not convert where their faces meet. The message gives the two faces, their meet and the two values |
| FACE_COVER | 1 | a system whose branch faces do not join to the face of its `Partial` type |
| SORT_FIBRANT | 1 | a type in `SSet` as the type of a path, a Sigma component, under a type former or as an instance; `SSet` as a binder type |

## 3. Type formers

> Write here: only the deviations from the formers. Do not copy the forms.
> Give each former whose status in `formers/tcc-json.md` is not DONE, and the
> open item (section 9) that records it.

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-json column of the realization matrix. `formers/tcc-json.md` gives
the host form of each former.

| ID | Status on tcc-json | Effect on this language | Open item |
|---|---|---|---|
| F12 | PARTIAL (M2) | `Path A x y` replaces `Eq` (ruling 2026-10-14). `refl` and `sym` compute (section 2.4). `trans` is reserved: M3 defines it with `hcomp`. | none (docs/PLAN.md, M3 row) |
| F13 | PARTIAL (M2) | `cong` computes over Path (section 2.4). `transport` is reserved: M3 defines it with `transp`. | none (docs/PLAN.md, M3 row) |

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

- O1. Closed in M2. The item was the template placeholder. It had no
  question.
- O2. Closed in M1. The k rule of section 7.1 was stricter than docs/IR.md
  section 2. The IR.md rule (2^k >= the table length) gives k 8 for
  `range`, and the back end refuses that document: the 256-row table does
  not fit in the usable rows. docs/IR.md now has the rule of section 7.1.
- O3. CIRCUIT_UNCONSTRAINED is a syntactic check. A private binder that is
  only in `x == x` or `x - x == 0` passes the check, but the proof does not
  constrain it.
- O4. No gate test gives CIRCUIT_LOWER yet. M1 has no Field-valued normal
  form other than literals, binders and `+ - *`. M3 (slice s5) found no
  circuit that gives it. A circuit has only Field binders (no interval
  binder and no type binder), thus the closed-type rules reduce each Kan
  term in a constraint. Examples: `hcomp Field i0 (fun k => [ i0 -> x ])
  (x * x)` gives `x * x`. With hfT = `hcomp (Type 0) i0 (fun i => [ i0 ->
  Field ]) Field`, `unglue (transp (<_> hfT) i0 (glue [ i0 -> x ] x))` and
  `unglue` of an `hcomp` in hfT give `x`. O4 stays open.
- O5. The JSON target writes a Path component of a data value as `null`,
  because the two ends are in the type (section 2.4). M4 adds loops in
  HITs: a path from a point to itself can then carry content. M4 must look
  at this rule again.
- O6. Decided in M2 (slice s4). An instance whose type is `Partial phi A`
  is refused with SORT_FIBRANT (src/json.c instance_json). The checker
  refuses a type in `SSet` under a type former, so only the type of the
  instance itself can be in `SSet`. A function with an interval binder is
  not an instance.
- O7. M2 has no application of a partial element. M3 adds it with `transp`
  and `hcomp` (docs/PLAN.md, M3 row).

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
