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

`inv : Field -> Field` is the inverse modulo r, with `inv 0 = 0` (M6 s5,
D14). At a literal k, `inv k` computes to k^(r-2) mod r. At a neutral value,
`inv x` is a stuck value. `inv` is a core name, thus `def inv` gives
REFUSE_NAME (D15).

### 2.2 Circuits (M1)

A program can also declare circuits:

```
circuit NAME [public BINDERS] [private BINDERS] (hint NAME := TERM)* := C (; C)*
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
lowering. `circuit`, `public`, `private`, `hint` and `range8` are keywords.
`witness` is a core name, so the witness binders use the keyword `private`.

A hint `hint h := e` (M6 s5, D14) is a Field value that the witness
computes. The checker checks e at Field over the public binders, the private
binders and the earlier hints. The constraints see h as a Field variable:
they do not see e. `langc witness` computes the value of h from e and the
values of the earlier binders. The command line gives no value for a hint.
Example (`examples/nonzero.glue`):

```
circuit nonzero private (x : Field) hint v := inv x := x * v == 1
```

The constraint `x * v == 1` proves that x is not 0. A residual `inv` in a
constraint gives CIRCUIT_LOWER (section 7.1).

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
| CIRCUIT_UNCONSTRAINED | 1 | a private binder or a hint that is in no constraint |
| CIRCUIT_UNUSED_PUBLIC | 1 | a public binder that is in no constraint |
| CIRCUIT_ENTRY | 1 | `circuit` or `witness` with a name that is not a circuit |
| WITNESS_FAIL | 1 | a witness that does not satisfy a constraint |
| WITNESS_ARGS | 2 | a witness argument that is not `BINDER=VALUE`, a missing or repeated binder, a value for a hint, or a value that is not a decimal below r |
| WITNESS_HINT | 1 | a hint term that does not compute to a Field literal |
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
- M2 has no application of a partial element. M3 adds it (section 2.5, O7).

```
def sysEnds : (i : I) -> Partial (~i \/ i) Nat
  := fun (i : I) => [ ~i -> 0, i -> 1 ]
```

**JSON.** A definition of a Path type is a proof, not an instance. A Path
component of a data value writes `null` when the path is constant: the ends
are in the type. A path with content gives JSON_VALUE (section 2.6, O5). An
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

### 2.5 Kan operations, Glue and univalence (M3)

M3 adds the Kan operations of CCHM, Glue types and univalence. There is no
new token. Each form is a core name with arguments, as `Path` and
`Partial`. The plan and the design calls are in docs/M3-PLAN.md.

| Form | Meaning |
|---|---|
| `transp (<i> A) phi a` | Transport along the line `<i> A`. The line is constant on `phi`. |
| `hcomp A phi u u0` | Homogeneous composition. `u : (i : I) -> Partial phi A`, `u0 : A`. |
| `Glue A [ phi -> pack T e, ... ]` | Glue type. `T : Type n`, `e : Equiv T A`. |
| `glue [ phi -> t, ... ] a` | Glue element. Check only. |
| `unglue g` | The `A` part of `g`. |

The line of `transp` and `comp` uses the line rule of `PathP` (section
2.4): `<i> A` with `A : Type n`, else PATH_TYPE or SORT_FIBRANT. `witness`
and `payload` are the Sigma projections and `pack` is the Sigma pair (M1).

**Typing rules.**

| Rule | Premises | Conclusion |
|---|---|---|
| transp | `<i> A` is a line in `Type n`; `phi` is an interval term; `a : A[i := i0]`; on each conjunction of `phi`, `A` at a fresh `i` converts to `A[i := i0]` (TRANSP_CONSTANT) | `transp (<i> A) phi a : A[i := i1]` |
| hcomp | `A : Type n`; `u : (i : I) -> Partial phi A`; `u0 : A`; on each conjunction of `phi`, `u i0` converts to `u0` (HCOMP_BASE) | `hcomp A phi u u0 : A` |
| Glue | `A : Type n`; the system checks against `Partial psi (Sigma (T : Type n) (Equiv T A))`, where `psi` is the join of its faces | `Glue A [ ... ] : Type n` |
| glue (check) | Against `Glue A [ psi -> pack T e ]`: the system checks against `Partial psi T` (FACE_COVER); `a : A`; on each conjunction of `psi`, `witness e t` converts to `a` (GLUE_BOUNDARY) | `glue [ ... ] a : Glue A [ ... ]` |
| unglue (infer) | `g : Glue A [ ... ]`. The checker copies the face and the system of the type of `g` into the core term (as D5 of M2) | `unglue g : A` |

`glue` with no declared Glue type, or `unglue` of a term that is not of a
Glue type, gives GLUE_EXPECTED.

**Evaluation.** The evaluator tries the rules in this order: (1) the face
`i1` rules, (2) the rule of the head former of the line or the type, (3)
else the value is stuck. `A[r]` is the line at `r`.

| Redex | Result |
|---|---|
| `transp L i1 a` | `a` |
| `transp (<i> T) phi a`, `T` is `Nat`, `Field`, `Unit`, `Flag` or `Type n` | `a` |
| `transp (<i> (x : A) -> B) phi f` | `fun y => transp (<i> B[x := w i]) phi (f (w i0))`, where `w i = transp (<j> A[~j \/ i]) (phi \/ i) y` |
| `transp (<i> Sigma (x : A) B) phi p`, also Prod | `pack (transp (<i> A) phi (witness p)) (transp (<i> B[x := v i]) phi (payload p))`, where `v i = transp (<j> A[i /\ j]) (phi \/ ~i) (witness p)` |
| `transp (<i> PathP (<j> A) a b) phi p` | `<j> comp (<i> A) (phi \/ ~j \/ j) (fun i => [ phi -> p @ j, ~j -> a[i], j -> b[i] ]) (p @ j)` |
| `transp` on Sum, Option, List | the constructor of the argument with `transp` on each field (`none` and `nil` give the argument). A neutral argument gives a stuck value |
| `Partial i1 A` | `A` |
| `hcomp A i1 u u0` | `u i1` |
| `hcomp T phi u u0`, `T` is `Nat`, `Field`, `Unit` or `Flag` | `u0` when each branch body of `u i` (fresh `i`, on its face) converts to `u0` (also when there is no branch); else stuck |
| `hcomp ((x : A) -> B) phi u f` | `fun x => hcomp B phi (fun i => [ phi -> (u i) x ]) (f x)` |
| `hcomp (Sigma (x : A) B) phi u p`, also Prod | `pack (hfill A phi u1 (witness p) i1) (comp (<i> B[x := hfill A phi u1 (witness p) i]) phi u2 (payload p))`, where `u1` and `u2` are the projections of `u` |
| `hcomp (PathP (<j> A) a b) phi u p` | `<j> hcomp A[j] (phi \/ ~j \/ j) (fun i => [ phi -> (u i) @ j, ~j -> a, j -> b ]) (p @ j)` |
| `hcomp` on Sum, Option, List | when `u0` and each branch body of `u` are the same constructor: that constructor with `hcomp` on each field; else stuck |
| `Glue A [ i1 -> pack T e ]`, `glue [ i1 -> t ] a`, `unglue g` with the face `i1` | `T`, `t`, `witness e g` |
| `unglue (glue s a)` | `a` |
| `transp` along a Glue line, `hcomp` in a Glue type | the Cubical Agda rules (`transpGlue`, `hcompGlue`; CCHM 6.2) with `ghcomp` and `gcomp` (D8) |
| `hcomp (Type n) phi E A` | `Glue A [ phi -> pack (E i1) (lineToEquiv (<i> E (~i))) ]` |

The fold and unfold families give a stuck value (O8). Section 2.6 gives
the rules at a family (M4).

**No regularity (D3).** `transp` along a line that does not use `i` is not
the identity in general. Only the closed-type rule gives the argument. For
example, `transp (<i> P 1200) i0 u` with a variable `P` is stuck (the
`equality moved` row of test/eval/expect.txt). The rule `hcomp A i0 u u0 =
u0` for a general `A` does not exist (D8). Only the closed-type rule and
`ghcomp` give the base `u0` when the system is empty. `Glue A [ ]` (face
`i0`) is a Glue type, not `A`.

**Conversion.**

- `transp`, `hcomp` and the Glue operations compare argument by argument
  (the line under a fresh interval variable, the faces by normal form, the
  systems by conv_sys).
- Glue eta: a neutral `g` converts to `glue s a` when `unglue g` converts to
  `a` and, on each conjunction of the face, `g` converts to the branch of
  `s`. The face and the system come from the `glue` value, not from a type.
- Two stuck `unglue g` values compare `g` only. Their faces and systems are
  copies from the type of `g`.
- Two `lineToEquiv` values compare their lines. When only one side is a
  `lineToEquiv`, it unfolds to its pair.
- No rule makes two values equal because of their type (as M2). The
  closed-type rules read the line or the type argument of the operation,
  not the type of a value.

**Derived operations.** Each operation is a checker rule that writes a core
term (D4 of M2).

| Operation | Type | Core term |
|---|---|---|
| `comp (<i> A) phi u u0` | `A[i1]` | `hcomp A[i1] phi (fun i => [ phi -> transp (<j> A[i \/ j]) i (u i) ]) (transp (<i> A) i0 u0)` |
| `hfill A phi u u0 r` | `A` | `hcomp A (phi \/ ~r) (fun i => [ phi -> u (i /\ r), ~r -> u0 ]) u0` |
| `trans p q` | `p : Path A x y`, `q : Path A y z` give `Path A x z` | `<i> hcomp A (~i \/ i) (fun j => [ ~i -> x, i -> q @ j ]) (p @ i)` |
| `transport p x` | `p : Path (Type n) A B`, `x : A` give `B` | `transp (<i> p @ i) i0 x` |
| `subst P p x` | `P : A -> Type n`, `p : Path A a b`, `x : P a` give `P b` | `transp (<i> P (p @ i)) i0 x` |
| `transportRefl x` | `x : A` gives `Path A (transp (<_> A) i0 x) x` | `<i> transp (<_> A) i x` |
| `J P d p` | `P : (y : A) -> Path A x y -> Type n`, `d : P x refl`, `p : Path A x y` give `P y p` | `transp (<i> P (p @ i) (<j> p @ (i /\ j))) i0 d` |
| `JRefl P d` | `Path (P x refl) (J P d refl) d` | `<i> transp (<_> P x refl) i d` |
| `isContr A` | `Type n` | `Sigma (c : A) ((y : A) -> Path A c y)` |
| `fiber f y` | `f : A -> B`, `y : B` | `Sigma (x : A) (Path B (f x) y)` |
| `isEquiv f` | `f : A -> B` | `(y : B) -> isContr (fiber f y)` |
| `Equiv A B` | `Type n` | `Sigma (f : A -> B) (isEquiv f)` |
| `idEquiv A` | `Equiv A A` | `pack (fun x => x) (fun y => pack (pack y (<_> y)) (fun z => <i> pack (payload z @ ~i) (<j> payload z @ (~i \/ j))))` |
| `ua e` | `e : Equiv A B`, `A` and `B` in `Type n`, give `Path (Type n) A B` | `<i> Glue B [ ~i -> pack A e, i -> pack B (idEquiv B) ]` |
| `uaBeta e x` | `Path B (transport (ua e) x) (witness e x)` | `<i> transp (<_> B) i (witness e x)` |

`uaBeta` checks that `transport (ua e) x` converts to
`transp (<_> B) i0 (witness e x)`, else PATH_BOUNDARY. The evaluator has
three internal templates with no surface name: `lineToEquiv (<i> A)` of
type `Equiv A[i0] A[i1]` (Cubical Agda `lineToEquiv`; a line with a closed
head gives the identity equivalence), `ghcomp A phi u u0 = hcomp A (phi \/
~phi) (fun i => [ phi -> u i, ~phi -> u0 ]) u0` and `gcomp`, which is
`comp` with `ghcomp` in place of `hcomp` (D8).

Laws that hold by computation (each is a `refl` in an example):
`transp L i1 a = a`; `transp (<_> Nat) phi n = n`, also for a variable `n`;
`hcomp A i1 u u0 = u i1`; `hfill A phi u u0 i0 = u0`; `trans refl refl` at
Nat literals is `refl`; `ua e @ i0 = A` and `ua e @ i1 = B`;
`unglue (glue s a) = a`; `transport (ua e) x = witness e x` for `B` in
Field or Nat.

**Names.** The new core names are `transp`, `hcomp`, `comp`, `hfill`,
`trans`, `transport`, `subst`, `transportRefl`, `J`, `JRefl`, `isContr`,
`fiber`, `isEquiv`, `Equiv`, `idEquiv`, `Glue`, `glue`, `unglue`, `ua` and
`uaBeta` (D10). A definition with one of these names gives REFUSE_NAME.
The reserved names are now `Bool` and `Text`.

**Circuits and JSON.** M3 adds no circuit code. A constraint side must
normalize to a Field normal form (section 7.1), so a stuck Kan term in a
constraint gives CIRCUIT_LOWER. In an instance, a stuck Kan value or a
value of a Glue type gives JSON_VALUE (section 9).

**Refusal codes (M3).**

| Code | Exit | The compiler refuses |
|---|---|---|
| TRANSP_CONSTANT | 1 | `transp (<i> A) phi a` where `A` is not constant on a conjunction of `phi`. The message gives the conjunction and the two ends of the line |
| HCOMP_BASE | 1 | `hcomp A phi u u0` (also `comp` and `hfill`) where `u i0` does not convert to `u0` on a conjunction of `phi`. The message gives the conjunction and the two values |
| GLUE_EXPECTED | 1 | `glue` with no declared Glue type; `unglue` of a term that is not of a Glue type |
| GLUE_BOUNDARY | 1 | `glue s a` where `witness e t` does not convert to `a` on a conjunction of the face |

Existing codes with new causes: PATH_TYPE and SORT_FIBRANT (a bad line of
`transp` or `comp`), FACE_COVER and FACE_AGREE (the systems of `hcomp`,
`Glue` and `glue`), REFUSE_NAME (the D10 names), TYPE_MISMATCH (`ua` of a
term that is not an equivalence, or `A` and `B` in two universes),
PATH_BOUNDARY (`uaBeta`), JSON_VALUE (a stuck Kan value or a Glue
instance).

### 2.6 Higher inductive types (M4)

M4 adds higher inductive types (HITs). There is no new token. The plan and
the design calls are in docs/M4-PLAN.md.

A HIT is a `family` with one or more path constructors. A path constructor
has fields, then one or more interval binders, then a boundary system. A
constructor with no binder is a point constructor. A family with only point
constructors is a plain family, and its rules do not change. A HIT
declaration is in domain/domain.lang only (rule R1): a program that declares
a family gets REFUSE_DATA.

```
family Circle := base | loop <i> [ ~i -> base, i -> base ]
family Trunc (A : Type 0) := inc (held : A) | squash (lo : Trunc A) (hi : Trunc A) <i> [ ~i -> lo, i -> hi ]
```

The grammar change is in `ctor`: `ctor := NAME group* ('<' NAME+ '>'
system)?`, where `system` is the system form of section 2.4.

| Form | Meaning |
|---|---|
| `c params fields` (a path constructor) | A path over the binders of `c`. `c params fields @ r` applies it. |
| `<i j>` binders | A higher constructor. `squashc` of Quot has two binders. |
| `hcomp D phi u u0` at a HIT | A canonical value. |
| `fold B cases x` at a HIT | The eliminator. `B` is explicit. One case for each constructor in declaration order, then the scrutinee. |
| `isProp A`, `isSet A` | Derived operations (below). |

**Typing rules.**

| Rule | Premises | Conclusion |
|---|---|---|
| path constructor (declaration) | The fields check as the family rule. A field of type Self or `Path Self a b` is recursive. A field type sees the parameters and the earlier fields of the same constructor. The binders are interval variables. The system checks against `Partial psi Self`, where Self is the family at its parameters. Each body can use the parameters, the fields, the binders and the constructors declared before it. Two branches agree where they meet (FACE_AGREE). The join of the faces covers the cube boundary `~i1 \/ i1 \/ ... \/ ~ik \/ ik` (HIT_BOUNDARY) | `c` is a path constructor with k binders |
| path constructor (use) | `c params fields`, each field checked as the family rule | `c params fields : PathP (<i1> ... PathP (<ik> Self) ...)`. The ends are the boundary bodies at each face. Thus `loop : Path Circle base base` |
| hcomp at a HIT | as section 2.5 (HCOMP_BASE) | `hcomp D phi u u0 : D` |
| fold at a HIT | `x : D params`; `B : Type n`; one case for each constructor (HIT_CASES). A point case is a function of the fields; a recursive field gets the result in `B`. A path case with k binders is a function of the fields to a PathP over the binders in `B`; a field of type `Path Self a b` gets a Path in `B` between the results. On each boundary branch `phi -> t`, the case at the binders converts to `fold B cases t` (HIT_CASE) | `fold B cases x : B` |

The checker copies `B` into the core term (OP_FOLD_HIT), because the
`hcomp` rule of the eliminator needs it. `fold` at a HIT is the `rec` of
design/DESIGN.md. `rec` is a keyword (`def rec`), thus the eliminator keeps
the name `fold`. M4 has no dependent eliminator. M5 adds `elim` (section
2.7).

**Evaluator rules.** The order is: (1) the face `i1` rules; (2) the rule of
the head former or of the head value; (3) else the value is stuck. `pc` is
a path constructor.

| Redex | Result |
|---|---|
| `pc params fields @ r1 .. @ rk`, a boundary face is `i1` at `r1 .. rk` | the body of that branch (the boundary rule, as reduce_sys of section 2.4) |
| `pc params fields @ r1 .. @ rk`, no face is `i1` | a canonical value (OP_PCON) |
| `transp (<i> D) phi x`, `D` a family with no parameters | `x` |
| `transp (<i> D (P i)) phi (c fields)` | `c` with `transp` on each field along its field type at `P i`. The interval arguments do not change. A path constructor reduces only when this result agrees with transport on each boundary face; else the value is stuck |
| `transp (<i> D (P i)) phi (hcomp (D (P i0)) psi u u0)` | `hcomp (D (P i1)) psi (fun j => [ psi -> transp (<i> D (P i)) phi (u j) ]) (transp (<i> D (P i)) phi u0)` |
| `transp` at a family with a neutral argument | stuck |
| `hcomp D i1 u u0` at a HIT | `u i1` |
| `hcomp D phi u u0` at a HIT, `phi` is not `i1` | a canonical value. kan_hcomp never reduces it |
| `hcomp` at a plain family | the data rule of section 2.5: the same constructor in `u0` and in each branch gives that constructor with `hcomp` on each field; else stuck |
| `fold B cases (c fields)`, a point constructor | the case of `c` applied to the fields; each recursive field is replaced by `fold B cases field` |
| `fold B cases (pc fields @ r1 .. @ rk)` | the case of `pc` applied to the fields (a field `p : Path Self a b` gives `<j> fold B cases (p @ j)`), then `@ r1 .. @ rk` |
| `fold B cases (hcomp D phi u u0)` | `hcomp B phi (fun i => [ phi -> fold B cases (u i) ]) (fold B cases u0)` |
| `fold B cases x`, `x` neutral | stuck |

There is no rule `hcomp A i0 u u0 = u0` for a general `A` (D8 of M3). Thus
`hcomp Circle i0 (fun i => [ i0 -> base ]) base` does not reduce to `base`.
The boundary rule and the `hcomp` rule of the eliminator agree: on a
boundary face, HIT_CASE makes the path case equal to `fold` of the body.

**Conversion.** Two OP_PCON values convert when they have the same
constructor and their arguments convert (the fields pairwise, the interval
arguments by normal form). An `hcomp` value at a HIT and a HIT `fold`
compare argument by argument. No rule makes two values equal because of
their type. HIT_CASE reads the boundary of the constructor (a declaration),
not the type of a value. Two variables `x y : Circle` do not convert.

**Printer.** A path constructor value prints with its arguments (eval row
`otp lawAt => fun j => encrypt @ j`). An `hcomp` value prints as `hcomp D
phi u u0` (the form of section 2.5). A HIT fold prints as `fold B cases x`.

**JSON.** A Path component of a data value writes `null` only when the path
is constant: its body at a fresh interval variable does not use the
variable. A path with content gives JSON_VALUE ("a path with content"). An
`hcomp` value or a path constructor value in an instance gives JSON_VALUE. A
point constructor of a HIT writes the family constructor form. This rule
closes O5.

**Derived operations.** Each is a checker rule that writes a core term (D2
of M3).

| Operation | Core term |
|---|---|
| `isProp A` | `(x : A) -> (y : A) -> Path A x y` |
| `isSet A` | `(x : A) -> (y : A) -> (p : Path A x y) -> (q : Path A x y) -> Path (Path A x y) p q` |

The laws are guest definitions: `truncIsProp A : isProp (Trunc A)` is
`fun x => fun y => squash x y` (examples/trunc.lang), and `quotIsSet A R :
isSet (Quot A R)` is `fun x => fun y => fun p => fun q => squashc x y p q`
(examples/quot.lang).

Laws that hold by computation (each is a `refl` in an example):
`loop @ i0 = base` and `loop @ i1 = base`; `transp (<_> Circle) i0 x = x`
for a variable `x`; `transp (<i> Trunc (L i)) i0 (inc a) = inc (transp (<i>
L i) i0 a)`; `hcomp Circle i1 u u0 = u i1`; `fold B b l base = b`; `<i> fold
B b l (loop @ i) = l`; `fold B f e s (cls a) = f a`; `interp (encrypt @ i) =
enc @ i` (examples/otp.lang).

**Names.** The new core names are `isProp` and `isSet`. A definition with
one of these names gives REFUSE_NAME. The smoke families in
domain/domain.lang are Circle (`base`, `loop`), Susp (`north`, `south`,
`merid`), Trunc (`inc`, `squash`), Quot (`cls`, `eqc`, `squashc`), OTPSpec
(`message`, `cipher`, `encrypt`) and GQuot (`gcls`, `geqc`). M7 adds the
family Circ (section 2.9).

**Circuits.** M4 adds no circuit code and does not change the IR. A
constraint side must normalize to a Field normal form (section 7.1), so a
stuck HIT term in a constraint gives CIRCUIT_LOWER (O4). examples/quotSq.glue
folds out of the graph quotient GQuot and gives the IR of `square`
(test/circuit/same-ir.txt).

**Refusal codes (M4).**

| Code | Exit | The compiler refuses |
|---|---|---|
| HIT_BOUNDARY | 1 | A path constructor whose boundary system does not cover the cube boundary of its binders. The message gives the constructor, the face that is not covered and the binder |
| HIT_CASE | 1 | `fold` at a HIT where a path case has the wrong type or does not convert to `fold B cases t` on a boundary branch `phi -> t`. The message gives the constructor and the expected type |
| HIT_CASES | 1 | `fold` at a HIT with the wrong number of cases, or a point case whose type does not match the constructor. The message gives the required number of cases, or the constructor and the expected type |

Existing codes with new causes: PARSE_EXPECT (a boundary system with no
binder; a binder with no system), FACE_AGREE (the boundary branches of a
path constructor), TYPE_MISMATCH (a boundary body that is not of the family
type; a field of a HIT), DOMAIN_FAMILY (a non-recursive field that depends
on a recursive field; a recursive field that is not Self or a Path in
Self), REFUSE_DATA (a family
in a program; rule R1), REFUSE_NAME (`isProp`, `isSet`), HCOMP_BASE (an
`hcomp` at a HIT whose base does not match), JSON_VALUE (a path with
content, an `hcomp` value or a path constructor value in an instance).

### 2.7 Quotient inductive types (M5)

M5 adds quotient inductive types (QITs), the dependent eliminator `elim`
and a block of two families (an inductive-inductive definition). The new
keyword is `mutual`. The new core name is `elim`. The plan, the design
calls and the USER rulings (2026-10-16) are in docs/M5-PLAN.md.

**Constructor equations.** A QIT is a HIT whose path constructors are
equations between constructor terms. An equation uses the path constructor
form of section 2.6: one binder and a boundary system. There is no `eq`
clause (RULED 2026-10-16 (USER), M5-PLAN question c). An end can hold a
nested application of a constructor, as the ends of `bswap`. The boundary
rule, HIT_BOUNDARY and HIT_CASE apply as in section 2.6.

```
family Bag (A : Type 0) := bnil | bcons (bx : A) (bxs : Bag A) | bswap (sx : A) (sy : A) (sxs : Bag A) <i> [ ~i -> bcons A sx (bcons A sy sxs), i -> bcons A sy (bcons A sx sxs) ]
family Int := pos (pn : Nat) | neg (nn : Nat) | zeq <i> [ ~i -> pos 0, i -> neg 0 ]
```

The `fold` case of an equation has the folded nested ends. With `bsize A
:= fold Nat 0 (fun x => fun n => natAdd n 1) (fun x => fun y => fun n =>
refl)`, `bsize A (bswap A x y xs @ k)` normalizes to `natAdd (natAdd (bsize
A xs) 1) 1`. `transp` along `<i> Bag (L i)` moves each field of `bswap`
(the fieldwise rule). For `bswap` and `zeq` the fieldwise rule agrees with
the transport of the ends (D2), thus no M5 equation stays stuck under O12
(c).

**The dependent eliminator.** `elim P cases x` (RULED 2026-10-16 (USER),
question b). `fold` does not change. `rec` stays a keyword.

| Rule | Premises | Conclusion |
|---|---|---|
| elim | `x : D params`; `P : (y : D params) -> Type n`, else TYPE_MISMATCH; one case for each constructor in declaration order (ELIM_CASES). A point case takes each field, immediately followed by an IH for a recursive data field or a Path field (as below; ELIM_CASES). A path case with k binders has the type `PathP (<i1> .. PathP (<ik> P (pc params fields @ i1 .. @ ik)) ..)` and takes the fields and the IHs. On each boundary branch `phi -> t`, its end converts to `elim P cases t` (ELIM_CASE) | `elim P cases x : P x` |

For a recursive data field `f : D params`, the IH has type `P f`. For a
Path field `p : Path (D params) a b`, the IH has type `PathP (<j> P (p @
j)) ha hb`. Here `ha` and `hb` eliminate the field ends `a` and `b`,
replacing earlier recursive fields by their IHs. For example, the
`wlink` case of `elim (fun _ => Nat)` takes `x, hx, y, hy, p, hp`,
with `hp : Path Nat hx hy`.

The evaluator rules of `elim`, in the order of section 2.6:

| Redex | Result |
|---|---|
| `elim P cases (c params fields)`, c a point constructor | the case of c applied to the original fields and their IHs: `elim P cases f` for a recursive data field `f`, and `<j> elim P cases (p @ j)` for a Path field `p` |
| `elim P cases (pc params fields @ r1 .. @ rk)` | the path case of pc applied to the fields and the IHs, then `@ r1 .. @ rk` |
| `elim P cases (hcomp D phi u u0)` at a HIT, phi not `i0` | `comp (<i> P (hfill D phi u u0 i)) phi (fun i => [ phi -> elim P cases (u i) ]) (elim P cases u0)`, with `hfill D phi u u0 i = hcomp D (phi \/ ~i) (fun j => [ phi -> u (i /\ j), ~i -> u0 ]) u0` (kan.c kan_elim_hcomp, through gcomp) |
| `elim P cases h`, h = `hcomp D i0 u u0` at a HIT | `hcomp (P h) i0 (fun i => [ i0 -> .. ]) (transp line i0 (elim P cases u0))`: the empty tube stays, as in the M4 `fold` rule. RULED 2026-10-16 (USER): option (ii) of docs/STATUS.md M5 s2 open item (d). Thus `elim (fun y => Circle) base (<i> loop @ i) h` and `fold Circle base (<i> loop @ i) h` give the same value |
| `elim P cases x`, x neutral | a neutral value |

At a plain family `elim` has no `hcomp` rule: the data rule of section 2.5
gives a constructor or a stuck value first. `elim` is a core name, thus
`def elim` gives REFUSE_NAME. A family of a `mutual` block has no `elim` in
M5 (ELIM_CASES; O13). Examples: `negNeg` (examples/int.lang, the `zeq` case
is the square `<i> <j> zeq @ i`), `ret` and `flipPath` (examples/otp.lang:
`transport flipPath message = cipher` by refl, with `flipPath = ua
(isoToEquiv OTPSpec OTPSpec flip flip ret ret)`).

**Kan rules at dependent and Path fields.** `transp` at a constructor with
a dependent field uses the Sigma method of section 2.5: field k moves along
its field type at the line, and each earlier field n is replaced by its
fill `fun i => transp (<j> T[DOM(L, i /\ j) / p, F n (i /\ j) / x n]) (phi
\/ ~i) x`. A Path field gets a PathP line, thus its ends are the
transported ends. `hcomp` at a plain family with a dependent field uses the
comp method of `hcomp` at Sigma. The fill of a dependent field is `fun r =>
hcomp B[r] (phi \/ ~r) (fun i => [ phi -> S (FIELD N (u (i /\ r))), ~r ->
S x ]) (transp (<i> B[i /\ r]) i0 x)`, with S = `transp (<j> B[(i \/ j)
/\ r]) i` and B[s] the field type at the parameters and the earlier fills
at s. A non-dependent field keeps the M4 rule. Thus `transp (<i> Quot Nat
(R i)) i0 (eqc Nat (R i0) 1 2 r @ k)` gives `eqc Nat (R i1) 1 2 (transp (<j>
R j 1 2) i0 r) @ k`. `transp` at a path constructor with a Path field
(`squashc` of Quot) stays stuck (O12 (c)).

**The mutual block.** `mutual`, then exactly two `family` declarations
(RULED 2026-10-16 (USER), question d):

```
mutual family Ctx := cempty | ext (eg : Ctx) (et : Ty eg)
family Ty (tg : Ctx) := tbase | arrow (ta : Ty tg) (tb : Ty tg)
```

| Rule | The block of A and B |
|---|---|
| (1) index | The last parameter of B (the index) has the type of A at the parameters of B before it, else MUTUAL_BLOCK. |
| (2) names | The checker checks the parameters of A and registers A, then checks the parameters of B and registers B. Then it checks the constructors of A, then the constructors of B. Both names are known before a constructor: the block is the one exception to rule R3. Outside a block, a family that names a later family gives REFUSE_REC. |
| (3) partner field | A field of A with the type `B params g`, where g is an earlier recursive field of the same constructor, is a partner field. It is also a dependent field. A different use of B in a field of A gives DOMAIN_FAMILY. |
| (4) fields of B | A field of B follows the field rule of section 2.6: a recursive field is B at its own parameters (a uniform index). |
| (5) rule R1 | A block is in domain/domain.lang only. A program that declares a block gets REFUSE_DATA. |
| (6) points only | A block has point constructors only. A path constructor in a block gives MUTUAL_BLOCK (O13). |
| universe | A gets the universe of its partner fields: the checker raises the universe of A and checks both declarations again with the final bounds. |

A block with one family gives MUTUAL_BLOCK. `mutual` with no `family`
gives PARSE_EXPECT. A constructor of B takes the index first: `tbase
cempty`, `arrow cempty (tbase cempty) (tbase cempty)`. `transp` and `hcomp`
move a partner field as a dependent field (its fill). Thus `transp` along
`<i> Ty (P @ i)`, with `P : Path Ctx cempty c1`, gives `tbase c1`.

**The block fold.** `fold MA MB casesA casesB x`. `MA : Type n`. Each form
of `MB`, with or without a binder type, checks against `MA -> Type
level(MA)`, else TYPE_MISMATCH. There is one case for each constructor of
the block in declaration order: the cases of A, then the cases of B (else
HIT_CASES).

- A case of A takes the fields. A recursive field gets `MA`. A partner
  field `t : B params g` gets `MB rg`, where `rg` is the result of `g`.
- A case of B takes the index result `rg : MA` first, then the fields. A
  recursive field gets `MB rg`. The case gives `MB rg`. A field of B that
  is not recursive and uses the index gives HIT_CASES.
- The result type is `MA` for `x` in A, and `MB (fold MA MB cases g)` for
  `x : B params g`.
- A case that does not match its type gives HIT_CASES.

The core op is OP_FOLD_BLOCK, not OP_FOLD_HIT (the s5 choice). Its inst is
A. Its args are MA, MB, the cases of A, the cases of B, then the
scrutinee: the arity is the constructor count of A, plus the constructor
count of B, plus 3. Reason: each OP_FOLD_HIT site reads the arity
`ctor_count + 2` and reads args[0] as the one result type. A new op keeps
each OP_FOLD_HIT site the same, and -Wswitch-enum finds each switch that
must handle the new op. The evaluator gives a case of A its fields, with
the recursive and the partner fields folded. It gives a case of B `fold ..
index` first, then the fields. A neutral scrutinee gives a neutral value.
Example: `length` and `tySize` in examples/ctx.lang.

**Neutral values.** `fold B cases x`, `elim P cases x` and a block fold
with a neutral scrutinee `x` are neutral values, as `fold` at Nat with a
variable. `transp` at a family with a neutral argument is a neutral value.
These are rules, not gaps (D10; before M5, O12 (a) and (b)).

**Conversion.** M5 does not change conversion. Two OP_ELIM values or two
OP_FOLD_BLOCK values convert when their arguments convert, argument by
argument. No conv rule reads a type: two variables `x y : Int` do not
convert (test/check/elim-int-vars.lang), and two block folds with different
cases do not convert. `hcomp` at a HIT stays canonical (section 2.6), and
there is no rule `hcomp A i0 u u0 = u0` (test/check/hcomp-i0-int.lang).

**Printer and JSON.** The printer writes `elim P cases x`. In an instance,
a stuck `elim` gives JSON_VALUE (src/json.c). A closed instance cannot hold
a stuck `elim`: test/emit/elim-instance.lang holds one under a function.

**Direction 2.** `fold` and `elim` on a representative reduce by the point
rule. examples/transportProp.lang: `P` is a `fold` out of `GQuot Nat
natSame` into `Type 0` with the `geqc` case `ua (rotE Field)`. `Pline a b
r` is the def `<i> P (geqc Nat natSame a b r @ i)`, and `transport (Pline a
b r) p` reduces by uaβ to the forward map `rot Field p`. The line is a def,
because a term `<i> ..` in an argument needs a declared type (PATH_EXPECTED;
glue-lang has no type ascription).

**Circuits.** M5 adds no circuit code and does not change the IR (RULED
2026-10-16 (USER), question a: the lowering is M7). examples/intSq.glue
folds out of Int and gives the IR of `square` (test/circuit/same-ir.txt).
No probed circuit gets a stuck `elim` to the lowering (O4).

**Refusal codes (M5).**

| Code | Exit | The compiler refuses |
|---|---|---|
| ELIM_CASES | 1 | `elim` with the wrong number of cases, or a point case whose type does not match (a missing IH). Also `elim` at a family of a `mutual` block. The message gives the constructor and the expected type |
| ELIM_CASE | 1 | `elim` where a path case has the wrong type or does not convert to `elim P cases t` on a boundary branch `phi -> t`. The message gives the constructor and the expected type |
| MUTUAL_BLOCK | 1 | A `mutual` block with one family, a path constructor in a block, or a second family whose last parameter does not have the type of the first family. The message gives the cause |

Existing codes with new causes: REFUSE_NAME (`elim`), TYPE_MISMATCH (a
motive `P` that is not a family of types over D; a block motive `MB` that
does not check against `MA -> Type level(MA)`), DOMAIN_FAMILY (a partner
field that is not at an earlier recursive field; the second family in a
different type in a field of the first family), REFUSE_DATA (a block in a
program), PARSE_EXPECT (`mutual` with no family), HIT_CASES (a block fold
with the wrong number of cases, a case of the wrong type, or a
non-recursive field of the second family that uses the index), JSON_VALUE
(a stuck `elim` in an instance).

### 2.8 Monads, folds and unfolds (M6)

M6 adds the monad laws, the fold and unfold laws over the cubical core and
the hint monad of the lowering. The new core names are `append` and `inv`.
The new circuit binder keyword is `hint`. The plan, the design calls D1 to
D16 and the USER rulings (2026-10-17) are in docs/M6-PLAN.md.

**append and List bind.** `append xs ys` is the join of the List instance
(D1; RULED 2026-10-17 (USER), question b: extend the core instance).

| Redex | Result |
|---|---|
| `append nil ys` | `ys` |
| `append (cons x xs) ys` | `cons x (append xs ys)` |
| `append xs nil`, xs neutral | `xs` (the right unit) |
| `append xs ys`, xs neutral, ys not `nil` | a neutral value |

A trap end of xs stays a trap. List bind goes through append (D2): `bind
nil f = nil` and `bind (cons a l) f = append (f a) (bind l f)`. Thus `bind
(cons a nil) f = f a` by refl for each f. The right unit keeps normal forms
stable under substitution: at `l := nil`, `append (f a) (bind l f)` gives
`f a`, as direct evaluation does. `append` is a core name, thus `def
append` gives REFUSE_NAME.

**elim at the core carriers and at Nat.** `elim P c1 c2 x` (D3, D6;
RULED 2026-10-17 (USER), question g). P is explicit. The IH comes after
each recursive field (the M5 rule). The core op is OP_ELIM_CORE at a
carrier and OP_ELIM_NAT at Nat.

| Scrutinee | First case | Second case |
|---|---|---|
| `o : Option A` | `P none` | `(a : A) -> P (some a)` |
| `s : Sum E A` | `(e : E) -> P (inl e)` | `(a : A) -> P (inr a)` |
| `l : List A` | `P nil` | `(a : A) -> (l : List A) -> P l -> P (cons a l)` |
| `n : Nat` | `zero : P 0` | `succ : (n : Nat) -> (ih : P n) -> P (natAdd n 1)` |

| Redex | Result |
|---|---|
| `elim P c1 c2 none`, `elim P c1 c2 (some a)` | `c1`, `c2 a` |
| `elim P c1 c2 (inl e)`, `elim P c1 c2 (inr a)` | `c1 e`, `c2 a` |
| `elim P c1 c2 nil`, `elim P c1 c2 (cons a l)` | `c1`, `c2 a l (elim P c1 c2 l)` |
| `elim P z s k`, k a literal | `s (k - 1) (.. (s 0 z))` |
| `elim P z s (natAdd x k)`, a successor spine | `s (natAdd x (k - 1)) (.. (s x (elim P z s x)))` |
| `elim P c1 c2 x`, x neutral | a neutral value |

A trap scrutinee gives the trap. A List with a trapped tail applies its
known cons cases, with the trapped IH (the fold does the same). A wrong
number of cases gives ELIM_CASES. A case of the wrong type gives ELIM_CASE
(at a family a point case keeps ELIM_CASES, as in M5). A scrutinee of
another type gives TYPE_MISMATCH (`elim needs a last argument in a domain
family, Option, Sum, List or Nat`). The core `option` and `either` stay the
non-dependent forms. The ELIM_CASES, ELIM_CASE, HIT_CASES and HIT_CASE
messages print each case type with the field names of the constructor.

**The Nat successor view.** (D5) `natAdd` with one neutral operand and one
positive literal has the offset normal form `natAdd x k`, with x neutral
and k that literal: `natAdd x 0 = x`, `natAdd 0 x = x`,
`natAdd k x = natAdd x k` and
`natAdd (natAdd x a) b = natAdd x (a + b)`. An overflow of `a + b` gives
the trap. With two neutral operands, `natAdd x y` stays stuck and is not
a successor spine. Literal addition keeps its overflow and trap behavior.
The form
`natAdd x k` is a successor spine: `fold f z (natAdd x k) = f (fold f z
(natAdd x (k - 1)))`, down to `fold f z x`. The same spine rule applies to
`elim` at Nat and to the `unfold` limit. The rule is stable under
substitution: `x := m` gives the m + k unroll. PRINT_MAX guards a large k.

**The fold laws and F6 uniqueness.** (D7) foldZero, foldSucc, foldCons and
foldPush (at Stack) hold by refl (examples/fold-laws.lang). Uniqueness: a
function that satisfies the fold equations is the fold. This is a proof
term (funExt over `elim`, with `trans` and `cong`) at Nat, List and a plain
family (foldNatUniq, foldListUniq and foldFamUniq). At a HIT the path cases
need squares (O14).

**unfold.** `unfold g n s` has an explicit Nat depth n (RULED 2026-10-17
(USER), question c). The declared type selects the form (D9): at `List A`,
`g : S -> Option (Prod A S)`; at Nat, `g : S -> Option S` (the core op
OP_UNFOLD_NAT). At another declared type, `unfold` gives TYPE_INFER. The
step goes through the core `option` (D8), thus it does not wait for `g s`:

| Redex | Result |
|---|---|
| `unfold g 0 s` | `nil` at List, `0` at Nat |
| `unfold g (natAdd n 1) s`, List | `option nil (fun p => cons (first p) (unfold g n (second p))) (g s)` |
| `unfold g (natAdd n 1) s`, Nat | `option 0 (fun r => natAdd (unfold g n r) 1) (g s)` |
| a limit `natAdd x k` (a spine) | at most k steps; after k successful steps, the tail is `unfold g x seed` |
| `unfold g x s`, x neutral | a neutral value |
| a trap limit | the trap |
| a trap step result, List | the cells already produced, with the trap as their tail |
| a trap step result, Nat | the trap |

At a literal limit and a constructor step result, `none` gives `nil` (0 at
Nat) and `some p` gives one more cell (one more successor at Nat). At Nat
the step counts the outer `some` and passes its payload only as the next
seed. The laws unfoldZero, unfoldNone and unfoldSome hold by refl at List
and at Nat (examples/unfold-laws.lang). Unfold uniqueness at List and at
Nat is a proof by `elim` at the limit (unfoldListUniq and unfoldNatUniq).
M6 has no unfold into a family (section 4).

**Kan rules at closed data.** (D10, D11; O8 closes, RULED 2026-10-17
(USER), question d) `transp (<_> T) phi a` gives `a`, a neutral `a` too,
when T is a closed line: a closed head (Nat, Field, Unit, Flag, Type n), or
Sum, Option, List, Prod or a family where each argument is a closed line or
a Nat literal. `hcomp T phi u u0` with a constant system gives `u0`, a
neutral `u0` too, when T is eligible. Eligibility is a separate recursive
check: Nat, Field, Unit and Flag; Sum, Option, List and Prod with eligible
fields; a plain family when each constructor field at the parameters is
eligible. Type n, a HIT, a dependent field, a partner field and an open
field are not eligible, at each depth. The rules read the line or the type
argument, not the type of a value. At a type that is not eligible, the
fieldwise rules and the neutral rules apply as before. Thus the fieldwise
`hcomp` at `Option A`, then `A := Circle`, agrees with direct evaluation at
`Option Circle` (examples/o8-laws.lang).

At `phi = i1`, `hcomp T phi u u0` gives `u i1` at every type before the
rules below apply. The table assumes `phi` is not `i1`; data means Sum,
Option, List or a plain family. Later rows apply when the constant-system
rule does not.

| u0 (base) | u i0 (branch) | Type | Result |
|---|---|---|---|
| any | a constant system | eligible | `u0` |
| constructor c | constructor c | data | fieldwise (M3) |
| constructor c | constructor d, d not c | data | HCOMP_BASE |
| neutral | constructor | data | HCOMP_BASE |
| constructor | neutral | data | HCOMP_BASE |
| neutral | neutral, not constant | data | a neutral `hcomp` |
| neutral | a constant system | ineligible data | a neutral `hcomp` |
| any | any | Type n | a Glue type (section 2.5) |
| any | any | a HIT | a canonical `hcomp` (section 2.6) |

At Sum, Option and List, `transp` along an open line at a neutral value
stays neutral (no regularity, D3 of M3). Pi, Sigma, Prod and Path keep
their fieldwise Kan rules. M6 does not drop an i0 branch of a system (D12):
no probe input fails without it. The printer writes `@` before each
interval argument of a path constructor in an application chain (`loop @
i`, not `loop i`).

**Overflow.** A trap is not conversion evidence: conversion does not
equate two traps. `refl` at `natAdd 18446744073709551615 1` gives
TYPE_REFL. Two successor laws by `refl` at a neutral n, applied at
18446744073709551615, do not compose into a closed `Path Nat 0 1`
(TYPE_MISMATCH).

**inv and hint.** Sections 2.1 to 2.3 give `inv` (the Fr inverse, `inv 0 =
0`), the circuit binder `hint h := e`, the hint example and the codes.
Section 7.1 gives the hint rows. The constraints see h as a variable of
type Field, not e (RULED 2026-10-17 (USER), question h). A residual `inv`
in a constraint normal form gives CIRCUIT_LOWER (O4 closes).

**Conversion.** M6 adds no conv rule. Two OP_APPEND, OP_ELIM_CORE,
OP_ELIM_NAT, OP_UNFOLD_NAT or OP_FIELD_INV values convert when their
arguments convert, argument by argument. No conv rule reads a type. `hcomp`
at a HIT stays canonical, and there is no rule `hcomp A i0 u u0 = u0`.

**Printer and JSON.** The printer writes `append xs ys`, `elim P c1 c2 x`,
`unfold g n s` and `inv x`. OP_APPEND, OP_ELIM_CORE, OP_ELIM_NAT and
OP_UNFOLD_NAT are in the not-a-type-former group of op_json. Each reduces
at closed arguments, thus a closed instance cannot hold one.

**Circuits.** (D13) The constrained monad is the current lowering (section
7.1). A fold or an unfold with a static depth normalizes before the
lowering: examples/foldPow.glue and examples/unfoldPow.glue give the IR of
`square`, `poly` and `nonzero` (test/circuit/same-ir.txt). M6 does not
change the IR of a current circuit (RULED 2026-10-17 (USER), question a).

**Refusal codes (M6).** The new code is WITNESS_HINT (section 2.3).
Existing codes with new causes: REFUSE_NAME (`append`, `inv`), ELIM_CASES
and ELIM_CASE (`elim` at a carrier or at Nat), TYPE_MISMATCH (`elim` at
another type; a proof that needs two traps to convert; a List step at a Nat
declared type), TYPE_INFER (`unfold` at a declared type that is not `List
A` or Nat), HCOMP_BASE (two constructor heads at data), CIRCUIT_LOWER (a
residual `inv` in a constraint), CIRCUIT_UNCONSTRAINED (an unused hint).

### 2.9 Direction 2 (M7)

M7 is Direction 2 of design/DESIGN.md: two circuits that define the same
relation are equal, as a path in a quotient. The lowering runs on
representatives. The path cases of a quotient eliminator are checker
obligations, and they have no circuit cost (docs/M7-PLAN.md).

**Obligation rules.** (D1) The `eqc`, `geqc` and `squashc` cases of `fold`
and `elim` are checker obligations (sections 2.6 and 2.7). The checker
refuses a wrong path case before the lowering: HIT_CASE for `fold`,
ELIM_CASE for `elim`. These are the M4 and M5 rules; M7 adds guest tests
only. test/check/geqc-case.glue has the `geqc` case `fun a => fun b => fun
r => refl` in a fold into Field (HIT_CASE). test/check/elim-geqc-case.glue
has the same case in `elim` (ELIM_CASE).

**The lowering runs on representatives.** (D5) M7 adds no lowering code and
does not change the IR. Each quotient value in a constraint reduces before
the lowering: the point rule at `gcls`, the boundary rule at a constant
interval, and the `hcomp` rule of the eliminator, then the closed-type
`hcomp` rule at Field. The lowering sees only literals, binders and `+ -
*`, as in M1. These circuits have the IR of test/circuit/square.circuit.json
(test/circuit/same-ir.txt): examples/quotEnd0.glue (`geqc .. @ i0`),
quotEnd1.glue (`geqc .. @ i1`), quotHcomp.glue (an `hcomp` in `GQuot Field
SqRel`) and quotElimHcomp.glue (the same `hcomp` through `elim`).
examples/evHcomp.lang has the law evHcomp: `sq` of the `hcomp` at `gcls
Field SqRel x` converts with `x * x` by `refl`. Its closed form at x = 3
evaluates to 9 (test/eval/expect.txt).

**A proof stays a term.** (D6) The lowering reads the point case at a
representative. A path case has no wire. Thus a large proof term in a path
case does not change the IR. The `geqc` case of examples/quotProof.glue
transports `r` two times along the line `Lk a b`, and the IR is the IR of
`square`. `Lk` is a def, because a term `<k> ..` as an argument needs a
declared type (PATH_EXPECTED).

**Circuits keep GQuot.** (D7) A `squashc` case into Field needs a square in
Field, and there is no core isSet Field. The case `fun u => fun v => fun p
=> fun q => <i> <j> p @ j` gives HIT_CASE (test/check/squashc-field.glue).
A circuit binder of type `isSet Field` is not a Field binder:
CIRCUIT_BINDER (test/check/isset-binder.glue). A conv rule that makes two
Field paths equal reads the type, and proof irrelevance is prohibited
(ruling 2026-10-14). An axiom gives REFUSE_AXIOM (rule R2). Thus circuits
use `GQuot` (no `squashc`), and quotRec of examples/quot.lang keeps its
isSet argument.

**The circuit syntax.** (D2) domain/domain.lang has the family `Circ`
(rule R1). Its constructors are `cpub`, `cpriv`, `clit (clv : Field)`,
`cadd (cal : Circ) (car : Circ)`, `cmul (cml : Circ) (cmr : Circ)` and
`cpow (cpb : Circ) (cpn : Nat)`. The 6 constructor names and the 7 field
names are domain names: a def with one of these names gives REFUSE_NAME
(test/check/domain-cpow.lang). The syntax has one public binder `cpub` and
one private binder `cpriv` (the shape of `square`). An indexed binder form
is an open item for M8 (docs/M7-PLAN.md question (h)). The interpreter `ev
y x c` is a guest `elim (fun d => Field)`. Its `cpow` case `fold (fun (t :
Field) => hb * t) hb n` uses the Nat fold of M6. Thus `cpow b n` gives the
value of `b` to the power n + 1. examples/circEval.lang evaluates `ev 9 3`
at each constructor (test/eval/expect.txt: 9, 3, 5, 12, 9 and 27).
examples/circSyntax.glue has `ev`, the pointwise relation `CRel`, the
interpreter `evQ` out of `GQuot Circ CRel` (D3) and the circuit `square`
with the constraint `evQ y x (gcls Circ CRel (cmul cpriv cpriv)) == y`. The
point rule and `ev` reduce the constraint to `x * x == y`. Thus the IR is
the IR of test/circuit/square.circuit.json (test/circuit/same-ir.txt).

**The circuit relations.** (D3, D4) examples/circRel.lang has `ev`, `CRel`
and `evQ` (as examples/circSyntax.glue) and the relations of the two
quotients. `CRel c1 c2 = (y : Field) -> (x : Field) -> Path Field (ev y x
c1) (ev y x c2)` is the pointwise relation of the lowering quotient `GQuot
Circ CRel` (D3). The interpreter `evQ y x q` is the fold of `q` into Field
with the point case `ev y x c` and the `geqc` case `r y x`. The `geqc` case
needs a Field path, and `CRel` gives it. `Sat c y x = Path Field (ev y x c)
y` is the satisfaction type of `c` at `y` and `x`. The DESIGN relation
`CEqv c1 c2 = (y : Field) -> (x : Field) -> Equiv (Sat c1 y x) (Sat c2 y
x)` is in `Type 0`, and GQuot accepts it (D4). `SatQ y x` is the fold of
`GQuot Circ CEqv` into `Type 0` with the point case `Sat c y x` and the
`geqc` case `ua (e y x)`. `SatP y x` is the fold of `GQuot Circ CRel` into
`Type 0` with the `geqc` case `<i> Path Field (r y x @ i) y`.

**The laws of the relations.** Each law in examples/circRel.lang is a def
with Pi binders and the body `refl`. evPoint: `evQ y x (gcls Circ CRel c)`
converts with `ev y x c` (the point rule). evEnd0 and evEnd1: `evQ y x
(geqc Circ CRel c1 c2 r @ i0)` converts with `ev y x c1`, and the `@ i1`
form converts with `ev y x c2` (the boundary rule at a constant interval).
evPath: the line `<i> evQ y x (geqc Circ CRel c1 c2 r @ i)` converts with
`r y x` (the path case). The line is an argument of `Path`, and it checks
without a def. satQFwd: `transport (QLine c1 c2 e y x) p` converts with
`witness (e y x) p`, the forward map of `e y x` (the computation rule of
`ua`, as transportPropFwd of examples/transportProp.lang). `QLine` is the
def of the line `<i> SatQ y x (geqc Circ CEqv c1 c2 e @ i)`, as `Pline` of
examples/transportProp.lang. A def with a Pi type is not a JSON instance,
thus test/json/circRel.json has no instance.

**The gap of CEqv.** (D4, O19) An `Equiv` of the two satisfaction types
does not give `Path Field (ev y x c1) (ev y x c2)`, because `y` is also an
input of `ev`. Thus `CEqv` gives no fold of `GQuot Circ CEqv` into Field,
and `GQuot Circ CEqv` carries properties only. The lowering uses `GQuot
Circ CRel`. The open item O19 records this gap.

**No ring normal form.** (D8) `x * x + 0` and `x * x` have different
normal forms (`fieldAdd (fieldMul x x) 0` and `fieldMul x x`). Thus
`refl` is not a proof of `CRel (cmul cpriv cpriv) (cadd (cmul cpriv cpriv)
(clit 0))`, and the check gives TYPE_REFL (test/check/ring-refl.lang). A
`CRel` between two terms with different normal forms needs a proof term
from the program. M7 adds no ring conv rule (docs/M7-PLAN.md question
(i)).

**The transport smoke example.** (D5, D6, D7, D9) `opt : CRel (cmul
cpriv cpriv) (cpow cpriv 1)` is `refl`, because both terms normalize to
`fieldMul x x`. The `cpow` case of `ev` is the Nat fold of M6, and `cpow
b n` is `b` to the power n + 1. The two terms are different
representatives, thus `geqc Circ CRel (cmul cpriv cpriv) (cpow cpriv 1)
opt` is a path between two points of the quotient. The circuit `square`
of examples/circOpt.glue reads this path at `i1`. The circuit `square` of
examples/circPow.glue reads the point `gcls Circ CRel (cpow cpriv 1)`.
Each constraint reduces before the lowering, and each IR equals
test/circuit/square.circuit.json (test/circuit/same-ir.txt). The witness
`y=10 x=3` of circOpt gives WITNESS_FAIL through the quotient
(test/circuit/refuse.txt). In examples/circTransport.lang, `SLine y x` is
the def of the line `<i> SatP y x (geqc Circ CRel (cmul cpriv cpriv)
(cpow cpriv 1) opt @ i)`, as `Pline` of examples/transportProp.lang.
`moveSat y x p` is `transport (SLine y x) p`. It moves a proof of `Sat
(cmul cpriv cpriv) y x` to a proof of `Sat (cpow cpriv 1) y x`. The law
moveSatRefl, `Path (Path Field 9 9) (moveSat 9 3 refl) refl`, is `refl`.
`langc eval` gives `<j> 9` for `m3`, the closed def `moveSat 9 3 refl`
(test/eval/expect.txt). test/json/circTransport.json has no instance.

### 2.10 Direction 4 (M8)

**The indexed binder form.** The domain family `CircV` (`vvar`, `vlit`,
`vadd`, `vmul` and `vpow`) is the circuit syntax with an indexed binder:
`vvar n` is the binder at the index `n` of an environment `Nat -> Field`
(D3 of docs/M8-PLAN.md, O20). An environment is an `elim` at Nat. `envSq
y x` gives `y` at index 0 and `x` at index 1. `envP a y x` gives `a`, `y`
and `x` at the indexes 0, 1 and 2. The value of an environment at a closed
index reduces. `evV en c` is the interpreter into Field (an `elim` at
`CircV`; the `vpow` case is the Nat fold of section 2.8, as the `cpow`
case of `ev`). `CRelV c1 c2` is the pointwise relation `(en : Nat ->
Field) -> Path Field (evV en c1) (evV en c2)`, and `evQV en` is the fold
of `GQuot CircV CRelV` into Field. These laws hold by refl
(examples/circVar.lang): evVPoint (`evQV en (gcls CircV CRelV c)`
converts with `evV en c`), evVSq (`evV (envSq y x) (vmul (vvar 1) (vvar
1))` converts with `x * x`) and optV (`CRelV (vmul (vvar 1) (vvar 1))
(vpow (vvar 1) 1)`). `langc eval` gives 9 for `evVsq` and 3 for `evVidx`
(test/eval/expect.txt). The circuit square of examples/circVarSq.glue
goes through `GQuot CircV CRelV`. The circuit poly of
examples/circVarPoly.glue has two public binders (`a` at index 0, `y` at
index 1) and one private binder (`x` at index 2). The two IRs equal
test/circuit/square.circuit.json and test/circuit/poly.circuit.json
(test/circuit/same-ir.txt). `CircV` is a new family next to `Circ`,
because a `cvar` constructor in `Circ` changes 7 files: each fold over
`Circ` gets one more case. The new family changes no M7 file. A def with
a constructor name of `CircV` gives REFUSE_NAME
(test/check/domain-vvar.lang).

**The specification HIT.** The domain family `SqSpec` has the points
`smul` and `spow` and the law `slaw <i> [ ~i -> smul, i -> spow ]` (D2
of docs/M8-PLAN.md). `smul` and `spow` are two representatives of square
(`cmul cpriv cpriv` and `cpow cpriv 1`), and `slaw` is the law between
them. `interpS` is the fold of `SqSpec` into `GQuot Circ CRel`: the point
cases are the two `gcls` points, and the law case is `lawQ`, the `geqc`
path of `opt`. The laws interpEnd0 and interpEnd1 hold by refl
(examples/spec.lang): `interpS (slaw @ i0)` and `interpS (slaw @ i1)`
convert with the two `gcls` points. The target of the law is the
satisfaction type at a representative, not a fold into Field (O19). Two
forms check. (a) `moveSpec y x p` moves a proof of `Sat` along the line
`<i> SatP y x (interpS (slaw @ i))` (`SpLine`). (b) `SpecSat y x` is the
fold of `SqSpec` into `Type 0` with the law case `ua (idSat y x)`. `idSat
y x` is an `Equiv` of `Sat (cmul cpriv cpriv) y x` and `Sat (cpow cpriv
1) y x` (`isoToEquiv`, both maps `fun p => p`). uaFwd holds by refl
(uaBeta): `transport (UaLine y x) p` converts with `witness (idSat y x)
p`, with `UaLine y x` the line `<i> SpecSat y x (slaw @ i)`. `langc eval`
gives `<j> 9` for `m3s` (`moveSpec 9 3 refl`) and for `m4s` (`transport
(UaLine 9 3) refl`) (test/eval/expect.txt). test/json/spec.json has no
instance. The circuit square of examples/specSq.glue takes `interpS
(slaw @ i1)`, and its IR equals test/circuit/square.circuit.json
(test/circuit/same-ir.txt). A def with a constructor name of `SqSpec`
gives REFUSE_NAME (test/check/domain-slaw.lang).

**The Poseidon hash.** examples/poseidon.lang has the Poseidon
permutation of circomlib over BN254 Fr (D4 to D6 of docs/M8-PLAN.md,
ruling (e)). The parameters and the constants come from the circomlibjs
source (src/poseidon_reference.js and src/poseidon_constants.js;
docs/STATUS.md M8 s3 gives the URLs and the commit hashes): t = 3 (the
capacity word 0 and 2 inputs), 8 full rounds, 57 partial rounds and the
S-box x^5. The file uses the plain form: the 195 round constants C and
the 3 x 3 MDS matrix M. circomlib circuits/poseidon.circom uses the
optimized form (C, S, M, P), and the two forms give the same hash. Each
constant is a closed def with one Field literal: the decimal form of the
hex value of the source, below r (D5). A round adds 3 constants, applies
the S-box `sbox5` to each word (`fullRd`) or to word 0 (`partRd`), then
applies the MDS mix `mdsMix`. `perm3` applies the 65 rounds to a state
of type `Prod Field (Prod Field Field)`. The eval rows
(test/eval/expect.txt): `wrap` (`(r - 1) + 1`) gives 0; `poseidonVec`
(word 0 of `perm3` at (0, 1, 2)) gives the circomlib value of
`poseidon([1, 2])`,
7853200120776062878684798364095072458815029376092732009249414926327459813530;
`fullOne` (`fullRd` at the zero constants and the state (1, 0, 0)) gives
column 0 of M. test/json/poseidon.json has 207 instances. The circuit
form is form (i) (D4): one hint for each S-box output and each state
word, and one constraint for each hint. A probe circuit gave 72 rows for
a full round and 54 rows for a partial round (the D4 estimate was 48 and
46). The circuit `poseidon` of examples/poseidon.glue (slice s4) has the
form (i): 276 hints and 277 constraints (one for each hint, then word 0
== y). It has 3644 rows (the estimate was 8 x 72 + 57 x 54 = 3654) and
k 12 (2^12 = 4096). Its witness at a = 1, b = 2 and the circomlib value
y satisfies each constraint (`satcheck` gives ok); y + 1 gives
WITNESS_FAIL (test/circuit/refuse.txt). The e2e row proves the circuit,
and the verifier contracts/src/verifiers/poseidon.sol accepts the proof.

**The transfer circuit.** The circuit `transfer` of
examples/transfer.glue (slice s5, D7) has one input note and one output
note. Its public binders are the nullifier `nf` and the output
commitment `cm`. Its private binders are the value `v`, the spend key
`sk`, the input randomness `rho` and the output randomness `rho2`. The
constraints are `nf == H(sk, rho)` and `cm == H(v, rho2)`, where H is
the hash of the circuit `poseidon` (word 0 of the permutation of the
state (0, x, y)). The shared `v` is the value conservation for one
input and one output. Each hash has the form (i):  the circuit has 552
hints (276 for each hash;  the prefix n for the nullifier, the prefix c
for the commitment) and 554 constraints (one for each hint, then
`ns64k0 == nf` and `cs64k0 == cm`). It has 7288 rows and k 13
(2^13 = 8192;  the D7 estimate was about 6000 rows and k 13). Its
witness at sk = 1, rho = 2, v = 3 and rho2 = 4 satisfies each
constraint (`satcheck` gives ok);  nf + 1 gives WITNESS_FAIL
(test/circuit/refuse.txt). At these inputs nf is the vector
poseidon([1, 2]) of the circuit `poseidon`. The e2e row proves the
circuit, and the verifier contracts/src/verifiers/transfer.sol accepts
the proof.

The input limits:  each source file can have 1 MiB (1048576 bytes) at
most.  Each JSON file of `langc satcheck` (the circuit and the witness)
can have 16 MiB (16777216 bytes) at most (USER ruling (a), M8 s5).  A
larger file gives IO_SIZE.  The circuit JSON of `transfer` has 1080055
bytes and its witness JSON has 1251199 bytes.

**The transfer specification.** `TransferSpec` is a domain family of
domain/domain.lang in the D1 form (D8):

```
family TransferSpec := note | nullifier | commitment | noDoubleSpend <i> [ ~i -> nullifier, i -> note ] | balanceCons <i> [ ~i -> note, i -> commitment ]
```

Its 3 points name the representatives of the transfer circuit (the
note, the nullifier and the commitment). Its 2 laws are paths between
them:  `noDoubleSpend` goes from `nullifier` to `note`, and
`balanceCons` goes from `note` to `commitment`. A def with a
TransferSpec name gives REFUSE_NAME (rule R4,
test/check/domain-noDoubleSpend.lang). The fold of TransferSpec into
`Type 0` (D2 (b)) sends each law to `ua` of an `Equiv` of satisfaction
types (slice s6).

examples/transferSpec.lang (slice s6) moves the law `noDoubleSpend` to a satisfaction proof of the transfer circuit. `Sat2 a b nf cm` is the satisfaction type of the 2 constraints of the circuit (`a` is the nullifier hash and `b` is the commitment hash), and `TSat` binds `a` and `b` to the hash `pos2` of the circuit. At the nullifier point the nullifier path starts at the public `nf` (`NullSat`), and at the commitment point the commitment path starts at the public `cm` (`CommSat`). The two representatives of each law have different normal forms, thus each `Equiv` is an iso with a proof term (the path inverse on one component). `transferLaw` is `transport` along `<i> TransferSat a b nf cm (noDoubleSpend @ i)`, and it is the forward map of `idNull` by uaBeta (`transferFwd`, refl). The eval row `transferD7` is a proof of `TSat` at the D7 inputs. The hash values are arguments of the law:  a hash in the types is computed again at each step of the Glue transport (s6 probe:  31.7 s for one eval, 0.36 s with the hash values as arguments). A law for all inputs with `pos2` in its type gives EVAL_FUEL.

examples/transferSpec.lang (slice s1 of M9, D4) also gives the law for all inputs (O21). `TSatH h nf cm v sk rho rho2` is `Sat2 (h sk rho) (h v rho2) nf cm`, with the hash `h : Field -> Field -> Field` as an argument. `transferSatH` is the law of `noDoubleSpend` for all `h` and all inputs (through `transferLaw`);  `transferD7H` is its instance at `pos2` with the D7 inputs, and it gives the pair of `transferD7`. A neutral `h` stops the hash in the Glue transport, thus the law gives no EVAL_FUEL. `BalLine a b nf cm` is `<i> TransferSat a b nf cm (balanceCons @ i)`, a path from `Sat2` to `CommSat`;  `balLaw` is `transport` along `BalLine`, and `balD7` gives the pair `nf`, `cm` at the D7 inputs. The other orientation of `BalLine` (from `CommSat` to `Sat2`) gives PATH_BOUNDARY (test/check/bal-orientation.lang).

**The credential specification.** `KycSpec` is a domain family of the form of `TransferSpec` (docs/M9-PLAN.md D3, slice s3 of M9):  the points `credential`, `kycNullifier` and `predicate` name the representatives of the credential circuit (examples/kyc.glue), `noReplay` is the law from the nullifier point to the credential point and `disclosure` is the law from the credential point to the predicate point. examples/kycSpec.lang gives the satisfaction types with the hash as an argument (D4):  `CredSat a b nf cm min attr d` is `Prod (Path Field a cm) (Prod (Path Field b nf) (Path Field (d + min) attr))`, `NfSat` turns the nullifier path and `PredSat` turns the predicate path. `KycSat` is the fold of `KycSpec` into `Type 0`:  each law goes to `ua` of an `Equiv` (an iso with the path inverse on one component). `kycLaw` is `transport` along `<i> KycSat a b nf cm min attr d (noReplay @ i)`, `discLaw` is `transport` along the `disclosure` line, and `kycLawH h cm nf scope min attr sk d` applies `kycLaw` at `a = h attr sk` and `b = h sk scope`. The instances `kycDH` and `discD` at `h = pos2` and the values of the circuits row `kyc` evaluate to pairs of constant paths (test/eval/expect.txt). A def with a constructor name of `KycSpec` gives REFUSE_NAME (rule R4;  test/check/domain-noReplay.lang).

```
family KycSpec := credential | kycNullifier | predicate | noReplay <i> [ ~i -> kycNullifier, i -> credential ] | disclosure <i> [ ~i -> credential, i -> predicate ]
```

The fifth public binder `sender` of the circuits `kyc` and `kycMerkle`
(M10 s1, O22) is the admitting address as a field value (the gate rows
use the anvil account 0 address 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
as the integer 1390849295786071768276380950238675083608645509734);  the
row `sender == sender` puts the binder in one constraint (a public binder
in no constraint gives CIRCUIT_UNUSED_PUBLIC), the row does not restrict
the value, and `KycGate.admit` checks the fifth instance against
`msg.sender` (`WrongSender`).  The constructor namespace of the domain
families is flat (src/front/check.c name_owner, rule R4):  a point or law
name is unique over all families, thus `TransferSpec` has the point
`nullifier` and `KycSpec` has the point `kycNullifier` (M10 ruling (f),
docs/M10-PLAN.md D6).

## 3. Type formers

> Write here: only the deviations from the formers. Do not copy the forms.
> Give each former whose status in `formers/tcc-json.md` is not DONE, and the
> open item (section 9) that records it.

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-json column of the realization matrix. `formers/tcc-json.md` gives
the host form of each former.

| ID | Status on tcc-json | Effect on this language | Open item |
|---|---|---|---|
| F12 | DONE (M3) | `Path A x y` replaces `Eq` (ruling 2026-10-14). `refl` and `sym` compute (section 2.4). `trans p q` is a composition by `hcomp` (section 2.5). | none |
| F13 | DONE (M3) | `cong` computes over Path (section 2.4). `transport p x` is `transp (<i> p @ i) i0 x` (section 2.5). There is no regularity: `transport refl x = x` holds as the path `transportRefl x`, not by computation (D3). | none |

## 4. Structures

> Write here: the carriers of Monad (F5), fold and unfold (F6, F7) and
> filter (F8) that the design uses. Name each carrier that the host does not
> give.

- **Monad**: `pure`, `map` and `bind` on Option, Sum E and List (the core
  instance), with the List join `append` (section 2.8). The laws are in
  examples/monad-laws.lang. Left identity holds by refl at each carrier
  (leftIdO, leftIdS and leftIdL). Right identity, associativity and the
  map law at a variable m are proof terms by `elim` on m (rightIdO/S/L,
  assocO/S/L and mapBindO/S/L). At List they also use `cong`, `sym`,
  `trans` and the lemmas appendAssoc and bindAppend (each by `elim` at
  List). At closed values each law holds by refl.
- **Algebra**: `fold` on Nat, List A, each domain family, each HIT and a
  `mutual` block (sections 2.6 and 2.7). `elim` is the dependent form at a
  family, at a carrier and at Nat. The laws are in examples/fold-laws.lang:
  foldZero, foldSucc, foldCons and foldPush hold by refl. Uniqueness (F6)
  is a proof term at Nat, List and a plain family (foldNatUniq,
  foldListUniq and foldFamUniq). Uniqueness at a HIT is O14.
- **Coalgebra**: `unfold g n s` into List A and into Nat, with an explicit
  Nat depth n (section 2.8). The laws are in examples/unfold-laws.lang:
  unfoldZero, unfoldNone and unfoldSome hold by refl in each form.
  Uniqueness is a proof by `elim` at the limit (unfoldListUniq and
  unfoldNatUniq). There is no `unfold` into a domain family: no domain
  family needs it (D9 of docs/M6-PLAN.md).
- **Filterable**: M6 does not change `filter` (formers/tcc-json.md F8).
- **Circuit quotient** (M7, section 2.9): the domain family `Circ`
  (`cpub`, `cpriv`, `clit`, `cadd`, `cmul` and `cpow`) is the syntax of a
  circuit with one public binder and one private binder. `ev y x c` is its
  interpreter into Field (an `elim` at `Circ`; the `cpow` case is the Nat
  fold of section 2.8). `CRel c1 c2` is the pointwise relation `(y :
  Field) -> (x : Field) -> Path Field (ev y x c1) (ev y x c2)`, and `evQ y
  x` is the fold of `GQuot Circ CRel` into Field. The lowering uses this
  quotient: it runs on the representative, and a circuit through the
  quotient gives the IR of the representative. `CEqv c1 c2` is an `Equiv`
  of the satisfaction types `Sat c1 y x` and `Sat c2 y x` at each `y` and
  `x`. `SatQ` is the fold of `GQuot Circ CEqv` into `Type 0` (the `geqc`
  case by `ua`), and `SatP` is the fold of `GQuot Circ CRel` into `Type
  0`. `GQuot Circ CEqv` carries properties only (O19). `opt : CRel (cmul
  cpriv cpriv) (cpow cpriv 1)` holds by refl, and `moveSat` moves a proof
  of `Sat` from `cmul cpriv cpriv` to `cpow cpriv 1` by `transport` along
  the `SLine` def. The laws evPoint, evEnd0, evEnd1, evPath, evHcomp,
  optRefl, moveSatRefl and satQFwd hold by refl (examples/circRel.lang,
  evHcomp.lang and circTransport.lang). There is no ring normal form (D8
  of docs/M7-PLAN.md): a relation between two different normal forms is a
  proof term. The indexed binder form is the family `CircV` (section
  2.10). The specification HIT is the family `SqSpec` (section 2.10).
- **Direction 4** (M8, section 2.10): the family `CircV` (the binder
  `vvar n`, an environment `Nat -> Field`) and its quotient `GQuot CircV
  CRelV`; the specification HIT `SqSpec` and its fold `interpS` into
  `GQuot Circ CRel`; the Poseidon permutation (examples/poseidon.lang,
  circomlib BN254, t = 3) and the circuits `poseidon` (3644 rows, k 12)
  and `transfer` (7288 rows, k 13); the specification HIT `TransferSpec`
  and the transport of its law `noDoubleSpend`. The laws of
  docs/M8-PLAN.md section 4: evVPoint, evVSq, optV, interpEnd0,
  interpEnd1 and uaFwd hold by refl (examples/circVar.lang and
  spec.lang); specMove (the eval rows `spec m3s` and `spec m4s`, `<j>
  9`), wrap, poseidonVec and transferD7 are eval rows
  (test/eval/expect.txt); transferFwd holds by refl (uaBeta). transferLaw
  takes the hash values as arguments: a law for all inputs with the hash
  in its type is O21 (closed in M9, slice s1). M8 adds no IR form and no
  conv rule. The lowering
  has no subterm sharing, thus a Poseidon round has one hint for each
  S-box output and state word (form (i) of docs/M8-PLAN.md D4).
- **ZK-KYC** (M9, section 2.10): the credential circuit `kyc` (D1 of
  docs/M9-PLAN.md: `cm = pos2 attr sk`, `nf = pos2 sk scope`, `d + min
  == attr` with `range8 d`; 7290 rows, k 13) and the Merkle credential
  circuit `kycMerkle` (D2: a public issuer root, depth 20; k 15); the
  specification HIT `KycSpec` (D3) with its fold `KycSat` into `Type 0`
  and the laws `kycLaw`, `discLaw` and `kycLawH` (examples/kycSpec.lang);
  the law for all inputs `transferSatH` (D4, O21). The halo2 back end
  reads a ceremony SRS pinned by SHA-256 (D5: halo2/ceremony.sha256,
  `make e2e-ceremony`; the ceremony verifiers stay under build/). The
  consumer contract `KycGate` (D7, contracts/src/KycGate.sol): the
  verifier, the scope and `min` are immutable, the issuer approves an
  anchor (`cm` or the Merkle root), and `admit` checks the instances,
  calls the verifier and records the nullifier once. The deployment
  (D6) is the anvil rehearsal, the check script and the runbook
  (contracts/README.md); the USER runs the broadcast. M9 adds no IR
  form, no conv rule and no src/ change. The sender binding is O22 and
  the deployment target is O23.
- **The sender binding** (M10, section 2.10):  the fifth public binder
  `sender` of `kyc` and `kycMerkle` (ruling (b):  the raw address as a
  Field;  ruling (c):  the row `sender == sender`, form B;  `public` 4
  to 5, one arith row and one copy:  kyc 7291 rows, k 13;  kycMerkle
  21899 rows, k 15) and the gate check:  `KycGate.admit` takes five
  instances and refuses with `WrongSender` if the fifth instance is not
  `msg.sender` (contracts/src/KycGate.sol).  The operations side (slice
  s2):  the read-only check derives the `--from` address of the
  admission call from the fifth instance of the proof and names it, the
  rehearsal and the deployment regression name the sender, and the
  runbook has the admission step (contracts/README.md).  A relayer
  cannot admit the proof of another holder (O22, closed).  Ruling (f):
  the domain name `kycNullifier` stays.  The Poseidon forms (ii) and
  (iii) are an estimate only (D7 of docs/M10-PLAN.md, ruling (g)):  the
  hypothesis that neither form changes k fails for `kycMerkle` (k 15 to
  14);  the forms are O24.  M10 adds no IR form, no conv rule and no
  src/ change.  The deployment target stays O23.
- **The structural row memo** (M11, section 7.1):  form (iii) of O24
  (rulings (a) to (h) of docs/M11-PLAN.md section 9).  The lowering
  shares an ARITH row that has the same op and the same operand cells,
  and a CONST row that has the same value, across all constraints of a
  circuit.  The rows and k of the 4 Poseidon circuits:  `poseidon` 1972
  rows, k 11;  `transfer` 3740 rows, k 12;  `kyc` 3743 rows, k 12;
  `kycMerkle` 10847 rows, k 14.  `square`, `poly`, `range` and `nonzero`
  do not change.  M11 adds no IR form, no conv rule and no new check
  code.  examples/memoKey.glue and the circuit refusal row 22 of
  test/circuit/refuse.txt test the op in the memo key.  Forms (ii) and
  (iv) stay O24.  The deployment target stays O23.
- **The literal fold** (M12, section 7.1):  form (iv) of O24 (rulings
  (a) to (h) of docs/M12-PLAN.md section 9).  T1 (slice s1):  the
  lowering puts a literal operand into the coefficients ql, qr, qm and
  qc of its arith row;  a literal that is a constraint side keeps its
  CONST row.  T2 (slice s2), the affine fold:  an arith row whose uses
  are all operands of arith rows goes into each consumer when the form
  of the consumer still fits the gate.  The rows and k under T2 (the D3
  T2 column of docs/M12-PLAN.md):  `poseidon` 990 rows, k 10;
  `kycMerkle` 5970 rows, k 13.  Section 9 O24 has the counts of each
  circuit.  M12 adds no IR form, no conv rule and no new check code:
  the fold is a lowering change.  examples/foldKey.glue and the circuit
  refusal row 23 of test/circuit/refuse.txt (slice s1) test the
  coefficients in the memo key.  Form (ii) stays O24.  The deployment
  target stays O23.

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
BINDER=VALUE...` writes NAME.witness.json. It computes the value of each hint
from its term (section 2.2). docs/IR.md is the contract for the
two documents. M1 uses the gate set of docs/IR.md section 4: advice a, b, c;
fixed ql, qr, qo, qm, qc, qrange, t; instance column 0; gate `arith` with
ql*a + qr*b + qm*a*b + qc - qo*c = 0; lookup `range8` with input qrange*a and
table t.

The compiler evaluates each constraint side to a normal form, with the binders
as variables. Then it lowers the normal form:

1. Rows 0 to n-1 are input rows, one for each binder: the public binders,
   then the private binders, then the hints. The value of the binder is in
   advice a. A hint row has the form of a private binder row: it has no
   instance copy, and its selectors are zero (item 5 can set qrange).
2. A Field literal k that is a constraint side is a constant row: qc = k,
   qo = 1. The value is in c.  A Field literal that is an operand of `+`,
   `-` or `*` has no row (item 3).
3. Each `+`, `-` or `*` is an arith row with qo = 1. The value is in c.
   The literal fold (M12 s1, O24 form (iv) T1):  each operand is
   S * cell + C, with S = 1, C = 0 for a wire and S = 0, C = k for a Field
   literal k.  `x + y` sets ql = Sx, qr = Sy, qc = Cx + Cy.  `x - y` sets
   ql = Sx, qr = r - Sy (that is, -Sy), qc = Cx - Cy.  `x * y` sets
   qm = Sx * Sy, ql = Sx * Cy, qr = Cx * Sy, qc = Cx * Cy.  Thus `x + y`
   on two wires sets ql = qr = 1, `x - y` sets ql = 1, qr = r - 1, `x * y`
   sets qm = 1, and `x + 5` sets ql = 1, qc = 5.  Cell a is an operand when
   ql or qm is not 0;  cell b is an operand when qr or qm is not 0.  Copies
   tie each operand cell to the wire of its operand.  A cell that is not
   an operand has no copy, and its value is 0.
4. `a == b` is a copy between the two wires. If the two wires are one cell,
   the compiler writes no copy.
5. `range8 e` on a binder sets qrange = 1 on the input row of the binder.
   On any other wire, it adds a range row: a copy of the wire in a, and
   qrange = 1.

The memo (M11 s1).  The lowering keeps one memo table for each circuit,
across all of its constraints.  It shares a constant row (item 2) with an
earlier constant row that has the same value, and an arith row (item 3)
with an earlier arith row that has the same 4 coefficients ql, qr, qm, qc
on the same operand cells (M12 s1:  the key is the row kind, the 4
coefficients and the operand cells;  a cell that is not an operand is not
in the key).  The compiler then writes no new row and uses the wire of
the earlier row.  Thus `x + y` and `y + x` get two rows, `x * y` and
`x + y` get two rows (examples/memoKey.glue, test/circuit/refuse.txt row
22), and `x + 5` and `x + 7` get two rows (examples/foldKey.glue, row
23).  Two rows with the same key hold the same value in each satisfying
witness.  A
binder has no row in the memo and no entry:  its wire is its input row
(item 1).  A range row (item 5) is not shared.  The memo changes no IR
form (docs/IR.md does not change).  It changes the rows, the copies and
the fixed values of a circuit that repeats a literal or an operation, and
it can make k smaller.  The literal fold (item 3) also changes no IR form
(M12 rulings (b) and (f)):  the gate `arith` accepts any value in each
coefficient column.  It changes the rows, the copies and the fixed values
of a circuit with a literal operand.

The affine fold (M12 s2, O24 form (iv) T2).  After the lowering, the
compiler gives each arith row a form:  a sum of monomials of degree 2 at
most on the cells of the rows that stay.  An arith row whose uses are all
operands of arith rows goes into each consumer row when the form of the
consumer, with the form of the row in place of its cell, still fits the
gate:  at most two cells, degree 2 at most, and no square of one cell
when two cells occur.  A consumer takes both of its operand rows when
the form fits, else the row of x, else the row of y.  The row goes when
each consumer takes it.  A range row (item 5) and each side of a check
(item 4) also use a row, thus such a row stays.  The compiler then gives
new numbers to the rows after the input rows, and to the cells of the
copies and the checks.  Thus `m0 * t0 + m1 * t1` with m0 and m1 Field
literals is one row (ql = m0, qr = m1), `(s + rc) * (s + rc)` is one row
(a = b = s, qm = 1, ql = 2 rc, qc = rc^2), and `x4 * (s + rc)` is one
row (qm = 1, ql = rc).  The affine fold changes no IR form and no memo
key.  The new form of each consumer has the value of the old form in
each satisfying witness:  the fold is a change of the lowering, not a
conv rule.  It changes the rows, the copies, the fixed values and k of a
circuit with an arith row inside an arith row.

The lowering has no row for `inv`. A residual `inv` in a normal form (for
example `inv x` with x a binder, also through a definition) gives
CIRCUIT_LOWER. A closed `inv k` computes to a literal before the lowering,
thus `x * inv 1 == x` lowers (examples/nonzero.glue, circuit invOne).

The copies are in this order: instance(0, i) to advice(a, i) for each public
binder i; for each row, a then b (each operand cell only, item 3); then the
`==` copies.

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
  constrain it. M6 (slice s5): the same applies to a hint. A hint that is
  only in `v == v` passes the check (docs/STATUS.md M6 s5, open item 2).
- O4. Closed in M6 (slice s5). The core op `inv` gives a stuck Field value
  that the lowering refuses: test/check/hint-residual.glue (`x * inv x ==
  1`) and test/check/inv-helper.glue (the same through a definition) give
  CIRCUIT_LOWER. The text below is the record before M6.
  No gate test gave CIRCUIT_LOWER. M1 has no Field-valued normal
  form other than literals, binders and `+ - *`. M3 (slice s5) found no
  circuit that gives it. A circuit has only Field binders (no interval
  binder and no type binder), thus the closed-type rules reduce each Kan
  term in a constraint. Examples: `hcomp Field i0 (fun k => [ i0 -> x ])
  (x * x)` gives `x * x`. With hfT = `hcomp (Type 0) i0 (fun i => [ i0 ->
  Field ]) Field`, `unglue (transp (<_> hfT) i0 (glue [ i0 -> x ] x))` and
  `unglue` of an `hcomp` in hfT give `x`. M5 (slice s6) found no circuit
  that gives it (O12 (f)). O4 stayed open for M7. M7 (slices s1 to s4)
  found no quotient value that gets to the lowering stuck (A1 to A4 of
  docs/M7-PLAN.md, section 2.9): the point rule, the boundary rule at a
  constant interval and the `hcomp` rule of the eliminator reduce each
  quotient value in a constraint, and each M7 circuit gives the IR of
  `square`.
- O5. Closed in M4. RULED 2026-10-15 (USER): D9 of docs/M4-PLAN.md. The
  JSON target writes a Path component of a data value as `null` only when
  the path is constant (its body at a fresh interval variable does not use
  the variable). A path with content, such as a loop, gives JSON_VALUE
  (section 2.6; test/emit/path-content.lang).
- O6. Decided in M2 (slice s4). An instance whose type is `Partial phi A`
  is refused with SORT_FIBRANT (src/json.c instance_json). The checker
  refuses a type in `SSet` under a type former, so only the type of the
  instance itself can be in `SSet`. A function with an interval binder is
  not an instance.
- O7. Closed in M3. M2 had no application of a partial element.
  `Partial i1 A` evaluates to `A`. Under each conjunction of `phi`, a term
  of type `Partial phi A` has type `A`, so the ordinary elimination rules
  for `A` apply there (D4 of docs/M3-PLAN.md, slice s1). `transp` and
  `hcomp` use it (section 2.5).
- O8. Closed in M6 (slice s4). RULED 2026-10-17 (USER): O8 closes as rules,
  not as gaps (docs/M6-PLAN.md question d; section 2.8, D10 and D11). The
  closed-line `transp` rule also reduces at neutral values of closed Sum,
  Option and List types. At eligible data, a constant `hcomp` system gives
  its base, including a neutral base. Other data cases follow the table in
  section 2.8: incompatible constructor heads give HCOMP_BASE, while two
  neutral heads can stay stuck. An open type can also leave Kan operations
  stuck. At a HIT, `hcomp` is canonical unless `phi = i1` (section 2.6).
  In an instance, a stuck Kan
  value, a canonical `hcomp` value or a value of a Glue type gives
  JSON_VALUE (src/json.c; test/emit/transp-hcomp.lang, hcomp-instance.lang
  and glue-instance.lang). The stuck values at a HIT are O12.
- O12. New in M4. The values that stay stuck in M4, and the gaps of M4.
  Narrowed in M5 (D10 of docs/M5-PLAN.md):
  - (a) and (b) are rules, not gaps (section 2.7, neutral values): `fold B
    cases x` with a neutral scrutinee `x`, and `transp` at a family with a
    neutral argument, are neutral values.
  - (c) Open, narrowed. `transp` of a path constructor whose fieldwise
    transport does not agree with transport on a boundary face stays stuck
    (there are no general boundary correction terms). The M5 equations
    `bswap` and `zeq` agree (docs/STATUS.md M5 s1, D2), thus they reduce.
    `transp` of `squashc` of Quot (a path constructor with Path fields)
    stays stuck (M5 s3).
  - (d) Closed in M5 (slice s3). RULED 2026-10-16 (USER): the Sigma
    method. `transp` and `hcomp` at a constructor with a dependent field
    or a Path field reduce (section 2.7).
  - (e) Closed in M5 (slice s2). RULED 2026-10-16 (USER): `elim` in M5.
    With `ret`, `transport (ua (isoToEquiv OTPSpec OTPSpec flip flip ret
    ret)) message = cipher` holds for the non-identity iso `flip`
    (examples/otp.lang).
  - (f) O4 closed in M6 (slice s5, the `inv` CIRCUIT_LOWER rows). M5 s6 probed CIRCUIT_LOWER with a stuck
    `elim` in a constraint: no stuck `elim` gets to the lowering. A binder
    `z : Int` gives CIRCUIT_BINDER (a circuit has Field binders only). An
    `elim` at `zeq @ k` with k := i0, at `hcomp Int i0 (fun i => [ i0 ->
    pos 0 ]) (pos 0)`, and at `hcomp Int k (fun i => [ k -> zeq @ i ])
    (pos 0)` with k := i0 each reduce, and the IR is equal to `square`.
- O13. New in M5. The full quotient inductive-inductive form is not in M5
  (RULED 2026-10-16 (USER), docs/M5-PLAN.md question d): equations (path
  constructors) in a `mutual` block (MUTUAL_BLOCK); non-uniform indices
  such as `Ty (ext g a)`; `elim` at a block family (ELIM_CASES). A block
  holds two families only.
- O14. New in M6. F6 uniqueness at a HIT: the path cases of the proof need
  squares (D7 of docs/M6-PLAN.md). M6 proves uniqueness at Nat, List and a
  plain family only.
- O15. New in M6. Resource limits at a huge literal: `dbl
  18446744073709551615` and `fold plus1 0 18446744073709551615` stop with
  OOM (the 1 GiB arena), and `unfold` at the limit 18446744073709551615
  stops with EVAL_FUEL. Each is a limit, not a wrong value.
- O16. New in M6. The TYPE_MISMATCH message for a proof that needs two
  traps to convert prints the same text for the two types (`Path Nat trap
  0`), thus it reads as a contradiction. A USER ruling can change the text.
- O17. Closed in M7 (slice s5). examples/hintTrap.glue holds the circuit
  `hintTrap` of the overflow fold below, and the row `1 WITNESS_HINT
  witness hintTrap hintTrap x=0` in test/circuit/refuse.txt is the
  automated WITNESS_HINT row. `langc check` gives ok, and `langc witness`
  gives WITNESS_HINT with exit 1 and no file. The text below is the record
  of M6.
  New in M6. The automated circuit refusal suite has no WITNESS_HINT
  row. This refusal is reachable: a hint with the Field term
  `fold (fun (y : Field) => y) 0 (natAdd 18446744073709551615 1)` computes
  to a trap, so `witness` gives WITNESS_HINT (exit 1). Refusing `axiom a :
  Field` with REFUSE_AXIOM (rule R2) does not exclude this overflow case.
- O18. New in M6. The closed-line rule of `transp` (section 2.8) accepts a
  HIT family with closed parameters (`Susp Nat`). RULED 2026-10-18 (USER):
  D10 of docs/M6-PLAN.md covers a HIT family with closed parameters (`Susp
  Nat`) (ruling (f), docs/M7-PLAN.md section 9). A family with a dependent
  field is not eligible for the `hcomp` rule, also when the field type
  does not use the earlier field.
- O19. New in M7 (slice s3, D4 of docs/M7-PLAN.md). The gap of `CEqv`
  (section 2.9, **The gap of CEqv.**). An `Equiv` of the two satisfaction
  types does not give `Path Field (ev y x c1) (ev y x c2)`, because `y` is
  also an input of `ev`. Thus `GQuot Circ CEqv` has no fold into Field and
  carries properties only (`SatQ`). The lowering uses `GQuot Circ CRel`
  (RULED 2026-10-18 (USER), docs/M7-PLAN.md section 9).
  M8 s2 (D2 of docs/M8-PLAN.md): the specification HIT `SqSpec` targets
  the satisfaction types, not a fold into Field. Its law case in `Type 0`
  is `ua` of an `Equiv` of the two `Sat` types (section 2.10). O19 stays
  open.
- O20. Closed in M8 (slice s1). The domain family `CircV` has the binder
  `vvar n`, the index `n` of an environment `Nat -> Field` (section 2.10).
  The text below is the record of M7.
  New in M7 (ruling (h), docs/M7-PLAN.md section 9). The binders of
  `Circ` are `cpub` and `cpriv`: one public binder and one private binder
  (the shape of square). A circuit with more binders (poly has two public
  binders) needs an indexed binder form (`cvar (n : Nat)` with an
  environment). RULED 2026-10-18 (USER): this form is M8 work.
- O21. Closed in M9 (slice s1). The law for all inputs takes the hash as
  an argument `h : Field -> Field -> Field` (docs/M9-PLAN.md D4).
  `transferSatH` of examples/transferSpec.lang is the law of
  `noDoubleSpend` for all `h` and all inputs (through `transferLaw`), and
  `transferD7H` is its instance at `pos2` (one instance, the D7 inputs).
  A neutral `h` stops the hash in the Glue transport: no EVAL_FUEL.
  `balLaw` is the transport of `balanceCons` along `BalLine`, from `Sat2`
  to `CommSat` (`balD7`). The text below is the record of M8.
  New in M8 (slice s6). The hash of open inputs. Probe-forced
  (slice s6): a law for all inputs with the Poseidon hash in its type
  (`transferSat : .. -> NullSat (pos2 sk rho) (pos2 v rho2) nf cm -> TSat
  nf cm v sk rho rho2`) gives EVAL_FUEL (20,000,000 steps, 2.1 s). A hash
  in a type family is computed again at each step of the Glue transport
  (the s6 probe v1 at closed inputs: 31.7 s). Thus `transferLaw` of
  examples/transferSpec.lang takes the hash values as arguments, and the
  eval row `transferSpec transferD7` is at the D7 inputs. The transport of
  `balanceCons` is also open: its iso (`idComm`) and its fold case (`ua
  (idComm ..)`) are in the example, but no def moves it. Open for M9
  (closed in slice s1: `balLaw` and `balD7`, above).
- O22. Closed in M10 (slices s1 and s2).  The sender binding.  M9 s6 left
  `KycGate.admit` without a bind of `msg.sender`:  a relayer that saw a
  proof before the holder's transaction could send it first and spend the
  nullifier.  M10 s1 did the circuit and the gate side:  the fifth public
  binder `sender` and the row `sender == sender` of `kyc` and `kycMerkle`
  (section 2.10), the four goldens, the error `WrongSender` and the check
  of the fifth instance against `msg.sender` in `KycGate.admit`, the
  five-instance verifiers and fixtures.  M10 s2 did the operations side:
  the read-only check derives the `--from` address of the admission call
  from the fifth instance of the proof and names it in its report line,
  the rehearsal and the deployment regression name the sender (the anvil
  account 0 is the deployer, the issuer and the holder of the e2e proofs),
  the deployment regression admits with an issuer that differs from the
  holder, and the runbook has the admission step (the transaction comes
  from the bound address;  the gate refuses any other sender).  A relayer
  cannot admit another holder's proof.
- O23. New in M9 (slice s6). The deployment target. The five USER facts
  of question (e) (the network, the chain id, the RPC URL, the explorer
  with its verify endpoint, the keystore account name) came as the
  literal word VALUE (2026-10-19): the Makefile variables and the
  runbook take them as parameters, the rehearsal ran at chain id 31337
  (anvil), and no value is in a repository file. The USER fills them at
  the broadcast. Also open: the check of the Axiom copy of the ceremony
  file against the Perpetual Powers of Tau response (a USER item).
  The pinned SRS supports circuits with k <= 15. A circuit requiring
  k > 15 needs a larger SRS file and a new pin (halo2/ceremony.sha256).
- O24. New in M10 (slice s4).  The Poseidon forms (ii) and (iii) of
  docs/M8-PLAN.md D4 (M10 ruling (g):  an estimate, no code).  The
  count (docs/STATUS.md M10 `### s4`, from the lowering rules of
  section 7.1 and the golden test/circuit/poseidon.circuit.json):  the
  S-box `sbox5 (s + rc)` of form (i) is 14 rows, because the lowering
  of M10 has no sharing and `s + rc` is lowered 5 times (5 constant rows, 5
  add rows, 4 mul rows);  a full round is 72 rows and a partial round
  is 54 rows.  Form (ii) (a degree-5 gate) saves 11 rows for each S-box
  on a binder, or 13 if the gate holds the round constant.  Form (iii)
  (a memo table) saves 8 rows for each such S-box and 8 rows for each
  partial round (the two sums `s + rc` that its three mix hints
  repeat).  If the memo spans all H hashes and also shares literal rows,
  it saves a further `585 * H - 9 + 195 * (H - 1)` rows:  the 9 MDS
  constants reused over 65 rounds per hash, plus the 195 other literal
  values (194 round constants and the closed first S-box output) reused
  across hashes.  This is 576 rows for one hash, 1356 for two and 4476
  for six, as in the table.  The hypothesis of
  docs/M10-PLAN.md D7 (neither form changes k) holds for `poseidon`
  (k 12), `kyc` and `transfer` (k 13) under form (ii) and under form
  (iii) over binder subterms, and fails for `kycMerkle`:  form (iii)
  gives k 14 (15323 rows), and form (ii) gives k 14 only with the
  constant in the gate (15659 rows).  The literal memo gives k 11 for
  `poseidon` and k 12 for `kyc` and `transfer`.  The forms stay open as
  a milestone candidate:  each changes the IR of each Poseidon circuit
  or the verifier generator (M9 ruling (h)) and needs a USER ruling
  first.  The pinned SRS (k <= 15) covers each k of the table.  The M10
  close (slice s5, USER answer (i) of 2026-10-20) keeps O24 open:  no
  milestone takes the forms yet.  M11 s1 (USER rulings (a) to (h) of
  docs/M11-PLAN.md section 9, 2026-10-20) builds form (iii) as the
  structural row memo of section 7.1, across all constraints of a
  circuit and also for the constant rows.  Form (iii) is DONE.  The
  measured rows and k (the M10 golden, then the memo):  `poseidon` 3644
  at k 12 to 1972 at k 11;  `transfer` 7288 at k 13 to 3740 at k 12;
  `kyc` 7291 at k 13 to 3743 at k 12;  `kycMerkle` 21899 at k 15 to
  10847 at k 14.  These are the numbers of the model (docs/M11-PLAN.md
  section 6).  `square`, `poly`, `range` and `nonzero` do not change.
  Form (ii) (the degree-5 gate) and form (iv) (a literal folded into ql,
  qr or qc of the arith gate, new in M11) stay open.  The depth table
  (docs/M11-PLAN.md D1, the M11 close):  the rows of one Merkle level =
  (kycMerkle - kyc) / 4, because `kycMerkle` has depth 4 (6 hashes, the
  bits b0 to b3);  rows(d) = the rows at depth 4 + (d - 4) x the level
  rows;  k 15 has 32752 usable rows.

  | Form | rows a level | kycMerkle (depth 4) | max depth at k 15 | depth 20 |
  |---|---|---|---|---|
  | (i), the current lowering | 3652 | 21899, k 15 | 6 | 80331, k 17 |
  | pointer memo (D3) | 3012 | 18059, k 15 | 8 | |
  | binder memo (P1, no hint rows) | 2556 | 15323, k 14 | 10 | |
  | structural memo (D2, a model) | 1776 | 10847, k 14 | 16 | 39263, k 16 |

  M11 s1 measured the structural memo:  kycMerkle 10847 rows, k 14, as
  the model.  At k 15 the Merkle depth is 6 under form (i) and 16 under
  the structural memo.  A depth-20 tree is above the pinned SRS under
  each form.  Ruling (d):  the depth stays 4 in M11.  At the M11 close
  O24 stays OPEN for forms (ii) and (iv).  M12 (USER rulings (a) to (h)
  of docs/M12-PLAN.md section 9, 2026-10-20) builds form (iv) in two
  steps:  T1, a literal operand into the coefficients ql, qr, qm, qc of
  its arith row (section 7.1 item 3), and T2, the affine fold (slice s2).
  T1 is DONE (M12 s1).  T2 is DONE (M12 s2, section 7.1, the affine
  fold).  The measured rows, k and copies (the memo, then T1, then T2;
  the D3 T1 and T2 columns of docs/M12-PLAN.md, exact):

  | Circuit | memo (M11) | T1 (M12 s1) | T2 (M12 s2) |
  |---|---|---|---|
  | poseidon | 1972, 11, 3256 | 1769, 11, 2477 | 990, 10, 1698 |
  | transfer | 3740, 12, 6512 | 3537, 12, 4954 | 1979, 11, 3396 |
  | kyc | 3743, 12, 6518 | 3540, 12, 4960 | 1982, 11, 3402 |
  | kycMerkle | 10847, 14, 19602 | 10644, 14, 14928 | 5970, 13, 10254 |
  | poly | 9, 5, 13 | 8, 5, 12 | 7, 5, 11 |
  | range | 4, 9, 4 | 3, 9, 3 | 3, 9, 3 |

  T1 removes the 203 constant rows of each Poseidon circuit that have
  only operand uses (the 204th is also a constraint side), so k does not
  change.  T2 removes the arith rows that go into their consumers:  k
  goes down by 1 for each Poseidon circuit.  `square` and `nonzero` do
  not change under T1 and T2;  `range` does not change under T2.  Form
  (iv) is DONE (T1 and T2).  The depth table of the fold
  (docs/M12-PLAN.md D4, the M12 close;  the model of the M11 table):

  | Form | rows a level | kycMerkle (depth 4) | max depth at k 15 | depth 20 |
  |---|---|---|---|---|
  | structural memo (M11) | 1776 | 10847, k 14 | 16 | 39263, k 16 |
  | memo + T1 | 1776 | 10644, k 14 | 16 | 39060, k 16 |
  | memo + T2 | 997 | 5970, k 13 | 30 | 21922, k 15 |

  T1 removes only the CONST rows that the memo shares over all hashes,
  so the rows of a level do not change.  Under T2 a depth-20 tree fits
  the pinned SRS (k 15), but the Merkle depth stays 4 (M12 ruling (e)).
  At the M12 close O24 stays OPEN for form (ii) only (the degree-5 gate,
  M12 ruling (g)).

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
