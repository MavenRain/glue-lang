# M2 plan: Path types and the interval

Status: DONE 2026-10-14 (slices s1 to s5). M1 is commit 503431c. The USER rules on the
questions of section 9 before slice s1 starts. No code changes in the kickoff.

## 1. Sources

| Source | What M2 takes from it |
|---|---|
| docs/PLAN.md, M2 row | Interval `I`. `Path A a b` replaces F12 and F13. `refl`, `sym`, `cong`, `funExt` by computation. The checker gets interval variables and the face lattice. |
| docs/PLAN.md, Rulings | Proof irrelevance is prohibited: no `Prop`, no `SProp`, no definitional proof irrelevance. Truncations are HITs with path constructors (M4). |
| docs/PLAN.md, Lowering rules | A Path in a set (Field) lowers to an equality constraint (a copy or an `arith` row). A proof stays a term. The lowering has nothing to emit for a path in a type with no wires. |
| design/DESIGN.md | The design is written in Cubical Agda (`--cubical`, `Cubical.Foundations.Prelude`). It uses `≡`, `sym`, `funExt`, `_∙_`, `cong`, `transport`, `ua` and `uaβ` "by computation", path constructors applied to interval variables (`interp (encrypt i) = enc i`), and squares (`isSet→SquareP ... i j`). |
| formers/FORMERS.md, F12 and F13 | `Eq A x y`, `refl`, `symm`, `trans`, `transport`, `cong`. Laws: `transport P refl u = u`, `cong f refl = refl`. The F12 rule "a proof has no runtime content; it erases from the target" does not agree with the ruling of 2026-10-14. M2 replaces it. |
| SPEC.md 2.2 | "`a == b` ... In M1 it is a constraint form. M2 makes it a Path type." |
| SPEC.md 7.1 | `a == b` lowers to a copy between two wires. The IR is docs/IR.md v1. |

The M1 code (anchors for the slices):

| Fact | Where |
|---|---|
| `Eq` is `OP_EQ` with 3 arguments. `refl` is the 0-ary constant `OP_REFL`. | src/front/core.h:28,41; src/front/check.c:93,105 |
| `symm`, `trans`, `cong`, `transport` are ops. They reduce only when the proof argument is `refl`. | src/front/check.c:118-121,790-848; src/front/eval.c:687,882-889 |
| `refl` checks `conv(a, b)` against the hint `Eq A a b` (code TYPE_REFL). | src/front/check.c:550-559 |
| Conversion has eta for functions. | src/front/eval.c:989,1034 |
| JSON writes an `Eq` component as `null`. A definition of an `Eq` type is not an instance. | src/json.c:106,185,215-234,300 |
| Tests that use `Eq`: examples equality, laws, sigma; test/check bad-refl, bad-eta; test/eval `sigma offer`; test/json/sigma.json; the old node harnesses test/regressions.js and test/printer-regressions.js (strings only). | |

## 2. Decisions of this plan

The USER can change each decision. Section 9 has the questions that need a
ruling.

- D1. The core former is `PathP (<i> A) a b`, a dependent path. `Path A a b`
  is the case where the line does not use `i`. Reason: a dependent `cong`,
  a dependent `funExt`, the squares of M4 (`squash/`) and the Glue lines of
  M3 need PathP. The printer writes `Path A a b` when the line is constant.
- D2. The surface syntax is the cubicaltt syntax of the kickoff: path
  abstraction `<i> t` and path application `p @ r`. They are separate from
  `fun` and from application, so each checker rule has one meaning.
- D3. `I` is not a type. `I` is not in `Type n`. `Path I i0 i1` is refused.
  Before s4, only `<i>` binds an interval variable. s4 adds interval binders
  in `fun` and Pi (section 3.7).
- D4. `refl`, `sym`, `cong`, `funExt` and `happly` are checker rules. Each
  rule writes an ordinary core path term (section 4). The evaluator has no
  rule for them. Thus each law of section 4 holds by computation: beta for
  `<i>`, the endpoint rule, the De Morgan normal form and path eta.
- D5. A neutral path application carries the two endpoints. The checker
  copies them from the type of `p` into the core term. Thus `p @ i0`
  evaluates to `a` when `p` is a variable of type `PathP L a b`.
- D6. The interval operators are ASCII: `~r`, `r /\ s`, `r \/ s`. The lexer
  stays byte based.
- D7. One commit for each slice. Each slice ends GREEN and STAGED, with its
  own commit message file. The next slice starts after the USER commits.
- D8. `trans`, `transport`, `transp`, `hcomp`, `Glue`, `ua`, `Partial` and
  `SSet` are reserved names from s1 (REFUSE_NAME, rule R4). M3 can then add
  the Kan operations with no change to valid M2 programs.

## 3. Core changes

### 3.1 Syntax

| Form | Meaning | Precedence |
|---|---|---|
| `I` | The interval. Only in `PathP (<i> A)` lines and in binders (s4). | |
| `i0`, `i1` | The end points of the interval. Core names. | atom |
| `~r` | Reversal. | tightest, prefix |
| `r /\ s` | Meet (min). | above `\/`, left |
| `r \/ s` | Join (max). | lowest, left |
| `Path A a b` | Paths in `A` from `a` to `b`. | application |
| `PathP (<i> A) a b` | Paths over the line `<i> A`. | application |
| `<i> t` | Path abstraction. The body extends as far as possible (as `fun`). | binder |
| `p @ r` | Path application. Left associative, at the application level: `f x @ i` is `(f x) @ i`. The right operand is an interval atom: `i0`, `i1`, a variable, `~` atom, or a parenthesized interval term. | application |

New tokens: `<`, `>`, `@`, `~`, `/\`, `\/`. The current lexer has none of
them (src/front/lexer.h:8-32), so no valid M1 program changes.

### 3.2 Interval values

An interval term evaluates to a normal form in the free De Morgan algebra on
the interval variables. That algebra is the free bounded distributive
lattice on the literals `i` and `~i`. The normal form is a disjunction of
conjunctions of literals, with no conjunction that contains another (an
antichain). `i0` is the empty disjunction. `i1` is the disjunction of the
empty conjunction. `~` uses the De Morgan laws and then distributes back to
the normal form. Two interval values are equal when their normal forms are
equal.

`i /\ ~i` is not `i0`. `i \/ ~i` is not `i1`. The interval is a De Morgan
algebra, not a Boolean algebra. A guest test guards this (section 8).

The new module src/front/interval.c holds the normal form, meet, join,
reversal and substitution. A literal is a de Bruijn level and a sign. An
interval value with more than 64 conjunctions is refused (INTERVAL_LIMIT),
so that `~` on a large term cannot use all memory.

### 3.3 Typing rules

| Rule | Premises | Conclusion |
|---|---|---|
| Path former | `L` is a line `<i> A` with `A : Type n` for a fresh interval variable `i`; `a : A[i := i0]`; `b : A[i := i1]` | `PathP L a b : Type n` |
| Path sugar | `A : Type n`, `a : A`, `b : A` | `Path A a b` is `PathP (<_> A) a b` |
| Abstraction (check) | Against `PathP L a b`: `t : L @ i` under the fresh interval variable `i`; `t[i := i0]` converts to `a`; `t[i := i1]` converts to `b` | `<i> t : PathP L a b` |
| Application (infer) | `p : PathP L a b`; `r` is an interval term | `p @ r : L @ r` |

The checker refuses `<i> t` with no expected type (PATH_EXPECTED). A
boundary that does not convert gives PATH_BOUNDARY. The message gives the
end (`i0` or `i1`) and both normal forms, as TYPE_REFL does.

An interval variable is in the de Bruijn context of the checker. Its value
is the interval value of one positive literal (not VAL_VAR). An interval
variable where the checker needs a term of a type gives INTERVAL_TERM. A
term of a type where the checker needs an interval term gives
INTERVAL_TERM.

### 3.4 Evaluator rules

| Redex | Result |
|---|---|
| `(<i> t) @ r` | `t[i := r]` |
| `p @ i0`, `p` neutral of type `PathP L a b` | `a` (from the core term, D5) |
| `p @ i1`, `p` neutral of type `PathP L a b` | `b` |
| `p @ r`, `p` neutral, `r` not an end point | a neutral path application |
| `~`, `/\`, `\/` | the normal form of section 3.2 |

New value kinds: VAL_IVL (an interval normal form), VAL_PLAM (a path
closure), VAL_PAPP (a neutral path application with its endpoints). New
core kinds: CORE_IVL (an interval term), CORE_PLAM, CORE_PAPP (p, r, a, b).
New op: OP_PATHP. Each new kind gets a case in each exhaustive switch
(-Wswitch-enum).

### 3.5 Conversion

- Interval values: equal normal forms.
- Path eta: when one side is VAL_PLAM, compare `p @ i` and `q @ i` for a
  fresh interval variable `i`. This is the same plan as the function eta at
  src/front/eval.c:989.
- Neutral path applications: compare the paths and the interval values.
- Two different variables of one path type do not convert. There is no rule
  that makes two proofs equal because of their type. A guest test guards
  this (section 8).

### 3.6 Printer and JSON

- The printer writes `<i> t`, `p @ r`, `~i`, `i /\ j`, `i \/ j`, and
  `Path A a b` for a constant line, else `PathP (<i> A) a b`.
- JSON: a definition of a Path type is a proof, not an instance (as for
  `Eq` now). A Path component of a data value writes `null`, as an `Eq`
  component does now: the end points are in the type. M4 (loops in HITs)
  must look at this rule again (SPEC open item O5, new).

### 3.7 The face lattice (slice s4)

A face is an interval term read as "the term is `i1`", as `IsOne` in Cubical
Agda. The face lattice is the De Morgan algebra divided by `i /\ ~i = i0`. A
face in normal form is an antichain of consistent conjunctions. Each
conjunction is an assignment (`i` means `i = 1`, `~i` means `i = 0`). The
module src/front/face.c gives the normal form, meet, join, entailment and
the restriction of a context by a conjunction (a substitution).

The guest forms of s4:

| Form | Rule |
|---|---|
| `fun (i : I) => t`, `(i : I) -> T` | An interval binder. The type is in the sort `SSet`, not in `Type n`. |
| `Partial phi A` | Partial elements of `A : Type n` on the face `phi`. Sort `SSet`. |
| `[ phi1 -> t1, phi2 -> t2 ]` | A system. Each `tk` checks under each conjunction of `phik`. Where two faces meet, the two terms must convert (FACE_AGREE). Against `Partial phi A`, the join of the faces must equal `phi` (FACE_COVER). |

A type in `SSet` cannot be the type of a Path, of a Sigma component or of an
instance (SORT_FIBRANT). M3 uses systems in `hcomp` and `transp`.

## 4. Derived operations and their laws

Each operation is a checker rule (D4). The table gives the core term that
the rule writes.

| Operation | Type | Core term |
|---|---|---|
| `refl` (check) | `PathP L a b` with a constant `L` and `a` converts to `b` | `<i> a` |
| `sym p` | `p : PathP L a b` gives `PathP (<i> L @ ~i) b a` | `<i> p @ ~i` |
| `cong f p` | `f : (x : A) -> B x`, `p : Path A a b` gives `PathP (<i> B (p @ i)) (f a) (f b)` | `<i> f (p @ i)` |
| `funExt h` | `h : (x : A) -> PathP (<i> B x) (f x) (g x)` gives `Path ((x : A) -> B x) f g` | `<i> fun x => h x @ i` |
| `happly p x` | `p : Path ((x : A) -> B x) f g` gives `Path (B x) (f x) (g x)` | `<i> (p @ i) x` |

`funExt` checks against `Path ((x : A) -> B x) f g`. With no expected type,
it reads `f` and `g` from the endpoints of the codomain of `h`. `funExt` is
a theorem with a body, not an axiom.

Laws that hold by computation (each is a `refl` in examples/paths.lang):

| Law | Why it computes |
|---|---|
| `sym (sym p) = p` | `~~i = i`, then path eta |
| `sym refl = refl` | beta |
| `cong (fun x => x) p = p` | beta, then path eta |
| `cong g (cong f p) = cong (fun x => g (f x)) p` | beta |
| `cong f refl = refl` | beta |
| `funExt (fun x => refl) = refl` | beta, function eta |
| `happly (funExt h) = h` (pointwise) | beta, path eta |
| `funExt (happly p) = p` | beta, function eta, path eta |
| `p @ i0 = a`, `p @ i1 = b` for a variable `p` | the endpoint rule |
| The connection square `<i> <j> p @ (i /\ j)` has the type `PathP (<i> Path A a (p @ i)) refl p` | `i0 /\ j = i0`, `i1 /\ j = j` |

## 5. F12, F13 and the examples

This section follows the recommendation of question 9c. The USER ruling can
change it.

| Item | M2 (slice s3) |
|---|---|
| `Eq A x y` | Removed. Programs write `Path A x y`. |
| `refl` | Kept. Now `<i> a` (section 4). |
| `symm` | Renamed `sym`. |
| `cong` | Kept, over Path. |
| `trans`, `transport` | Removed in M2, reserved (D8). M3 defines `_∙_` with `hcomp` and `transport` with `transp`. |
| FORMERS.md F12, F13 | The glue-lang realization is Path. The text "a proof ... erases from the target" goes. Matrix cells: `Path` and the slice anchors. |
| examples/equality.lang | `Eq` to `Path`, `symm` to `sym`. `round` (trans) and `moved` (transport) go to the M3 test list in docs/STATUS.md. |
| examples/laws.lang, sigma.lang | `Eq` to `Path`. The laws stay `refl`. |
| test/json/sigma.json | The type text changes to `Path Nat n 1200`. The value stays `null`. |
| test/eval `sigma offer` | Prints `pack 1200 (<i> 1200)`. |
| test/regressions.js, test/printer-regressions.js | Old harnesses (they stay). Only the strings change: `Eq` to `Path`. No new node test. |

## 6. Circuits

This section follows the recommendation of question 9b. The checker forms
the type `Path Field a b` for each constraint `a == b`. The constraint form
`a == b` does not change. The lowering reads the two endpoints of that
type. The IR documents stay byte for byte the same: the gate goldens in
test/circuit do not change, and `make e2e` does not change. An ordinary
definition can now state a circuit relation as a type family, for example
`def sqRel : Field -> Field -> Type 0 := fun x y => Path Field (x * x) y`.
M7 (Direction 2) uses such families.

## 7. New check codes

| Code | Exit | The compiler refuses | Slice |
|---|---|---|---|
| PATH_EXPECTED | 1 | `<i> t` against a type that is not a Path, or with no expected type; `p @ r` where `p` is not a path | s1 |
| PATH_BOUNDARY | 1 | `<i> t` whose end at `i0` or `i1` does not convert to the endpoint of the type | s1 |
| PATH_TYPE | 1 | `Path A a b` or `PathP (<i> A) a b` where `A` is not in `Type n` (for example `I`) | s1 |
| INTERVAL_TERM | 1 | an interval term where a term of a type is necessary, or the opposite | s1 |
| INTERVAL_LIMIT | 1 | an interval normal form with more than 64 conjunctions | s1 |
| FACE_AGREE | 1 | two branches of a system that do not convert where their faces meet | s4 |
| FACE_COVER | 1 | a system whose faces do not join to the face of its `Partial` type | s4 |
| SORT_FIBRANT | 1 | a type in `SSet` as a Path type, a Sigma component or an instance | s4 |

Parse refusals (codes from test/parse/expect.txt, extended in s1): `<i` with
no `>`, `p @` with no interval atom, `/\` or `\/` with no left operand.

## 8. Guest tests

All tests are glue-lang programs and gate tables. No new node, Python or
shell harness.

| Slice | Tests |
|---|---|
| s1 | examples/paths.lang: `<i> a`, application at `i0`, `i1` and a variable, a square with `/\` and `\/`, PathP over a constant line. test/check rows: one for each s1 code (PATH_BOUNDARY at `i0` and at `i1`). test/eval rows: endpoint values, the normal form of `<i> <j> p @ (i /\ ~j)`. Parse rows. |
| s2 | The law table of section 4, each as `refl` in examples/paths.lang. Guards: (1) `fun p q => refl : (p q : Path Nat 0 0) -> Path (Path Nat 0 0) p q` gives TYPE_REFL (no proof irrelevance); (2) `<i> p @ (i /\ ~i)` is not `<i> p @ i0` (TYPE_REFL; De Morgan, not Boolean); (3) `def sym := ...` gives REFUSE_NAME. |
| s3 | Rewritten equality, laws and sigma examples; the json and eval goldens of section 5; circuit goldens unchanged; `sqRel` in examples/field.lang; the reserved names `trans` and `transport` give REFUSE_NAME. |
| s4 | examples/faces.lang: `Partial`, systems on `~i \/ i`, on `i /\ j`, and an interval binder. test/check rows: FACE_AGREE, FACE_COVER, SORT_FIBRANT. |

Mutation: each slice changes one rule in the C (for example, no boundary
check; `~` as the identity; endpoint rule off) and shows that a gate row
fails. Use `make -B` for each rebuild (make 3.81 compares mtimes to the
second).

## 9. Questions for the USER

Rulings (USER, 2026-10-14, "take defaults for a-d"):

- a. The interval is the De Morgan interval (CCHM).
- b. `==` stays sugar for `Path Field a b`.
- c. Delete F12 and F13 in M2 (slice s3).
- d. The face lattice is in M2, as slice s4.

a. Interval structure: De Morgan (CCHM) or Cartesian?
   Recommendation: De Morgan (CCHM, `~`, `/\`, `\/`). Reasons: (1)
   design/DESIGN.md is Cubical Agda, and the interval of Cubical Agda is the
   De Morgan interval; its `sym`, `funExt`, `isSet→SquareP` and `ua`/`uaβ`
   use it. (2) PLAN M2 asks for `sym` by computation before the Kan
   operations (M3). With `~`, `sym p` is `<i> p @ ~i` and `sym (sym p) = p`
   by computation. A Cartesian interval has no `~`, so `sym` needs a
   composition, which is M3. (3) The connections `/\` and `\/` give the
   contractibility of singletons with no composition.

b. Circuit constraints: `Path Field a b` in the surface, `==` as sugar, or
   circuits separate until M3?
   Recommendation: keep `==` as sugar for `Path Field a b` (section 6).
   Reasons: SPEC 2.2 says that M2 makes `==` a Path type; the lowering rule
   of PLAN already maps a Path in Field to a copy; the circuit goldens and
   `make e2e` stay byte for byte the same, which is the M2 check that the IR
   did not change; a second surface spelling gives no new power before M7.

c. F12 and F13: delete, or keep as derived forms over Path for one
   milestone?
   Recommendation: delete them in M2 (section 5). Reasons: (1) `trans` and
   `transport` are not derived forms over Path without `hcomp` and `transp`
   (M3). An M2-only rule "transport along a constant path is the identity"
   is regularity, which CCHM does not have, so M3 would have to take back a
   definitional equality. (2) An `Eq` alias gives two spellings for one
   former, and the printer writes `Path` in each golden. (3) The F12 text
   "a proof ... erases from the target" does not agree with the ruling of
   2026-10-14, so F12 must change in each case. Cost: the examples `round`
   and `moved` leave for one milestone.

d. The face lattice: build it in M2 (slice s4, with `Partial`, systems and
   interval binders as its guest consumer), or move it to M3 with its first
   Kan consumer?
   Recommendation: build it in M2 (s4). Reasons: the PLAN M2 row puts it in
   M2; M3 already has `transp`, `hcomp` for five formers, Glue, `ua` and
   `uaβ`; `Partial` and systems give each face lattice function a guest
   test. If the USER moves it to M3, M2 ends after s3 plus the close.

## 10. Slices

Each slice is one session. Each prompt is in ~/Documents.

| Slice | Content | Prompt |
|---|---|---|
| s1 | Lexer, parser, AST; interval.c; core and value kinds; OP_PATHP; `<i> t`, `p @ r`; endpoint rule; conversion; printer; boundary check; the s1 codes and tests. `Eq` stays. DONE 2026-10-14 (docs/STATUS.md `## M2`). | glue-lang-M2-s1-PROMPT.md |
| s2 | `refl` over Path (beside `Eq`), `sym`, `cong`, `funExt`, `happly`; the law table; the guards. DONE 2026-10-14 (docs/STATUS.md `## M2`). | glue-lang-M2-s2-PROMPT.md |
| s3 | F12/F13 per ruling c; circuits per ruling b; examples, goldens, old harness strings; reserved names; IR unchanged. DONE 2026-10-14 (docs/STATUS.md `## M2`). | glue-lang-M2-s3-PROMPT.md |
| s4 | Face lattice (face.c), interval binders, `SSet`, `Partial`, systems, the s4 codes and tests. DONE 2026-10-14 (docs/STATUS.md `## M2`). | glue-lang-M2-s4-PROMPT.md |
| s5 | Close: SPEC 2, 3 and 9, FORMERS F12/F13 and matrix, PLAN M2 DONE, STATUS, mutant roll-up, M3 kickoff prompt. DONE 2026-10-14 (docs/STATUS.md `### s5`). | glue-lang-M2-s5-PROMPT.md |

Gates for each slice (all GREEN before staging): `make check`, `make
check-clang`, `make e2e` (last, before staging), `forge build --root
contracts --sizes`; `cargo fmt --check` and clippy only if halo2/ changes
(M2 does not plan a halo2/ change).

IR check for each slice: after `make e2e`, `git diff --exit-code --
test/circuit contracts/src/verifiers`. If the verifiers did not change,
restore contracts/test/fixtures from HEAD and run `forge test --root
contracts` again, so that the slice diff has no proof churn. If they did
change, stop: M2 must not change the IR without a USER ruling.
