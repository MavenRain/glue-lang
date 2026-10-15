# M3 plan: Kan operations, Glue and univalence

Status: RULED (2026-10-14). Kickoff 2026-10-14. M2 is commit aad6057. The
USER ruled on the questions of section 9: each ruling is the recommendation,
so the decisions of section 2 do not change. No code changes in the kickoff.

## 1. Sources

| Source | What M3 takes from it |
|---|---|
| docs/PLAN.md, M3 row | `transp` and `hcomp` for Pi, Sigma, Path, universes, coproducts. Glue types, `ua`, uaBeta by computation. Application of a partial element. The tests `round` and `moved` come back. |
| docs/PLAN.md, Rulings | No proof irrelevance. The De Morgan interval (CCHM). `==` is `Path Field a b`, and the IR does not change. |
| docs/PLAN.md, Lowering rules | `transp` along `ua e` lowers to the forward map of `e`, at compile time. A proof stays a term. |
| SPEC.md 2.4 | Interval normal forms, PathP typing, the end point rule, path eta, faces, `Partial phi A`, systems, SSet, the M2 codes. |
| SPEC.md 9 | O5 (a Path component writes `null`), O6 (a `Partial` instance gives SORT_FIBRANT), O7 (application of a partial element is M3). |
| docs/STATUS.md `## M2` | `### s3`: the M3 tests `round` and `moved`. `### s4`: the restriction method (quote, then eval in the restricted environment, one core branch for each conjunction). `### s5`: the review method (no conv rule reads a type). |
| docs/M2-PLAN.md 2, 3 | D1 (core PathP), D4 (derived operations are checker rules that write core terms), D5 (a neutral `p @ r` carries its ends), D8 (reserved names). |
| design/DESIGN.md | Cubical Agda. `transport (cong P (eq/ e)) p` (line 102), `isoToEquiv`, `ua spec≃impl` and uaβ "computes definitionally" (lines 151-161), uaβ in the list of needs (line 176), `A ≃ B = Σ[ f ∈ (A → B) ] isEquiv f` (line 183), `_∙_` (line 189), transport of a function type along `ua` (line 169). |

Code anchors (M2 code; the slices find the lines again with `rg -n`):

| Fact | Where |
|---|---|
| Path former, line handling, ends checked at `i0` and `i1` | src/front/check.c path_former, line_type, under_ivl |
| Path abstraction check, path application infer (ends copied into the core term) | check.c check_plam, infer_papp, core_papp |
| Derived M2 rules (D4) | check.c refl_path, rule_sym, cong_path; BUILTINS table (rows `refl` .. `SSet`); RESERVED (`trans`, `transport`, `transp`, `hcomp`, `Glue`, `ua`) |
| Systems, restriction, agreement | check.c check_sys, restrict_ctx, branches_agree; elab_ivl |
| Interval binders | check.c check_lam, infer_lam, apply_rest; eval.c ivl_binder |
| Path application value, end point rule | src/front/eval.c papp_value |
| Systems and Partial values | eval.c reduce_sys (a face `i1` gives its body), reduce_partial (normalizes the face only) |
| Conversion | eval.c conv_loop, conv_fun, conv_path, conv_sys, value_on_face |
| Quote and print | eval.c quote_value, quote_binder, quote_papp, print_binder |
| Face lattice | src/front/face.c (face_norm, face_meet, face_join, face_equal, face_leq, face_conj, face_conj_sets) |
| Ops and kinds | src/front/core.h (OP_PATHP, OP_INTERVAL, OP_SSET, OP_PARTIAL, OP_SYS; VAL_STUCK = an operation with a neutral scrutinee) |

## 2. Decisions of this plan

The USER can change each decision. Section 9 has the questions that need a
ruling.

- D1. `transp` and `hcomp` are evaluator primitives with neutral forms
  (question b). New ops OP_TRANSP and OP_HCOMP in CORE_OP and VAL_OP, as
  OP_SYS in M2 s4 (few switch sites). A stuck Kan operation is VAL_STUCK.
  Reason: the computation rule depends on the head former of the type, and
  the head is known only after evaluation (`<i> P (p @ i)` becomes
  `<i> Nat -> Nat` when `P` is applied).
- D2. The derived operations (`comp`, `hfill`, `trans`, `transport`,
  `subst`, `transportRefl`, `J`, `JRefl`, `isContr`, `fiber`, `isEquiv`,
  `Equiv`, `idEquiv`, `ua`, `uaBeta`) are checker rules that write core
  terms, as D4 of M2. Their laws hold by computation (section 4).
- D3. No regularity (CCHM, as Cubical Agda) (question a). `transp` along a
  line that does not use `i` does not reduce to the identity in general. The
  rules of section 3.4 give the identity for the closed base types (`Nat`,
  `Field`, `Unit`, `Flag`, `Type n`), and the structural rules carry it
  through Pi, Sigma, Prod and the coproducts. `transport refl x = x` is the
  path `transportRefl x`. `J P d refl = d` is the path `JRefl P d`.
- D4. Application of a partial element (O7): `Partial i1 A` evaluates to
  `A`. A system with a branch face that is `i1` already evaluates to that
  branch (M2 reduce_sys). Under a system branch, the restriction makes the
  face `i1`, so a term `v : Partial phi A` has the type `A` there. No new
  syntax and no new core op. Inference gets the same rule, so `(u i) @ j`
  works under the face of `u`.
- D5. Syntax. `transp (<i> A) phi a`. `hcomp A phi u u0` with
  `u : (i : I) -> Partial phi A`: `phi` is explicit, because `u` can be a
  variable. `Glue A [ phi1 -> pack T1 e1, ... ]`: the face of a Glue type is
  the join of the branch faces (as cubicaltt), because the system is a
  syntactic argument. `glue [ phi1 -> t1, ... ] a` checks only against a
  declared Glue type. `unglue g` infers. ASCII names only (D6 of M2):
  `Equiv`, not `≃`, and `uaBeta`, not `uaβ`.
- D6. `transport p x` takes `p : Path (Type n) A B` (Cubical Agda).
  `subst P p x` takes a family `P`. Thus the M3 test `moved` is
  `fun P u => subst P same u`, not `transport P same u` (the F13 form).
- D7. The Kan rules are in a new module src/front/kan.c (kan.h). Each rule
  is a core term template. eval.c dispatches OP_TRANSP, OP_HCOMP and the
  Glue ops to kan.c, and kan.c evaluates the template in the environment
  extended with the values of the arguments. Reason: a rule that makes a
  `fun` or `<i>` value needs a closure, and a closure is a core body. The
  quote and eval functions exist and have tests (M2 s4 restriction).
- D8. The Glue Kan rule follows Cubical Agda (Agda.Builtin.Cubical.Glue,
  `equivProof` with the extra branch `(phi = i0) -> c`). The rule uses the
  generalized composition of Cubical Agda (the Univalence.agda comment
  "Because of ghcomp" on uaβ):
  `ghcomp A phi u u0 = hcomp A (phi \/ ~phi) (fun i => [ phi -> u i, ~phi -> u0 ]) u0`,
  and `gcomp (<i> A) phi u u0` is `comp` with `ghcomp` in place of `hcomp`
  (section 4). `ghcomp` is uniform (stable under each substitution). With an
  empty system (`phi` is `i0`), `ghcomp` gives the base `u0` by the face
  `i1` rule, because `~i0` is `i1`. With the extra branch, the extension of
  the empty partial fiber is the center by the same face rule. Without
  `ghcomp`, `transport (ua e) x` is `hcomp B i0 (...) (...)`: uaBeta fails
  for a variable `B`, and a circuit gives CIRCUIT_LOWER. Do not add a rule
  `hcomp A i0 u u0 = u0`. It is like regularity, and the HITs of M4 refute
  it. Thus
  `transport (ua e) x` evaluates to `transp (<_> B) i0 (first e x)`, and
  `uaBeta e x` is `<i> transp (<_> B) i (first e x)` (Cubical Agda:
  `uaβ e x = transportRefl (equivFun e x)`). For a closed base type `B`
  (Field), the value is `first e x`. That is the PLAN lowering rule, and it
  needs no lowering code.
- D9. One commit for each slice (D7 of M2). The plan is in the s1 commit.
- D10. New core names (REFUSE_NAME for a definition with the name):
  `transp`, `hcomp`, `comp`, `hfill`, `trans`, `transport`, `subst`,
  `transportRefl`, `J`, `JRefl`, `isContr`, `fiber`, `isEquiv`, `Equiv`,
  `idEquiv`, `Glue`, `glue`, `unglue`, `ua`, `uaBeta`. Each slice moves its
  names from RESERVED or adds them to BUILTINS. s1 adds all of them to
  RESERVED first, so that no later slice changes a valid program.

## 3. Core changes

### 3.1 Syntax

| Form | Meaning | Slice |
|---|---|---|
| `transp (<i> A) phi a` | Transport along the line `<i> A`. Constant on `phi`. | s1 |
| `hcomp A phi u u0` | Homogeneous composition. `u : (i : I) -> Partial phi A`, `u0 : A`. | s2 |
| `comp (<i> A) phi u u0`, `hfill A phi u u0 r` | Derived (section 4). | s2 |
| `Glue A [ phi -> pack T e, ... ]` | Glue type. `T : Type n`, `e : Equiv T A`. | s4 |
| `glue [ phi -> t, ... ] a` | Glue element. Check only. | s4 |
| `unglue g` | The `A` part of `g`. | s4 |

No new token. Each form is a core name with arguments (BUILTINS), as `Path`
and `Partial`. The line argument of `transp` and `comp` uses the PathP line
rule (path_former): `<i> A` with `A : Type n`, else PATH_TYPE or
SORT_FIBRANT.

### 3.2 Typing rules

| Rule | Premises | Conclusion |
|---|---|---|
| transp | `<i> A` is a line in `Type n`; `phi` is an interval term; `a : A[i := i0]`; on each conjunction of `phi`, `A` at a fresh `i` converts to `A[i := i0]` (TRANSP_CONSTANT) | `transp (<i> A) phi a : A[i := i1]` |
| hcomp | `A : Type n`; `u : (i : I) -> Partial phi A`; `u0 : A`; on each conjunction of `phi`, `u i0` converts to `u0` (HCOMP_BASE) | `hcomp A phi u u0 : A` |
| Glue | `A : Type n`; the system checks against `Partial psi (Sigma (T : Type n) (Equiv T A))`, where `psi` is the join of its faces | `Glue A [ ... ] : Type n` |
| glue (check) | Against `Glue A [ psi -> pack T e ]`: each branch of the system checks against `T` on its face; the faces join to `psi` (FACE_COVER); `a : A`; on each conjunction of `psi`, `first e t` converts to `a` (GLUE_BOUNDARY) | `glue [ ... ] a : Glue A [ ... ]` |
| unglue (infer) | `g : Glue A [ ... ]`; the checker copies the face and the system into the core term (as D5 of M2) | `unglue g : A` |

`glue` with no declared Glue type, or `unglue` of a term that is not of a
Glue type, gives GLUE_EXPECTED.

### 3.3 Interval and face additions

- `face_forall(phi, i)` in face.c: the join of the conjunctions of `phi`
  that do not use `i` (CCHM `∀i.phi`). The Glue rule (s5) uses it.
- `~phi` of a face is the De Morgan reversal and then face_norm.
  `phi /\ ~phi` is `i0` in the face lattice, so a system
  `[ phi -> x, ~phi -> c ]` needs no agreement (D8).

### 3.4 Evaluator rules

Order: (1) the face `i1` rules, (2) the rule of the head former, (3) else
stuck (VAL_STUCK). `A[r]` is the line at `r`. `transpFill`, `hfill` and
`comp` are the section 4 terms.

| Redex | Result | Slice |
|---|---|---|
| `transp L i1 a` | `a` | s1 |
| `transp (<i> T) phi a`, `T` in `Nat`, `Field`, `Unit`, `Flag`, `Type n` (closed, no parameters) | `a` | s1 |
| `transp (<i> (x : A) -> B) phi f` | `fun y => transp (<i> B[x := w i]) phi (f (w i0))`, where `w i = transp (<j> A[~j \/ i]) (phi \/ i) y` | s1 |
| `transp (<i> Sigma (x : A) B) phi p`, Prod | `pack (transp (<i> A) phi (first p)) (transp (<i> B[x := v i]) phi (second p))`, where `v i = transp (<j> A[i /\ j]) (phi \/ ~i) (first p)` | s1 |
| `Partial i1 A` | `A` | s1 |
| `hcomp A i1 u u0` | `u i1` | s2 |
| `hcomp T phi u u0`, `T` closed (`Nat`, `Field`, `Unit`, `Flag`) | `u0` when each branch body of `u i` (fresh `i`, on its face) does not use `i` and converts to `u0` (true when there is no branch); else stuck | s2 |
| `hcomp ((x : A) -> B) phi u f` | `fun x => hcomp B phi (fun i => [ phi -> (u i) x ]) (f x)` | s2 |
| `hcomp (Sigma (x : A) B) phi u p`, Prod | `pack (hfill A phi u1 (first p) i1) (comp (<i> B[x := hfill A phi u1 (first p) i]) phi u2 (second p))`, `u1`, `u2` = the projections of `u` | s2 |
| `transp (<i> PathP (<j> A) a b) phi p` | `<j> comp (<i> A) (phi \/ ~j \/ j) (fun i => [ phi -> p @ j, ~j -> a[i], j -> b[i] ]) (p @ j)` | s2 |
| `hcomp (PathP (<j> A) a b) phi u p` | `<j> hcomp A[j] (phi \/ ~j \/ j) (fun i => [ phi -> (u i) @ j, ~j -> a, j -> b ]) (p @ j)` | s2 |
| `transp` on Sum, Option, List | the constructor of the argument, with `transp` on its fields (List: on each element); a neutral argument is stuck | s3 |
| `hcomp` on Sum, Option, List | the data rule: base and branch bodies all the same constructor gives that constructor with `hcomp` on the fields; else stuck | s3 |
| `Glue A [ i1 -> pack T e ]`, `glue [ i1 -> t ] a`, `unglue g` with face `i1` | `T`, `t`, `first e g` | s4 |
| `unglue (glue s a)` | `a` | s4 |
| `transp`, `hcomp` on a Glue line or type | Cubical Agda method with `ghcomp` and `gcomp` (D8; CCHM 6.2) | s5 |
| `hcomp (Type n) phi E A` | `Glue A [ phi -> pack (E i1) (lineToEquiv (<i> E (~i))) ]` | s5 |

The closed-type `hcomp` rule needs no literal test. On `phi`, the face rule
gives `u i1`, so only a constant system is confluent with the face rule.
Example: `round` is `<i> 1200` (system `[ ~i -> 1200, i -> 1200 ]`, base
`1200`). The rule is for the closed types only. It is not the general rule
`hcomp A i0 u u0 = u0` that D8 refuses.

Other formers (Family, the fold and unfold families) give a stuck value in
M3. Each slice lists them in docs/STATUS.md (new open item O8).

### 3.5 Conversion

- OP_TRANSP, OP_HCOMP and the Glue ops compare argument by argument (the
  line under a fresh interval variable, the faces by normal form, the
  systems by conv_sys).
- Glue eta (s4): `g` converts to `glue s a` when `unglue g` converts to `a`
  and, on each conjunction of the face, `g` converts to the branch of `s`.
  The face and the system come from the `glue` value, not from a type.
- No rule makes two values equal because of their type (M2 review method).
  The base rule of section 3.4 reads the line of `transp`, which is an
  argument, not the type of a value. A guest row guards this (section 8).

### 3.6 Printer and JSON

- The printer writes `transp (<i> A) phi a`, `hcomp A phi u u0`,
  `Glue A [ ... ]`, `glue [ ... ] a` and `unglue g`.
- JSON: the slice finds the json.c refusal code for a value or type that is
  not data, uses it for a stuck Kan value and a Glue instance, and records
  it in SPEC 9.

## 4. Derived operations and their laws

Each operation is a checker rule (D2). The table gives the core term.

| Operation | Type | Core term | Slice |
|---|---|---|---|
| `comp (<i> A) phi u u0` | `A[i1]` | `hcomp A[i1] phi (fun i => [ phi -> transp (<j> A[i \/ j]) i (u i) ]) (transp (<i> A) i0 u0)` | s2 |
| `hfill A phi u u0 r` | `A` | `hcomp A (phi \/ ~r) (fun i => [ phi -> u (i /\ r), ~r -> u0 ]) u0` | s2 |
| `trans p q` | `p : Path A x y`, `q : Path A y z` give `Path A x z` | `<i> hcomp A (~i \/ i) (fun j => [ ~i -> x, i -> q @ j ]) (p @ i)` | s3 |
| `transport p x` | `p : Path (Type n) A B`, `x : A` give `B` | `transp (<i> p @ i) i0 x` | s3 |
| `subst P p x` | `P : A -> Type n`, `p : Path A a b`, `x : P a` give `P b` | `transp (<i> P (p @ i)) i0 x` | s3 |
| `transportRefl x` | `x : A` gives `Path A (transp (<_> A) i0 x) x` | `<i> transp (<_> A) i x` | s3 |
| `J P d p` | `P : (y : A) -> Path A x y -> Type n`, `d : P x refl`, `p : Path A x y` give `P y p` | `transp (<i> P (p @ i) (<j> p @ (i /\ j))) i0 d` | s3 |
| `JRefl P d` | `Path (P x refl) (J P d refl) d` | `<i> transp (<_> P x refl) i d` | s3 |
| `isContr A` | `Type n` | `Sigma (c : A) ((y : A) -> Path A c y)` | s4 |
| `fiber f y` | `f : A -> B`, `y : B` | `Sigma (x : A) (Path B (f x) y)` | s4 |
| `isEquiv f` | `f : A -> B` | `(y : B) -> isContr (fiber f y)` | s4 |
| `Equiv A B` | `Type n` | `Sigma (f : A -> B) (isEquiv f)` | s4 |
| `idEquiv A` | `Equiv A A` | `pack (fun x => x) (fun y => pack (pack y (<_> y)) (fun z => <i> pack (second z @ ~i) (<j> second z @ (~i \/ j))))` | s4 |
| `ua e` | `e : Equiv A B` gives `Path (Type n) A B` | `<i> Glue B [ ~i -> pack A e, i -> pack B (idEquiv B) ]` | s4 |
| `uaBeta e x` | `Path B (transport (ua e) x) (first e x)` | `<i> transp (<_> B) i (first e x)` | s5 |
| `lineToEquiv (<i> A)` | `Equiv A[i0] A[i1]` | internal template (Cubical Agda `lineToEquiv`, `isEquivTransport`) | s5 |
| `ghcomp A phi u u0` | `A` | internal template (D8): `hcomp A (phi \/ ~phi) (fun i => [ phi -> u i, ~phi -> u0 ]) u0` | s5 |
| `gcomp (<i> A) phi u u0` | `A[i1]` | internal template (D8): `ghcomp A[i1] phi (fun i => [ phi -> transp (<j> A[i \/ j]) i (u i) ]) (transp (<i> A) i0 u0)` | s5 |

The projection names (`first`, `second`) and the Sigma pair (`pack`) are
the M1 names. s4 checks that a Sigma projection works on `Equiv` values.

Laws that hold by computation (each is a `refl` in an example):

| Law | Why it computes | Slice |
|---|---|---|
| `transp L i1 a = a` | face `i1` rule | s1 |
| `transp (<_> Nat) phi n = n`, also for a variable `n` | base rule | s1 |
| `transp (<_> Nat -> Nat) i0 f = f` | Pi rule, base rule, function eta | s1 |
| `transp (<_> Sigma (n : Nat) (Path Nat n n)) i0 p` gives `pack (first p) ...` | Sigma rule | s1 |
| `hcomp A i1 u u0 = u i1` | face `i1` rule | s2 |
| `hfill A phi u u0 i0 = u0`; `hfill A phi u u0 i1` converts to `hcomp A phi (fun i => [ phi -> u i ]) u0`, with a literal system `u` in the example | `~i0 = i1`, face rule; conv_sys and value_on_face handle the nested system. There is no eta for partial elements (it would be a rule by type) | s2 |
| `trans refl refl` at Nat literals is `refl` | closed-type rule (constant system) | s3 |
| `round = <i> 1200` (`trans same back`) | closed-type rule (constant system) | s3 |
| `transport refl x = x` for `x : Nat` | base rule | s3 |
| `ua e @ i0 = A`, `ua e @ i1 = B` | Glue face rule | s4 |
| `unglue (glue s a) = a` | Glue beta | s4 |
| `transport (ua e) x = first e x` for `B` in Field or Nat | D8, base rule | s5 |
| `transport (ua (idEquiv A)) x = transp (<_> A) i0 x` | D8 | s5 |

## 5. Effect on examples

| Item | M3 |
|---|---|
| examples/equality.lang | `round : Path Nat price price := trans same back` (s3). `moved : (P : (n : Nat) -> Type 0) -> (u : P price) -> P cost := fun P u => subst P same u` (s3, D6). |
| test/eval `equality moved` | Without regularity (D3, question a): `fun P => fun u => transp (<i> P 1200) i0 u` (the printer may write `<_>` for a constant line, s3 decides and records the exact text). With regularity: `fun P => fun u => u`. |
| test/eval `equality round` | `<i> 1200` |
| New examples | examples/kan.lang (s1, s2), examples/kanpath.lang (s2), examples/derived.lang (s3), examples/glue.lang (s4, s5), examples/iso.lang (s5, guest `isoToEquiv`), a circuit file for section 6 |
| reserved-trans.lang, reserved-transport.lang (check rows) | They give REFUSE_NAME before and after: `trans` and `transport` move from RESERVED to BUILTINS (core names), and the message changes from "reserved" to "a core name". s3 updates the expected message if the gate checks it. |

## 6. Circuits

The IR must not change. The gate goldens in test/circuit stay byte for byte
the same, and `make e2e` gives the same verifiers. The lowering
(circuit.c lower_check) evaluates the `Path Field a b` type of each
constraint and reads the two ends. M3 adds no lowering code: each Kan term
in a constraint must evaluate to a Field normal form (D8 and the base rule).
A stuck Kan term in a constraint gives CIRCUIT_LOWER. That is the first
gate row for O4 (s5).

Guest test (s5): a circuit `squareUa` states the `square` relation through
`transport (ua swapE) (pair (x * x) y)`, where `swapE : Equiv (Prod Field
Field) (Prod Field Field)` comes from guest `isoToEquiv` (examples/iso.lang,
the swap and its two eta laws). Its constraint is
`second (transport (ua swapE) (pair (x * x) y)) == first (...)`, so its IR
must be the IR of `square`. A new gate table test/circuit/same-ir.txt
(PROG NAME GOLDEN) runs `langc circuit` and compares with the existing
golden. `make e2e` does not read this table, so contracts/ does not change.
Fallback if guest `isoToEquiv` fails in s5: `ua (idEquiv Field)`.

## 7. New check codes

| Code | Exit | The compiler refuses | Slice |
|---|---|---|---|
| TRANSP_CONSTANT | 1 | `transp (<i> A) phi a` where `A` is not constant on a conjunction of `phi`. The message gives the conjunction and the two types | s1 |
| HCOMP_BASE | 1 | `hcomp A phi u u0` (also `comp`, `hfill`) where `u i0` does not convert to `u0` on a conjunction of `phi`. The message gives the conjunction and the two values | s2 |
| GLUE_EXPECTED | 1 | `glue` with no declared Glue type; `unglue` of a term that is not of a Glue type | s4 |
| GLUE_BOUNDARY | 1 | `glue s a` where `first e t` does not convert to `a` on a conjunction of the face | s4 |
| CIRCUIT_LOWER (M1 code) | 1 | first gate row: a constraint with a stuck Kan term | s5 |

Existing codes with new causes: PATH_TYPE and SORT_FIBRANT (a bad line of
`transp` or `comp`), FACE_COVER and FACE_AGREE (the systems of `hcomp`,
`Glue` and `glue`), REFUSE_NAME (the D10 names), TYPE_MISMATCH (a
`Partial phi A` value against `A` where `phi` is not `i1`).

## 8. Guest tests

All tests are glue-lang programs and gate tables. No new node, Python or
shell harness. The native C harnesses (test/*-regressions.c) can get cases
for kan.c and face_forall.

| Slice | Tests |
|---|---|
| s1 | examples/kan.lang: `transp` at `i1`, on Nat, Field, Type 0, Pi, Sigma, Prod; the s1 laws of section 4; D4 (`u i` under the face of `u`, `(u i) @ j`). Check rows: TRANSP_CONSTANT for `transp (<i> P (p @ i)) i1 u` and for `transp (<i> A[i /\ j]) j a` (refused), with `transp (<i> A[i /\ j]) ~j a` accepted; PATH_TYPE for a line in I, reserved names of D10. Eval rows: the transp of a function on Nat is the function (eta), a stuck `transp (<i> P n) i0 u` with a variable `P`. Guard: two variables `x y : Nat` stay different after `transp` (no rule by type). |
| s2 | examples/kanpath.lang: `hcomp` at `i1`, the closed-type rule (a constant system on a variable base, and a system that uses `i`, which is stuck), Pi, Sigma, PathP; `comp`, `hfill` and their end laws; `transp` along a PathP line (a square). Check rows: HCOMP_BASE (also through `comp`). Eval rows: a stuck `hcomp` on Nat with a system that uses `i`; a constant system on a variable base gives the base. |
| s3 | examples/derived.lang: `trans`, `transport`, `subst`, `J`, `transportRefl`, `JRefl`, transp and hcomp on Sum, Option, List. equality.lang `round` and `moved` with their eval rows. Check rows: REFUSE_NAME for each s3 name. |
| s4 | examples/glue.lang: `Glue` at faces `i1` and `j`, `glue`, `unglue`, Glue beta and eta, `Equiv`, `idEquiv`, `ua` ends. Check rows: GLUE_EXPECTED (x2), GLUE_BOUNDARY, FACE_COVER on `glue`. JSON: the Glue instance refusal. |
| s5 | `transp` and `hcomp` on Glue, `hcomp` on `Type 0`, `uaBeta` as `refl` rows on Nat and Field, a `uaBeta` row with a variable `B` (for Nat and Field, the closed-type rule hides a defect in `ghcomp`), `transport (ua e)` on a function type (DESIGN line 169), examples/iso.lang, the same-ir table, the CIRCUIT_LOWER row. |

Mutation: 3 mutants for each slice, `make -B` for each rebuild (make 3.81
compares mtimes to the second). The slice prompts name them.

## 9. Questions for the USER

a. Regularity: none (CCHM), or a rule that makes `transp` along a constant
   line the identity (the old `equality moved` golden)?
   Recommendation: none (D3). Reasons: (1) Glue types and `hcomp` in the
   universe are not regular. With a regularity rule, `transp` along a
   constant Glue line has two different results (the Glue rule and the
   identity), so conversion depends on the order of the rules. (2) Cubical
   Agda, the language of design/DESIGN.md, has no regularity. DESIGN uses
   uaβ and transport only as paths. (3) The base and structural rules give
   `transport refl x = x` by computation for Nat, Field and the types built
   from them, which is each circuit case. (4) M2-PLAN 9c already chose this:
   regularity in M2 would be a definitional equality that M3 takes back.
   Cost: the `equality moved` golden changes to a `transp` term, and
   `transportRefl` and `JRefl` are paths.
   Ruling (USER, 2026-10-14): none (CCHM), as recommended. D3 stays.

b. `transp` and `hcomp`: checker rules that write core terms (as D4 of M2),
   or evaluator primitives with neutral forms?
   Recommendation: evaluator primitives (D1), with the derived operations
   as checker rules (D2). Reasons: (1) the rule depends on the head former
   of the type after evaluation; a checker rule sees only the syntax, so
   `transport (cong P p) x` with a variable `P` cannot reduce later when
   `P` is applied. (2) The neutral form is necessary for a variable family.
   (3) The derived operations stay D4: their laws hold by computation.
   Ruling (USER, 2026-10-14): evaluator primitives, with the derived
   operations as checker rules, as recommended. D1 and D2 stay.

c. Glue in M3 in full, or only the Glue of `ua` (the face `~i \/ i`)?
   Recommendation: in full, over two slices (s4: the Glue type, `glue`,
   `unglue`, the equivalence names, `ua`; s5: the Kan rules for Glue, the
   universe, uaBeta, the circuit test). Reasons: (1) `hcomp` in `Type n` is
   a Glue type, so without full Glue, `trans` of two type paths (for
   example `trans (ua e) (ua f)`) is stuck, and M7 and M8 compose such
   paths. (2) M4 HITs need `hcomp` values in the universe. (3) The Cubical
   Agda method (D8) gives uaBeta by computation only through the general
   rule. Cost: s5 is the largest slice. If the USER chooses the restricted
   Glue, s5 has only the `ua` line and `hcomp` in `Type n` stays stuck.
   Ruling (USER, 2026-10-14): full Glue over s4 and s5, as recommended.

d. Slice count and order?
   Recommendation: 6 slices, section 10. The kickoff order (s2 Path and
   universes before s3 hcomp) changes: `transp` along a PathP line is a
   `comp`, so it needs `hcomp`. Path and PathP go to s2 with `hcomp`.
   Ruling (USER, 2026-10-14): 6 slices in the order of section 10, as
   recommended.

## 10. Slices

Each slice is one session or more. Each prompt is in ~/Documents. Each
prompt carries its own gates, IR check, close steps and traps.

| Slice | Content | Prompt |
|---|---|---|
| s1 | RESERVED D10; kan.c; OP_TRANSP; `transp` typing (TRANSP_CONSTANT); `Partial i1 A = A` (D4); transp rules for closed types, Pi, Sigma, Prod, `Type n`; printer; tests. | glue-lang-M3-s1-PROMPT.md |
| s2 | OP_HCOMP; `hcomp` typing (HCOMP_BASE); face `i1` rule, data rule for closed types, Pi, Sigma, Prod; `comp`, `hfill`; transp and hcomp for PathP; tests. | glue-lang-M3-s2-PROMPT.md |
| s3 | transp and hcomp for Sum, Option, List; `trans`, `transport`, `subst`, `transportRefl`, `J`, `JRefl`; `round` and `moved`; tests. | glue-lang-M3-s3-PROMPT.md |
| s4 | `isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`; `Glue`, `glue`, `unglue` (typing, face rules, beta, eta); `ua`; GLUE codes; tests. | glue-lang-M3-s4-PROMPT.md |
| s5 | face_forall; `ghcomp`, `gcomp`; transp and hcomp for Glue (D8); `hcomp` in `Type n`, `lineToEquiv`; `uaBeta`; examples/iso.lang; the same-ir circuit test; the CIRCUIT_LOWER row. | glue-lang-M3-s5-PROMPT.md |
| s6 | Close: SPEC 2.5 (new) and 9 (O7 closed, O8), FORMERS if a cell changes, PLAN M3 DONE, STATUS `## M3`, mutant roll-up, review, M4 kickoff prompt. | glue-lang-M3-s6-PROMPT.md |

Gates for each slice (all GREEN before staging): `make -B check`, `make
check-clang`, `forge build --root contracts --sizes`, then `make e2e` last.
`cargo fmt --check` and clippy only if halo2/ changes (M3 plans no halo2/
change).

IR check for each slice: after `make e2e`, `git diff --exit-code --
test/circuit contracts/src/verifiers`. If it is clean, restore
contracts/test/fixtures from HEAD and run `forge test --root contracts`
again (33 passed). If the verifiers changed, stop: M3 must not change the
IR without a USER ruling.
