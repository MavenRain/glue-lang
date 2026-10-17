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
(`message`, `cipher`, `encrypt`) and GQuot (`gcls`, `geqc`).

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
  `unglue` of an `hcomp` in hfT give `x`. M5 (slice s6) found no circuit
  that gives it (O12 (f)). O4 stays open for M7.
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
- O8. Kan values that stay stuck (docs/M3-PLAN.md 3.4). Narrowed in M4:
  the families have Kan rules (section 2.6), thus `transp (<_> Color) i0
  red` gives `red`. RULED 2026-10-15 (USER): `hcomp` is a canonical value
  at a HIT only. Thus `hcomp` stays stuck at a Sum, Option, List or plain
  family when `u0` and the branches do not have the same constructor, and
  at a stuck type. `transp` stays stuck at a neutral value of a Sum, Option
  or List. The fold and unfold families have no Kan rule; over the cubical
  core they are M6 (docs/PLAN.md, M6 row). In an instance, a stuck Kan
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
  - (f) O4 stays open for M7. M5 s6 probed CIRCUIT_LOWER with a stuck
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

## 10. Milestones

> Write here: the content of each milestone, and a dated status line under
> the current milestone.

| Milestone | Content |
|---|---|
| M0 | Host probe (`probe/CAPABILITY.md`); the core types construct and check; refusal list; examples and tests |
| M1 | Core operations |
| M2 | Queries and reads, or the contract |
| M3 | Hardening: speed, limits, a checked certificate for the output |
