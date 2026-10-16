# M5 plan: quotient inductive types

Status: RULED (2026-10-16). The USER accepted the recommendation of each
section 9 question (a to f). M4 is the s6 close commit 5e76e43. No code
changes in the kickoff. The plan goes into the s1 commit.

## 1. Sources

| Source | What M5 takes from it |
|---|---|
| docs/PLAN.md, M5 row | QITs (quotient inductive types, with inductive-inductive definitions). |
| docs/PLAN.md, M7 row | Direction 2 on circuits: the lowering of `rec` runs on representatives; `eq/` and `squash/` are checker obligations with no circuit cost. M5 does not lower (question a). |
| docs/PLAN.md, Rulings and Lowering rules | No proof irrelevance: no `Prop`, truncations are HITs. The IR does not change. `Quotient eliminator \| Runs on representatives`. |
| SPEC.md 2.6 | The HIT forms: path constructors with binders and a boundary system, OP_PCON, the boundary rule, `fold B cases x` (OP_FOLD_HIT, `B` in the core), the Kan rules at families, D3 (`hcomp` at a HIT is canonical), D8 (no rule `hcomp A i0 u u0 = u0`), conversion argument by argument, HIT_BOUNDARY, HIT_CASE, HIT_CASES. |
| SPEC.md 9 | O4 (no CIRCUIT_LOWER row), O8 (stuck Kan values at data types), O12 (a) to (f) (the stuck values and gaps of M4). |
| docs/STATUS.md `## M4` | s4: dependent fields, Path fields, `field_line_transp` gives NULL at a dependent field or a Path field (O12 (d)); quotRecOne abstracts over `isSet Nat`. s5: the gap (the retraction `(x : OTPSpec) -> Path OTPSpec (g (f x)) x` needs a dependent eliminator); GQuot (an `hfill` square needs the square as its base); 5 probed constraints lower. s6: the 16-row mutant roll-up, the review checklist, the open items. |
| docs/M4-PLAN.md 2 and 3.4 | D1 to D12; the rule order (face `i1`, head former, else stuck); the eliminator rules (point, path, `hcomp`). |
| design/DESIGN.md 60-116 | Direction 2: `Circuit/∼` with `[_]`, `eq/`, `squash/`; `rec : isSet B -> (f : Circuit -> B) -> resp -> Circuit/∼ -> B`; `transportProp e p = transport (cong P (eq/ e)) p`. Line 116: the minimal operations are `[_]`, `eq/`, `rec` and transport. |
| design/DESIGN.md 118-196 | Direction 4: OTPSpec, `interp`, `spec≃impl = isoToEquiv (iso interp interp⁻¹ sec ret)`, `ua`, uaβ. `sec` and `ret` are postulates in the design. |
| design/DESIGN.md 246, 325 | PrivateTransfer and ZKKyc: Direction 2 quotients the circuit space by relation equivalence, and properties transport to each equivalent circuit. Direction 4 states the laws on a HIT spec. |
| Altenkirch and Kaposi (QIITs, the syntax of type theory); CCHM 6.3 | A QIT is a HIT whose path constructors are equations between point terms, often with a set truncation. An inductive-inductive definition declares a family and a second family indexed by it together. The dependent eliminator on an `hcomp` value is `comp` along `P (hfill ...)`. |

Code anchors (M4 code, 5e76e43; the slices find the lines again with `rg -n`):

| Fact | Where |
|---|---|
| Family grammar: `'family' NAME group* ':=' ctor ('\|' ctor)*`, `ctor := NAME group* ('<' NAME+ '>' system)?` | src/front/parser.c lines 4-7, parse_family (552), parse_path_ctor (534) |
| Family check: params, then each ctor (fields, then the boundary) | check.c check_family (3254), check_ctor (3215), check_boundary (3203), boundary_core (3160: each face of each binder) |
| Field rule: `recursive` 0/1/2, `dependent`, `ends[2]`; DOMAIN_FAMILY | check.c check_field (3109; errors at 3142, 3147), self_applied (3094) |
| No forward reference: a later name gives REFUSE_REC (rule R3) | check.c find_known (1691), 1730-1736 |
| Constructor use: the params, then the fields | check.c ctor_app (1631, type at 1656), pcon_app (1594), pcon_end (1570) |
| HIT fold: case types, folded ends, recode | check.c fold_hit_rule (959), hit_case_type (901), hit_case_end (845; INTERNAL for a constructor declared after it), case_field_type (878), recode_case (942), hit_case_eta (1215), rule_fold (999), fold_family (813) |
| `isSet` | check.c rule_is_set (2775), core_is_set (2654) |
| Reserved names, the builtin rows | check.c RESERVED (190-192), BUILTINS (RULE_FOLD 143), name_owner (3049) |
| Evaluator: boundary rule, HIT fold | eval.c pcon_boundary (1200), reduce_pcon (1218), fold_hit (795), eval_hit_case (737), eval_fold_hit_boundary (893), fold_boundary_path (852), stuck_fold_hit (778), papp_value (1514), reduce_sys (1181), OP_FOLD_HIT dispatch (1386) |
| Kan rules at families | kan.c kan_hcomp (1351; HIT guard at 1393), closed_type (272), plain_family (283), hit_type (287), transp_ctor (927), transp_pcon_agrees (889), transp_hcomp (967), kan_fold_hcomp (991), kan_fold_path_field (868), field_line_transp (838: NULL at a dependent field or a Path field), hcomp_ctor (1299) |
| Core structs and ops | src/front/core.h FieldInfo (174-180), CtorInfo (182-193), FamilyInfo (197-205), OP_PCON (42-44), OP_FOLD_FAMILY (54), OP_FOLD_HIT (55-57) |
| Keywords | src/front/lexer.c 21-31 (no `mutual`, `and` or `with` token) |

## 2. Design calls

Section 9 has the questions and the USER rulings (2026-10-16: each
recommendation). The calls below agree with the rulings.

- D1. A QIT is a HIT (a `family` in domain/domain.lang) whose path
  constructors are equations: one binder, and the two ends are point terms
  (a field, or a point constructor applied to the parameters and to point
  terms). A QIT can also have a set truncation constructor (the `squashc`
  form of Quot). M5 adds no syntax and no new core flag (question c): the
  M4 path constructor is the equation form. Example: `bswap (x : A) (y : A)
  (xs : Bag A) <i> [ ~i -> cons A x (cons A y xs), i -> cons A y (cons A x
  xs) ]`.
- D2. Constructor equations with recursive ends (s1). The ends of `bswap`
  hold a recursive field under two constructors. M4 folds a nested
  constructor end (eval.c eval_fold_hit_boundary, check.c hit_case_end), but
  no M4 row tests a recursive field under a constructor in a boundary. s1
  probes this first and fixes what fails. Hypothesis to test: the `transp`
  boundary check (transp_pcon_agrees) holds for each equation, because
  `transp` commutes with each point constructor of an end. If a probe
  refutes it, the row stays stuck (O12 (c)) and s1 records it.
- D3. The dependent eliminator `elim P cases x` (s2, question b). `P : (x :
  D params) -> Type n` is explicit. One case for each constructor in
  declaration order, the scrutinee last (as `fold`). A case takes each
  field; after each recursive field it takes the induction hypothesis (IH):
  `P f` for a field `f : Self`, `PathP (<j> P (p @ j)) ha hb` for a field
  `p : Path Self a b`, where `ha` and `hb` are `elim P cases` of the ends.
  A point case gives `P (c params fields)`. A path case with k binders
  gives `PathP (<i1> .. PathP (<ik> P (pc params fields @ i1 .. @ ik)) ..)`
  with the ends `elim P cases t` of the boundary bodies `t` (the recode of
  hit_case_end, with the IHs in place of the recursive results). `elim` is
  at each family, plain and HIT. It is a new core name (REFUSE_NAME). The
  core op is OP_ELIM (inst = the family; args = `P`, the cases, then the
  scrutinee). `fold` stays (D6 of M4); `elim` does not replace it.
- D4. The `elim` rules (s2): a point constructor gives its case applied to
  the fields and the IHs; `pc fields @ r1 .. @ rk` gives the path case
  applied, then `@ r1 .. @ rk`; an `hcomp` value at a HIT gives
  `comp (<i> P (hfill D phi u u0 i)) phi (fun i => [ phi -> elim P cases (u i) ]) (elim P cases u0)`,
  with `hfill D phi u u0 i = hcomp D (phi \/ ~i) (fun j => [ phi -> u (i /\ j), ~i -> u0 ]) u0`;
  a neutral scrutinee is stuck. `comp` is the M3 composition from `transp`
  and `hcomp` (kan.c gcomp, D8 of M3); s2 confirms its arity with `rg`. The
  `hcomp` rule is a new kan.c template next to kan_fold_hcomp (D11 of M4).
  At a plain family there is no `hcomp` rule: the data rule of M3 gives a
  constructor or a stuck value.
- D5. O12 (d) (s3, question e). `transp` at a constructor with a dependent
  field uses the Sigma method of M3: field k moves along its field type at
  the line, where each earlier field is replaced by its transport fill
  `transp (<j> T (P (i /\ j))) (phi \/ ~i) f`. A Path field moves by the M3
  PathP rule (its ends are the transported ends). `hcomp` at a plain family
  with a dependent field uses the `comp` method of hcomp_pair. Thus
  `transp` of `eqc` along a line of Quot parameters reduces. The inductive-
  inductive block needs this (D7: `ext g t` has `t : Ty g`).
- D6. The inductive-inductive block (s4, s5, question d). Syntax: the new
  keyword `mutual`, then exactly two `family` declarations: `mutual family
  Ctx := cempty | ext (g : Ctx) (t : Ty g) family Ty (g : Ctx) := tbase |
  arrow (a : Ty g) (b : Ty g)`. The rules: (1) the last parameter of the
  second family (the index) has the type of the first family at its
  parameters; (2) the checker registers both names before it checks a
  constructor, so the block is the one exception to rule R3; (3) a field of
  the first family can have the type of the second family at an earlier
  field of the first family (a partner field, `recursive` 3); (4) a
  recursive field of the second family is the second family at its own
  parameters (uniform, as Self today); (5) a block is in domain/domain.lang
  only (rule R1); (6) M5 blocks have point constructors only. Equations in
  a block and non-uniform indices (`Ty (ext g a)`) are a new open item
  (O13).
- D7. The fold of a block (s5): `fold MA MB casesA casesB x`. `MA : Type
  n`, `MB : MA -> Type n`. One case for each constructor of the block in
  declaration order. A first-family case: a recursive field gets `MA`, a
  partner field `t : Ty g` gets `MB rg` (`rg` is the result of `g`). A
  second-family case takes the index result `rg : MA` first, then its
  fields (a recursive field gets `MB rg`), and gives `MB rg`. The result is
  `MA` for `x` in the first family and `MB (fold MA MB cases g)` for `x :
  Ty g`. No `elim` at a block in M5 (O13). The op is OP_FOLD_HIT with the
  two motives (or a new OP_FOLD_BLOCK if the arity test needs it; s5
  decides and records it).
- D8. Direction 2 in the checker and the evaluator (s6, question a). M5
  states the forms and computes them: `fold` and `elim` on a
  representative (`cls a`) reduce by the point rule; `transportProp` is a
  guest definition: `transport (<i> P (geqc A R a b r @ i)) p`, where `P`
  is a `fold` into a universe with a `ua` path case, reduces by uaβ to the
  forward map. The quotient is GQuot for a fold into a universe (`Type 0`
  is not a set, so Quot cannot have a `squashc` case there). M5 adds no
  lowering code. M7 owns the circuit lowering.
- D9. Kept from M4: D3 (`hcomp` at a HIT is canonical; a HIT is never a
  closed type), D8 of M3 (no rule `hcomp A i0 u u0 = u0`), conversion
  argument by argument. No conv rule reads a type (USER ruling 2026-10-14,
  no proof irrelevance). No IR change.
- D10. O12 (a) and (b) are not gaps: a neutral scrutinee and `transp` at a
  family with a neutral argument are neutral values, as `fold` at Nat with
  a variable. SPEC moves them from O12 to the rule table (s7). (c) stays
  open (no general boundary correction terms); D2 records which equations
  it touches. (e) closes with `elim` (s2). (f) O4 stays open for M7.
- D11. New names: `elim` (core name, s2), `mutual` (keyword, s4). The smoke
  families are ASCII: Bag (`bnil`, `bcons`, `bswap`), Int (`pos`, `neg`,
  `zeq`), Ctx (`cempty`, `ext`) and Ty (`tbase`, `arrow`). s1 and s4 confirm
  with `rg -w` that no example or test uses these names. The kickoff `rg -w`
  (2026-10-16) found 4 names in use, so the plan has new names:
  `swap` (a def in examples/iso.lang and examples/squareUa.glue) is now
  `bswap`; `size` (a def in examples/formers.lang, the Lot parameter) is now
  `bsize`; `empty` (a def in examples/formers.lang and test/regressions.js)
  is now `cempty`; `base` (the Circle constructor) is now `tbase` in Ty.
- D12. One commit for each slice. The plan is in the s1 commit.

## 3. Core changes

### 3.1 Syntax

| Form | Meaning | Slice |
|---|---|---|
| `pc fields <i> [ ~i -> t0, i -> t1 ]`, `t0` and `t1` point terms | A constructor equation (D1). The M4 form, no grammar change | s1 |
| `elim P cases x` | The dependent eliminator (D3). `P` explicit, one case for each constructor, the scrutinee last | s2 |
| `mutual family A ... family B (x : A) ...` | An inductive-inductive block (D6). `mutual` is a new keyword | s4 |
| `fold MA MB cases x` at a block | The fold of a block (D7) | s5 |

The grammar change (s4): `decl := 'mutual' family family | ...`, where
`family` is the M4 family form. No other new token.

### 3.2 Typing rules

| Rule | Premises | Conclusion | Slice |
|---|---|---|---|
| equation (declaration) | the M4 path constructor rule; the ends can hold recursive fields under constructors | as M4 | s1 |
| elim | `x : D params`; `P : (y : D params) -> Type n`; one case for each constructor (ELIM_CASES); a point case has the type of D3; a path case has the PathP type of D3 (ELIM_CASE); on each boundary branch `phi -> t` the case at the binders converts to `elim P cases t` (ELIM_CASE) | `elim P cases x : P x` | s2 |
| block (declaration) | two families after `mutual` (MUTUAL_BLOCK); the index rule (1) of D6 (MUTUAL_BLOCK); both names known before the constructors; a partner field `t : B .. g` needs an earlier field `g` of the first family (DOMAIN_FAMILY); the M4 field rule otherwise | two families with `partner` set | s4 |
| block (use) | as ctor_app; a second-family constructor takes the parameters, the index, then the fields | `c params g fields : B params g` | s4 |
| block fold | `MA : Type n`; `MB : MA -> Type n`; one case for each constructor of the block (HIT_CASES); the case types of D7 | `MA` or `MB (fold MA MB cases g)` | s5 |

### 3.3 Evaluator rules

Order (as M4): (1) the face `i1` rules, (2) the rule of the head former or
the head value, (3) else stuck.

| Redex | Result | Slice |
|---|---|---|
| `fold B cases (pc fields @ r)`, `pc` an equation with recursive ends | as M4 (the path case applied, then `@ r`); the case type ends fold the nested constructors | s1 |
| `transp (<i> D (P i)) phi (pc fields @ r)`, `pc` an equation | as M4: fieldwise, if transp_pcon_agrees holds (D2) | s1 |
| `elim P cases (c fields)` | the case of `c` applied to the fields and the IHs (`elim P cases f`; `<j> elim P cases (p @ j)` for a Path field) | s2 |
| `elim P cases (pc fields @ r1 .. @ rk)` | the path case applied to the fields and the IHs, then `@ r1 .. @ rk` | s2 |
| `elim P cases (hcomp D phi u u0)`, D a HIT | `comp (<i> P (hfill D phi u u0 i)) phi (fun i => [ phi -> elim P cases (u i) ]) (elim P cases u0)` (D4) | s2 |
| `elim P cases x`, `x` neutral | stuck | s2 |
| `transp (<i> D (P i)) phi (c fields)`, a dependent field | the Sigma method of D5 | s3 |
| `transp` at a Path field | the PathP rule of M3 on that field (D5) | s3 |
| `hcomp` at a plain family, a dependent field | the `comp` method of hcomp_pair (D5) | s3 |
| `transp (<i> B (P i)) phi (c g fields)`, a block | as a family with parameters; the index is a parameter | s4 |
| `fold MA MB cases (c fields)`, first family | the case applied; a recursive field gets `fold .. f`; a partner field `t` gets `fold .. t` | s5 |
| `fold MA MB cases (c g fields)`, second family | the case applied to `fold .. g`, then the fields (recursive as above) | s5 |

### 3.4 Conversion

- OP_ELIM: argument by argument. The block fold: argument by argument.
- No rule makes two values equal because of their type. ELIM_CASE reads
  the boundary of a constructor (a declaration), not the type of a value.
  A guest row guards this in s2 (two variables of Int stay different).

### 3.5 Printer and JSON

- The printer writes `elim P cases x` and the block fold as `fold MA MB
  cases x`.
- JSON: a point constructor of Bag, Int, Ctx or Ty writes the family
  constructor form (the M4 rule). A stuck `elim` value in an instance gives
  JSON_VALUE (the M3 rule for a stuck value). A path constructor value
  gives JSON_VALUE (D9 of M4). No golden changes.

## 4. Derived operations and their laws

No new derived operation. `elim` is an eliminator, not a derived
operation. The laws are guest definitions; each law below is a `refl` row
that holds by computation.

| Law | Why it computes | Slice |
|---|---|---|
| `(bswap A x y xs) @ i0 = bcons A x (bcons A y xs)` | the boundary rule | s1 |
| `bsize (bcons A x (bcons A y xs)) = succ (succ (bsize xs))` | the point rule (bsize := `fold Nat 0 (fun x => fun n => succ n) (fun x => fun y => fun n => refl)`) | s1 |
| `<i> bsize (bswap A x y xs @ i) = <_> succ (succ (bsize xs))` | the path rule; the ends fold the nested constructors (D2) | s1 |
| `negate (pos n) = neg n`, `negate (neg n) = pos n`, `<i> negate (zeq @ i) = <i> zeq @ ~i` | the point and path rules (negate := `fold Int (fun n => neg n) (fun n => pos n) (<i> zeq @ ~i)`) | s1 |
| `transp (<i> Bag (L i)) i0 (bcons (L i0) a xs) = bcons (L i1) (transp (<i> L i) i0 a) (transp (<i> Bag (L i)) i0 xs)` | the constructor rule | s1 |
| `elim P b l base = b`; `<i> elim P b l (loop @ i) = l` at PathP | the point and path rules | s2 |
| `negNeg : (x : Int) -> Path Int (negate (negate x)) x := fun x => elim (fun y => Path Int (negate (negate y)) y) (fun n => refl) (fun n => refl) (<i> <j> zeq @ i) x` | `negate (negate (zeq @ i)) = zeq @ ~~i = zeq @ i` (De Morgan) | s2 |
| `ret : (x : OTPSpec) -> Path OTPSpec (flip (flip x)) x` by `elim` with the path case `<i> <j> encrypt @ i`, where `flip := fold OTPSpec cipher message (<i> encrypt @ ~i)` | the path rule; `~~i = i` | s2 |
| `transport (ua flipEquiv) message = cipher`, `flipEquiv := isoToEquiv OTPSpec OTPSpec flip flip ret ret` | uaβ (D8 of M3); closes the s5 gap for a non-identity iso | s2 |
| `transp (<i> Quot (A i) (R i)) i0 (eqc .. a b r @ k)` reduces to `eqc` of the transported fields | the Sigma method (D5) | s3 |
| `length (ext cempty (tbase cempty)) = 1` with `length := fold Nat (fun n => Nat) 0 (fun rg => fun rt => succ rg) (fun rg => 0) (fun rg => fun ra => fun rb => 0)` | the block fold (D7) | s5 |
| `transportProp`: `transport (<i> P (geqc A R a b r @ i)) p = fwd p` | the path rule, then uaβ (D8) | s6 |

## 5. Effect on examples

| Item | M5 |
|---|---|
| domain/domain.lang | s1: `family Bag (A : Type 0) := bnil \| bcons (bx : A) (bxs : Bag A) \| bswap (sx : A) (sy : A) (sxs : Bag A) <i> [ ~i -> bcons A sx (bcons A sy sxs), i -> bcons A sy (bcons A sx sxs) ]` and `family Int := pos (pn : Nat) \| neg (nn : Nat) \| zeq <i> [ ~i -> pos 0, i -> neg 0 ]`. s4: the block `mutual family Ctx := cempty \| ext (eg : Ctx) (et : Ty eg) family Ty (tg : Ctx) := tbase \| arrow (ta : Ty tg) (tb : Ty tg)`. A field name is a core name, so the fields have distinct names. The existing rows (Color, Item, Stack, Lot, Circle, Susp, Trunc, Quot, OTPSpec, GQuot) and their eval rows do not change. |
| New examples | examples/bag.lang, examples/int.lang (s1, s2), examples/ctx.lang (s4, s5), examples/transportProp.lang (s6). elim rows in examples/circle.lang and examples/otp.lang (s2). A .glue example in s6 if a circuit through a QIT gives a known IR (section 6). |
| examples/otp.lang | s2: `flip`, `ret`, `flipEquiv`, the `transport (ua flipEquiv) message = cipher` row; the comment at line 45 (no elim in M4) changes. |
| SPEC.md | 2.7 (new, M5): equations, `elim`, the block, the codes. 9: O12 (a), (b) to the rules, (d), (e) closed, (c) narrowed, O13 new. s7 writes it. |

## 6. Circuits

The IR must not change. The gate goldens in test/circuit stay byte for
byte the same, and `make e2e` gives the same verifiers. M5 adds no
lowering code (D8, question a). A constraint side must normalize to a Field
normal form (SPEC 7.1); `fold` and `elim` on a representative reduce by
the point rule.

Guest test (s6): a .glue circuit through Bag or Int whose constraint
normalizes to the constraint of an existing circuit, with a same-ir row (as
quotSq in M4). The circuit name keys the goldens (test/gate.sh), so the
row reuses the goldens of the existing circuit. Probe first: is `Path Field
(x + (y + s)) (y + (x + s))` a `refl`? If not, the bswap case of a Field
sum has no term, and s6 uses GQuot or Int instead. s6 also probes
CIRCUIT_LOWER (O4) with a stuck `elim` term in a constraint.

## 7. New check codes

| Code | Exit | The compiler refuses | Slice |
|---|---|---|---|
| ELIM_CASES | 1 | `elim` with the wrong number of cases, or a point case whose type does not match (the message gives the constructor and the expected type) | s2 |
| ELIM_CASE | 1 | `elim` where a path case has the wrong type or does not convert to `elim P cases t` on a boundary branch `phi -> t` (the message gives the constructor and the expected type) | s2 |
| MUTUAL_BLOCK | 1 | A `mutual` block that does not hold two families, or whose second family has no last parameter of the first family's type (the message gives the cause) | s4 |

Existing codes with new causes: REFUSE_NAME (`elim`, s2), TYPE_MISMATCH
(a motive that is not a family of types over D, s2), DOMAIN_FAMILY (a
partner field not at an earlier field of the first family; a second-family
field at another index, s4), REFUSE_DATA (a block in a program, s4),
PARSE_EXPECT (`mutual` with no family, s4), HIT_CASES (a block fold with
the wrong number of cases, s5), JSON_VALUE (a stuck `elim` in an
instance, s2).

## 8. Guest tests per slice

All tests are glue-lang programs and gate tables. No new node, Python or
shell harness. The native C harnesses (test/*-regressions.c) can get rows.
3 mutants for each slice, `make -B` for each rebuild.

| Slice | Tests |
|---|---|
| s1 | examples/bag.lang and examples/int.lang: the s1 laws of section 4; `bsize` and `negate` eval rows; JSON goldens (a Bag value, an Int value). Check rows: HIT_BOUNDARY (bswap with one face), HIT_CASE (a `bsize` bswap case with wrong ends), TYPE_MISMATCH (a `zeq` end of type Nat). Guard: `hcomp Int i0 (fun i => [ i0 -> pos 0 ]) (pos 0)` does not convert to `pos 0`. Mutants: (1) eval_fold_hit_boundary keeps the inner constructor unfolded; (2) transp_pcon_agrees always true; (3) hit_case_end takes the `~i` end twice. |
| s2 | examples/int.lang `negNeg`; examples/circle.lang the elim rows; examples/otp.lang `flip`, `ret`, `flipEquiv`, the transport row; an `elim` of an `hcomp` value (eval row with the exact text). Check rows: ELIM_CASES (a missing case; a point case with no IH), ELIM_CASE (a path case with wrong ends), TYPE_MISMATCH (a motive `Nat`), REFUSE_NAME (`def elim`). Guard: two variables `x y : Int` do not convert. Emit row: a stuck `elim` in an instance (JSON_VALUE). Mutants: (1) the IH of a recursive field is the field itself; (2) the path rule drops `@ r`; (3) the hcomp rule uses `P u0` as the line. |
| s3 | the Quot `eqc` transport row (section 4); a plain family with a dependent field (a new domain family, or Lot) under `hcomp` with the same constructor (eval row); a Path field under `transp` (a Trunc `squash` along `<i> Trunc (L i)`). Mutants: (1) the fill uses `phi` and not `phi \/ ~i`; (2) the Path field keeps its old ends; (3) field_line_transp NULL again at a dependent field. |
| s4 | examples/ctx.lang: the block, constructor values, the `transp` row at `Ty (P i)`, JSON goldens. Check rows: MUTUAL_BLOCK (one family; an index of the wrong type), DOMAIN_FAMILY (a partner field at a later field; a Ty field at another index), REFUSE_DATA (a block in a program), PARSE_EXPECT (`mutual` then `def`), REFUSE_REC (a forward reference outside a block, unchanged). Mutants: (1) the index rule off; (2) the block registers the second name after the first family's constructors; (3) the partner field is `recursive` 0. |
| s5 | examples/ctx.lang: `length`, the block fold laws, a Ty fold row that uses `rg`. Check rows: HIT_CASES (a missing Ty case; a Ty case with no index result). Mutants: (1) the second-family case gets `g` and not `fold .. g`; (2) a partner field gets `MA`; (3) the case order puts Ty first. |
| s6 | examples/transportProp.lang (D8); the .glue example and its same-ir row if section 6 finds one; a CIRCUIT_LOWER row if a probe gives one. Mutants: (1) the uaβ row with the inverse map; (2) the same-ir golden set to `range`; (3) the representative point case swapped. |
| s7 | No code. SPEC 2.7 and 9, FORMERS if a cell changes, PLAN M5 DONE, STATUS `## M5`, the mutant roll-up, the review (no conv rule reads a type; no rule `hcomp A i0 u u0 = u0`; `hcomp` at a HIT stays canonical), the M6 kickoff prompt. |

## 9. Questions for the USER

a. The M5 and M7 boundary: M5 = the QIT forms, `elim` and `rec` on
   representatives in the checker and the evaluator only, the lowering
   stays in M7; or M5 also lowers?
   Recommendation: M5 = checker and evaluator; the lowering stays in M7.
   Reasons: (1) docs/PLAN.md puts Direction 2 in M7; (2) M5 keeps the IR
   fixed, so each slice keeps the `make e2e` byte-identical check; (3) M4
   quotSq shows that a fold on a representative already lowers by
   normalization, so M7 starts from a working base.
   Ruling (USER, 2026-10-16): the recommendation. M5 = the checker and the
   evaluator; the lowering stays in M7; the IR does not change.

b. The dependent eliminator (O12 (e); the M4 ruling was `fold` only): add
   `elim` in M5 (D3, D4), or keep `fold` only?
   Recommendation: add `elim` in M5 (slice s2). Reasons: (1) QIT induction
   (`negNeg`, the laws over a variable) needs it; (2) it closes the s5 gap:
   `ret` for the `flip` iso, and `transport (ua flipEquiv) message =
   cipher` for a non-identity iso; (3) `rec` stays a keyword, so `elim` is
   a new core name with no lexer change. Cost: one slice; the `hcomp` rule
   needs `comp` and `hfill` (D4).
   Ruling (USER, 2026-10-16): the recommendation. `elim` in M5, slice s2;
   `fold` stays.

c. The syntax of a constructor equation: reuse the M4 path constructor
   (D1), or a new `eq` clause that desugars to it?
   Recommendation: reuse the M4 form. Reasons: (1) no new token and no
   desugar step; (2) an equation is a path constructor with one binder, so
   the boundary rule, HIT_BOUNDARY and HIT_CASE apply as they are; (3) the
   set truncation (`squashc`, two binders) uses the same form. Cost: the
   USER writes `<i> [ ~i -> t0, i -> t1 ]` and not `t0 = t1`.
   Ruling (USER, 2026-10-16): the recommendation. The M4 path constructor
   form; no `eq` clause.

d. The inductive-inductive form: a `mutual` block of two families in
   domain/domain.lang (D6, D7), in M5 or in a later milestone? And with
   which limits?
   Recommendation: in M5 (slices s4 and s5), with the D6 limits: two
   families, a uniform index, point constructors only, `fold` with two
   motives, no `elim` at a block. Reasons: (1) docs/PLAN.md names
   inductive-inductive definitions in M5; (2) the limits keep R3 with one
   exception and keep the M4 field rule for Self; (3) the full QIIT form
   (equations in a block, non-uniform indices such as `Ty (ext g a)`, the
   block `elim`) is large, so it becomes O13. Cost: the new keyword
   `mutual`. Alternative with no new token: a family can name the next
   family when that family's last parameter has its type; the
   recommendation is the keyword, because the block is then explicit in
   the source and in the error messages.
   Ruling (USER, 2026-10-16): the recommendation. The `mutual` block in M5
   (s4, s5) with the D6 limits; the full QIIT form is O13.

e. The O12 items to close: close (d) with the Sigma method (D5, slice s3),
   or give (d) a diagnostic code and keep it stuck?
   Recommendation: close (d) in s3; (a) and (b) move to the rule table
   (D10); (c) stays open; (e) closes with `elim`; (f) O4 stays open for M7.
   Reasons: (1) the block needs (d): `hcomp` at Ctx meets `ext g t` with
   `t : Ty g`, a dependent field; (2) the Sigma method is M3 code (transp
   on Sigma, hcomp_pair), so the slice is a template reuse; (3) a
   diagnostic code would refuse terms that CCHM computes. Cost: one slice.
   If the USER rules a diagnostic, s3 adds KAN_FIELD (exit 1) at the
   conversion failure that meets the stuck value, and the block (s4) can
   not use `hcomp` at Ctx.
   Ruling (USER, 2026-10-16): the recommendation. s3 closes (d) with the
   Sigma method; no KAN_FIELD code.

f. Slice count and order?
   Recommendation: 7 slices (section 10): s1 constructor equations and the
   QIT smoke, s2 `elim`, s3 O12 (d), s4 the block declarations, s5 the
   block fold, s6 the Direction 2 smoke, s7 close. Reasons: (1) `elim`
   comes before the Direction 2 smoke and after the equations it tests;
   (2) s3 comes before the block (question e); (3) the block fold is a
   slice of its own, because its case types depend on the motive `MB`.
   If the USER rules (d) later, s4 and s5 go and M5 has 5 slices.
   Ruling (USER, 2026-10-16): the recommendation. 7 slices in the order of
   section 10.

## 10. Slices

Each slice is one session or more. Each prompt is in ~/Documents and reads
~/Documents/glue-lang-M5-COMMON.md for the gates, the IR check, the close
steps and the shared traps.

| Slice | Content | Prompt |
|---|---|---|
| s1 | Constructor equations with recursive ends (D1, D2): probes, fixes, Bag and Int in the domain, examples/bag.lang and examples/int.lang, tests. | glue-lang-M5-s1-PROMPT.md |
| s2 | `elim` (D3, D4): OP_ELIM, the case types with IHs, the point, path and `hcomp` rules, ELIM_CASES, ELIM_CASE, REFUSE_NAME; the elim rows; `ret`, `flipEquiv`; tests. | glue-lang-M5-s2-PROMPT.md |
| s3 | O12 (d) (D5): `transp` at a dependent field and a Path field, `hcomp` at a dependent field; tests. | glue-lang-M5-s3-PROMPT.md |
| s4 | The block (D6): `mutual`, the index rule, the partner field, MUTUAL_BLOCK; Ctx and Ty in the domain; examples/ctx.lang; tests. | glue-lang-M5-s4-PROMPT.md |
| s5 | The block fold (D7); the laws; tests. | glue-lang-M5-s5-PROMPT.md |
| s6 | Direction 2 smoke (D8): `transportProp`, the representative rows, the .glue example and the same-ir row, the CIRCUIT_LOWER probe; tests. | glue-lang-M5-s6-PROMPT.md |
| s7 | Close: SPEC 2.7 and 9, FORMERS if a cell changes, PLAN M5 DONE, STATUS `## M5`, mutant roll-up, review, M6 kickoff prompt. | glue-lang-M5-s7-PROMPT.md |

Gates for each slice (all GREEN before staging): `make -B check`, `make
check-clang`, `forge build --root contracts --sizes`, then `make e2e` last.
`cargo fmt --check` and clippy only if halo2/ changes (M5 plans no halo2/
change).

IR check for each slice: after `make e2e`, `git diff --exit-code --
test/circuit contracts/src/verifiers`. If it is clean, restore
contracts/test/fixtures from HEAD and run `forge test --root contracts`
again (33 passed). If the verifiers changed, stop: M5 must not change the
IR without a USER ruling.
