# glue-lang M7 plan: circuits quotiented by relation equivalence

Status: RULED (2026-10-18). The USER ruled questions (a) to (i) of section
9: each recommendation. M6 is the s7 close commit c8c1d5e. The kickoff
makes no code change. The s1 commit includes this plan, as in M2 to M6.

M7 is Direction 2 of design/DESIGN.md: a circuit relation is a value, and two
circuits that define the same relation are equal (a path in a quotient). Thus
a property proved at one representative transports to each equivalent
circuit. The docs/PLAN.md M7 row: "The lowering of `rec` runs on
representatives; `eq/` and `squash/` cases are checker obligations with no
circuit cost."

## 1. Sources

| Source | Facts used |
|---|---|
| docs/PLAN.md | Rulings (2026-10-14: no proof irrelevance); the M7 row; the M8 row (Direction 4 is M8); the lowering table (`Quotient eliminator: Runs on representatives`, `transp along ua e: the forward map of e`, `Path in a set (Field): an equality constraint`, "A proof stays a term") |
| design/DESIGN.md | Line 25 and `### Direction 2` (lines 60 to 117): `Circuit = Public -> Witness -> Type`, `Satisfies`, `C1 ~ C2 = forall x w -> Satisfies C1 x w ≃ Satisfies C2 x w`, the set quotient, `rec`, `transportProp`; the two `### How Directions 2 and 4 use` sections (lines 246 and 325) |
| SPEC.md | 2.6 (HITs, `fold B`, the `hcomp` rule of the eliminator, D3); 2.7 (QITs, `elim`, **Direction 2.**, **Circuits.**); 2.8 **Circuits.** (D13); 7.1 (the lowering); 9: O3, O4, O12 (c) and (f), O13, O14, O17, O18 |
| docs/STATUS.md | `## M6` and `### s7: close` (open items, staged review corrections) |
| examples | quotSq.glue (fold out of `GQuot Field SqRel`, IR of square); intSq.glue (fold out of Int); transportProp.lang (`P` fold into `Type 0` with the `geqc` case `ua (rotE Field)`, the `Pline` def, uaβ); quot.lang (quotRec with an isSet argument; `quotSquashcStuck` is O12 (c)) |
| domain/domain.lang | Lines 26 to 32: Quot (`cls`, `eqc`, `squashc`), GQuot (`gcls`, `geqc`) |
| test/circuit | same-ir.txt (8 rows), expect.txt (5 rows), refuse.txt (14 rows) |

Code anchors (rg at c8c1d5e): src/circuit.c `no_circuit_form` 143,
`lower_value` 148, `lower_check` 189, `lower_circuit` 225; src/front/check.c
`fold_hit_rule` 1009, `rule_elim` 1673; src/front/eval.c `fold_hit` 1009
(declared at 907), `eval_hit_case` 928, `fold_hit_sys` 957, `stuck_fold_hit`
972; src/front/front.c line 13 (domain/domain.lang is compiled into langc as
build/domain.c). M7 changes none of these functions (section 3).

### Kickoff probes (2026-10-18)

Each probe is a file in the session scratchpad, not in the repository. Each
`langc circuit` had `-o`. "IR SAME" means that `cmp` against
test/circuit/square.circuit.json finds no difference.

| Probe | Constraint or term | Result |
|---|---|---|
| A1 | `sq (geqc Field SqRel x x refl @ i0) == y` | exit 0, IR SAME |
| A2 | the same at `@ i1` | exit 0, IR SAME |
| A3 | `sq (hcomp (GQuot Field SqRel) i0 (fun i => [ i0 -> gcls Field SqRel x ]) (gcls Field SqRel x)) == y` | exit 0, IR SAME |
| A4 | A3 through `elim (fun d => Field) (fun a => a * a) (fun a => fun b => fun r => r)` | exit 0, IR SAME |
| A5 | the `geqc` case `fun a => fun b => fun r => refl` (wrong type) | HIT_CASE, exit 1 |
| A6 | a Quot fold with a binder `(s : isSet Field)` | CIRCUIT_BINDER, exit 1 |
| A7 | a Quot fold with the `squashc` case `fun u => fun v => fun p => fun q => <i> <j> p @ j` | HIT_CASE, exit 1 |
| A8 | the `geqc` case `transport (Lk a b) (transport (Lk a b) r)` (`Lk` is a def `<k> SqRel a b`) | exit 0, IR SAME |
| D0 | a scratch langc with `family Circ := cpub \| cpriv \| clit (clv : Field) \| cadd (cal : Circ) (car : Circ) \| cmul (cml : Circ) (cmr : Circ)` in domain/domain.lang; `ev` (an `elim` into Field), `CRel`, `evQ`, `Sat`, `CEqv` (`Equiv` of `Sat`), `SatQ` (a fold into `Type 0` with the case `ua (e y x)`), `SatP` | `langc check` ok |
| D1 | `evQ y x (gcls Circ CRel (cmul cpriv cpriv)) == y` | exit 0, IR SAME |
| D2 | `evQ y x (geqc Circ CRel c c r0 @ i1) == y`, `c = cmul cpriv cpriv` | exit 0, IR SAME |
| D3 | `refl : CRel (cmul cpriv cpriv) (cadd (cmul cpriv cpriv) (clit 0))` | TYPE_REFL (`fieldMul x x` against `fieldAdd (fieldMul x x) 0`) |
| D4 | `refl : CRel (cmul cpriv cpriv) (cmul cpriv (cmul cpriv (clit 1)))` | TYPE_REFL |
| D5 | Circ with `cpow (cpb : Circ) (cpn : Nat)` (case `fold (fun (t : Field) => hb * t) hb n`); `opt : CRel (cmul cpriv cpriv) (cpow cpriv 1) := fun y => fun x => refl`; `evQ y x (geqc Circ CRel (cmul cpriv cpriv) (cpow cpriv 1) opt @ i1) == y` | exit 0, IR SAME |
| D6 | `evQ y x (gcls Circ CRel (cpow cpriv 1)) == y` | exit 0, IR SAME |
| D7 | `SLine` (a def `<i> SatP y x (geqc .. opt @ i)`), `moveSat y x p = transport (SLine y x) p`; `m3eq : Path (Path Field 9 9) (moveSat 9 3 refl) refl := refl` | check ok; `eval m3` gives `<j> 9` |

## 2. Design calls

- D1. The shallow form stays and gets its obligation rules as guest tests.
  The `eqc`, `geqc` and `squashc` cases of `fold` and `elim` are checker
  obligations (HIT_CASE, ELIM_CASE; SPEC.md 2.6 and 2.7). A wrong path case
  in a circuit file is refused before the lowering (A5, A7). No code
  change.
- D2. The deep form: a circuit syntax family `Circ` in domain/domain.lang
  (rule R1), with the constructors `cpub`, `cpriv`, `clit (clv : Field)`,
  `cadd`, `cmul` and `cpow (cpb : Circ) (cpn : Nat)`. The names are free
  (rg over examples, test, domain and src at c8c1d5e). The interpreter
  `ev y x c` is a guest `elim (fun d => Field)`; `cpow` uses the M6 Nat
  fold. In M7 the syntax has one public binder and one private binder (the
  shape of square). Question (h) asks about an indexed form.
- D3. The circuit quotient for the lowering is `GQuot Circ CRel`, with the
  pointwise relation `CRel c1 c2 = (y : Field) -> (x : Field) -> Path Field
  (ev y x c1) (ev y x c2)`. The interpreter out of the quotient, `evQ y x q
  = fold Field (fun c => ev y x c) (fun c1 => fun c2 => fun r => r y x) q`,
  needs a Field path in its `geqc` case, and `CRel` gives it.
- D4. The DESIGN relation `CEqv c1 c2 = (y : Field) -> (x : Field) -> Equiv
  (Sat c1 y x) (Sat c2 y x)`, with `Sat c y x = Path Field (ev y x c) y`, is
  in `Type 0`, and GQuot accepts it (D0). Its fold into `Type 0` with the
  case `ua (e y x)` checks (`SatQ`). It does not give a fold into Field: an
  `Equiv` of the two satisfaction types does not give `Path Field (ev y x
  c1) (ev y x c2)`, because `y` is also an input of `ev`. Thus `GQuot Circ
  CEqv` carries properties only. The new open item O19 records this gap.
- D5. No new lowering code and no IR change. Each quotient value in a
  constraint reduces before the lowering: the point rule, the boundary
  rule at a constant interval, and the `hcomp` rule of the eliminator, then
  the closed-type `hcomp` rule at Field (A1 to A4, D1, D2, D5, D6). The
  lowering sees only literals, binders and `+ - *`, as in M1.
- D6. "A proof stays a term": a large proof term in a path case gives the
  same IR (A8). The lowering reads the point case at a representative; a
  path case has no wire.
- D7. Circuits keep GQuot. A `squashc` case needs a square in Field; there
  is no core isSet Field (A7, HIT_CASE), and a binder of that type is not a
  Field binder (A6, CIRCUIT_BINDER). A conv rule that makes two Field paths
  equal reads the type (proof irrelevance, ruling 2026-10-14), and an axiom
  gives REFUSE_AXIOM (rule R2). quotRec keeps its isSet argument.
- D8. No Field ring normal form. `x * x + 0` and `x * (x * 1)` do not
  convert with `x * x` (D3, D4: TYPE_REFL). Thus `CRel` at two syntax terms
  with different normal forms needs a proof term from the program. M7 adds
  no ring rule (question (i)).
- D9. The transport smoke example: `opt : CRel (cmul cpriv cpriv) (cpow
  cpriv 1)` is `refl`, because both terms normalize to `fieldMul x x`. The
  two terms are different representatives (different constructors), thus
  `geqc .. opt` is a path between two points of the quotient. `moveSat`
  moves the satisfaction proof of `x * x == y` to `cpow cpriv 1` (D7). The
  line is a def (`SLine`), because a term `<i> ..` in an argument needs a
  declared type (PATH_EXPECTED).
- D10. Kept: D3 of M4 (`hcomp` at a HIT is a canonical value; a HIT is
  never a closed type), D8 of M3 (no rule `hcomp A i0 u u0 = u0`), D13 of
  M6 (the constrained monad is the current lowering). No conv rule reads a
  type.
- D11. The M6 open items in M7:
  - O17 closes: examples/hintTrap.glue (the circuit of the staged review
    corrections) and the row `1 WITNESS_HINT witness hintTrap hintTrap x=0`
    in test/circuit/refuse.txt.
  - O3 stays open: a semantic check needs a rank or solver argument,
    outside Direction 2.
  - O4 (closed in M6): the record text "O4 stays open for M7" gets the M7
    result (no quotient value gets to the lowering stuck; A1 to A4).
  - O12 (c) stays open: `transp` of `squashc` stays stuck; circuits use
    GQuot (D7).
  - O13 and O14 stay open: Direction 2 needs no equation in a mutual block
    and no F6 uniqueness at a HIT.
  - O18: a USER confirmation only (question (f)).
  - O19 (new): D4.

## 3. Core changes

- Syntax: none.
- Typing rules: none.
- Evaluator rules: none.
- Kan rules: none.
- Conversion: none.
- Printer and JSON: none.
- Domain: domain/domain.lang gets the family `Circ` (D2; slice s2). The new
  constructor and field names become domain names (a `def` with one of them
  gives REFUSE_NAME; s2 adds a row).
- IR: none. The IR and the verifier of each current circuit stay
  byte-identical (the IR check of the M7 COMMON file).

## 4. Derived operations and their laws

Each law is a `refl` in a guest example (the slice is in brackets).

| Law | Statement | Holds by |
|---|---|---|
| evPoint | `Path Field (evQ y x (gcls Circ CRel c)) (ev y x c)` | refl, point rule (s3) |
| evEnd0, evEnd1 | `Path Field (evQ y x (geqc Circ CRel c1 c2 r @ i0)) (ev y x c1)`, and `@ i1` with `c2` | refl, boundary rule (s3) |
| evPath | `Path (Path Field (ev y x c1) (ev y x c2)) (<i> evQ y x (geqc Circ CRel c1 c2 r @ i)) (r y x)` | refl, path case (s3) |
| evHcomp | `Path Field (sq (hcomp (GQuot Field SqRel) i0 (fun i => [ i0 -> gcls Field SqRel x ]) (gcls Field SqRel x))) (x * x)` | refl, the eliminator `hcomp` rule, then the closed-type rule at Field (s1; A3) |
| optRefl | `opt : CRel (cmul cpriv cpriv) (cpow cpriv 1)` | refl (s4; D5) |
| moveSatRefl | `Path (Path Field 9 9) (moveSat 9 3 refl) refl` | refl (s4; D7) |
| satQFwd | `transport (QLine c1 c2 e y x) p` is the forward map of `e y x` (`QLine` a def `<i> SatQ y x (geqc Circ CEqv c1 c2 e @ i)`) | refl by uaβ, as transportPropFwd (s3) |

## 5. Effect on examples

No current example or golden changes. New files (one circuit `square` for
each same-ir program, because the gate keys the goldens by the circuit
name):

| File | Slice | Content |
|---|---|---|
| examples/quotEnd0.glue, quotEnd1.glue | s1 | A1, A2 |
| examples/quotHcomp.glue, quotElimHcomp.glue | s1 | A3, A4 |
| examples/quotProof.glue | s1 | A8 (a proof term in the `geqc` case) |
| examples/circSyntax.glue | s2 | `ev`, D1 |
| examples/circRel.lang | s3 | `CRel`, `evQ`, `CEqv`, `SatQ`, `SatP`, the s3 laws of section 4 |
| examples/circOpt.glue, circPow.glue | s4 | D5, D6 |
| examples/circTransport.lang | s4 | `opt`, `SLine`, `moveSat`, `moveSatRefl` |
| examples/hintTrap.glue | s5 | the O17 circuit |

## 6. Circuits

The lowering runs on representatives (D5): each new circuit through a
quotient normalizes to the normal form of `x * x == y`. New same-ir rows,
each against the square golden: quotEnd0, quotEnd1, quotHcomp,
quotElimHcomp, quotProof (s1); circSyntax (s2); circOpt, circPow (s4). The
IR of each current circuit does not change (the 8 current same-ir rows and
the 5 expect rows stay). expect.txt gets no row: the gate keys a golden by
the circuit name, and each new circuit is `square`. refuse.txt gets one
WITNESS_FAIL row through the quotient (s4: `witness circOpt square y=10
x=3`) and the O17 row (s5).

## 7. New check codes

None. Existing codes with new guest rows:

| Code | Exit | Refusal | Slice |
|---|---|---|---|
| HIT_CASE | 1 | a wrong `geqc` case in a circuit file (A5); a `squashc` case into Field (A7) | s1 |
| ELIM_CASE | 1 | a wrong `geqc` case of `elim` in a circuit file | s1 |
| CIRCUIT_BINDER | 1 | a binder `(s : isSet Field)` (A6) | s1 |
| REFUSE_NAME | 1 | `def cpow` (a domain name) | s2 |
| TYPE_REFL | 1 | `refl : CRel (cmul cpriv cpriv) (cadd (cmul cpriv cpriv) (clit 0))` (D3) | s3 |
| WITNESS_HINT | 1 | `witness hintTrap hintTrap x=0` (O17) | s5 |

## 8. Guest tests per slice

- s1: 5 examples, 5 same-ir rows, 4 check refusal files
  (test/check/geqc-case.glue, elim-geqc-case.glue, squashc-field.glue,
  isset-binder.glue), and an eval row for evHcomp.
- s2: the domain family `Circ`; examples/circSyntax.glue; 1 same-ir row; 1
  check refusal (test/check/domain-cpow.lang, REFUSE_NAME); eval rows for
  `ev` at each constructor (closed binders).
- s3: examples/circRel.lang (evPoint, evEnd0, evEnd1, evPath and satQFwd
  as refl defs); 1 check refusal (test/check/ring-refl.lang, TYPE_REFL).
- s4: examples/circOpt.glue, circPow.glue and circTransport.lang; 2 same-ir
  rows; 1 refuse row (WITNESS_FAIL); the eval row `moveSat 9 3 refl` (`<j>
  9`).
- s5: examples/hintTrap.glue; the O17 refuse row; SPEC.md 9 (the O4
  record, O17 closed, O19 new).
- Mutants (each slice prompt lists 2 or 3): s1 drops the path-case
  conversion in fold_hit_rule (the A5 row kills it); s2 swaps the `cadd` and
  `cmul` cases in a scratch copy of the example (the same-ir row kills it);
  s4 changes `cpn` to 2 in circOpt (IR DIFF kills it).

## 9. Questions for the USER

The USER ruled each question on 2026-10-18: each recommendation. The D
calls already use the recommendations, so no D call changes. Each
recommendation was checked against the sources and the probes of section 1.

- (a) The form of a circuit relation. (i) Shallow: the current `circuit`
  form with a quotient value in a constraint; M7 adds the obligation tests
  only. (ii) Deep: the syntax family `Circ` (D2), the interpreter, the
  quotient `GQuot Circ CRel`, and a `circuit` whose constraint is the
  interpreter at a representative. Recommended: (ii), with the (i) tests in
  s1. Reasons: D0, D1, D2, D5 and D6 show that the family declares, that
  the interpreter at a closed representative gives the IR of square, and
  that no lowering code is necessary. DESIGN Direction 2 quotients
  circuits, not values; (i) adds nothing that quotSq does not show.
  RULED 2026-10-18 (USER):  the recommendation ((ii), the deep form, with the (i) tests in s1).
- (b) The relation. Recommended: the pointwise `CRel` for the circuit
  quotient (D3), and the DESIGN `CEqv` as a second quotient that carries
  properties only (D4, `SatQ` with `ua`), with the gap as O19. Reasons:
  GQuot accepts `CEqv` at `Type 0` (D0), but the interpreter out of the
  quotient needs a Field path in its `geqc` case, and `CEqv` does not give
  it. The kickoff prompt recommended the DESIGN form if GQuot accepts it;
  the probe shows that GQuot accepts it, but it cannot be the lowering
  quotient.
  RULED 2026-10-18 (USER):  the recommendation (the pointwise `CRel` for the lowering, `CEqv` for properties, O19).
- (c) The `squash/` case. Recommended: keep GQuot for circuits; a
  `squashc` case stays a checker obligation at a target with an isSet
  argument (quotRec); no core isSet Field (D7). Reasons: A6, A7; a conv
  rule for Field paths reads the type (ruling 2026-10-14); an axiom gives
  REFUSE_AXIOM. O12 (c) stays open.
  RULED 2026-10-18 (USER):  the recommendation (GQuot for circuits, no core isSet Field).
- (d) Lowering code. Recommended: no new lowering code and no IR change
  (D5). Reasons: each probe through a quotient gives IR SAME. Each new
  circuit through a quotient gets a same-ir row (section 6).
  RULED 2026-10-18 (USER):  the recommendation (no lowering code, no IR change).
- (e) The transport smoke example. Recommended: yes, as D9: `cmul cpriv
  cpriv` and `cpow cpriv 1` (the M6 Nat fold), `opt` by refl, `moveSat`
  through the `SLine` def, and both circuits give the IR of square (D5, D6,
  D7). Reason: the kickoff form `x * x` against `pow x 2` needs a ring
  proof (D8); `cpow cpriv 1` has the same normal form as `x * x`.
  RULED 2026-10-18 (USER):  the recommendation (`cmul cpriv cpriv` and `cpow cpriv 1`).
- (f) The M6 open items (D11). Recommended: O17 closes with an automated
  WITNESS_HINT row (s5); O3, O12 (c), O13 and O14 stay open; O18 is a USER
  confirmation that D10 of M6 covers a HIT family with closed parameters
  (`Susp Nat`). Reasons: D11.
  RULED 2026-10-18 (USER):  the recommendation (O17 closes in s5;  O18 confirmed).
- (g) Slices. Recommended: s1 to s6 of section 10. Reason: each slice is
  one session and has its own gate rows.
  RULED 2026-10-18 (USER):  the recommendation (slices s1 to s6).
- (h) The binders of `Circ`. Recommended: M7 keeps `cpub` and `cpriv` (one
  public binder and one private binder, the square shape); an indexed
  binder form (`cvar (n : Nat)` with an environment) is a new open item for
  M8. Reasons: each M7 same-ir row is against square; poly has two public
  binders and needs the indexed form.
  RULED 2026-10-18 (USER):  the recommendation (`cpub` and `cpriv`;  the indexed form is an M8 open item).
- (i) Ring laws. Recommended: no ring conv rule in M7 (D8); a relation
  between two different normal forms is a proof term. Reasons: a ring
  normal form is a new core rule that touches each Field term; M7 does not
  need it for the smoke example.
  RULED 2026-10-18 (USER):  the recommendation (no ring conv rule in M7).

## 10. Slices (one session each)

| Slice | Content | Gate delta (from the M6 close) |
|---|---|---|
| s1 | The shallow obligation rules: A1 to A4 and A8 as examples with same-ir rows; the A5, A6, A7 and `elim` refusal files; SPEC.md 2.9 (new section: Direction 2, M7) starts | examples +5, same-ir +5, check refusals +4, evals +1 |
| s2 | The `Circ` family in domain/domain.lang; `ev`; circSyntax.glue; the REFUSE_NAME row | examples +1, same-ir +1, check refusals +1, evals +6 |
| s3 | `CRel`, `evQ`, `CEqv`, `SatQ`, `SatP`; the s3 laws of section 4; the TYPE_REFL row; O19 | examples +1, check refusals +1 |
| s4 | `cpow`, `opt`, circOpt.glue, circPow.glue, circTransport.lang; the WITNESS_FAIL row | examples +3, same-ir +2, circuit refusals +1, evals +1 |
| s5 | O17: hintTrap.glue and the WITNESS_HINT row; SPEC.md 9 (the O4 record, O17, O19); the s1 to s4 mutant review | examples +1, circuit refusals +1 |
| s6 | Close: STATUS `### s6: close`, the PLAN M7 row DONE, SPEC.md 4 and 9, the mutant roll-up, the M8 kickoff prompt | none |

Each slice runs the gates of ~/Documents/glue-lang-M7-COMMON.md and the IR
check after `make e2e` (e2e stays at 5 rows, forge test 44 passed).
