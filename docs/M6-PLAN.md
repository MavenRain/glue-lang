# M6 plan: monad laws, folds, unfolds and the hint monad

Status: RULED (2026-10-17).  The USER ruled questions (a) to (h) of section
9:  each recommendation.  M5 is the s7 close commit 8abf7e2.  The kickoff
makes no code change.  The plan goes into the s1 commit (D16).

## 1. Sources

| Source | What M6 takes from it |
|---|---|
| docs/PLAN.md, Rulings 8 to 18, M6 row (52), M7 row (53) | The M6 scope and the M7 boundary.  No proof irrelevance. |
| docs/PLAN.md, Lowering rules (57 to 70) | `Pi over a finite domain, fold \| Unrolled`;  `unfold \| Needs a static depth bound`;  "A proof stays a term". |
| design/DESIGN.md 15, 224, 291 | The former list (proof irrelevance is prohibited).  `advice : Fin n -> Field`:  the cells of a hint. |
| formers/FORMERS.md R3 (51 to 54), F5 to F8 (105 to 165) | F5 (105 to 117):  the monad (FORMERS writes `bind f t`;  glue-lang writes `bind m f`).  F6 (119 to 136):  the fold and its uniqueness (133 to 136).  F7 (138 to 152):  unfold into List with a step limit n, into Nat with `g : S -> Option S` (the result is the number of steps), into a core type with several constructors.  F8 (154 to 165). |
| SPEC.md 3 (the tcc-json column) | F5 DONE over Option, Sum E and List.  F6 DONE over Nat, List and each family.  F7 DONE into List with a Nat step limit.  F8 DONE over Option and List. |
| SPEC.md 2.5 (235 to 315) | The closed-type rules (273, 280).  "The fold and unfold families give a stuck value (O8)" (290).  No regularity, D3 (293 to 299).  Conversion (301 to 315). |
| SPEC.md 2.6 (380) and 2.7 (516) | `elim` (548 to 565), the block fold (614 to 638), the neutral values (640 to 643), the codes (669 to 685). |
| SPEC.md 4, 7.1 and 9 | Section 4 Structures (702 to 711) is the template placeholder:  the M6 close fills it.  7.1:  the lowering (744 to 781).  9:  the open items (789 to 867), O3 (801), O4 (804), O8 (828), O12 (839;  (c) at 844), O13 (863). |
| docs/STATUS.md `## M5` (1829) | `### s7: close` (2458):  22/22 KILLED, review notes (a) (2594) and (b) (2599), open items (2574 to 2593). |
| docs/IR.md 42 to 44, 73 to 75 | Only gates, lookups and copies constrain.  Each advice column has one witness array, of length rows. |
| halo2/src/circuit.rs 141 to 154, 170 | The prover assigns each advice cell.  `constrain_equal` is for copies only. |
| test/circuit/square.circuit.json, `langc satcheck` | Each input row has all its fixed values at 0.  No gate, lookup or copy reads advice b and c on the input rows.  satcheck gives `ok` with advice[1][0] = 7 and advice[2][1] = 12345. |
| docs/M5-PLAN.md | The shape of this plan.  M5 D10 (stuck values become rules), D11 (names), D12 (the plan in s1). |

Code anchors (M5 code, 8abf7e2;  the slices find the lines again with `rg -n`):

| Fact | Where |
|---|---|
| Ops | src/front/core.h OP_PURE 49, OP_MAP 50, OP_BIND 51, OP_FOLD_NAT 52, OP_FOLD_LIST 53, OP_FOLD_FAMILY 54, OP_FOLD_HIT 55, OP_FOLD_BLOCK 58, OP_ELIM 62, OP_UNFOLD 64, OP_FILTER 65 |
| The core instance | core.h Carrier (CARRIER_OPTION, CARRIER_LIST, CARRIER_SUM) 97 to 102;  the Core.inst comment 127 to 128 |
| Builtins | src/front/check.c `either` 140 (OP_EITHER), `option` 141 (OP_OPTION_ELIM), `pure` to `unfold` 142 to 148, `natAdd` 150 |
| Check rules | check.c carrier_of 470, rule_pure 728, rule_map 738, rule_bind 758, rule_filter 784, fold_simple 801, fold_family 817, fold_hit_rule 963, fold_block_rule 1056, rule_fold 1108, rule_unfold 1135, rule_elim 1436 |
| Evaluator, monad | src/front/eval.c names 22 to 28, arity 45 to 49, blocked 273, list_tail 341, reduce_pure 544, map_list 556, reduce_map 569, bind_list 595, reduce_bind 616 |
| Evaluator, folds and unfold | eval.c reduce_fold_nat 633, reduce_fold_list 647, stuck_fold 669, fold_family 682, fold_block 721, eval_hit_case 780, fold_hit 861, unfold_end 1002, reduce_unfold 1025, reduce_filter 1089 |
| Evaluator, arithmetic and dispatch | eval.c Field ops 1121, nat_args 1158 (natAdd), varied_arity 1213, reduce_op 1377 (op cases 1445 to 1471) |
| Fr arithmetic | src/front/fr.c fr_mul 143.  There is no inverse and no power function. |
| Kan | src/front/kan.c closed_type 272, data_type 277, plain_family 283, type_former 292, data_ctor 314, transp_data 329, transp_ctor 986, kan_fold_hcomp 1060 (empty face 1066 to 1094), kan_elim_hcomp 1105 (empty face 1114 to 1153), transp_family 1159, kan_transp 1165, hcomp_constant 1356, hcomp_same_ctor 1384, hcomp_data 1509, kan_hcomp 1529 |
| Circuit | src/circuit.c ROW_INPUT 42, no_circuit_form 143, lower_value 148, lower_check 189, lower_circuit 223 (UNCONSTRAINED 246 to 248, UNUSED_PUBLIC 250), put_copies 319, the witness input fill 404 |
| Circuit syntax | src/front/parser.c the circuit form 611, parse_binders 619 (TOK_KW_PRIVATE) |
| Switch sites | src/json.c op_json 203:  OP_PURE, OP_MAP, OP_BIND 271 to 273, OP_UNFOLD 274, OP_FOLD_NAT 275, OP_FOLD_LIST 280, OP_FILTER 281.  eval.c 1445 to 1465.  No hit in kan.c or circuit.c. |

Current forms (8abf7e2):  `pure a` (check only;  the declared type selects
the carrier);  `map f m`;  `bind m f` (the carrier comes from m;  f goes into
the same carrier;  Sum keeps E);  `filter p m` (Option and List;
`p : A -> Flag`);  `fold step z n` (Nat);  `fold f z xs` (List);
`fold case.. x` (a family);  `fold B cases x` (a HIT);
`fold MA MB casesA casesB x` (a block);  `elim P cases x` (a domain family
only);  `unfold g limit seed` (into List only;  `g : S -> Option (Prod A S)`).

Evaluation at a neutral argument (8abf7e2):  `pure` reduces.  map, bind and
filter are blocked (VAL_STUCK;  `none` and `inl` pass through).  List map
keeps the known cells over `map f tail`.  bind_list with a neutral tail or a
neutral `f x` is stuck as a whole.  fold Nat is blocked on n.  fold List folds
the known cells over a stuck `fold f z tail`.  Family, HIT, block and elim
folds at a neutral scrutinee are neutral.  unfold is blocked on the limit,
and a neutral step result gives a stuck `unfold g left seed` tail.

Kickoff probes (part a, `build/langc check`;  PASS = exit 0;  FAIL =
TYPE_REFL):

| Law | Carrier | Closed | Variable |
|---|---|---|---|
| left identity, concrete f, variable a | Option, Sum Nat, List | PASS | PASS |
| left identity, variable f and a | Option, Sum | - | PASS |
| left identity, variable f and a | List | - | FAIL (`bind (cons a nil) f` against `f a`) |
| right identity `bind m pure = m` | Option, Sum, List | PASS | FAIL (stuck bind) |
| associativity | Option, Sum, List | PASS | FAIL (stuck binds) |
| map law `map f m = bind m (fun x => pure (f x))` | Option, Sum, List | PASS | FAIL |
| `map f (cons x m) = cons (f x) (map f m)` | List | - | PASS |
| `bind (cons x m) pure = cons x (bind m pure)` | List | - | FAIL |
| F6 `fold f z 0 = z` | Nat | - | PASS |
| F6 `fold f 0 (natAdd n 1) = ..` | Nat | - | FAIL (`natAdd n 1` is stuck, so the fold is blocked) |
| F6 `fold f 0 (cons x l) = natAdd x (fold f 0 l)` | List | - | PASS |
| F6 `fold f z nil = z` | List | PASS (laws.lang foldNil) | not run |
| F7 `unfold g 0 s = nil`, `unfold g 1 s = cons s nil` | List | - | PASS |
| `elim` at Nat, List Nat, Option Nat | - | TYPE_MISMATCH ("elim needs a last argument in a domain family") | - |

Kan side (kan.c, 8abf7e2):  kan_transp gives `a` at the face i1, at a closed
head and at a family with no parameters.  A family with parameters goes to
transp_family (a constructor or an hcomp, else stuck).  Sum, Option and List
move field by field only when data_ctor(a) holds;  a neutral `a` gives
VAL_STUCK OP_TRANSP (1213).  kan_hcomp gives `u i1` at the face i1, and `u0`
at a closed type when hcomp_constant holds (true on an empty system).
hcomp_data at Sum, Option, List or a plain family needs `u0` a constructor and
the same head in each branch, else VAL_STUCK OP_HCOMP (1573).  A HIT gives a
canonical VAL_OP OP_HCOMP (1571).  "The fold and unfold families" (SPEC.md 290
and 834) names the formers F6 and F7.  In the code they are neutral OP_FOLD_*,
OP_UNFOLD and OP_BIND values under transp and hcomp, with no type former and
no case in kan.c.

## 2. Design calls

- D1.  `append xs ys` is a new core name, the join of the List instance (a
  new op OP_APPEND).  `append nil ys = ys`.
  `append (cons x xs) ys = cons x (append xs ys)`.  `append xs nil = xs` for
  a neutral xs.  Else the value is neutral.  The right unit keeps normal forms
  stable under substitution:  `append (f a) (bind l f)` at `l := nil` gives
  `f a`, as direct evaluation does.  `def append` gives REFUSE_NAME (D15).
  Check:  the name `append` has no hit in src/front, examples, test or domain.
- D2.  bind_list goes through append:  `bind nil f = nil`, and
  `bind (cons a l) f = append (f a) (bind l f)`.  Thus
  `bind (cons a nil) f = f a` by refl for each f (left identity).  Check
  (changed):  no row of test/eval/expect.txt has `bind`, so no current eval
  row changes its printed form.  s1 adds the new eval rows, and runs the
  examples again to find a changed printed form.
- D3.  `elim` at the core carriers (question g):  `elim P cnone csome o`,
  `elim P cinl cinr s` and `elim P cnil ccons l`.  ccons takes a, l, then the
  IH `P l` (the M5 IH rule).  P is explicit.  `elim` reduces at a constructor
  and is neutral at a neutral scrutinee.  Reuse ELIM_CASES and ELIM_CASE, and
  fix the `_x0` binder print of ELIM_CASES (M5 s2 (b)).  The M5 OP_ELIM inst
  is a family:  s1 selects the tag for a carrier (a Carrier value, or a new op
  OP_ELIM_CORE).  The core `option` (OP_OPTION_ELIM) and `either` (OP_EITHER)
  stay the non-dependent forms (check.c 140, 141).
- D4.  The monad laws.  Left identity is refl at each carrier (after D2).  At
  a variable, right identity, associativity and the map law are proof terms
  that use D3 (`cong`, `trans`, `funExt`).  At closed values, each law is
  refl.  List associativity needs two guest lemmas by `elim`:  append
  associativity, and `bind (append xs ys) g = append (bind xs g) (bind ys g)`.
- D5.  The Nat successor view.  `natAdd x k`, with one operand a literal
  k > 0 and the other neutral, is a successor spine.
  First normalize literal offsets: `natAdd x 0 = x`, `natAdd 0 x = x`,
  `natAdd k x = natAdd x k` for neutral x, and
  `natAdd (natAdd x a) b = natAdd x (a + b)` for literal a and b.
  Literal addition keeps the current overflow and trap behavior.  These
  evaluator rules are required for dependent elimination: at k = 2 the
  step returns `P (natAdd (natAdd x 1) 1)`, which must convert to
  `P (natAdd x 2)`.  A literal on the left needs the same canonical index.
  `fold f z (natAdd x k) = f (fold f z (natAdd x (k - 1)))`, down to
  `fold f z x`.  The same rule applies to `elim` at Nat (D6) and to the
  `unfold` limit.  The rule is stable under substitution:  `x := m` gives the
  m + k unroll.  PRINT_MAX guards a large k.  Check:  `natAdd` reduces in
  eval.c nat_args (1158);  its name rows are eval.c 25 and check.c 150.
- D6.  `elim` at Nat:  `elim P z s n`, with
  `s : (n : Nat) -> P n -> P (natAdd n 1)`.  It reduces at a literal and at a
  successor spine (D5), and it is neutral at a neutral n.
- D7.  F6 uniqueness is a guest proof term (D3, D6, `trans`, `cong`,
  `funExt`) at Nat, List and a plain family (the M5 `elim`).  At a HIT, the
  path cases need squares:  a new open item O14.
- D8.  The unfold step goes through `option` at a neutral step result:
  `unfold g (natAdd n 1) s = option nil (fun p => cons (first p) (unfold g n (second p))) (g s)`.
  Thus the step does not wait for `g s`.  Check:  the core `option` exists
  (check.c 141, OP_OPTION_ELIM), so D8 uses it.
- D9.  F7 into Nat:  `unfold g n s : Nat` when `g : S -> Option S` (the
  declared type selects the form).  The laws:  `unfold g 0 s = 0`;  at the
  limit `natAdd n 1`, `g s = none` gives 0 and `g s = some r` gives
  `natAdd (unfold g n r) 1`.  Unfold uniqueness (the List and Nat forms) is a
  proof by `elim` on the depth.  The form into a family stays out of M6:  no
  domain family needs it, and SPEC.md section 4 says so.
- D10.  O8 at closed data types.  For `transp` along `<_> T`, T is built from
  the closed heads (Nat, Field, Unit, Flag, Type n) by Sum, Option, List, Prod
  and a family with closed parameters.  `transp` gives `a` for each `a`, a
  neutral `a` too.  `hcomp` needs a separate recursive eligibility check:
  Nat, Field, Unit and Flag; Sum, Option, List and Prod with eligible fields;
  plain families only when their instantiated constructor fields pass the
  same check, including recursive fields.  Type n, HITs and open field types
  are ineligible, including inside a non-HIT wrapper.  At an eligible type,
  `hcomp` with a constant system (hcomp_constant) gives `u0`, a neutral `u0`
  too.  All other types keep their existing fieldwise or neutral rules.
  This preserves substitution: the fieldwise result at `Option A`, then
  `A := Circle`, must agree with direct evaluation at `Option Circle`.
  The rules read the line or the type argument, not the type of a value.
- D11.  O8:  the other stuck values become rules (as M5 D10).  `transp` along
  an open line at a neutral value is neutral (no regularity, D3 of M3).
  `hcomp` at non-HIT data stays neutral when neither D10 nor the existing
  fieldwise rules apply (D3 of M4: a canonical hcomp only at a HIT).
  A fieldwise rule with projections fails the
  boundary, because `c (proj_c y) = y` is not a rule for a neutral y.  s4
  probes first:  two different constructor heads give HCOMP_BASE (a
  constructor branch at i0 must convert to `u0`).  Classify the remaining
  cases by neutral base, neutral branch and field eligibility; the probe
  alone does not make the neutral-branch case exhaustive.  Then O8 closes.
- D12.  (changed by the check)  A system drops each branch whose face is i0
  when it is built (a rule on the term, not on a type), if one branch or more
  stays.  A system with i0 on each face stays as it is:  kan_fold_hcomp
  (kan.c 1094) and kan_elim_hcomp (kan.c 1146) build that empty tube on
  purpose (M5 s3 ruling (d) (ii)), and a printed `[ ]` gives PARSE_EXPECT.
  Conversion of two systems skips each branch whose face is i0 (a rule on
  terms).  This closes M5 s3 (b) (the fills at r = i1 keep an `i0 -> ..`
  branch) and the s7 review note (b) (`hfill .. 1` converts to `h`).  s4
  probes the note (b) input first (the empty-face `elim` hcomp at a dependent
  motive).  If the probe shows no failure, D12 is optional.
- D13.  The constrained monad is the current lowering (SPEC.md 7.1):  each
  Field value of a constraint side has its rows in the circuit.  No change.  A
  fold or an unfold with a static scrutinee or depth normalizes before the
  lowering (the `Unrolled` row of the PLAN lowering table).
- D14.  The hint monad (question h).  A circuit binder `hint h := e` comes
  after the public and private binders.  The checker types the constraints
  with h as a variable of type Field:  the constraint relation does not see
  e.  e is a Field term over the earlier binders, and it can use `inv` (the Fr
  inverse, with `inv 0 = 0`).  Lowering:  an input row with zero selectors
  and no instance copy (the shape of a private binder row).  `langc witness`
  computes the value from e;  the command line gives no value for a hint.
  CIRCUIT_UNCONSTRAINED covers an unused hint.  A residual `inv` in a
  constraint normal form gives CIRCUIT_LOWER: the first gate row of O4.
  `inv` is an ordinary core operation, so a closed use such as `inv 1`
  computes before lowering and is allowed in a constraint.  Check (changed): src/ has
  no Fr inverse and no power function, so s5 adds `fr_inv` to fr.c (the power
  r - 2, where r is the Fr modulus, and 0 at 0).  `hint` gets a keyword token
  next to TOK_KW_PRIVATE (parser.c 611, 619).  `hint` and `inv` have no name
  hit in examples, test or domain (one comment word in
  test/check/glue-no-hint.lang).
- D15.  Names (as M5 D11):  `append` and `inv` are new core names;  `hint` is
  a circuit binder keyword.  A definition with a core name gives REFUSE_NAME.
- D16.  The plan is in the s1 commit (as M5 D12).

## 3. Core changes

### 3.1 Syntax

- `append`, `inv`, `elim` at a carrier and the Nat form of `unfold` are
  builtin rows in the application form.  There is no new term syntax.
- `elim` at a carrier keeps the M5 surface `elim P cases x`.  The case count
  comes from the carrier:  2 at Option, Sum E, List and Nat.
- The circuit form gets hint binders (s5;  a hint term ends at the next
  `hint` or `:=`):

```
circuit NAME [public BINDERS] [private BINDERS] (hint NAME := TERM)* := constraint (; constraint)*
```

### 3.2 Typing rules

- `append : List A -> List A -> List A`.  Two different A give TYPE_MISMATCH.
- `elim P cases x` at a carrier:  x has the type C = Option A, Sum E A,
  List A or Nat, and `P : C -> Type n`.  The cases:  Option
  `cnone : P none`, `csome : (a : A) -> P (some a)`;  Sum
  `cinl : (e : E) -> P (inl e)`, `cinr : (a : A) -> P (inr a)`;  List
  `cnil : P nil`, `ccons : (a : A) -> (l : List A) -> P l -> P (cons a l)`;
  Nat `z : P 0`, `s : (n : Nat) -> P n -> P (natAdd n 1)`.  The result is
  `P x`.  A wrong count gives ELIM_CASES.  A wrong case type gives
  ELIM_CASE.  A scrutinee of another type gives TYPE_MISMATCH (a new cause
  text).
- `unfold g n s : Nat` when the declared type is Nat and `g : S -> Option S`.
  The List form does not change.  A step type that matches neither form gives
  TYPE_MISMATCH.
- `inv : Field -> Field`.
- `hint h := e`:  e checks at Field in the context of the public binders, the
  private binders and the earlier hints.  The constraints check in that
  context with each h a Field variable.  e does not enter the constraint
  context.

### 3.3 Evaluator rules

- `append` (D1) and bind_list through append (D2).
- `elim` at a carrier (D3, D6):  it reduces at a constructor, a literal or a
  successor spine, else it is neutral.
- The successor view (D5) for fold Nat, `elim` at Nat and the unfold limit.
- The unfold step through `option` (D8) and the Nat form (D9).
- `inv` at a literal uses fr_inv.  `inv` at a neutral value is neutral.
- The O8 rules (D10, D11) are Kan rules in kan.c, at kan_transp and at
  kan_hcomp, before hcomp_data.
- The system build drops i0 branches (D12).

### 3.4 Conversion

- No new conv rule reads a type.  OP_APPEND, the carrier `elim` and `inv`
  convert argument by argument as neutral values.
- `append xs nil = xs` is an evaluator rule, so the two sides have one normal
  form.
- `natAdd x k` is the canonical normal form of a positive literal-offset
  spine; D5 removes zero offsets, moves a literal to the right and combines
  nested literal offsets before the dependent step is applied.  No new Nat
  form is added.
- D12:  the conversion of two systems skips each branch whose face is i0.

### 3.5 Printer and JSON

- OP_APPEND prints `append xs ys`.  The carrier `elim` prints
  `elim P cases x`.  `inv` prints `inv x`.
- s1 fixes the `_x0` binder print of ELIM_CASES (M5 s2 (b)).  s4 fixes the
  `loop i` printer quirk (M5 s2 (a)) and lists each changed row.
- json.c op_json gets a case for each new op (no `default:`).  A stuck value
  in an instance gives JSON_VALUE (COMMON trap).
- The IR JSON of a hint is an input row, as a private binder row.  The IR
  schema does not change (question a).

## 4. Derived operations and their laws

Derived operations:  `append` is in the core (D1).  `appendAssoc` and
`bindAppend` are guest lemmas by `elim` at List (D4).  The table shows the
state after M6.  s1 to s3 confirm each row.  At closed values each law is
refl (D4).

| Law | Option | Sum E | List | How | Terms (slice) |
|---|---|---|---|---|---|
| left identity `bind (pure a) f = f a` | refl | refl | refl (D2) | computation | `leftIdO`, `leftIdS`, `leftIdL` (s1) |
| right identity `bind m pure = m` | proof | proof | proof | `elim` (D3) | `rightIdO`, `rightIdS`, `rightIdL` (s1) |
| associativity `bind (bind m f) g = bind m (fun x => bind (f x) g)` | proof | proof | proof (and `appendAssoc`, `bindAppend`) | `elim` (D3) | `assocO`, `assocS`, `assocL` (s1) |
| map law `map f m = bind m (fun x => pure (f x))` | proof | proof | proof | `elim` (D3) | `mapBindO`, `mapBindS`, `mapBindL` (s1) |
| `fold f z 0 = z`, `fold f z (natAdd n 1) = f (fold f z n)` | Nat:  refl (D5) | | | computation | `foldZero`, `foldSucc` (s2) |
| `fold f z nil = z`, `fold f z (cons a l) = f a (fold f z l)` | | | refl | computation | `foldNil` (laws.lang), `foldCons` (s2) |
| F6 uniqueness | Nat, List, plain family:  proof (D7);  HIT:  O14 | | | `elim` | `foldNatUniq`, `foldListUniq`, `foldFamUniq` (s2) |
| F7 laws (the List and Nat forms) | refl (D5, D8, D9) | | | computation | `unfoldZero`, `unfoldNone`, `unfoldSome` (s3) |
| F7 uniqueness | proof by `elim` on the depth | | | `elim` (D6) | `unfoldListUniq`, `unfoldNatUniq` (s3) |

The uniqueness form (F6, FORMERS 133 to 136):  a function h that satisfies
the two fold equations as paths at each argument is equal to the fold, by
`funExt` over an `elim` proof.  The F7 form is the same on the depth.

## 5. Effect on examples

- examples/monad.lang (2 to 10), examples/laws.lang (7 to 11) and
  examples/algebra.lang (3 to 8) do not change.  They check as before.  s1
  runs them again:  a changed printed form goes into STATUS with its reason.
- New examples:  examples/monad-laws.lang (s1), examples/fold-laws.lang (s2),
  examples/unfold-laws.lang (s3).  Each term of section 4 is in one of them.
- The hint circuit (s5) and the fold and unfold circuits (s6) go next to the
  current circuit sources (the slice finds the place with `rg -n 'square'`).

## 6. Circuits

The IR and the verifiers of the 4 current circuits stay byte-identical:  no
current circuit has a hint binder.  s1 to s4 do not change the lowering.  s5
adds one hint circuit (for example `nonzero`:  `private x`,
`hint v := inv x`, `x * v == 1`) with new goldens and one e2e row (the first
halo2 test of an advice cell that only its constraints read), and the
`inv x == y` refusal row (CIRCUIT_LOWER).  s6 adds fold and unfold circuits:
a same-ir row when the normal form is the normal form of a current circuit,
else new goldens.  A new circuit adds new verifier and fixture files;  the
current files stay byte-identical (the IR check of the COMMON file).

## 7. New check codes

None is planned.  ELIM_CASES and ELIM_CASE (D3, D6), REFUSE_NAME (D15),
CIRCUIT_UNCONSTRAINED (an unused hint), CIRCUIT_LOWER (residual `inv` in a
constraint normal form) and TYPE_MISMATCH get new causes.  A slice that needs a new code
adds it with a refusal row and lists it in STATUS.

## 8. Guest tests per slice

Baseline (the M5 close, 8abf7e2):  native circuit 9, path 42, face 35, HIT
44;  examples 33, parse refusals 18, check refusals 123, evals 209, json 27
builds, build refusals 8, circuits 4, same-ir 3, circuit refusals 12,
satcheck refusals 6;  node regressions 6, printer 2;  e2e 33 passed.

| Slice | Rows | Mutants (3 or more) |
|---|---|---|
| s1 | Eval:  `append` at nil, cons, a neutral xs with nil, a neutral pair;  bind at a neutral tail;  `elim` at Option, Sum and List (constructor, neutral).  Example:  monad-laws.lang.  Refusals:  `def append` (REFUSE_NAME);  a wrong count (ELIM_CASES) and a wrong case (ELIM_CASE) at each carrier;  `elim` at Field (TYPE_MISMATCH).  The `_x0` print row. | Remove the right-unit rule;  cons bind without append;  the IH before l in ccons. |
| s2 | Eval:  fold at `natAdd x 3`, `elim` at Nat (literal, spine, neutral).  Example:  fold-laws.lang.  Refusals:  a wrong `s` type (ELIM_CASE). | The spine at k instead of k - 1;  the spine with the literal on the left only;  `elim` Nat with the IH dropped. |
| s3 | Eval:  unfold at a neutral `g s` (the `option` step), the Nat form (none, some, limit 0).  Example:  unfold-laws.lang.  Refusals:  the Nat form with a List step type (TYPE_MISMATCH). | The step waits for `g s`;  the Nat form counts one step too many;  the List form at a Nat declared type. |
| s4 | Eval:  `transp` at a closed line and a neutral `a`;  `hcomp` with a constant system and a neutral `u0`;  an open line at a neutral value;  the HCOMP_BASE probe row;  the D12 rows (`hfill .. 1`);  the `loop i` rows. | The closed rule at an open line;  the hcomp rule at a HIT;  no i0 drop. |
| s5 | Circuit:  `nonzero` goldens, the e2e row, a satcheck row.  Eval:  `inv 0`, `fieldMul 3 (inv 3)`.  Refusals:  `inv x == y` (CIRCUIT_LOWER), an unused hint (CIRCUIT_UNCONSTRAINED), `def inv` (REFUSE_NAME). | `inv 0` not 0;  a hint row with an instance copy;  the constraint context sees e. |
| s6 | Circuits:  a fold circuit and an unfold circuit (same-ir rows where they apply, else goldens). | The unroll one step short;  a static depth left blocked;  a same-ir row against the wrong circuit. |
| s7 | No new rows.  The mutant roll-up of s1 to s6. | None. |

Additional regression obligations:

- s2: refl for `natAdd (natAdd n 1) 1 = natAdd n 2`, zero offsets and a
  literal on the left; dependent `elim` at `natAdd n 2` and `natAdd 2 n`,
  with a motive that retains its Nat index.  Remove offset normalization
  as a mutant.  Record any existing printed forms changed by D5.
- s4: compare the fieldwise `hcomp` at `Option A` after `A := Circle` with
  direct evaluation at `Option Circle`; both retain `some (hcomp Circle ..)`.
  Also cover a List of Circle and a plain family with a HIT field.  Excluding
  a HIT only at the outer head is a mutant.  Do not use the transp predicate
  for hcomp eligibility; nested universes need their existing universe rule.
- s5: `x * inv 1 == x` lowers successfully after normalization; `inv x == y`
  gives CIRCUIT_LOWER, including through a guest helper.  A source-level ban
  on every use of `inv` in a constraint is a mutant.

## 9. Questions for the USER

The USER ruled each question on 2026-10-17:  each recommendation.  The D
calls already use the recommendations, so no D call changes.

- (a) The hint monad and the IR:  does a hint need an IR change?
  Recommendation:  no IR change.  Reasons:  (1) IR v1 already has witness
  cells that no gate, lookup or copy reads (IR.md 42 to 44, 73 to 75);
  (2) satcheck accepts such a cell (the square probe);  (3) halo2 assigns
  each advice cell (circuit.rs 141 to 154).  A hint lowers to an input row
  with zero selectors and no instance copy.  New goldens only;  the hint e2e
  row is the first halo2 test of such a cell.
  RULED 2026-10-17 (USER):  the recommendation (no IR change).
- (b) The monad surface:  extend the core instance, or add a monad record?
  Recommendation:  extend the current core instance (core.h 97 to 102) with
  the core `append` and its right-unit rule (D1, D2).  Reasons:  (1) a monad
  record gives the same computation;  (2) a record needs a type-function
  field that the core does not have.
  RULED 2026-10-17 (USER):  the recommendation (extend the core instance).
- (c) `unfold`:  which depth form?  Recommendation:  the explicit Nat depth
  (FORMERS F7 and the PLAN lowering table), plus the F7 form into Nat (D9)
  and the `option` step (D8).  Reasons:  (1) the lowering needs a static
  depth bound (PLAN 57 to 70);  (2) the `option` step gives the F7 laws by
  refl at a neutral step result;  (3) the Nat form is the FORMERS F7 Nat
  case.
  RULED 2026-10-17 (USER):  the recommendation (the explicit Nat depth, D8,
  D9).
- (d) O8:  close it as rules or as gaps?  Recommendation:  M6 closes O8 as
  rules, not as gaps (D10, D11), and keeps D3 (of M4) and D8 (of M3).
  Reasons:  (1) M5 D10 closed the M4 stuck values the same way;  (2) the
  rules read the line or the type argument, not the type of a value; (3) s4
  records the remaining neutral cases and checks substitution through data
  that contains HIT fields.
  RULED 2026-10-17 (USER):  the recommendation (O8 closes as rules).
- (e) Open items:  which close in M6?  Recommendation:  O12 (c) and O13 stay
  open (M6 needs no boundary correction and no full QIIT form).  O4 can close
  with the `inv` CIRCUIT_LOWER row (s5).  The s7 review note (b) and s3 (b)
  go to s4 (D12).  s4 fixes the `loop i` printer quirk (M5 s2 (a)) and lists
  each changed row.  M5 s2 (b) goes to s1 (D3).  M5 s2 (c), s5 and the s6
  option stay as they are.  A new O14 (F6 uniqueness at a HIT).
  RULED 2026-10-17 (USER):  the recommendation.
- (f) Slices:  s1 to s7 of section 10?  Recommendation:  yes.  Reasons:
  (1) `elim` at the carriers (s1, s2) comes before each proof that uses it;
  (2) the Kan rules (s4) are apart from the laws, so each mutant set stays
  small;  (3) the lowering (s5, s6) comes last, so s1 to s4 keep the IR check
  clean.
  RULED 2026-10-17 (USER):  the recommendation (slices s1 to s7).
- (g) `elim` at the core carriers Nat, Option, Sum E and List?
  Recommendation:  yes (D3, D6).  Reasons:  (1) without it, the laws at a
  variable and the F6 uniqueness law have no proof term;  (2) the other
  choice, rules that read the shape of a function on a stuck value (such as
  `bind m pure = m`), puts confluence at risk;  (3) it reuses ELIM_CASES,
  ELIM_CASE and the M5 IH rule.
  RULED 2026-10-17 (USER):  the recommendation (`elim` at the carriers).
- (h) The hint surface:  a binder or a core type?  Recommendation:  the
  circuit binder `hint h := e` and the ordinary core operation `inv` for
  witness calculation (D14).  Only a residual `inv` in a constraint normal
  form is refused.  Reasons: (1) a core type `Hint A` with computing laws
  reduces `bind (pure e) f` to `f e`, so the checker sees `h = e`, which the
  circuit does not constrain;  (2) a binder keeps h a variable in the circuit
  relation; (3) residual `inv` in a constraint gives CIRCUIT_LOWER, the first
  gate row of O4.
  RULED 2026-10-17 (USER):  the recommendation (the `hint` binder, `inv`).

## 10. Slices

Each slice is one session or more.  Each prompt is in ~/Documents and reads
~/Documents/glue-lang-M6-COMMON.md for the gates, the IR check, the close
steps and the shared traps.

| Slice | Content | Prompt |
|---|---|---|
| s1 | `append`, List bind through append, `elim` at Option, Sum E and List;  the monad laws by refl and by proof terms (D1 to D4, D15, D16);  examples/monad-laws.lang;  tests. | glue-lang-M6-s1-PROMPT.md |
| s2 | The Nat successor view, `elim` at Nat, the fold laws by refl, F6 uniqueness at Nat, List and a plain family (D5 to D7);  examples/fold-laws.lang;  tests. | glue-lang-M6-s2-PROMPT.md |
| s3 | The unfold step through `option`, the F7 form into Nat, the F7 laws, unfold uniqueness (D8, D9);  examples/unfold-laws.lang;  tests. | glue-lang-M6-s3-PROMPT.md |
| s4 | O8:  the closed data rules, the HCOMP_BASE probe, the neutral rules, the i0 branch drop, the printer quirk (D10 to D12);  tests. | glue-lang-M6-s4-PROMPT.md |
| s5 | The two monads at lowering:  `hint`, `inv`, fr_inv, the witness fill, the hint circuit, the CIRCUIT_LOWER row (D13, D14);  tests. | glue-lang-M6-s5-PROMPT.md |
| s6 | Smoke:  the fold and unfold circuits, the nonzero hint circuit (same-ir rows where they apply). | glue-lang-M6-s6-PROMPT.md |
| s7 | Close:  SPEC 2.8 (new), 4 and 9, FORMERS, PLAN M6 DONE, STATUS `## M6`, the mutant roll-up, review, the M7 kickoff prompt. | glue-lang-M6-s7-PROMPT.md |

Gates for each slice (all GREEN before staging):  `make -B check`,
`make check-clang`, `forge build --root contracts --sizes`, then `make e2e`
last.  `cargo fmt --check` and clippy only if halo2/ changes (M6 plans no
halo2/ change).

IR check for each slice:  after `make e2e`,
`git diff --exit-code -- test/circuit contracts/src/verifiers`.  If it is
clean, restore contracts/test/fixtures from HEAD and run
`forge test --root contracts` again (33 passed at the M5 close).  s5 and s6
add goldens, so the diff is nonzero before e2e:  compare it before and after
e2e.  If a current verifier changed, stop:  M6 must not change the IR of a
current circuit.
