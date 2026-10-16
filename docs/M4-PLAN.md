# M4 plan: higher inductive types

Status: RULED (2026-10-15). Kickoff 2026-10-15. M3 is the s6 close commit
(the USER commits it after this kickoff; the kickoff was written on
f5a5dca with the s6 docs staged). Section 9 has the questions for the
USER. No code changes in the kickoff. The plan goes into the s1 commit.

## 1. Sources

| Source | What M4 takes from it |
|---|---|
| docs/PLAN.md, M4 row | HITs: point and path constructors, higher constructors, `hcomp` values. Smoke: the circle, the propositional truncation, the set quotient `A / R` with `[_]`, `eq/`, `squash/`, `rec`. |
| docs/PLAN.md, Rulings | No proof irrelevance: truncations are HITs with path constructors, no `Prop`. The De Morgan interval. The IR does not change. |
| docs/PLAN.md, Lowering rules | `Quotient eliminator | Runs on representatives`. A proof stays a term. |
| SPEC.md 2.5 | The M3 forms, the rule order (face `i1`, head former, else stuck), the closed-type rules, no regularity (D3), `ghcomp` and `gcomp` (D8), conversion argument by argument, the M3 codes. |
| SPEC.md 9 | O5 (a Path component writes `null`; a loop carries content). O8 (`hcomp` at Sum, Option, List with different constructors stays stuck; `transp` at a domain family stays stuck). |
| docs/STATUS.md `## M3` | `### s5`: hcomp_univ gives a Glue type, `probe_head`, the closure memo, PRINT_MAX 128 KB. `### s6`: the review method (no conv rule reads a type; only the face `i1` rule, the closed-type rule and `ghcomp` give the base), the mutant roll-up, the open items O4, O5, O8, O9. |
| docs/M3-PLAN.md 2 | D1 (Kan operations are evaluator primitives with neutral forms), D2 (derived operations are checker rules that write core terms), D3 (no regularity), D7 (kan.c rule templates), D8 (`ghcomp`, `gcomp`, no rule `hcomp A i0 u u0 = u0`: "the HITs of M4 refute it"), D10 (core names). |
| docs/M3-PLAN.md 3.4 | The evaluator rules and their order. The data rule for Sum, Option and List (CCHM 4.4: `hcomp` on the same constructor gives that constructor with `hcomp` on the fields). |
| design/DESIGN.md 60-116 | Direction 2: `data Circuit/∼` with `[_]`, `eq/`, `squash/ : isSet Circuit/∼`; `rec Bset f resp` with the three cases (point, path, square); `transportProp` by `transport (cong P (eq/ e))`. |
| design/DESIGN.md 118-196 | Direction 4: `data OTPSpec` with the points `message`, `cipher` and the path `encrypt : message ≡ cipher`; `interp` by cases, `interp (encrypt i) = enc i`; `spec≃impl`, `ua`, transport of `correctness`. |
| Cubical Agda (CCHM 6.3, Agda.Builtin.Cubical.HCompU, transp on data types) | `hcomp` at a HIT is a canonical value. `transp` at a HIT with parameters: constructors commute, `hcomp` values commute. The eliminator commutes with `hcomp`. |

Code anchors (M3 code; the slices find the lines again with `rg -n`):

| Fact | Where |
|---|---|
| Family declaration grammar: `'family' NAME group* ':=' ctor ('|' ctor)*`, `ctor := NAME group*` | src/front/parser.c lines 1-12 (grammar comment), parse_family, TOK_KW_FAMILY |
| A family is legal only in the domain file (rule R1, REFUSE_DATA) | src/front/check.c `REFUSE_DATA`; src/front/ast.h DECL_FAMILY, ORIGIN_DOMAIN |
| Family info, constructor info, the universe of a family | src/front/core.h FamilyInfo (param_count, ctor_count, universe), the ctor table; check.c `self`, `self_uses` (the field rule), check_params, family_app, HEAD_FAMILY |
| `fold` at a family: one case for each constructor, the scrutinee last | check.c fold_family (OP_FOLD_FAMILY, inst = the family, args = the cases, then the scrutinee); eval.c the OP_FOLD_FAMILY case of reduce_op (a stuck fold is VAL_STUCK; a constructor value is VAL_OP OP_CTOR with inst = the constructor) |
| Kan rules | src/front/kan.c kan_transp (probe_head, closed_type, transp_data, data_ctor), kan_hcomp (closed_type with `a->kind != VAL_UNIV`, hcomp_constant, hcomp_data with hcomp_same_ctor, hcomp_pair, hcomp_glue, hcomp_univ), data_type (OP_SUM, OP_OPTION, OP_LIST only), ghcomp_rule, gcomp, field_hcomp, field_transp (OP_KAN_FIELD) |
| Ops | src/front/core.h OP_FAMILY, OP_CTOR, OP_FOLD_FAMILY, OP_PROJ, OP_SYS, OP_TRANSP, OP_HCOMP, OP_KAN_* |
| Kan cases of the evaluator | eval.c reduce_op (the cases OP_TRANSP .. OP_UNGLUE), conv_loop, conv_sys, value_on_face, reduce_sys (a face `i1` gives its body), papp_value (the end point rule) |
| Interval binders and systems | check.c check_sys, restrict_ctx, branches_agree, elab_ivl, check_lam, apply_rest; eval.c ivl_binder; face.c face_conj, face_join, face_forall |
| JSON of a Path component (O5) | src/json.c line 76 (`VAL_PLAM`: a Path component; op_json writes null), line 189 (`OP_PATHP` writes null), line 110 (`some` wraps a Path), line 319 (an instance of a Path type); JSON_VALUE at line 64 |
| Gate tables | test/gate.sh (examples/*.lang and *.glue; eval rows, json goldens, check rows, emit rows, circuit rows, same-ir rows) |

## 2. Decisions of this plan

The USER can change each decision. Section 9 has the questions that need a
ruling.

- D1. A HIT is a `family` with at least one path constructor (question b).
  A path constructor has fields, then one or more interval binders, then a
  boundary system: `loop <i> [ ~i -> base, i -> base ]`. A constructor
  with no binder is a point constructor. A family with only point
  constructors is a plain family: its rules do not change. Reason: the
  boundary system is the form that the evaluator needs (the boundary rule
  is the M2 reduce_sys rule), and it covers higher constructors and
  boundaries that are not literal Path types. A Path-typed constructor is a
  later sugar if the USER wants it.
- D2. HIT declarations live in domain/domain.lang (rule R1 stays, question
  f). The three smoke HITs and the Direction 4 specification go into the
  sample domain. Reason: a family is a core name source (rule R4); the
  domain file is the design slot. M8 can lift R1 for programs.
- D3. `hcomp` at a HIT is a canonical value (CCHM 6.3) (question a). At a
  plain family, Sum, Option and List the data rule of M3 s3 stays (the
  same constructor in the base and in each branch gives that constructor
  with `hcomp` on the fields; else stuck). D8 of M3 stays: no rule
  `hcomp A i0 u u0 = u0`; the closed-type `hcomp` rule stays for Nat,
  Field, Unit and Flag only. kan_hcomp never reduces an `hcomp` at a HIT.
- D4. `transp` at a family with parameters: the constructor rule (a point
  or path constructor gives that constructor with `transp` on its fields
  and the interval arguments unchanged), the `hcomp` rule (an `hcomp`
  value gives an `hcomp` of the transports) and else stuck. `transp` along
  a line whose head is a family with no parameters gives the argument (the
  closed-type rule of kan_transp, as Nat). This closes the family part of
  O8 and changes test/emit/stuck-transp.lang (section 5).
- D5. A path constructor applied to its fields is a path: the checker
  gives `loop` the type `Path Circle base base`, and `c fields` with k
  binders the type `PathP (<i1> ... PathP (<ik> Self) ...)` whose ends are
  the boundary bodies. The core term of a path constructor name is a path
  abstraction over its binders around the new op OP_PCON (inst = the
  constructor; args = the parameters, the fields, then the k interval
  terms). Thus `loop @ i` is a papp beta step, and the boundary rule is
  reduce_sys on OP_PCON: when a boundary face is `i1` at the interval
  arguments, the value is that body; else OP_PCON is canonical.
- D6. The eliminator is `fold` with one case for each constructor, point
  and path, the scrutinee last (question c). The point cases follow the
  family convention (a function of the fields; a field of type Self gets
  the recursive result). A path case is a function of the fields (a Self
  field gets the recursive result; a `Path Self a b` field gets the path of
  the recursive results) whose value is a path over the binders. Its
  boundary obligation: on each boundary branch `phi -> t`, the case at
  the binders converts to `fold cases t` (HIT_CASE). No dependent `elim`
  in M4: Direction 2 uses `rec` and `transport`, Direction 4 uses the
  interpreter and `ua`. The core op is OP_FOLD_HIT (inst = the family;
  args = the result type `B`, the cases, then the scrutinee): `B` is
  copied into the core term (as D5 of M2 copies the ends), because the
  `hcomp` rule of the eliminator needs it.
- D7. The eliminator rules: a point constructor gives its case applied; a
  path constructor value `c fields @ r1 .. @ rk` gives the path case
  applied, then `@ r1 .. @ rk`; an `hcomp` value gives
  `hcomp B phi (fun i => [ phi -> fold B cases (u i) ]) (fold B cases u0)`;
  a neutral scrutinee is stuck.
- D8. Conversion: OP_PCON compares the constructor, then the arguments
  (fields pairwise, interval arguments by normal form). An `hcomp` value
  at a HIT compares argument by argument (as M3). No rule reads a type.
- D9. JSON (O5, question d): a Path component writes `null` only when
  the path is constant (the body at a fresh interval variable does not
  use the variable, as hcomp_constant tests); a path that uses its
  variable gives JSON_VALUE ("a path with content has no JSON form"). An
  `hcomp` value or an OP_PCON value in an instance gives JSON_VALUE (the
  M3 rule for a Kan value). A point constructor of a HIT writes the
  family constructor form.
- D10. Derived operations (D2 of M3): `isProp A`, `isSet A` as checker
  rules that write core terms (section 4). No new reserved name is taken
  from a program: `rec` is a keyword of `def rec`, thus the eliminator
  keeps the name `fold` (D6). The constructor names of the smoke HITs
  are ASCII (D6 of M2): `cls` for `[_]`, `eqc` for `eq/`, `squashc` for
  `squash/`; `inc` and `squash` for the truncation.
- D11. kan.c keeps the rule templates (D7 of M3). The HIT rules are new
  templates: `transp` on a constructor, `transp` on an `hcomp` value, the
  eliminator on an `hcomp` value. The boundary rule and the eliminator
  cases are in eval.c (reduce_op), next to OP_FOLD_FAMILY.
- D12. One commit for each slice (D9 of M3). The plan is in the s1 commit.

## 3. Core changes

### 3.1 Syntax

| Form | Meaning | Slice |
|---|---|---|
| `family D params := c1 fields \| c2 fields <i> [ phi -> t, ... ] \| ...` | A HIT: a constructor with interval binders and a boundary system is a path constructor | s1 |
| `c fields` (a path constructor) | A path over the binders (D5); `c fields @ r` applies it | s1 |
| `<i j>` binders | A higher constructor (two or more binders; `squashc` is the smoke case) | s4 |
| `hcomp D phi u u0` at a HIT | A canonical value (D3) | s2 |
| `fold B? cases x` at a HIT | The eliminator (D6). The checker infers `B` from the first point case when it can; the s3 prompt fixes the surface form | s3 |
| `isProp A`, `isSet A` | Derived (section 4) | s3, s4 |

The grammar change is in `ctor`: `ctor := NAME group* ('<' NAME+ '>' system)?`
where `system` is the M2 system form `[ face -> term, ... ]`. No new
token: `<`, `>` and `[` are M2 tokens.

### 3.2 Typing rules

| Rule | Premises | Conclusion | Slice |
|---|---|---|---|
| path constructor (declaration) | the fields check as the family rule (a Self field or a `Path Self a b` field is recursive); the binders are interval variables; the system checks against `Partial psi Self` where `Self` is the family at its parameters; each body is a term of type Self that uses the fields, the parameters, the binders and the constructors declared before it; the branches agree on overlaps (FACE_AGREE); the join of the faces covers the cube boundary `~i1 \/ i1 \/ ... \/ ~ik \/ ik` (HIT_BOUNDARY) | the constructor is a path constructor with k binders | s1 |
| path constructor (use) | `c params fields` with each field checked as the family rule | `c params fields : PathP ... Self ...` with the ends read from the boundary at each face (D5) | s1 |
| hcomp at a HIT | as M3 (HCOMP_BASE) | `hcomp D phi u u0 : D`, a canonical value | s2 |
| fold at a HIT | `x : D params`; `B : Type n`; one case for each constructor; a point case has the family case type; a path case with fields `f` and k binders has the type `(f' : ...) -> PathP ...` where `f'` replaces each recursive field by its result type and the ends are `fold B cases t` for the boundary bodies `t`; on each boundary branch the case converts to `fold B cases t` (HIT_CASE) | `fold B cases x : B` | s3 |
| higher constructor | the path constructor rule with k = 2 or more; the cube boundary has 2k faces | as above | s4 |

A family with a parameter in `Type n` (the truncation and the quotient
have one) checks with the family universe rule (the universe of the family
is the largest universe of a field). s1 checks that check_params accepts a
parameter whose type is a universe, and extends it if not.

### 3.3 Interval and face additions

No new face operation. The cube boundary face of k binders is the join of
`~i` and `i` for each binder (face_join). The coverage test is face_leq of
that join against the join of the branch faces (as FACE_COVER on `glue`).

### 3.4 Evaluator rules

Order: (1) the face `i1` rules, (2) the rule of the head former or the
head value, (3) else stuck. `c` is a constructor, `pc` a path constructor,
`cases` the eliminator cases, `B` the result type copied into the core.

| Redex | Result | Slice |
|---|---|---|
| `pc params fields @ r1 .. @ rk` with a boundary face that is `i1` at `r1 .. rk` | the body of that branch (reduce_sys method) | s1 |
| `pc params fields @ r1 .. @ rk`, no face `i1` | canonical OP_PCON | s1 |
| `transp (<i> D) phi x`, `D` a family with no parameters | `x` (the closed-type rule of kan_transp) | s2 |
| `transp (<i> D (P i)) phi (c fields)`, point or path constructor (with its interval arguments) | `c` with `transp` on each field along the field type at `P i`; the interval arguments unchanged | s2 |
| `transp (<i> D (P i)) phi (hcomp (D (P i0)) psi u u0)` | `hcomp (D (P i1)) psi (fun j => [ psi -> transp (<i> D (P i)) phi (u j) ]) (transp (<i> D (P i)) phi u0)` | s2 |
| `transp` at a family with a neutral argument | stuck | s2 |
| `hcomp D i1 u u0` at a HIT | `u i1` (the face rule, as M3) | s2 |
| `hcomp D phi u u0` at a HIT, `phi` not `i1` | canonical (D3); kan_hcomp returns the value as is | s2 |
| `hcomp` at a plain family | the data rule of M3 s3 (the same constructor gives that constructor with `hcomp` on the fields; else stuck). This closes the family part of O8 | s2 |
| `fold B cases (c fields)`, point | the case of `c` applied to the fields, each recursive field replaced by `fold B cases field` (as fold_family) | s3 |
| `fold B cases (pc fields @ r1 .. @ rk)` | the case of `pc` applied to the fields (recursive fields as above; a `Path Self` field as `<j> fold B cases (p @ j)`), then `@ r1 .. @ rk` | s3 |
| `fold B cases (hcomp D phi u u0)` | `hcomp B phi (fun i => [ phi -> fold B cases (u i) ]) (fold B cases u0)` | s3 |
| `fold B cases x`, `x` neutral | VAL_STUCK | s3 |

The rules for a path constructor value and for the eliminator hold for k
binders from s1; s4 adds the tests with k = 2. The boundary rule and the
`hcomp` commute rule are confluent: on a boundary face, the path case at
that face is `fold` of the body by HIT_CASE.

### 3.5 Conversion

- OP_PCON: the same constructor, then the arguments (D8). OP_FOLD_HIT:
  argument by argument. An `hcomp` value at a HIT: argument by argument
  (as OP_HCOMP in M3).
- No rule makes two values equal because of their type. HIT_CASE reads the
  boundary of the constructor (a declaration), not the type of a value. A
  guest row guards this (section 8, s3: two variables of a HIT stay
  different).

### 3.6 Printer and JSON

- The printer writes a path constructor value as `pc fields @ r1 .. @ rk`,
  an `hcomp` value as `hcomp D phi u u0` (the M3 form) and a HIT fold as
  `fold B cases x`.
- JSON (D9): json.c line 76 (a Path component) tests the body at a fresh
  interval variable; a constant path writes `null`, a path with content
  gives JSON_VALUE. OP_PCON and an `hcomp` value in an instance give
  JSON_VALUE. SPEC.md 9 O5 records the rule.

## 4. Derived operations and their laws

Each operation is a checker rule (D2 of M3). The table gives the core term.

| Operation | Type | Core term | Slice |
|---|---|---|---|
| `isProp A` | `Type n` | `(x : A) -> (y : A) -> Path A x y` | s3 |
| `isSet A` | `Type n` | `(x : A) -> (y : A) -> (p : Path A x y) -> (q : Path A x y) -> Path (Path A x y) p q` | s4 |
| `truncIsProp A` | `isProp (Trunc A)` | `fun x => fun y => squash x y` (a guest definition in the example, not a core name) | s3 |
| `quotIsSet A R` | `isSet (Quot A R)` | `fun x => fun y => fun p => fun q => squashc x y p q` (guest) | s4 |

Laws that hold by computation (each is a `refl` in an example):

| Law | Why it computes | Slice |
|---|---|---|
| `loop @ i0 = base`, `loop @ i1 = base` | the boundary rule | s1 |
| `(eqc a b r) @ i0 = cls a` | the boundary rule with fields | s1 |
| `transp (<_> Circle) i0 x = x`, `x` a variable | the closed-type rule for a family with no parameters (D4) | s2 |
| `transp (<i> Trunc (L i)) i0 (inc a) = inc (transp (<i> L i) i0 a)` | the constructor rule | s2 |
| `hcomp Circle i1 u u0 = u i1` | the face rule | s2 |
| `fold B b l base = b` | the point rule | s3 |
| `<i> fold B b l (loop @ i) = l` (a `refl` at `Path (Path B b b)`) | the path rule and path eta | s3 |
| `fold B cases (hcomp Circle phi u u0)` with a literal system converts to `hcomp B phi (fun i => [ phi -> fold B cases (u i) ]) (fold B cases u0)` | the `hcomp` commute rule; conv_sys | s3 |
| `fold B f e s (cls a) = f a` at the quotient | the point rule; the design's `rec Bset f resp [ C ] = f C` | s4 |
| `<i> fold B f e s (eqc a b r @ i) = e a b r` | the path rule; `rec Bset f resp (eq/ e i) = resp e i` | s4 |
| `interp (encrypt @ i) = enc @ i` as a `refl` at `Path (Path Impl m c) (<i> interp (encrypt @ i)) enc` | the path rule (DESIGN line 141) | s5 |
| `transport (ua specImpl) message = interp message` for `Impl` in Field or Nat | D8 of M3 and the point rule (DESIGN lines 154-169) | s5 |

## 5. Effect on examples

| Item | M4 |
|---|---|
| domain/domain.lang | New families: `family Circle := base \| loop <i> [ ~i -> base, i -> base ]` and `family Susp (A : Type 0) := north \| south \| merid (apex : A) <i> [ ~i -> north A, i -> south A ]` (s1; a constructor takes the family parameters explicitly, so the boundary names `north A` and `south A`; a field name is a core name, so the field is `apex`); `family Trunc (A : Type 0) := inc (a : A) \| squash (x : Trunc A) (y : Trunc A) <i> [ ~i -> x, i -> y ]` (s3); `family Quot (A : Type 0) (R : A -> A -> Type 0) := cls (a : A) \| eqc (a : A) (b : A) (r : R a b) <i> [ ~i -> cls a, i -> cls b ] \| squashc (x : Quot A R) (y : Quot A R) (p : Path (Quot A R) x y) (q : Path (Quot A R) x y) <i j> [ ~i -> p @ j, i -> q @ j, ~j -> x, j -> y ]` (s4); `family OTPSpec := message \| cipher \| encrypt <i> [ ~i -> message, i -> cipher ]` (s5). The s1 prompt checks that the sample domain's existing rows (Color, Item, Stack, Lot) and their eval rows do not change. |
| test/emit/stuck-transp.lang | `transp (<_> Color) i0 red` reduces to `red` in s2 (D4). The row needs a new stuck witness: `transp (<i> Lot (n i)) i0 x` with a neutral `x`, or an `hcomp` value at Circle in an instance (JSON_VALUE). s2 decides and records the exact row. |
| New examples | examples/circle.lang (s1, s2, s3), examples/trunc.lang (s3), examples/quot.lang (s4), examples/otp.lang (s5), examples/quotSq.glue (s5, section 6). Each with its test/json golden, eval rows and check rows. |
| test/json goldens with `null` | Each existing `null` is a constant path; the rule of D9 keeps it. s2 confirms with the gate. |
| SPEC.md | 2.6 (new, M4): the HIT forms, typing, evaluation, conversion, JSON, the codes. 9: O5 closed with D9, O8 narrowed (the families), O12 (new) for what stays stuck. s6 writes it. |

## 6. Circuits

The IR must not change. The gate goldens in test/circuit stay byte for
byte the same, and `make e2e` gives the same verifiers. M4 adds no
lowering code: a constraint side must normalize to a Field normal form
(SPEC 7.1), and the eliminator on a representative reduces by the point
rule (the PLAN lowering row `Quotient eliminator | Runs on
representatives`). A stuck HIT term in a constraint gives CIRCUIT_LOWER.

Guest test (s5): examples/quotSq.glue states the `square` relation through
the quotient. With `SqRel := fun a => fun b => Path Field (a * a) (b * b)`
and `sq := fun q => fold Field (fun a => a * a) (fun a => fun b => fun r => r) (squareCase) q`
over `Quot Field SqRel`, the circuit constraint is `sq (cls x) == y`, so
its IR must be the IR of `square`. The row goes into
test/circuit/same-ir.txt (`quotSq square square`). `make e2e` does not
read this table, so contracts/ does not change. The `squarec` case is a
square in Field; `isSet Field` is not a core fact, so the case is written
with `hcomp` (an `hfill` square) or, if that fails in s5, the quotient
example in the .glue file omits `squashc` (a quotient without the set
truncation is still a HIT; the .lang example keeps `squashc`).

## 7. New check codes

| Code | Exit | The compiler refuses | Slice |
|---|---|---|---|
| HIT_BOUNDARY | 1 | A path constructor whose boundary system does not cover the cube boundary of its binders. The message gives the constructor, the uncovered face and the binders | s1 |
| HIT_CASE | 1 | `fold` at a HIT where a path case does not convert to `fold B cases t` on a boundary branch `phi -> t`. The message gives the constructor, the conjunction and the two values | s3 |
| HIT_CASES | 1 | `fold` at a HIT with the wrong number of cases, or a case whose type does not match the constructor (the message gives the constructor and the expected type) | s3 |

Existing codes with new causes: PARSE (a boundary system with no binder;
a binder with no system), FACE_AGREE (the boundary branches of a path
constructor; the eliminator's path case on an overlap), TYPE_MISMATCH (a
boundary body that is not of the family type; a field of a HIT), REFUSE_DATA
(a HIT in a program file; rule R1), HCOMP_BASE (an `hcomp` value at a HIT
whose base does not match), JSON_VALUE (a path with content, an `hcomp`
value or a path constructor value in an instance), CIRCUIT_LOWER (a stuck
HIT term in a constraint; O4 stays open if s5 finds no case).

## 8. Guest tests

All tests are glue-lang programs and gate tables. No new node, Python or
shell harness. The native C harnesses (test/*-regressions.c) can get cases
for the boundary face and for kan.c.

| Slice | Tests |
|---|---|
| s1 | examples/circle.lang: `loop @ i0` and `loop @ i1` as `refl` rows; `loop` has the type `Path Circle base base`; `<i> loop @ i` converts to `loop` (path eta); a definition `loopInv := <i> loop @ ~i`; the eval row of `loop @ i` under a binder (the printed form). Check rows: HIT_BOUNDARY (a loop with only `~i -> base`), FACE_AGREE (two branches that disagree on `i /\ j` with two binders), TYPE_MISMATCH (a body of type Nat), REFUSE_DATA (a HIT in a program), PARSE (`<i>` with no system). JSON: a `def` of type Circle with the value `base` (the constructor form); the existing domain rows unchanged. Mutants: (1) the boundary coverage test off; (2) the boundary rule reads the first branch only; (3) the path constructor's type takes the `i1` body for both ends. |
| s2 | examples/circle.lang: `hcomp Circle i1 u u0 = u i1`; an `hcomp` value with a literal system that stays canonical (eval row); `transp (<_> Circle) i0 x = x`; `transp (<i> Trunc (L i)) i0 (inc a)` (needs the s3 family; s2 uses `Lot` or a one-parameter test family in the domain instead); the `transp` of an `hcomp` value (eval row with the exact text). Emit rows: an `hcomp` value in an instance (JSON_VALUE); the new stuck-transp witness. JSON: `null` goldens unchanged; a path with content in an instance (JSON_VALUE). Guard: `hcomp Circle i0 (fun i => [ ]) base` does not convert to `base` (a check row with `refl` that is refused: no `hcomp A i0 u u0 = u0`). Mutants: (1) kan_hcomp reduces an `hcomp` at a HIT by the closed-type rule; (2) the family closed-type rule applies with parameters; (3) the JSON constant-path test always writes `null`. |
| s3 | examples/circle.lang: `fold B b l base = b`, `<i> fold B b l (loop @ i) = l`, the `hcomp` commute row, `fold` into Nat with `l := refl` (the degree-zero map), `fold` into `Path`-valued B. examples/trunc.lang: `Trunc`, `inc`, `squash`, `truncIsProp`, `fold` out of `Trunc A` into a type with a given `isProp` witness, `isProp` as a core name. Check rows: HIT_CASE (a loop case whose ends are not `b`), HIT_CASES (a missing path case; a point case of the wrong type), REFUSE_NAME for `isProp`. Guard: two variables `x y : Circle` are not convertible (bad-refl code). Mutants: (1) HIT_CASE off; (2) the path rule applies the case but drops `@ r`; (3) the `hcomp` commute rule gives `fold B cases u0`. |
| s4 | examples/quot.lang: `Quot`, `cls`, `eqc`, `squashc` with two binders; `(squashc x y p q) @ i @ j` on the four faces (four `refl` rows); `fold` out of the quotient (the design's `rec Bset f resp`); `quotIsSet`; `isSet`. Check rows: HIT_BOUNDARY on a square with three faces; FACE_AGREE on a square whose corners disagree; HIT_CASE on a `squashc` case that is not a square of the point cases. Mutants: (1) the cube boundary of k binders uses the first binder only; (2) the square case check tests the `i` faces only; (3) `isSet` writes `isProp`. |
| s5 | examples/otp.lang: `OTPSpec`, `interp` by `fold`, `interp (encrypt @ i) = enc @ i`, `specImpl : Equiv OTPSpec Impl` through the guest `isoToEquiv` of examples/iso.lang (the inverse is `fold` into OTPSpec with the path case `encrypt`), `ua specImpl`, `transport (ua specImpl) message` as a `refl` row for `Impl := Nat` (DESIGN lines 154-169). examples/quotSq.glue and the same-ir row (section 6). A CIRCUIT_LOWER row if a circuit gives one. Mutants: (1) the point case of `interp` swapped; (2) the same-ir golden set to `range`; (3) the `transp` constructor rule drops the interval arguments. |
| s6 | No code. SPEC 2.6 and 9, FORMERS if a cell changes, PLAN M4 DONE, STATUS `## M4`, the mutant roll-up, the review (the rule of M3: no conv rule reads a type; no rule `hcomp A i0 u u0 = u0`), the M5 kickoff prompt. |

Mutation: 3 mutants for each slice, `make -B` for each rebuild (make 3.81
compares mtimes to the second). The slice prompts name them.

## 9. Questions for the USER

a. `hcomp` as a canonical value: at HITs only (D3), or at each data type
   (Sum, Option, List, the plain families), as Cubical Agda?
   Recommendation: HITs only. Reasons: (1) the M3 s3 data rule is the CCHM
   4.4 rule for strict inductive types, and the lineage of the evaluator
   is CCHM; (2) a canonical `hcomp` at Sum, Option or List changes the
   JSON target (an instance of `Sum` could hold an `hcomp`) and the
   lowering of the coproduct (a tag wire has no `hcomp` row), and the IR
   must not change; (3) the eliminators of the data types (`either`,
   `optionElim`, `fold` on List and Nat) would each need the commute rule.
   Cost: at a Sum, Option or List with different constructors the value
   stays stuck (O8 narrows, it does not close). M6 (fold and unfold over
   the cubical core) can revisit.
   Ruling (USER, 2026-10-15): HITs only.

b. The syntax of a HIT declaration: a path constructor with interval
   binders and a boundary system (D1), or a constructor with a Path type?
   Recommendation: binders and a boundary system. Reasons: (1) the boundary
   system is the form the evaluator needs (the boundary rule is reduce_sys
   of M2), and the checker derives the Path type from it (D5); (2) higher
   constructors (`squashc` with two binders) and boundaries that are not
   literal Path types have the same form; (3) no new checker rule for a
   constructor field of Path type. Cost: the USER writes the boundary
   (`loop <i> [ ~i -> base, i -> base ]`, not `loop : Path Circle base
   base`). A Path-typed sugar can come later.
   Ruling (USER, 2026-10-15): binders and a boundary system.

c. The eliminator: `fold` with a case for each constructor, point and
   path (D6, the design's `rec`), or a new name, and a dependent `elim`?
   Recommendation: `fold`, non-dependent, no `elim` in M4. Reasons: (1)
   `fold case... x` is the eliminator of each family today, with the same
   case convention; (2) `rec` is a keyword (`def rec`), so the design's
   name cannot be a core name without a lexer change; (3) Direction 2 uses
   `rec` and `transport`, Direction 4 the interpreter and `ua`: neither
   needs a dependent eliminator. Cost: SPEC says "`fold` at a HIT is the
   `rec` of design/DESIGN.md". If the USER wants `elim`, it is a slice
   after s4 (the dependent path case needs PathP over `B (pc @ i)`).
   Ruling (USER, 2026-10-15): `fold`, non-dependent, no `elim` in M4.

d. The O5 JSON rule for a path: `null` for a constant path and JSON_VALUE
   for a path with content (D9), or keep `null` for each path, or
   serialize the path?
   Recommendation: D9. Reasons: (1) `null` is right when the content is
   in the type (the M2 reason), and a loop refutes that; (2) the test is
   on the value (the body at a fresh variable), not on the type, as the
   review rule asks; (3) a serialized path has no consumer: the circuit
   target reads Field normal forms, not paths. Cost: an instance that
   holds a loop is refused. The existing `null` goldens stay (constant
   paths).
   Ruling (USER, 2026-10-15): D9.

e. Slice count and order?
   Recommendation: 6 slices, section 10. The kickoff order (s1 point
   constructors and `hcomp` values, s2 path constructors) changes: an
   `hcomp` value at a HIT needs the path constructors first (its tube
   bodies are path constructor values), and a family with only point
   constructors keeps the data rule (D3). So s1 is the declaration with
   the boundary rule and the circle, s2 the Kan rules, s3 the eliminator
   and the truncation, s4 the higher constructors and the quotient, s5
   the Direction 4 smoke and the circuit test, s6 the close.
   Ruling (USER, 2026-10-15): 6 slices in the section 10 order.

f. HIT declarations in domain/domain.lang only (rule R1, D2), or in each
   program?
   Recommendation: domain only in M4. Reasons: (1) a family is a core name
   source (rule R4), and the domain file is the design slot; (2) each
   example sees the smoke HITs with no import (a .glue file has no
   import); (3) no change to the R1 check. Cost: the smoke HITs live in
   the sample domain next to Color, Item, Stack and Lot. M8 (Direction 4
   with a Telcoin circuit) can lift R1 for HITs.
   Ruling (USER, 2026-10-15): domain only in M4.

## 10. Slices

Each slice is one session or more. Each prompt is in ~/Documents. Each
prompt carries its own gates, IR check, close steps and traps.

| Slice | Content | Prompt |
|---|---|---|
| s1 | Grammar of a path constructor (binders, boundary system); DECL_FAMILY and FamilyInfo carry the binders and the boundary; HIT_BOUNDARY, FACE_AGREE on the boundary; OP_PCON; the boundary rule; the Path type of a path constructor (D5); printer; JSON (the constructor form); Circle in the domain; examples/circle.lang; tests. | glue-lang-M4-s1-PROMPT.md |
| s2 | kan.c: `hcomp` at a HIT is canonical, the data rule at a plain family, the closed-type rule for a family with no parameters, `transp` on constructors and on `hcomp` values at a family with parameters; JSON D9 (O5); the stuck-transp witness; tests. | glue-lang-M4-s2-PROMPT.md |
| s3 | OP_FOLD_HIT with `B` in the core; the point, path and `hcomp` rules of the eliminator; HIT_CASE, HIT_CASES; `isProp`; Trunc in the domain; examples/trunc.lang; the circle laws; tests. | glue-lang-M4-s3-PROMPT.md |
| s4 | Higher constructors (k = 2 tests): the cube boundary, the square case; `isSet`; Quot in the domain; examples/quot.lang (the design's Direction 2 `rec`); tests. | glue-lang-M4-s4-PROMPT.md |
| s5 | OTPSpec in the domain; examples/otp.lang (Direction 4: `interp`, `specImpl`, `ua`, transport of the law); examples/quotSq.glue and the same-ir row; the CIRCUIT_LOWER row if one exists; tests. | glue-lang-M4-s5-PROMPT.md |
| s6 | Close: SPEC 2.6 (new) and 9 (O5 closed, O8 narrowed, O12), FORMERS if a cell changes, PLAN M4 DONE, STATUS `## M4`, mutant roll-up, review, M5 kickoff prompt. | glue-lang-M4-s6-PROMPT.md |

Gates for each slice (all GREEN before staging): `make -B check`, `make
check-clang`, `forge build --root contracts --sizes`, then `make e2e` last.
`cargo fmt --check` and clippy only if halo2/ changes (M4 plans no halo2/
change).

IR check for each slice: after `make e2e`, `git diff --exit-code --
test/circuit contracts/src/verifiers`. If it is clean, restore
contracts/test/fixtures from HEAD and run `forge test --root contracts`
again (33 passed). If the verifiers changed, stop: M4 must not change the
IR without a USER ruling.
