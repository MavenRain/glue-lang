# glue-lang status

The build implements milestone M1 of docs/PLAN.md: the vertical slice from
a `circuit` declaration to a Solidity verifier. It has the lang-template
core of M0 (the sample domain, the former checker, the JSON target). M2
(the interval, Path types and the face lattice) is DONE: see `## M2`.

## Implemented

- `Field`: the BN254 scalar field Fr, literals and infix `+ - *`
  (src/front/fr.c, 32-bit limbs).
- `circuit NAME public (...) private (...) := C; C; ...`: constraints
  `a == b` on Field terms and `range8 e`. Helper definitions inline at
  lowering (examples/poly.glue).
- Lowering to Plonkish IR v1 (docs/IR.md): one `arith` gate, one `range8`
  lookup, copies. `langc circuit`, `langc witness`, `langc satcheck`.
- `glue-halo2` (halo2/): PSE halo2 KZG on BN254 with SHPLONK. `check`,
  `keygen`, `prove`, `verify`, `solidity`.
- `contracts/`: one Solidity verifier for each example circuit, the Forge
  tests and the deploy script.
- `make e2e`: for each example circuit (square, range, poly), the whole
  chain from `langc circuit` to `forge test` and a simulation of the
  deploy script.

## Remaining work

1. M3: the Kan operations `transp` and `hcomp`, Glue, `ua` and uaBeta,
   application of a partial element, and the tests `round` and `moved`
   (docs/PLAN.md, M3 row; `### s3` below).

## Known limits

- `make e2e` uses the INSECURE development SRS (`--dev-srs`). A real
  deploy needs `--srs FILE` from a trusted setup.
- The prover is randomized. Each `make e2e` writes new proofs to
  `contracts/test/fixtures/`.
- CIRCUIT_UNCONSTRAINED is a syntactic check (SPEC.md O3). A private
  binder that is only in `x == x` passes it.
- No refusal test for CIRCUIT_LOWER (SPEC.md O4). M3 s5 found no circuit
  that gives it: a circuit has only Field binders, thus the closed-type
  rules reduce each Kan term in a constraint.
- The Solidity generator takes one instance column, no rotated instance
  query and at least one advice column (halo2/README.md).

## Internal boundaries

The C compiler (`src/`, TinyCC) holds the front end, the lowering and the
witness. `test/gate.sh` checks the examples, the refusal tables, the
circuit goldens (`test/circuit/`), the same-IR table
(`test/circuit/same-ir.txt`: a circuit in a second example file must
give an IR golden byte for byte)
and the satcheck refusals (`test/satcheck/`). docs/IR.md is the only interface between the compiler
and the back end.

The halo2 back end (`halo2/`, Rust) reads IR v1 and refuses each document
that IR.md does not allow. `cargo test --manifest-path halo2/Cargo.toml`
checks the hand fixtures and the compiler goldens.

The Foundry project (`contracts/`) holds the generated verifiers. Forge
tests check each verifier against its proof fixture. The deploy script
selects a verifier by name and reads the key and the chain from the forge
flags.

## M1 integration fixes (session 4)

- docs/IR.md section 2: the k rule said `2^k` at least each table length.
  That gives k 8 for `range`, and the back end refuses that document (the
  256-row table does not fit in the 250 usable rows). IR.md now has the
  compiler rule: `max(rows, L) + 16 <= 2^k`.
- docs/IR.md section 5: the generated verifier has
  `public view returns (bool)`, not `external returns (bool)`. The
  selector and the ABI are the same, so IR.md now gives the generated
  signature.
- contracts/: the Forge test and the deploy script covered `square` only.
  They now cover each example circuit. The verifiers moved from
  `src/Halo2Verifier.sol` to `src/verifiers/NAME.sol`.

## M2

The plan is docs/M2-PLAN.md. Each slice is one session (section 10).

### s1: interval and Path core (2026-10-14)

- Syntax: the interval `I` with `i0`, `i1`, `~r`, `r /\ s` and `r \/ s`;
  the path abstraction `<i> t`; the path application `p @ r`; the formers
  `Path A a b` and `PathP A a b`. src/front/interval.c keeps an interval
  term in a normal form. `Eq` and its M1 rules stay (s3 removes them).
- New check codes: PATH_EXPECTED, PATH_BOUNDARY, PATH_TYPE, INTERVAL_TERM,
  INTERVAL_LIMIT. Tests: examples/paths.lang, test/json/paths.json, 6
  check refusals, 3 parse refusals, 6 eval rows.
- Reserved names: `transp`, `hcomp`, `Glue`, `ua`, `Partial`, `SSet`.
- Deviation from M2-PLAN section 7: in an interval position, an unknown
  name gives NAME_UNKNOWN or REFUSE_REC, not INTERVAL_TERM. The reason:
  elab_ivl sends the name to resolve_head. A local term variable, a
  definition or a core name in an interval position gives INTERVAL_TERM.
- Gates: `make check` exit 0 (examples 15, parse refusals 13, check
  refusals 40, evals 36, json 12, build refusals 2, circuits 3, circuit
  refusals 10, satcheck refusals 6, native circuit regressions 9 and 0
  failed, gate 0 failures, regressions 6 and 0 failed, printer 2).
  `make check-clang` clean. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (square, range, poly).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. The IR and the verifiers did not change. contracts/test/fixtures
  is restored from HEAD, and `forge test --root contracts` passes again
  (33 passed, 0 failed).
- Mutants (`make -B` for each, source restored after): 3 KILLED, 0
  SURVIVED.
  1. Boundary check off (check.c check_plam, `0 &&` before
     `conv_values`): KILLED. The check rows path-boundary-i0.lang and
     path-boundary-i1.lang give exit 0, not PATH_BOUNDARY.
  2. `~` as the identity (eval.c eval_ivl, case IVL_REV): KILLED, 9 gate
     failures. examples/paths.lang gives PATH_BOUNDARY for `reversed`. The
     6 paths eval rows, the paths JSON build and interval-limit.lang
     (PATH_BOUNDARY, not INTERVAL_LIMIT) fail.
  3. Endpoint rule off (eval.c papp_value, `0 &&` before `ends != NULL`):
     KILLED, 8 gate failures. The checker refuses `middle` with
     PATH_BOUNDARY (`p @ i0` does not reduce to `1`). examples/paths.lang,
     the 6 paths eval rows (`startOf` is one of them) and the paths JSON
     build fail.

### s2: refl, sym, cong, funExt and happly (2026-10-14)

- New core names: `sym` (1 argument), `funExt` (1), `happly` (2). Each is
  a checker rule (D4). There is no new Op. `refl` checks against a PathP
  with a constant line and equal ends. `cong` accepts a Path argument. The
  M1 `Eq` cases of `refl` and `cong` and the M1 `symm` stay (s3 removes
  them).
- Core terms: `refl` writes `<i> a`, `sym p` writes `<i> p @ ~i`, `cong f
  p` writes `<i> f (p @ i)`, `funExt h` writes `<i> fun x => h x @ i`,
  `happly p x` writes `<i> (p @ i) x`. `funExt` checks against a constant
  Path over a Pi type. With no expected type, it reads `f` and `g` from the
  ends of the codomain of `h`.
- Helpers in check.c: core_shift (de Bruijn weakening), core_papp
  (infer_papp uses it too), close_over, under_ivl, line_type.
- Limits: a path abstraction needs a declared type, so `sym (<i> 1)` gives
  PATH_EXPECTED. Thus the law `sym refl = refl` uses the definition
  `reflOne : Path Nat 1 1 := refl`. `happly` takes 2 arguments, so the law
  `funExt (happly p) = p` writes `fun x => happly p x`.
- Tests: examples/paths.lang has each law of M2-PLAN section 4 as a
  `refl` (14 new definitions). test/json/paths.json does not change,
  because a Path definition writes no instance. 3 check refusals:
  no-proof-irrelevance.lang (TYPE_REFL), de-morgan.lang (TYPE_REFL,
  `i /\ ~i` is not `i0`), reserved-sym.lang (REFUSE_NAME). 2 eval rows:
  `symStart` (`sym p @ i0` gives 2) and `happlyFunExt`.
- Gates: `make check` exit 0 (examples 15, parse refusals 13, check
  refusals 43, evals 38, json 12, build refusals 2, circuits 3, circuit
  refusals 10, satcheck refusals 6, native circuit regressions 9 and 0
  failed, gate 0 failures, regressions 6 and 0 failed, printer 2).
  `make check-clang` clean. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (square, range, poly; 33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. The IR and the verifiers did not change. contracts/test/fixtures
  is restored from HEAD, and `forge test --root contracts` passes again
  (33 passed, 0 failed).
- Mutants (`make -B` for each, source restored and compared after): 3
  KILLED, 0 SURVIVED.
  1. `sym` with no `~` (check.c rule_sym, `rev = core_index(c, CORE_VAR,
     0u)`): KILLED, 1 gate failure. The eval row `symStart` gives
     `fun p => 1`, not `fun p => 2`.
  2. `cong` with no application of `f` (check.c cong_path, `core_bind(c,
     CORE_PLAM, "i", NULL, at)`): KILLED, 10 gate failures. The checker
     refuses `congRefl` with TYPE_REFL (`<i> 1` against `<i> f 1`).
     examples/paths.lang, the 8 paths eval rows and the paths JSON build
     fail. `congId` and `congComp` do not find this mutant, because each
     side becomes `p`.
  3. `funExt` with `f` and `g` exchanged (check.c funext_type, `tyop(c,
     OP_PATHP, 3, line, g, f)`): KILLED, 10 gate failures. The checker
     refuses `happlyFunExt` with TYPE_MISMATCH (`Path Nat (g x) (f x)`
     against `Path Nat (f x) (g x)`). The same 10 rows fail.
     `funExtRefl` does not find this mutant, because its ends are equal.

#### Staged review (2026-10-14)

- Fixed dependent `cong` against a constant expected path. For example,
  `cong f refl` with `f : (x : A) -> B x` must check at
  `Path (B a) (f a) (f a)`. The rule now preserves an inferred dependent
  function type before using the expected path to check a lambda.
- Added test/path-regressions.c to `make check`: 12 native C cases for
  dependent path operations and refusal of invalid `cong` arguments. Three
  cases reproduce the defect on the original staged checker.
- Validation on an isolated copy of the index: `make check` passed,
  including the 12 new cases; the new C harness passed strict Clang checks.
  The e2e target passed using the existing halo2 binary (33 Forge tests).
  Generated verifiers match the index byte for byte. The repository's
  original proof fixtures were preserved.

### s3: Path replaces F12 and F13; `==` is Path Field (2026-10-14)

- Removed: the core names `Eq`, `symm`, `trans` and `transport`, and the
  ops OP_EQ, OP_REFL, OP_SYMM, OP_TRANS, OP_TRANSPORT and OP_CONG (core.h,
  the eval.c tables and dispatch, reduce_proof, json.c). `refl` and `cong`
  keep only the Path rules (refl_path, cong_path). Their BUILTINS rows use
  OP_PATHP as a placeholder op. `cong` with an argument that is not a path
  gives PATH_EXPECTED (M1 gave TYPE_MISMATCH). `Eq` is an unknown name
  (NAME_UNKNOWN). `trans` and `transport` are reserved names (REFUSE_NAME,
  D8).
- Circuits (ruling b): for each constraint `a == b`, the checker forms the
  core type `Path Field a b` (check.c field_path). ConstraintInfo has one
  core term: the Path type for EQ, the operand for RANGE8. The lowering
  (circuit.c lower_check) evaluates the term and reads the two ends of the
  Path value. EQ_FIELD stays for a side that is not Field.
- Examples: equality.lang uses `Path` and `sym` (`same`, `back`,
  `bumped`). laws.lang and sigma.lang use `Path`. field.lang has `sqRel :
  Field -> Field -> Type 0` and `sqThree : sqRel three nine := refl`.
- Goldens: test/json/sigma.json has the type text `Sigma (n : Nat) (Path
  Nat n 1200)`. The equality, laws and field JSON did not change. Eval
  rows: `sigma offer` gives `pack 1200 (<i> 1200)`. The row `equality
  moved` is removed. New rows: `equality back` (`<i> 1200`), `equality
  bumped` (`<i> 1201`), `field sqThree` (`<i> 9`), `field sqRel`.
  bad-refl.lang and bad-eta.lang use `Path`. test/gate.sh checks the Path
  message `the start normalizes to 1201, the end normalizes to 1200`. New
  check rows: unknown-eq.lang (NAME_UNKNOWN), reserved-trans.lang and
  reserved-transport.lang (REFUSE_NAME), bad-sqrel.lang (TYPE_REFL for
  `sqRel three ten`). In the old node harnesses test/regressions.js and
  test/printer-regressions.js, only the strings changed (`Eq` to `Path`).
- FORMERS F12 and F13 (text and matrix) change in s5 (close).
- M3 tests (removed in s3; they come back with `hcomp` and `transp`):
  1. `round : Path Nat price price := trans same back`
     (examples/equality.lang).
  2. `moved : (P : (n : Nat) -> Type 0) -> (u : P price) -> P cost :=
     fun P u => transport P same u`, with the eval row `equality moved =>
     fun P => fun u => u`.
- Gates: `make check` exit 0 (examples 15, parse refusals 13, check
  refusals 47, evals 41, json 12, build refusals 2, circuits 3, circuit
  refusals 10, satcheck refusals 6, native circuit regressions 9 and 0
  failed, native path regressions 12 and 0 failed, gate 0 failures,
  regressions 6 and 0 failed, printer 2). `make check-clang` clean. `forge
  build --root contracts --sizes` exit 0. `make e2e` exit 0 (33 Forge
  tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. The IR and the verifiers did not change. contracts/test/fixtures
  is restored from HEAD, and `forge test --root contracts` passes again
  (33 passed, 0 failed).
- Mutants (`make -B` for each, source restored after): 2 KILLED, 0
  SURVIVED.
  1. The lowering reads the ends swapped (circuit.c lower_check, `args[2]`
     as the left side, `args[1]` as the right side): KILLED, 4 gate
     failures (the 3 circuit goldens and the circuit row with no `-o`).
     satcheck does not find this mutant, because a copy is symmetric.
  2. `Eq` as an alias of `Path` (check.c BUILTINS row `{"Eq", 3,
     RULE_PATH, OP_PATHP, OP_NAT, 0}`): KILLED, 1 gate failure
     (unknown-eq.lang exits 0).

### s4: face lattice, Partial and systems (2026-10-14)

- New file src/front/face.c (face.h): face_norm, face_meet, face_join,
  face_equal, face_leq, and conjunction k as a substitution. The Makefile
  and test/regressions.js list it.
- Syntax: the lexer reads `[`, `]` and `,`. The parser reads a system
  `[ phi -> t, psi -> u ]` (TERM_SYS: left = face, right = body, next =
  the next branch). New parse rows: sys-no-body.lang and sys-unclosed.lang.
- Core (design call 4 changed): a system is CORE_OP and VAL_OP with OP_SYS
  (inst = the branch count, args face0, body0, face1, body1, ...), not a
  new CoreKind or ValKind. Quote walks the op arguments. Eval normalizes
  faces and reduces full branches. Conversion compares covers and the
  bodies under each overlap. New ops: OP_INTERVAL and OP_SSET (0-ary),
  OP_PARTIAL (args face,
  A). eval.c keeps each face of a system and of a Partial in face normal
  form, so conv compares faces by normal form.
- Interval binders: `fun (i : I) => t` and `(i : I) -> T` bind with
  push_ivl. The domain of the Pi and of the lambda is the op OP_INTERVAL.
  The value of such a lambda keeps this domain, so quote, print and the
  eta rule of conv apply the body to an interval variable. Conversion,
  quote and print also open an interval Pi with an interval variable. In an
  application `f r` to the domain I, the checker elaborates r with
  elab_ivl.
- SSet: a Pi with the domain I, a Pi with a domain or a codomain in SSet,
  and `Partial phi A` are in SSet. `SSet` is a sort: it is not a binder
  type. A bare `I` as the type of a path stays PATH_TYPE.
- Codes: SORT_FIBRANT (a type in SSet as the type of a path, as a Sigma
  component, under a type former, or as an instance; SSet itself as a
  binder type),
  PARTIAL_EXPECTED (a system without a declared type `Partial phi A`),
  FACE_AGREE and FACE_COVER.
- Instance rule (rule b): an instance with the type `Partial phi A` gives
  SORT_FIBRANT (json.c instance_json). The checker refuses a type in SSet
  under a type former, so only the instance type itself can be in SSet. A
  function with an interval binder is not an instance.
- Restriction method: for each conjunction of a branch face, the checker
  builds the scope and the environment again from level 0. An interval
  level that the conjunction sets becomes a constant (val_ivl_end). The
  type of each local term is eval(quote_value(type, its level)) in the
  restricted environment below it. The checker checks the body against
  eval(quote_value(A, level)) in the restricted environment. Each
  conjunction has a separate core branch, so a specialized body is used
  only on the face where it checks.
- A branch on the empty face (no conjunction) is checked in the context as
  it is. The checker checks FACE_COVER before FACE_AGREE. The FACE_AGREE
  message names the two branch faces, their meet face and the two values.
- Application of a partial element is not in s4.
- Tests: examples/faces.lang (sysEnds, sysMeet, endPt, sysSplit, ivlFun,
  ivlAt0, ivlAt1, ivlRev, sysRev) and test/json/faces.json. sysSplit is
  the empty face row: the meet `i /\ ~i` is empty, so the branches need
  not agree. endPt agrees only by the end point rule in the restricted
  environment. New check rows: face-agree.lang and face-meet.lang
  (FACE_AGREE), face-cover.lang (FACE_COVER), sset-path.lang,
  sset-sigma.lang and sset-binder.lang (SORT_FIBRANT), partial-expected.lang
  (PARTIAL_EXPECTED). New build refusal row: partial-instance.lang
  (SORT_FIBRANT). New eval rows: 8 rows `faces ...` (for example `faces
  ivlAt0 => 3` and `faces sysRev => fun i => [ i -> 0, ~i -> 1 ]`).
- Gates: `make check` exit 0 (examples 16, parse refusals 15, check
  refusals 54, evals 49, json 13, build refusals 3, circuits 3, circuit
  refusals 10, satcheck refusals 6, native circuit regressions 9 and 0
  failed, native path regressions 12 and 0 failed, gate 0 failures,
  regressions 6 and 0 failed, printer 2). `make check-clang` clean. `forge
  build --root contracts --sizes` exit 0. `make e2e` exit 0 (33 Forge
  tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. The IR and the verifiers did not change. contracts/test/fixtures
  is restored from HEAD, and `forge test --root contracts` passes again
  (33 passed, 0 failed).
- Mutants (`make -B` for each, source restored after): 3 KILLED, 0
  SURVIVED.
  1. Agreement check off (check.c branches_agree, `if (0 &&
     !conv_values(...))`): KILLED, 2 gate failures (face-agree.lang and
     face-meet.lang exit 0).
  2. Cover check off (check.c check_sys, `if (0 && !face_equal(cover,
     ...))`): KILLED, 1 gate failure (face-cover.lang exits 0).
  3. face_norm keeps `i /\ ~i` (face.c conj_consistent returns 1):
     KILLED, 10 gate failures (examples/faces.lang gives FACE_AGREE on
     sysEnds, the 8 eval rows `faces ...`, the build of
     examples/faces.lang).

### s4 review fixes (2026-10-14)

- HIGH: check_sys kept only the last core body for a face with several
  conjunctions. A branch `[ i \/ ~i -> refl ]` could then give `<i> 0` at
  the end of a `Path Nat 1 1`. Each conjunction now has its own core branch.
  The checker checks agreement between those branches too.
- MEDIUM: conversion, quote and print opened a dependent interval Pi with
  an ordinary variable. A valid alias of `(i : I) -> Partial i Nat` gave
  INTERNAL. Each operation now opens that Pi with an interval variable.
- MEDIUM: system conversion compared the branch arrays. Equal partial
  elements with different branch order, duplicate branches or different
  face decompositions failed as dependent indices. Conversion now compares
  covers and compares the bodies under each conjunction of each overlap.
  It preserves free interval variables and distinct partial values.
- test/face-regressions.c adds 20 native C cases to `make check`. The
  original staged source fails 15 cases. The fixed source passes all 20.
  AddressSanitizer and UndefinedBehaviorSanitizer also pass all 20.
- Validation after the fixes: `make check` (including check-clang),
  `forge build --root contracts --sizes` and `make e2e` pass. The IR and
  verifier diff is empty. After restoring the generated proof fixtures,
  `forge test --root contracts` passes all 33 tests again.

### s5: close (2026-10-14)

Summary of M2. glue-lang has the De Morgan interval `I` (`i0`, `i1`, `~`,
`/\`, `\/`), the dependent path type `PathP (<i> A) a b` with the sugar
`Path A a b`, path abstraction `<i> t` and path application `p @ r`.
`refl`, `sym`, `cong`, `funExt` and `happly` are checker rules that write
ordinary path terms, so their laws hold by computation. `Path` replaces the
template formers F12 and F13. `a == b` in a circuit is the type
`Path Field a b`, and the IR did not change. The face lattice gives
`Partial phi A`, systems and interval binders in the sort `SSet`. SPEC.md
section 2.4 is the reference. No definitional proof irrelevance: two
different variables of one path type do not convert.

- Docs: SPEC.md 2.2 (`==` is `Path Field a b`), 2.4 (new: syntax, interval
  values, typing rules, evaluation and conversion, derived operations,
  faces, JSON, M2 refusal codes), 3 (F12 and F13 rows), 9 (O1 closed; O5,
  O6 and O7 new). formers/FORMERS.md: the glue-lang text of F12 and F13
  and their tcc-json matrix cells. docs/PLAN.md: M2 DONE; the M3 row lists
  application of a partial element and the tests `round` and `moved`.
- Mutant roll-up (each with `make -B`, source restored after): 11 KILLED,
  0 SURVIVED.

| Slice | Mutant | Result |
|---|---|---|
| s1 | boundary check off (check_plam) | KILLED |
| s1 | `~` as the identity (eval_ivl) | KILLED |
| s1 | end point rule off (papp_value) | KILLED |
| s2 | `sym` with no `~` (rule_sym) | KILLED |
| s2 | `cong` with no application of `f` (cong_path) | KILLED |
| s2 | `funExt` with `f` and `g` exchanged (funext_type) | KILLED |
| s3 | the lowering reads the ends swapped (lower_check) | KILLED |
| s3 | `Eq` as an alias of `Path` (BUILTINS) | KILLED |
| s4 | agreement check off (branches_agree) | KILLED |
| s4 | cover check off (check_sys) | KILLED |
| s4 | face_norm keeps `i /\ ~i` (conj_consistent) | KILLED |

- Review of the M2 diff (`git diff 503431c..HEAD -- src`). The two review
  agents stopped on API errors, so the review is a targeted read in the
  main session. Result: no defect found.
  1. Proof irrelevance: conv_loop, conv_path, conv_sys and value_on_face
     (eval.c) compare values by their structure. A neutral path converts
     to a path abstraction only by path eta. VAL_PAPP compares the interval
     values and then the paths. Two systems convert when their covers are
     equal and their bodies convert under each conjunction of each overlap.
     A neutral partial element does not convert to a system. No rule reads
     the type of the two values.
  2. Switch cases: `make check-clang` uses `-Wswitch-enum -Werror`. The
     only `default:` arms in src are lexer.c (a switch on a character) and
     circuit.c fixed_cell (M1, not in the M2 diff).
  3. Boundaries: path_former (check.c) checks `a` against the line at `i0`
     and `b` against the line at `i1`. check_plam converts both ends
     (mutant s1-1). check_sys checks FACE_COVER and then FACE_AGREE for
     each conjunction (mutants s4-1 and s4-2, the s4 review fixes).
- Gates: `make -B check` exit 0 (examples 16, parse refusals 15, check
  refusals 54, evals 49, json 13, build refusals 3, circuits 3, circuit
  refusals 10, satcheck refusals 6, native circuit regressions 9, native
  path regressions 12, native face regressions 20, each with 0 failed;
  gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0. `make
  e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).

## M3

### s1: transp, Partial at i1, and the M3 plan (2026-10-14)

- Plan: docs/M3-PLAN.md (new) is the M3 plan with the USER rulings
  applied.
- D10 names: `transp` is a checker rule (BUILTINS row, RULE_TRANSP), so it
  is not in RESERVED. RESERVED adds the 14 names of the later slices:
  `comp`, `hfill`, `subst`, `transportRefl`, `J`, `JRefl`, `isContr`,
  `fiber`, `isEquiv`, `Equiv`, `idEquiv`, `glue`, `unglue`, `uaBeta`. A def
  with one of these names, or with the name `transp`, gives REFUSE_NAME
  (one test/check/reserved-NAME.lang file each).
- Core: core.h adds OP_TRANSP, OP_KAN_DOM and OP_KAN_COD. eval.c has their
  names, arities and reduce_op cases. src/front/kan.c and kan.h (new) have
  kan_transp, kan_dom and kan_cod. Each rule is a core term (D7). The
  Makefile FRONT list and test/regressions.js have kan.
- Typing (check.c rule_transp): `transp (<i> A) phi a`. The first argument
  must be a line `<i> A` with `A` in Type n (PATH_TYPE if not, also for a
  line in I). `a` has the type `A[i0]`. The line must be constant on each
  conjunction of `phi` (TRANSP_CONSTANT). The type is `A[i1]`.
- Evaluator (kan.c kan_transp): the face `i1` gives `a`. Only the head of
  the line at a fresh interval variable is read. Type n, Nat, Field, Unit
  and Flag give `a`. Pi, Sigma and Prod give the rule terms of M3-PLAN 3.4.
  Each other head gives a stuck `transp`.
- D4 (eval.c reduce_partial): `Partial i1 A` is `A`. restrict_ctx evaluates
  each type again under the face. check_sys accepts total systems against
  the reduced expected type `A`, preserving branch typing and agreement.
- Conversion: no new code. The VAL_STUCK case of conv_loop compares the
  arguments: the line by path eta at a fresh interval variable, the face by
  its normal form. Row: stuckEq in examples/kan.lang.
- Quote and print: no new code. A stuck value prints as
  `transp (<i> A) phi a`.
- JSON: json.c gives INTERNAL for a Kan operation as a type former (a type
  is never one). A stuck `transp` in an instance value gives JSON_VALUE
  ("the instance holds a stuck term"). Row: test/emit/stuck-transp.lang.
  SPEC section 9 gets this code in s6.
- Guest tests: examples/kan.lang (new) and test/json/kan.json. The example
  has `transp` at the face `i1`; on Nat, Field, Type 0, Pi (eta, and a
  domain that changes), Sigma and Prod; the 4 s1 laws of M3-PLAN section 4
  as `refl` rows; a stuck `transp` with a variable line; two equal stuck
  values that convert; `transp (<i> A (i /\ j)) ~j a`; the D4 rows d4Var
  (`u i` under the face of `u`) and d4Path (`(u i) @ j`). 13 eval rows
  (kan). Check rows: transp-constant.lang and transp-meet.lang
  (TRANSP_CONSTANT), transp-interval.lang (PATH_TYPE), transp-guard.lang
  (TYPE_REFL: `refl : Path Nat (transp (<_> Nat) i0 x) y` is refused).
  Build row: stuck-transp.lang (JSON_VALUE).
- The Pi line with a domain that changes has this normal form:
  `fun P => fun x => fun y => fun p => fun f => fun _x0 => f (transp (<j> P (p @ ~j)) i0 _x0)`.
  `f` gets a `transp` of its argument, not the argument.
- D4 changed one guest row. test/emit/partial-instance.lang was
  `def px : Partial i1 Nat := [ i1 -> 0 ]`. With D4 its type is Nat and
  the instance emits `0`. The refusal fixture is now
  `def px : Partial i0 Nat := [ i0 -> 0 ]` (SORT_FIBRANT, exit 1, no
  stdout: the same row intent). test/check/partial-expected.lang also uses
  `i0`, so an incomplete system against Nat still gives PARTIAL_EXPECTED.
  Full-face systems are positive native C regressions.
- Mutants (each with `make -B check`, source restored after): 3 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| TRANSP_CONSTANT check off (rule_transp) | check transp-constant.lang and transp-meet.lang |
| Pi rule passes `y` for `w i0` (kan.c pi_rule) | eval kan piLine |
| `Partial i1 A = A` rule off (reduce_partial) | examples/kan.lang (d4Var: TYPE_MISMATCH) |

- The Pi mutant is `app(m, var(m, 1u), (zero(m), var(m, 0)))`. Without the
  call to `zero`, `make check-clang` stops on an unused function, and no
  guest row runs.
- Gates: `make -B check` exit 0 (examples 17, parse refusals 15, check
  refusals 73, evals 62, json 14, build refusals 4, circuits 3,
  circuit refusals 10, satcheck refusals 6, native circuit regressions 9,
  native path regressions 12, native face regressions 20, each with 0
  failed; gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0. `make
  e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).
- Open items:
  - O8 (new, M3-PLAN 3.4): `transp` stays stuck at Sum, Option and List
    (rule in s3), at PathP (rule in s2), and at Family and the fold and
    unfold families (stuck in M3).
  - For the USER: `def px : (i : I) -> Partial i Nat := fun i => [ i -> 0 ]`
    gives exit 0 for `langc build`. Maybe a def with a type in SSet is not
    refused when the type is a Pi over I. Not in s1 scope.

- Review fixes (2026-10-14): total systems had failed with PARTIAL_EXPECTED
  after their expected Partial type reduced to its base type. check_sys
  now handles that reduction. Twelve added C cases cover full-face
  systems, application, path boundaries, nested restriction, disagreement,
  wrong branch types, incomplete systems, and dependent Pi, Sigma and Prod
  transport. Native face regressions: 32 checked, 0 failed. `make -B check`
  and `make check-clang` pass. The end-to-end gate passes all 33 Forge
  tests using the unchanged Halo2 binary. Circuit IR and verifiers remain
  unchanged.

### s2: hcomp, comp and hfill; Kan rules for PathP (2026-10-15)

- Files: src/front/core.h, eval.c, check.c, kan.c and kan.h, and
  src/json.c. Guest tests: examples/kanpath.lang and test/json/kanpath.json
  (new), test/check/hcomp-base.lang and comp-base.lang (new),
  test/check/expect.txt and test/eval/expect.txt.
- Core: core.h adds OP_HCOMP (args: A, the face, u, u0). json.c gives
  INTERNAL for OP_HCOMP as a type former, as for the other Kan operations.
  core.h also declares value_bound (with VALUE_BOUND_OPEN) and
  value_on_face. kan.c uses them.
- Typing (check.c rule_hcomp): `hcomp A phi u u0 : A`. `A` is in Type n,
  `u : (i : I) -> Partial phi A` and `u0 : A`. HCOMP_BASE: on each
  conjunction of `phi`, `u i0` must convert to `u0`. Rows:
  test/check/hcomp-base.lang and comp-base.lang (`[ i1 -> 1 ]` with the
  base `0`, HCOMP_BASE).
- comp and hfill are checker rules that write an hcomp core term (D2):
  - rule_comp: `comp (<i> A) phi u u0 : A[i1]`, with
    `u : (i : I) -> Partial phi A[i]` and `u0 : A[i0]`. The core term is
    `hcomp A[i1] phi (fun i => [ phi -> transp (<j> A[i \/ j]) i (u i) ]) (transp (<i> A) i0 u0)`.
  - rule_hfill: `hfill A phi u u0 r : A`, with the premises of hcomp and an
    interval term `r`. The core term is
    `hcomp A (phi \/ ~r) (fun i => [ phi -> u (i /\ r), ~r -> u0 ]) u0`.
- Evaluator (kan.c kan_hcomp, M3-PLAN 3.4). The rules apply in this order:
  - The face `i1` gives `u i1`.
  - A closed type (Nat, Field, Unit or Flag, not Type n) gives `u0` if `u`
    is constant (hcomp_constant). The test applies `u` to a fresh interval
    variable. Each branch of the system must be `u0` on each conjunction of
    its face. A value that is not a system must convert to `u0`. Else the
    hcomp is stuck.
  - Pi: an hcomp at each argument.
  - Sigma: pair_fill makes the filler `H` of the first component, then
    hcomp_pair_rule gives `pack (hcomp A phi u1 (p1 p)) (comp (<i> B(H i)) phi u2 (p2 p))`.
    The comp is the core term of rule_comp.
  - Prod: the Sigma rule with a constant second type.
  - PathP: hcomp_pathp_rule gives `<j> hcomp A[j] (phi \/ ~j \/ j) ...`,
    with a branch for each end of the path.
  - Each other head gives a stuck hcomp.
- kan_transp: the PathP rule (pathp_rule) follows the Prod rule. transp
  along a PathP line gives `<j> hcomp` with three branches: `phi`, `~j` (the
  left end) and `j` (the right end). The rule pushes the transp into each
  branch, so there is no nested system. line_at accepts OP_PATHP. kan_dom of
  a PathP gives its inner line. kan_cod of a PathP at `i0` or `i1` gives that
  end, else INTERNAL ("a PathP end that is not i0 or i1").
- eval.c: a partial element (an OP_SYS value) distributes over each
  eliminator. sys_apply gives the system of the branches applied to the
  argument. sys_project gives the system of the projected branches.
  sys_papp gives the system of the branches at `r`, after the ends test of
  papp_value.
- Conversion (part c): no new code. conv_sys joins the faces, so a branch
  at the face `i0` has no effect. reduce_sys is unchanged. No conv rule
  reads a type. `hcomp A i0 u u0 = u0` is not a rule (D8).
- Row change: `kan sigmaLine` now gives
  `fun p => pack (witness p) (<j> (payload p) @ j)`. The PathP transp rule
  computes the old stuck `transp (<i> Path Nat ...) i0 (payload p)`. The
  result is the eta form of `payload p`, so the row is correct.
- Guest tests: examples/kanpath.lang has 15 eval rows (kanpath):
  hcompAt1, constBase, stuckBase, piHcomp, compConst, fillAt0, fillAt1,
  sigmaHcomp, prodHcomp, prodStuck, pathEnd, pathSquare, pathHcomp,
  pathHcompEnd and pathHcompStuck. pathEnd and pathHcompEnd are also
  `refl` proofs in the example. test/json/kanpath.json has no instance
  (`{"glue-lang":1,"instances":[]}`).
- hcompAt1 is `fun q => hcomp Nat i1 (fun i => [ i1 -> q @ i ]) 3` (row
  `fun q => 5`). The system uses `i`, so the closed-type rule cannot give
  the result. Only the face `i1` rule can.
- Mutants (each with `make -B check`, source restored after): 3 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| face `i1` rule off (kan_hcomp, `0 &&`) | examples/kanpath.lang (pathEnd: TYPE_REFL, the start is `hcomp Nat i1 ...`); eval kanpath hcompAt1 |
| closed-type rule skips the branch test (hcomp_constant, `return 1;`) | eval kanpath stuckBase |
| PathP transp drops the `~j` branch (pathp_rule face and system) | examples/kanpath.lang (pathEnd: TYPE_REFL, the start is `x`, the end is `y`); eval kanpath pathEnd |

- For mutants 1 and 3, examples/kanpath.lang does not check, so each
  kanpath eval row fails (18 gate failures each). Mutant 2 fails 5 rows:
  stuckBase, fillAt1, prodStuck, pathSquare and pathHcompStuck.
- Gates: `make -B check` exit 0 (examples 18, parse refusals 15, check
  refusals 75, evals 77, json 15, build refusals 4, circuits 3,
  circuit refusals 10, satcheck refusals 6, native circuit regressions 9,
  native path regressions 12, native face regressions 32, each with 0
  failed; gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0. `make
  e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).
- Open items:
  - O8 (s1): the PathP transp rule is now in kan.c. transp stays stuck at
    Sum, Option and List (rule in s3), and at Family and the fold and
    unfold families.
  - O8 (s2, new): hcomp stays stuck at Type n, Sum, List, Option and a
    stuck type. A transp of a system value along a stuck line stays
    stuck.

### s3: coproduct Kan rules, trans, transport, subst and J (2026-10-15)

- Files: src/front/core.h, eval.c, check.c and kan.c, and src/json.c. Guest
  tests: examples/derived.lang and test/json/derived.json (new),
  examples/equality.lang (`round` and `moved`), test/emit/stuck-transp.lang
  (the comment on line 1 only) and test/eval/expect.txt. test/check/ and
  test/json/equality.json do not change.
- Core: core.h adds OP_KAN_FIELD (kan.c only; args: x; inst: the field
  index of the constructor x). eval.c reduce_field gives field `inst` of a
  constructor value. A neutral value gives a stuck value. A system gives the
  system of the fields. Each branch is split into reachable conjunctions,
  then projected under the conjunction's substitutions. Empty faces are
  discarded. This avoids projecting `none` or `nil` on an unreachable
  branch and keeps internal `?` operations out of rendered field tubes.
  Probe levels are shared through kan.h; substitution stays stuck during
  head probes. json.c gives INTERNAL for OP_KAN_FIELD, as for the other
  Kan operations.
- Evaluator (kan.c, M3-PLAN 3.4):
  - data_type (Sum, Option, List) and data_ctor (inl, inr, none, some, nil,
    cons). line_at accepts the data formers. kan_dom gives the left type of
    a Sum and the element type of an Option or a List. kan_cod gives the
    right type of a Sum, else INTERNAL ("a codomain of an Option or List
    line").
  - kan_transp, after the PathP rule: if the head is a data former and `a`
    is a constructor, transp_data applies. none and nil give `a`. inl and
    some give `C (transp (<i> DOM(L, i)) phi x)`. inr gives
    `inr (transp (<i> COD(L, i, i0)) phi x)`. cons gives
    `cons (transp (<i> DOM(L, i)) phi x) (transp L phi xs)`. A neutral `a`
    gives a stuck transp.
  - kan_hcomp, before the stuck return: for a data type, hcomp_data applies
    when `u0` and each branch of `u i` (`i` fresh) have the same
    constructor on each conjunction of the face of the branch
    (hcomp_same_ctor, with value_bound as in hcomp_constant). The result is
    `C (hcomp T phi (fun i => KAN_FIELD n (u i)) x)` for each field
    (field_hcomp). OP_KAN_FIELD distributes over the system, so the tube
    has no nested system. Else the hcomp is stuck.
- Typing (check.c): RULE_TRANS, RULE_TRANSPORT, RULE_SUBST,
  RULE_TRANSPORT_REFL, RULE_J and RULE_J_REFL. Each rule writes a core term
  (D2). Helpers: path_base (PATH_EXPECTED), path_body, transp_zero and
  j_motive.
  - rule_trans: `trans p q : Path A x z`, with `p : Path A x y` and
    `q : Path A y z`. The core term is
    `<i> hcomp A (~i \/ i) (fun j => [ ~i -> x, i -> q @ j ]) (p @ i)`.
    The printer writes the tube binder as `j`. An end mismatch gives
    TYPE_MISMATCH.
  - rule_transport: `transport p x : B`, with `p : Path (Type n) A B` and
    `x : A` (D6). The core term is `transp (<i> p @ i) i0 x`.
  - rule_subst: `subst P p x : P b`, with `P : A -> Type n`,
    `p : Path A a b` and `x : P a`. The core term is
    `transp (<i> P (p @ i)) i0 x`.
  - rule_transport_refl: `transportRefl x : Path A (transp (<_> A) i0 x) x`.
    The core term is `<i> transp (<_> A) i x`.
  - rule_j: `J P d p : P y p`, with `P : (y : A) -> Path A x y -> Type n`,
    `d : P x refl` and `p : Path A x y`. The core term is
    `transp (<i> P (p @ i) (<j> p @ (i /\ j))) i0 d`. j_motive checks `P`
    at a fresh `y` and a fresh path.
  - rule_j_refl: `JRefl P d : Path (P x refl) (J P d refl) d`. The core
    term is `<i> transp (<_> P x refl) i d`. `x` comes from the type of
    `P`. An `x` that depends on `y` gives a refusal (value_bound). The start
    of the type is `transp (<_> P x refl) i0 d`. It converts to
    `J P d refl` (row jReflFive).
- Names: trans, transport, subst, transportRefl, J and JRefl move from
  RESERVED to BUILTINS. The reserved-*.lang rows keep REFUSE_NAME. The
  message is now "X is a core name (rule R4)". No new check codes.
- Design facts:
  - trans, transport, subst and J infer their path argument, as sym and
    cong do in M2. A literal `refl` in that position needs a definition
    (derived.lang `five` and `natLine`).
  - subst and J infer the family `P`. A lambda is not a motive.
  - The printer writes `<i>` for a constant line. Rows:
    `equality round => <i> 1200` and
    `equality moved => fun P => fun u => transp (<i> P 1200) i0 u`.
- Guest tests: examples/derived.lang has 27 definitions and 24 eval rows
  (derived): transRefl, transStart, transVar, transportLaw, transportVar,
  substVar, transportReflVar, transportReflNat, jVar, jReflVar, jReflFive,
  sumConst, sumInl, sumInr, optNone, optSome, listNil, listCons, sumStuck,
  sumHcomp, sumHcompInr, optHcomp, listHcomp and sumHcompStuck. `five` and
  `natLine` are helper paths. transStart and transEnd are `refl` proofs of
  the ends of `trans p q` with a variable type. test/json/derived.json has
  no instance (`{"glue-lang":1,"instances":[]}`). examples/equality.lang
  adds `round` (trans) and `moved` (subst), with 2 eval rows.
- test/emit/stuck-transp.lang: the transp stays stuck, because `q` computes
  to a PathP hcomp at Type 0 (O8 s2). Only the comment changes.
- Native review regressions: test/path-regressions.c adds 7 evaluation
  cases. Option and List ignore empty-face branches; Sum, Option and List
  project constructors under their face restrictions. Disjunctive faces
  are split, including a tube with distinct varying fields on each
  conjunction. The first 6 cases fail on the original s3 implementation
  (2 INTERNAL errors and 4 rendered internal operations). All 7 pass after
  the projection fix; native path regressions now total 23. `make check`
  passes, and the test source passes the compiler's strict warning flags.
- Mutants (each with `make -B check`, source restored after): 3 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| Sum rule ignores `inr` (transp_data, `inr` gives NULL) | eval derived sumConst; eval derived sumInr |
| `trans` drops the `~i -> x` branch (rule_trans, system `[ i -> q @ j ]`, face kept) | examples/derived.lang (transStart: TYPE_REFL, the start is `[ i0 -> z ]`, the end is `x`) |
| `J` gives `p @ i` as the second argument of `P` (rule_j) | examples/derived.lang (jReflFive: TYPE_MISMATCH, the start is `transp (<i> P 5 5) i0 d`) |

- Mutant 1 fails 2 rows. sumConst has the line `<i> Sum Nat Nat`. Sum is
  not a closed type, so transp_data computes the row. For mutants 2 and 3,
  examples/derived.lang does not check, so each derived eval row fails and
  the json build of derived.lang fails (26 gate failures each). Mutant 2
  does not kill `equality round`: its type Nat is closed, so the
  closed-type hcomp rule gives the same result. transStart has a variable
  type `A`. Mutant 3 also fails jVar.
- Mutant form: mutant 2 sets the branch count of OP_SYS to 1 (inst 1, 2
  args). With inst 2 and 2 args the evaluator gives INTERNAL (the wrong
  number of arguments), which is not a semantic kill. Mutant 3 adds
  `(void)pij;`, because `make check-clang` refuses a variable that is set
  but not used.
- Gates: `make -B check` exit 0 (examples 19, parse refusals 15, check
  refusals 75, evals 103, json 16, build refusals 4, circuits 3,
  circuit refusals 10, satcheck refusals 6, native circuit regressions 9,
  native path regressions 16, native face regressions 32, each with 0
  failed; gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0. `make
  e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).
- Open items:
  - O8 (s3): transp stays stuck at Family, at the fold and unfold
    families, and at a neutral value of a Sum, Option or List (sumStuck).
    hcomp stays stuck at Type n and at a stuck type. At a Sum, Option or
    List, hcomp stays stuck when `u0` and the branches do not have the same
    constructor (sumHcompStuck: a branch is a variable path).
  - O9 (s3, new): `refl` cannot be inferred, so it is not accepted as the
    path argument of trans, transport, subst or J. Use a definition.

### s4: equivalences, Glue, glue, unglue and ua (2026-10-15)

- Files: src/front/core.h, eval.c and check.c, src/json.c and
  test/path-regressions.c. Guest
  tests: examples/glue.lang and test/json/glue.json (new); in test/check/,
  glue-no-hint.lang, unglue-nat.lang, glue-boundary.lang,
  glue-face-cover.lang, reserved-ua.lang and reserved-Glue-type.lang (new);
  test/emit/glue-instance.lang (new); rows in test/check/expect.txt,
  test/eval/expect.txt and test/emit/expect.txt.
- Core: core.h adds OP_GLUE (args: A, the face, the system of `pack T e`),
  OP_GLUE_ELEM (args: a, the face, the system of t) and OP_UNGLUE (args: g,
  the face, the system of the Glue type).
- Evaluator (eval.c, M3-PLAN 3.4): reduce_glue, reduce_glue_elem and
  reduce_unglue. On the face i1 the system is its branch body:
  `Glue A [ i1 -> pack T e ]` is T, `glue [ i1 -> t ] a` is t and
  `unglue g` is `first e g`. `unglue (glue s a)` is a. Else Glue and glue
  are canonical values, and unglue of a neutral value is stuck. project
  gives the projection of one value.
- Conversion (3.5): conv_glue gives Glue eta. A neutral g is equal to
  `glue s a` when `unglue g` converts to a and, on each conjunction of the
  face, g converts to the branch of s. The face and the system come from
  the glue value, not from a type. No conv rule reads a type.
- Printer: print_glue writes `Glue A [ ... ]`, `glue [ ... ] a` and
  `unglue g`.
- JSON (json.c, 3.6): an instance of a Glue type gives JSON_VALUE ("a value
  of a Glue type"). On the face i1 a Glue type is T, so only a face that is
  not i1 gets to this case. OP_UNGLUE as a type gives INTERNAL, as the Kan
  operations do. OP_GLUE_ELEM as a type gives INTERNAL.
- Typing (check.c): RULE_IS_CONTR, RULE_FIBER, RULE_IS_EQUIV, RULE_EQUIV,
  RULE_ID_EQUIV, RULE_GLUE, RULE_GLUE_ELEM, RULE_UNGLUE and RULE_UA. Each
  rule writes a core term (D2).
  - core_is_contr, core_fiber, core_is_equiv, core_equiv and core_id_equiv
    write the terms of M3-PLAN section 4. `idEquiv A` takes no type
    argument in the core. path_core writes a Path term. Core and Value
    retain the universe of checked type terms in sort_level (n + 1 for
    Type n, zero when unknown). Elaboration records it; evaluation, quote
    and shifting preserve it. type_level uses that information for nominal,
    neutral and computed types, with structural reconstruction for generated
    type values. FamilyInfo supplies a domain family's universe. arrow_cod
    gives B of a non-dependent `A -> B` (value_bound).
  - `first` and `second` on an Equiv value: rule_part sends a Sigma
    argument to sigma_part (OP_WITNESS and OP_PAYLOAD). rule_sigma_part
    shares sigma_part. Prod does not change.
  - papp_ends writes CORE_PAPP with core ends. core_papp calls it.
  - rule_glue: A : Type n. The system checks against
    `Partial psi (Sigma (T : Type n) (Equiv T A))`, where psi is the join
    of its faces (sys_face, D5). An argument that is not a system gives
    PARTIAL_EXPECTED.
  - rule_glue_elem: glue checks only against a declared Glue type, else
    GLUE_EXPECTED. The system checks against `Partial psi T` (FACE_COVER),
    with T from the witness of the Glue system. a checks against A.
    glue_boundary: on each conjunction of psi, `(witness e) t` converts to
    a, else GLUE_BOUNDARY.
  - rule_unglue: `unglue g` infers A. The core copies the face and the
    system of the type of g (D5 of M2). A term that is not of a Glue type
    gives GLUE_EXPECTED.
  - rule_ua: `ua e : Path (Type n) A B`, with `e : Equiv A B`. The core is
    `<i> Glue B [ ~i -> pack A e, i -> pack B (idEquiv B) ]`. A type that
    is not `Equiv A B`, or A and B in different universes, gives
    TYPE_MISMATCH.
- Names: isContr, fiber, isEquiv, Equiv, idEquiv, Glue, glue, unglue and ua
  move from RESERVED to BUILTINS. RESERVED keeps Bool, Text and uaBeta. The
  reserved-*.lang rows keep REFUSE_NAME. No new check codes.
- Guest tests: examples/glue.lang has 17 definitions and 12 eval rows
  (glue): firstId, centerId, secondId, glueAt1, gl, ungl, ung, ungAt1, eta,
  uaEnd0, uaEnd1 and uaIdAt0. It has Equiv, idEquiv, `first` and `second`
  on idEquiv, isContr and fiber (contrId), Glue at the faces i1 (glueAt1)
  and j (gl), glue, unglue, unglue at the face i1 (ungAt1), Glue beta
  (beta), Glue eta (eta) and the ends of ua as `refl` rows (uaEnd0,
  uaEnd1, uaId0, uaId1). test/json/glue.json has 3 instances (firstId,
  centerId, ungAt1). Check rows: glue-no-hint.lang and unglue-nat.lang
  GLUE_EXPECTED, glue-boundary.lang GLUE_BOUNDARY, glue-face-cover.lang
  FACE_COVER, reserved-ua.lang and reserved-Glue-type.lang REFUSE_NAME.
  reserved-glue.lang (s1) covers glue. The file system is not case
  sensitive, so the Glue row has the name reserved-Glue-type.lang. Build
  refusal row: glue-instance.lang JSON_VALUE.
- Design facts:
  - The FACE_COVER row uses a Glue face `j \/ k` with two branches and
    `glue [ j -> 5 ] 5`. check_sys checks each live body before coverage.
  - For glue systems, a branch on i0 has no selected branch type. Its
    unreachable body is erased to unit while retaining its false face.
    Coverage, live branch typing, base typing and GLUE_BOUNDARY remain
    checked. `glue [ i0 -> 5 ] 5` constructs an empty-face Glue element.
  - unglue maps over partial systems before an outer face is selected,
    as application and projections do.
  - Glue eta applies only when the other side is neutral.
- Mutants (each with `make -B check`, source restored after): 3 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| Glue face i1 rule off (reduce_glue, always the canonical Glue value) | examples/glue.lang (glueAt1: TYPE_REFL, the start is `Glue Nat pack Field e`, the end is `Field`) |
| GLUE_BOUNDARY off (glue_boundary, the conv test is `0 && ...`) | glue-boundary.lang GLUE_BOUNDARY (exit 0) |
| unglue beta off (reduce_unglue, a glue element gives a stuck unglue) | examples/glue.lang (beta: TYPE_REFL, the start is `unglue (glue [ j -> 5 ] 5)`, the end is `5`) |

- For mutants 1 and 3, examples/glue.lang does not check, so each glue
  eval row fails and the json build of glue.lang fails. Mutant 1 also
  kills uaEnd0 alone (TYPE_REFL, the start is `Glue Field pack Nat e`).
- Mutant form: mutant 3 returns
  `val_op(m, VAL_STUCK, OP_UNGLUE, 0, 0, a, n)` for a glue element. A
  mutant that only removes the beta line sends the glue element to
  `blocked`, which gives INTERNAL ("unglue of a value that is not a glue
  element"). That is not a semantic kill.
- Gates: `make -B check` exit 0 (examples 20, parse refusals 15, check
  refusals 81, evals 115, json 17, build refusals 5, circuits 3,
  circuit refusals 10, satcheck refusals 6, native circuit regressions 9,
  native path regressions 40, native face regressions 32, each with 0
  failed; gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0. `make
  e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).
- Open items:
  - O8 (s4): transp and hcomp at a Glue type stay stuck. s5 adds them
    (M3-PLAN 3.4, D8).

- Staged review (2026-10-15): three MEDIUM findings fixed. O10 and O11
  are closed. No CI or test weakening was found.
  - Empty-face glue construction rejected valid terms with
    TYPE_MISMATCH, including `glue [ i0 -> 5 ] 5`. The checker now
    erases only unreachable glue branch bodies.
  - `ua (idEquiv Color)` and `ua (idEquiv (P x))` gave TYPE_INFER.
    Checked universe metadata also covers types computed by fold,
    projected types and types restricted to a face.
  - unglue of a partial system caused INTERNAL during normalization.
    The evaluator now preserves and maps its branches.
  - Seventeen C regressions cover these fixes and refusal guards.
    The original staged implementation fails 15 of the 40 native path
    cases; the fixed implementation passes all 40. ASan and UBSan pass.
    `make -B check` and the full e2e pipeline pass, including 33 Forge
    tests. The pipeline reuses the unchanged Halo2 binary.

### s5: Glue Kan rules, hcomp in the universe and uaBeta (2026-10-15)

The main loop built s5 by hand (sessions s5-a to s5-m). The builder agents
stopped on a rate limit and on a model error, and wrote no file.

- Faces (face.c, face.h): `face_forall` gives the CCHM forall at a level:
  the join of the conjunctions that do not use the level (i0 when there
  is none). test/face-regressions.c has 3 new native cases.
- Core (core.h): two internal ops, used only in kan.c. OP_KAN_GLUE (args:
  the Glue line L and r; inst 0 is A, inst 1 + k is the pack of branch k
  at r). OP_KAN_L2E (args: L) is lineToEquiv L in a canonical form. A
  projection, or a conversion against a value that is not OP_KAN_L2E,
  unfolds it to a pack. json.c has a case for each op.
- Kan rules (kan.c, M3-PLAN 3.4, D8): `ghcomp_rule` and `gcomp`;
  `transp_glue` and `hcomp_glue` (transp and hcomp at a Glue type);
  `hcomp_univ` (hcomp in `Type n` gives a Glue type of lineToEquiv);
  `line_to_equiv` and closed templates for isContr, fiber, isEquiv and
  idContr. A system body is evaluated eagerly, thus each Glue branch body
  is restricted to each conjunction with value_on_face. At an end where
  the Glue face is i1, `kan_glue` reads the part at a fresh level and then
  puts it on that end.
- Head probes (Machine.probe, kan.c `probe_head`): a transp in a probe
  stays stuck. A head that is not a type former is read again at the
  value_bound of the line.
- Evaluator (eval.c): `env_bound` reads the closure environment and does
  not open each closure (that evaluated the tube bodies again).
  `closure_apply` has a memo (Machine.opens, OpenMemo in core.h, 65536
  buckets). The key is the closure, the variable level and the kind of
  the argument. The memo is not used in a probe. This is safe because
  eval is pure outside a probe and the arena does not reset. print_value
  has a NULL guard: an arena OOM in the printer now gives a diagnostic
  (exit 1), not a crash.
- Typing (check.c): `uaBeta e x` (RULE_UA_BETA), with e : Equiv A B and
  x : A, gives Path B (transport (ua e) x) (first e x). Its core is `<i>
  transp (<_> B) i (first e x)`. If the end at i0 does not convert to
  `transport (ua e) x`, the checker gives PATH_BOUNDARY. `arrow_cod` now
  converts B at two fresh variables: with the env_bound change, a
  function type B such as Nat -> Nat looked dependent on the binder.
  PRINT_MAX (check.c:18) is 128 KB, not 64 KB (USER ruling).
- Codes: no new code. A dependent function in isEquiv or fiber gives
  TYPE_NOT_FUNCTION. A pair with a dependent function in ua gives
  TYPE_MISMATCH.
- Guest tests:
  - examples/gluekan.lang (41 definitions): the smoke rows and uaBeta at
    Nat, Field, a variable B, a variable A and a function type. 19 eval
    rows. test/eval/thu.txt is the golden of `thu` (106,675 bytes),
    compared by a cmp block in test/gate.sh.
  - examples/iso.lang: the general isoToEquiv (option (a), from the
    Cubical Agda lemIso; each hfill is an hcomp with the extra branch
    `~j`). Two wrong `~j` branches (in sq1 and in fill0) give FACE_AGREE,
    thus the check is not vacuous. swapE is generic over A: a closed
    `def swapE : Equiv ...` is a JSON instance that holds a function
    (JSON_VALUE). 7 eval rows; test/json/iso.json has 6 instances.
  - examples/squareUa.glue: the iso.lang definitions (a .glue file has no
    import) and a circuit `square` with `transport (ua (swapE Field))`.
    Its IR and witness are byte-equal to test/circuit/square.*.json.
    test/circuit/same-ir.txt (`PROG NAME GOLDEN`) has the row `squareUa
    square square`; test/gate.sh checks it. `make e2e` does not read this
    table. A wrong golden (`range`) gives 1 failure.
  - test/check/equiv-dependent.lang (TYPE_NOT_FUNCTION) and
    test/check/ua-dependent.lang (TYPE_MISMATCH): a dependent codomain
    through `arrow_cod`.
  - test/emit/stuck-transp.lang is now `fold 1 2 3 (transp (<_> Color) i0
    red)` (transp stays stuck at a domain family, O8). The old row was
    stuck only because of a head probe defect.
- Mutants (each with `make -B check`, source restored after): 6 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| 1 ghcomp drops `~phi -> u0` (ghcomp_rule sys2 -> sys1) | examples/gluekan.lang check at `ubn` (PATH_BOUNDARY from uaBeta), 22 failures |
| 2 transp_glue drops `~xi -> first ctr` (`tube_of(m, fs, xn + 1u)` -> `xn`) | examples/gluekan.lang check at `ubn` (normal form `[  ]`), 22 failures |
| 3 face_forall returns phi | native `face_forall (~i \/ i, i) is i0` |
| 4 memo key forced to 0 (`open_key(arg) * 0u`) | eval gluekan `thp` (OOM at the arena limit), then `thf`; 2 failures |
| 5 arrow_cod back to the s4 value_bound body | examples/gluekan.lang check at `tf` (TYPE_MISMATCH), 22 failures |
| 6 arrow_cod without the conv test | native path `isEquiv still rejects a dependent function` (INTERNAL); equiv-dependent.lang gives INTERNAL |

- For mutants 1 and 2, `ubv`, `ubvt` and `uba` each kill the mutant
  alone. The s5 plan said that the closed-type rule hides a ghcomp defect
  at Nat and Field. For mutants 1 and 2 that is false: `ubn` (Nat) kills
  them.
- ua-dependent.lang does not kill mutant 6. A dependent function type
  `(n : Nat) -> Lot n` does not convert to a function type `Nat -> B`,
  thus the Equiv test in `ua_core` refuses it without the conv test of
  `arrow_cod`.
- CIRCUIT_LOWER: no circuit gives it (SPEC.md O4). A circuit has only
  Field binders, thus the closed-type rules reduce each Kan term in a
  constraint. No refuse.txt row.
- Gates: `make -B check` exit 0 (examples 23, parse refusals 15, check
  refusals 83, evals 142, json 19, build refusals 5, circuits 3, same-ir
  1, circuit refusals 10, satcheck refusals 6, native circuit regressions
  9, native path regressions 40, native face regressions 35, each with 0
  failed; gate 0 failures; regressions 6 and 0 failed; printer 2). `make
  check-clang` clean. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 Forge tests passed).
- IR check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
  exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
  --root contracts` passes again (33 passed, 0 failed).
- Open items:
  - O8 (s5): transp and hcomp at a Glue type and hcomp at `Type n` now
    reduce. These stay stuck: transp at Family, at a domain family and at
    a neutral value of a Sum, Option or List; hcomp at a stuck type, and
    at a Sum, Option or List when `u0` and the branches do not have the
    same constructor.
  - `thu` (transp in Glue over a variable A) prints a 107 KB tower of
    stuck `hcomp A` and `transp (<_> A)` terms. The value is correct
    without regularity (M3 ruling 9a).
  - PRINT_MAX is 128 KB by USER ruling (for `thu`). A larger normal form
    gives EVAL_PRINT.
  - The normal form of `swapE` (679 bytes) prints a nested system
    `[ i \/ ~i -> [ ... ] ]` in one Prod component and a flat system in
    the other. This is a print form, not a semantic value. iso.lang has
    no eval row for swapE.
  - O4 stays open (CIRCUIT_LOWER above).

### s6: close (2026-10-15)

Summary of M3. glue-lang has the Kan operations of CCHM: `transp` along a
line (s1) and `hcomp` with a system and a base (s2). The rules cover the
closed types, Pi, Sigma and Prod, PathP (s2), Sum, Option and List (s3),
Glue types and the universe (s5). `comp`, `hfill`, `trans`, `transport`,
`subst`, `transportRefl`, `J` and `JRefl` are checker rules that write
core terms, so their laws hold by computation (s2, s3). s4 adds the
equivalences (`isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`), Glue
types with `glue` and `unglue`, and `ua`. s5 adds `uaBeta`, the Kan rules
of Glue with `ghcomp` and `gcomp` (D8), `lineToEquiv` and `hcomp` in
`Type n`. There is no regularity (D3): `transp (<i> P 1200) i0 u` stays
stuck, and the `moved` row records that normal form. The tests `round` and
`moved` of M2 s3 came back (s3). The IR did not change: `make e2e` gives
byte-identical verifiers, and the same-IR gate row (s5,
`examples/squareUa.glue`) passes. SPEC.md section 2.5 is the reference.
No definitional proof irrelevance: no conv rule reads a type (review rule
1 below).

The main loop built s6 by hand (sessions s6-a to s6-c). The review agents
stopped on a rate limit and on a model error, and wrote no file. s6 adds
no op, no code and no test row.

- Docs (the s6 change): SPEC.md 2.4 (the partial-element note points to
  2.5), 2.5 (new: the forms, typing rules, evaluation rules in order, no
  regularity, conversion, the derived operations and their laws, the D10
  names, circuits and JSON, the M3 refusal codes), 3 (F12 and F13 DONE in
  M3), 9 (O7 closed; O8 new). formers/FORMERS.md: the glue-lang text of
  F12, F13 and F14 and the tcc-json cells of F12, F13 and F14.
  docs/PLAN.md: M3 DONE 2026-10-15 with the slice commits. docs/STATUS.md:
  this section.

#### Mutant roll-up

Each mutant ran with `make -B check`, source restored after: 18 KILLED, 0
SURVIVED (3 in each of s1 to s4, 6 in s5). The slice sections give the
mutant forms and the failure counts.

| Slice | Mutant | Result | Killing row |
|---|---|---|---|
| s1 | TRANSP_CONSTANT check off (rule_transp) | KILLED | check transp-constant.lang, transp-meet.lang |
| s1 | Pi rule passes `y` for `w i0` (kan.c pi_rule) | KILLED | eval kan piLine |
| s1 | `Partial i1 A = A` rule off (reduce_partial) | KILLED | examples/kan.lang (d4Var: TYPE_MISMATCH) |
| s2 | face `i1` rule off (kan_hcomp) | KILLED | examples/kanpath.lang (pathEnd: TYPE_REFL); eval kanpath hcompAt1 |
| s2 | closed-type rule skips the branch test (hcomp_constant) | KILLED | eval kanpath stuckBase |
| s2 | PathP transp drops the `~j` branch (pathp_rule) | KILLED | examples/kanpath.lang (pathEnd: TYPE_REFL); eval kanpath pathEnd |
| s3 | Sum rule ignores `inr` (transp_data) | KILLED | eval derived sumConst, sumInr |
| s3 | `trans` drops the `~i -> x` branch (rule_trans) | KILLED | examples/derived.lang (transStart: TYPE_REFL) |
| s3 | `J` gives `p @ i` as the second argument of `P` (rule_j) | KILLED | examples/derived.lang (jReflFive: TYPE_MISMATCH) |
| s4 | Glue face `i1` rule off (reduce_glue) | KILLED | examples/glue.lang (glueAt1: TYPE_REFL) |
| s4 | GLUE_BOUNDARY off (glue_boundary) | KILLED | check glue-boundary.lang (exit 0) |
| s4 | unglue beta off (reduce_unglue) | KILLED | examples/glue.lang (beta: TYPE_REFL) |
| s5 | ghcomp drops `~phi -> u0` (ghcomp_rule) | KILLED | examples/gluekan.lang (ubn: PATH_BOUNDARY) |
| s5 | transp_glue drops `~xi -> first ctr` (tube_of) | KILLED | examples/gluekan.lang (ubn: normal form `[  ]`) |
| s5 | face_forall returns phi | KILLED | native face `face_forall (~i \/ i, i) is i0` |
| s5 | memo key forced to 0 (open_key) | KILLED | eval gluekan thp (OOM at the arena limit), thf |
| s5 | arrow_cod back to the s4 value_bound body | KILLED | examples/gluekan.lang (tf: TYPE_MISMATCH) |
| s5 | arrow_cod without the conv test | KILLED | native path `isEquiv still rejects a dependent function` |

#### Review

Review of the M3 diff (`git diff aad6057..HEAD -- src`: 8 files, +2453
-32). The review agents stopped on API errors (a Fable rate limit, then
the Opus fallback once), so the review is a targeted read in the main
session (s6-a, s6-b). Result: no defect found.

1. No conv rule reads a type, and there is no rule `hcomp A i0 u u0 = u0`
   for a general `A` (D8). Read: kan.c kan_hcomp, hcomp_constant,
   hcomp_same_ctor, hcomp_data, hcomp_pi_rule, hcomp_pair_rule,
   hcomp_pathp_rule, hcomp_pair, hcomp_glue, hcomp_univ, kan_l2e,
   glue_elem, ghcomp_rule, gcomp, kan_transp, kan_dom, kan_cod,
   closed_type and data_type; eval.c reduce_glue, reduce_glue_elem,
   reduce_unglue, the Kan cases of reduce_op, conv_sys, conv_glue and
   conv_loop. Only the face `i1` rule, the closed-type rule (Nat, Field,
   Unit, Flag; kan_hcomp excludes `Type n`) and `ghcomp` give the base.
   hcomp_data gives `u0` for `none` and `nil` only when each branch is
   that constructor (the data rule with no field). hcomp_univ with the
   face `i0` gives `Glue u0 [ ]`, and reduce_glue keeps it a Glue value:
   only the face `i1` reduces. hcomp_glue with the face `i0` still builds
   `glue`. Conversion is argument by argument. Glue eta reads the `glue`
   value. Two stuck `unglue` compare `g` only.
2. Switch cases: `make check-clang` uses `-Wswitch-enum -Werror`.
   `git diff aad6057..HEAD -- src | rg '^\+.*default\s*:'` gives no line.
3. Globals: no file-scope declaration on a `+` line of the diff.
   `rg -n '^(static )?[a-z_A-Z].*;$' src/front/kan.c` gives no line.
4. NULL on a diagnostic: kan.c kan_fail and the OOM path set a diagnostic
   and return NULL. hcomp_data, hcomp_glue, hcomp_univ and transp_glue
   return NULL with no diagnostic when the rule does not apply, and each
   caller tests `v != NULL || m->diag->set`. check.c rule_transp,
   hcomp_base, glue_boundary, rule_glue_elem, rule_unglue, ua_core, rule_ua
   and rule_ua_beta: each FAIL path returns NULL or 0, and each
   `diag->set` test returns before a second FAIL. closure_apply (eval.c)
   and conv_values return NULL or 0 on a NULL argument.

#### Gates

Gates on HEAD f5a5dca (s6-a). s6 changes no code or test file, and
test/gate.sh and the Makefile read no doc file, so the counts hold.
`make -B check` exit 0 (native circuit regressions 9, native path
regressions 42, native face regressions 35, examples 23, parse refusals
15, check refusals 83, evals 142, json 19, build refusals 5, circuits 3,
same-ir 1, circuit refusals 10, satcheck refusals 6, each with 0 failed;
gate 0 failures; regressions 6 of 6; printer 2). `make check-clang` exit
0. `forge build --root contracts --sizes` exit 0. `make e2e` exit 0 (33
Forge tests passed). IR check: `git diff --exit-code -- test/circuit
contracts/src/verifiers` exit 0. contracts/test/fixtures is restored from
HEAD, and `forge test --root contracts` passes again (33 passed, 0
failed).

#### Open items

- O4 (M1, SPEC.md 9): no gate test gives CIRCUIT_LOWER. s5 found no
  circuit that gives it. Milestone: none.
- O5 (M2, SPEC.md 9): the JSON target writes a Path component of a data
  value as `null`. A loop in a HIT carries content. Milestone: M4.
- O8 (M3, SPEC.md 9): Kan values that stay stuck. Family and the fold and
  unfold families have no rule. `transp` stays stuck at a domain family
  and at a neutral value of a Sum, Option or List. `hcomp` stays stuck at
  a stuck type, and at a Sum, Option or List when `u0` and the branches do
  not have the same constructor. Milestone: `hcomp` values in M4; the
  fold and unfold families in M6.
- O9 (s3): `refl` cannot be inferred, so it is not accepted as the path
  argument of `trans`, `transport`, `subst` or `J`. Use a definition.
  Milestone: none.
- For the USER (s1): `def px : (i : I) -> Partial i Nat := fun i => [ i
  -> 0 ]` gave exit 0 for `langc build` in s1. s6 did not run it again.
  Milestone: none.
- O10 and O11 closed in the s4 staged review. The s5 print facts (`thu`
  107 KB, PRINT_MAX 128 KB by USER ruling, the `swapE` print form) are
  facts, not items.

## M4

Summary of M4. glue-lang has higher inductive types (HITs). A HIT is a
`family` in domain/domain.lang with one or more path constructors: a
constructor with interval binders and a boundary system (s1). The Kan
rules apply at families (s2): `transp` at a family transports each field,
and `hcomp` at a HIT is a canonical value (D3). The JSON target writes
`null` for constant paths and refuses paths with content (D9, O5 closed).
`fold B cases x` is the eliminator at a HIT: it computes on a point
constructor, on a path constructor and on an
`hcomp` value (s3). s3 adds the propositional truncation `Trunc`. s4 adds
dependent field types, Path fields, constructors with two binders, `isSet`
and the set quotient `Quot` with `cls`, `eqc` and `squashc`. s5 adds the
Direction 4 smoke (`OTPSpec`, examples/otp.lang) and the quotient circuit
(examples/quotSq.glue, the same IR as square). There is no dependent
eliminator (O12 (e)), and `hcomp A i0 u u0 = u0` is not a rule (D8). The
IR did not change: `make e2e` gives byte-identical verifiers. SPEC.md
section 2.6 is the reference. No definitional proof irrelevance: no conv
rule reads a type (`### s6` review rule 1).

### s1: HIT declarations, path constructors and the M4 plan (2026-10-15)

- Plan: docs/M4-PLAN.md (new) is the M4 plan with the USER rulings
  applied. s1 changes only the section 5 domain row (the Susp family and
  the two name facts below).
- Syntax (src/front/ast.h, src/front/parser.c): a constructor can bind
  interval names and give a boundary system: `loop <i> [ ~i -> base, i ->
  base ]`. Ctor has binders, binder_count and boundary. parse_path_ctor
  reads the binders `<i j>` and then a system. A binder list without a
  system, or a system without a binder list, gives PARSE_EXPECT.
- Core (src/front/core.h, src/front/eval.c): OP_PCON (after OP_CTOR) is a
  path constructor value. Its arity is params + fields + binders
  (varied_arity). CtorInfo has binder_count, binders and boundary.
  FamilyInfo has path_ctors. pcon_boundary evaluates the boundary system
  in the constructor's environment and returns the body of the first face
  that is i1, else NULL. reduce_pcon applies the boundary rule when a face
  is i1; else the value is canonical. print_pcon prints `loop @ i` and
  `merid Nat 0 @ i`. conv and quote did not change: the generic VAL_OP
  rule covers OP_PCON.
- Typing (src/front/check.c): check_decl checks a family in a program
  first and then refuses it (REFUSE_DATA), so a bad HIT in a program gives
  its own code. check_boundary builds the boundary core (boundary_core),
  then tests each atom `~i` and `i` of each binder against the join of
  the system faces (HIT_BOUNDARY, hint `Partial join Self`), then checks
  the system (check_sys: FACE_AGREE, TYPE_MISMATCH). check_ctor fills the
  new CtorInfo fields. check_family counts path_ctors before checking
  constructors. pcon_end computes the end of
  a path constructor at i0 or i1 of binder j: the boundary is evaluated
  with that binder set to the end, quoted at level L + k (the other
  binders stay variables), then wrapped in CORE_PLAM for the binders
  after j. pcon_app gives a path constructor at its fields the type
  `Path (F params) (end i0) (end i1)` (D5). ctor_app dispatches to
  pcon_app when binder_count > 0.
- JSON (src/json.c): OP_PCON is not a type former. family_json refuses a
  path constructor value in an instance (`a path constructor value`).
- Domain (domain/domain.lang): `family Circle := base | loop <i> [ ~i ->
  base, i -> base ]` and `family Susp (A : Type 0) := north | south |
  merid (apex : A) <i> [ ~i -> north A, i -> south A ]`, appended after
  the last def. The existing rows did not move. Two facts the plan text
  did not have: a constructor takes the family parameters explicitly (as
  `makeLot 3 250` does), so the Susp boundary names `north A` and
  `south A`; and a field name is a core name (rule R4), so the field is
  `apex`, not `a` (the name `a` renamed the binder `a` to `_x0` in the
  kan meetLine and derived substVar eval goldens).
- Tests: examples/circle.lang (9 defs: pt, loopPath, loopStart, loopEnd,
  loopEta, loopInv, m, mStart, mEnd) with test/json/circle.json (only `pt`
  is an instance; a Path-typed def is not). Check rows: hit-boundary.lang
  HIT_BOUNDARY, hit-face-agree.lang FACE_AGREE, hit-body-nat.lang
  TYPE_MISMATCH, hit-bad-ends.lang TYPE_MISMATCH, hit-program.lang
  REFUSE_DATA. Parse rows: ctor-no-system.lang and ctor-no-binder.lang
  PARSE_EXPECT. Eval rows: `circle loopPath => <i> loop @ i`, `circle
  loopInv => <i> loop @ ~i`, `circle m => <i> merid Nat 0 @ i`, `circle
  pt => base`. test/check/self.lang renames `loop` to `spin` (`loop` is
  now a domain constructor).
- Mutants (each with `make -B check`, source restored after): 3 KILLED,
  0 SURVIVED.

| Mutant | First failing row |
|---|---|
| Boundary coverage test off (check.c boundary_core, `face_leq(&h, atom, join) != 1 && 0`) | check hit-boundary.lang (REFUSE_DATA, not HIT_BOUNDARY) |
| Boundary rule reads the first branch only (eval.c pcon_boundary, `k < 1u`) | examples circle.lang (INTERNAL at loopPath: no face i1 at the end 1 of i); 7 failures in total |
| Path constructor type takes the i1 body at both ends (check.c pcon_end, `val_ivl_end(m, 1)`) | examples circle.lang (TYPE_MISMATCH at `m`: found `Path (Susp Nat) (south Nat) (south Nat)`); hit-bad-ends.lang gave exit 0; 7 failures in total |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35;
  examples 24, parse refusals 17, check refusals 88, evals 146, json 20
  builds, build refusals 5, circuits 3, same-ir 1, circuit refusals 10,
  satcheck refusals 6; node regressions 6, printer 2). `make check-clang`
  exit 0. `forge build --root contracts --sizes` exit 0. `make e2e` exit
  0. `git diff --exit-code -- test/circuit contracts/src/verifiers` exit
  0. contracts/test/fixtures restored from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Open items:
  - The emit row for a stuck value at a HIT (M4-PLAN section 5,
    test/emit/stuck-transp.lang) is deferred to s2.
  - check_field does not accept a field of type `Path Self a b` as a
    recursive field (s1-b design item 6). Quot `squashc` (s4) needs it.
    Milestone: s3.

#### Staged review fixes (2026-10-15)

- HIGH: the plain family fold accepted HIT cases without boundary checks.
  Mapping the two poles of `Susp Nat` to 0 and 1 let `cong` construct
  `Path Nat 0 1`. fold_family now refuses HITs with TYPE_MISMATCH until
  the coherent eliminator in s3. check_family counts every path
  constructor before checking fields and boundaries, so a boundary
  cannot use the plain fold while its own family is being checked.
- MEDIUM: a single path constructor exposed its fields as record
  projections. Projecting `go x @ i` at a neutral interval failed with
  INTERNAL. proj_app now refuses these projections with TYPE_MISMATCH.
- Added test/hit-regressions.c to `make check`: 10 native C regressions
  cover both refusals, the boundary fold bypass, plain family folds and
  projections, higher dimensional boundaries, and scoped parameters.
- Validation: the new tests reproduced both defects before the fixes.
  `make check` passes, including all 10 HIT regressions. The new C test
  also passes the Clang warning and switch checks used by the project.

### s2: Kan rules at families and hcomp values (2026-10-15)

- Kan (src/front/kan.c, src/front/kan.h, src/front/eval.c): plain_family
  and hit_type test an OP_FAMILY head (path_ctors == 0 or > 0);
  type_former accepts OP_FAMILY, so the probe keeps a family head.
  transp_sys distributes `transp L phi` over an OP_SYS value, one branch
  per conjunction, with L, phi and the body put on that face (the
  reduce_field idiom: value_bound, face_conj, value_on_face); a bound at
  or past KAN_PROBE gives a stuck transp; an empty system is unchanged;
  kan_transp calls it right after the probe guard. field_line_transp
  computes `transp (<i> T[DOM(L, i, p) / p]) phi x` for a field of type T
  (scope = the family parameters): T is wrapped in param_count CORE_LAM
  binders and applied to OP_KAN_DOM with INST = p. kan_dom takes the
  parameter index (kan.h; eval.c `kan_dom(m, a[0], a[1], inst)`);
  line_at accepts a family head; kan_cod refuses a family line; a
  recursive field (no parameters) uses L. transp_ctor: a constructor
  (OP_CTOR or OP_PCON) at a family line with parameters gives the
  constructor at the parameters of `L i1`, each field transported by
  field_line_transp, an interval argument unchanged; the result goes
  through the evaluator (opn + run), so the boundary rule of a path
  constructor at an end fires. The staged review adds a boundary agreement
  check before accepting fieldwise transport of a path constructor (below).
  transp_hcomp: `transp L phi (hcomp A0 psi
  u u0) = hcomp (L i1) psi (fun j => transp L phi (u j)) (transp L phi
  u0)`. kan_transp after closed_type: a family head with param_count 0
  gives the value as is (the D4 closed rule, also at a HIT: transpConst);
  else transp_family (a ctor or pcon to transp_ctor, an OP_HCOMP value to
  transp_hcomp, else stuck). hcomp: same_head (an OP_CTOR must match
  INST) in hcomp_same_ctor; hcomp_ctor is the data rule at a plain family
  (the constructor at the parameters of A, hcomp on each field at the
  field type at those parameters, via field_hcomp and instance4);
  hcomp_data accepts OP_CTOR; kan_hcomp sends `data_type(a) ||
  plain_family(m, a)` to hcomp_data and, before the stuck fallback,
  returns a VAL_OP OP_HCOMP at a HIT (D3: canonical).
- JSON (src/json.c): path_json (OP_PATHP) writes `null` when the body at
  a fresh interval variable converts to the body at i0 (conv_values at
  bound + 1), else JSON_VALUE "a path with content" (D9). A free-level
  scan was wrong: a closure in the body can capture the variable without
  a use (the node regression `printed types preserve binding under
  normalization`, case `Option (Path (Nat -> Nat) plus plus)` = `some
  refl`, failed with it). family_json refuses an OP_HCOMP value ("an
  hcomp value") before the OP_PCON test.
- Tests: examples/circle.lang gets 9 defs under `-- M4 s2`: hcompOne,
  transpConst and northT are refl rows (`<i> base`, `<i> x`, `<i> north
  (L i1)`); hcompLoop, transpHcomp, meridT, lotT, lotHcomp and
  colorHcompStuck are eval rows (test/eval/expect.txt, 6 new): hcompLoop
  `hcomp Circle k (fun i => [ k -> loop @ i ]) base` (canonical);
  transpHcomp `hcomp (Susp (L i1)) k (fun i => [ k -> transp (<_x0> Susp
  (L _x0)) i0 (f i) ]) (transp (<i> Susp (L i)) i0 (f i0))`; meridT `<j>
  merid (L i1) (transp (<i> L i) i0 a) @ j`; lotT `makeLot (n i1) p`;
  lotHcomp `makeLot 3 (hcomp Nat k (fun i => [ k -> q @ i ]) 1)`;
  colorHcompStuck stuck. `[ ]` does not parse (PARSE_EXPECT), so the i0
  witnesses use the constant face `[ i0 -> base ]` and `[ i0 -> north
  Nat ]`. Check row: test/check/hcomp-i0-circle.lang TYPE_REFL ("the
  start normalizes to hcomp Circle i0 (fun i => [ i0 -> base ]) base,
  the end normalizes to base"): there is no `hcomp A i0 u u0 = u0` rule
  (D8). Emit rows (test/emit/expect.txt): hcomp-instance.lang JSON_VALUE
  "an hcomp value"; transp-hcomp.lang JSON_VALUE "an hcomp value";
  path-content.lang (`def p : Sigma (x : Circle) (Path Circle x x) :=
  pack base loop`) JSON_VALUE "a path with content".
  test/emit/stuck-transp.lang is replaced by a positive native regression:
  its Color transport now computes, and its fold returns `1`.
  transp-hcomp.lang is the new refusal witness of M4-PLAN section 5.
  test/json/circle.json is unchanged (pt is the only instance); no
  `null` golden changed.
- Golden update: test/eval/thu.txt (gluekan thu, a large neutral) was
  regenerated with `build/langc eval examples/gluekan.lang thu`. transp
  now distributes over the nested systems in it, so a stuck form such as
  `transp (<j> A) _x2 (? i0 (unglue x) k X)` became `[ k -> transp (<j>
  A) _x2 X' ]` with X' put on the face k. The file went from 106675 to
  41145 bytes.
- Mutants (each with `make -B check`, source restored after, `git diff`
  identical before and after): 3 KILLED, 0 SURVIVED.

| Mutant | First failing row |
|---|---|
| A HIT takes the closed-type hcomp rule (kan.c kan_hcomp, `\|\| hit_type(m, a)`) | check hcomp-i0-circle.lang (exit 0, not TYPE_REFL); build transp-hcomp.lang and hcomp-instance.lang gave exit 0; 3 failures in total |
| The family closed-type transp rule applies with one parameter (kan.c kan_transp, `param_count < 2u`) | examples circle.lang (PATH_BOUNDARY at meridT: at i0 it normalizes to `north (L i0)`, the type needs `north (L i1)`); the 10 circle eval rows failed behind it; 12 failures in total |
| The JSON constant-path test always writes `null` (json.c path_json, `x != NULL`) | build path-content.lang (exit 0, not JSON_VALUE); 1 failure in total |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  10; examples 24, parse refusals 17, check refusals 89, evals 152, json
  20 builds, build refusals 7, circuits 3, same-ir 1, circuit refusals
  10, satcheck refusals 6; node regressions 6, printer 2). `make
  check-clang` exit 0. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed; the 3 deploy simulations ran). `git diff
  --exit-code -- test/circuit contracts/src/verifiers` exit 0.
  contracts/test/fixtures restored from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Open items:
  - O8: `hcomp` at Sum, Option and List with different constructors
    stays stuck, and so does `hcomp` at a plain family whose branches
    give different constructors (hcomp_same_ctor).
  - Dependent field types are out of s2: a field type sees the family
    parameters only. The s3 change to check_field for `Path Self a b`
    needs the field type at the transported earlier fields.
  - Fieldwise transport of a parameterized path constructor can fail to
    commute with its boundaries. Such transport stays stuck; general
    boundary correction terms remain open. The emit witness is
    test/emit/transp-hcomp.lang (an `hcomp` value).
  - `hcomp` at a HIT with a constant tube stays canonical (D8): `hcomp
    Circle i0 (fun i => [ i0 -> base ]) base` does not reduce to `base`.

#### Staged review fixes (s2)

- HIGH: fieldwise transport of a path constructor did not commute with
  boundary restriction. The native FunH case has a boundary `funEnd A B
  (f a)`. Evaluating transport at i1 gives `funEnd ... (transp M i0 (f a))`,
  but normalizing at a fresh interval and then restricting to i1 inserted
  a backward transport of the forward transport of `a` inside `f`.
  These values do not convert. The regression passes before s2 and fails
  on the original staged s2 rule.
- Fix: transp_pcon_agrees checks the transported candidate against
  transport of the source on every conjunction of every boundary face.
  A mismatch keeps the original transport stuck. Existing Susp reductions
  are preserved; general boundary correction terms remain open.
- Tests: test/hit-regressions.c checks both endpoints after normalization,
  with the mismatch on the second boundary branch. It also retains the
  removed stuck-transp refusal scenario as a positive test: the Color
  transport and fold now compute to `1`.
- Validation: `make check` passes (native HIT 12, all other counts
  unchanged). The C regression file passes Clang with `-Wall -Wextra
  -Wswitch-enum -Werror`. Removing the boundary check reproduces the
  endpoint regression. `make e2e` passes using the existing unchanged
  halo2 back end: all three proofs, 33 Forge tests and three deploy
  simulations. Circuit IR and generated verifiers remain byte-identical.
- No remaining CI weakening or verified correctness finding.

### s3: the HIT eliminator and the truncation (2026-10-15)

- Surface form: `fold B cases x` at a HIT (D6). `B` is always explicit.
  One case follows for each constructor in declaration order, then the
  scrutinee. The core op is OP_FOLD_HIT (src/front/core.h): inst is the
  family, the args are the result type, the cases and the scrutinee. The
  printer name is `fold` and the arity VARIES (src/front/eval.c).
- Checker (src/front/check.c): rule_fold sends a HIT family to
  fold_hit_rule. A wrong argument count gives HIT_CASES ("fold at the
  higher inductive family Sneak needs the result type, then 3 cases, then
  the scrutinee; found 4 arguments"). hit_case_type builds the type of
  each case: a point case has the family case type with each recursive
  field replaced by `B`; a path case with k binders has the type `(f' :
  ...) -> PathP ...` whose ends are `fold B cases t` for the boundary
  bodies `t` (hit_case_end folds each body with eval_fold_hit_boundary, so an end
  reduces when the body is a constructor). HIT_CASE is the typed-ends
  fact: recode_case re-codes an inner TYPE_MISMATCH, PATH_BOUNDARY or
  TYPE_REFL of a case into HIT_CASES (a point case) or HIT_CASE (a path
  case) with the constructor and the expected type; another inner code
  stays. hit-case-loop.lang is a PATH_BOUNDARY re-coded, hit-case-ends.lang
  a TYPE_MISMATCH re-coded. `isProp` is a core name next to `isContr`
  (rule_is_prop, core_is_prop): `isProp A = (x : A) -> (y : A) -> Path A
  x y`. check_field: s1 pinned a recursive field to a family with no
  parameters, which refused `Trunc A`. self_applied relaxes it: the field
  type must be the family applied to its own parameters in order
  (parameter j is at level j in the field scope, index p - 1 - j; p = 0
  is the s1 case). Every consumer of FieldInfo.recursive (fold_family,
  fold_hit, field_line_transp, the hcomp field types, case_type, the
  boundary scope) treats the field type as the result type or the family
  line, which stays correct for this uniform occurrence. The
  DOMAIN_FAMILY message is "the recursive field %s must have the type %s
  applied to its own parameters".
- Evaluator (src/front/eval.c): fold_hit on a point constructor applies
  the case to the fields, a recursive field arrives folded; on a path
  constructor it applies the case to the fields, then papp_value to each
  interval argument (the boundary rule of the path constructor fires at
  an end); on an OP_HCOMP value it calls kan_fold_hcomp; on an OP_SYS
  value fold_hit_sys folds each branch body and keeps the faces (the
  shape of sys_apply and transp_sys); on a neutral it gives a VAL_STUCK
  OP_FOLD_HIT (stuck_fold_hit). eval_fold_hit_boundary exports the fold for the
  checker.
- Kan (src/front/kan.c, src/front/kan.h): kan_fold_hcomp is D11: `fold B
  cases (hcomp D phi u u0) = hcomp B phi (fun i => fold B cases (u i))
  (fold B cases u0)`, the shape of transp_hcomp. The tube at i is a
  system value, so the fold under the binder goes through fold_hit_sys.
  The design wrapped the tube body in a one-branch system; that wrapper
  is removed, because it printed a nested system `[ k -> [ k -> l @ i ]
  ]`. The rule prints `fun i => [ k -> l @ i ]`.
- JSON (src/json.c): op_json lists OP_FOLD_HIT with the other stuck ops
  (JSON_VALUE).
- Domain (domain/domain.lang): `family Trunc (A : Type 0) := inc (held :
  A) | squash (lo : Trunc A) (hi : Trunc A) <i> [ ~i -> lo, i -> hi ]`
  (D10). A field name is a core name, so held, lo and hi were checked
  free of every example, test, domain and src binder.
- Examples: examples/circle.lang gets 7 defs under `-- M4 s3`: foldBase,
  foldLoop and foldHcomp are refl rows (the point rule, the path rule and
  the hcomp rule under the binders B b l k); deg0, degBase, toPath and
  toPathBase give the eval rows `degBase => 0`, `deg0 => fun c => fold
  Nat 0 (<i> 0) c`, `toPathBase => <i> 0`. examples/trunc.lang is new:
  truncZero `inc Nat 0` (the JSON instance `{"tag":"inc","held":0}` in
  test/json/trunc.json), truncIsProp `fun A => fun x => fun y => squash A
  x y`, truncRec (a fold into B with a witness `pB : isProp B`, path case
  `fun x => fun y => pB x y`) and truncRecInc (eval row `fun pB => 0`).
  test/json/circle.json gains the instance degBase. The gate loops over
  examples/*.lang, so trunc needs no list edit.
- Check rows (test/check/expect.txt, 7 new): hit-case-loop.lang `fold Nat
  0 (<i> 1) base` HIT_CASE; hit-case-ends.lang (a variable `p : Path Nat
  1 1` as the loop case) HIT_CASE; hit-cases-missing.lang `fold Nat 0
  base` HIT_CASES; hit-cases-point.lang `fold Nat flagYes refl base`
  HIT_CASES; reserved-isProp.lang REFUSE_NAME; hit-vars-circle.lang and
  hit-vars-trunc.lang (refl between two variables of a HIT) TYPE_REFL.
- Native rows (test/hit-regressions.c): the connected-points row now
  gives HIT_CASE (the Susp fold with the case `fun (a : Nat) => refl`);
  the interior row is positive (`<i> fold Nat 0 refl (loop @ i)` is
  refl); `fold 0 0 base` gives HIT_CASES; the Sneak boundary row gives
  HIT_CASES by argument count (retitled "HIT boundaries refuse the plain
  family fold shape"); new row "a recursive field is the family at its
  own parameters" with `family Twist (A : Type 0) := tw (inner : Twist
  Nat)` DOMAIN_FAMILY. HIT 12 -> 13.
- Mutants (each with `make -B check`, source restored after, `git diff
  --stat` identical before and after): 3 KILLED, 0 SURVIVED.

| Mutant | First failing row |
|---|---|
| recode_case returns NULL without re-coding (check.c) | native `HIT fold cannot distinguish connected points: expected HIT_CASE, got TYPE_REFL` (make stops at the native suite); the mutant binary also gives hit-case-loop.lang PATH_BOUNDARY |
| fold_hit at OP_PCON drops the papp_value loop (eval.c) | native `HIT fold computes on a path constructor interior: expected ok, got TYPE_REFL`; the mutant binary also fails examples/circle.lang at foldLoop (the start normalizes to `<i> l`, the end to `l`) |
| kan_fold_hcomp base argument replaced by `var(m, 0)` (kan.c) | examples circle.lang TYPE_REFL at foldHcomp (the start normalizes to `hcomp B k (fun i => [ k -> l @ i ]) base`, the end to `hcomp B k (fun i => [ k -> l @ i ]) b`) |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  13; examples 25, parse refusals 17, check refusals 96, evals 156, json
  21 builds, build refusals 7, circuits 3, same-ir 1, circuit refusals
  10, satcheck refusals 6; node regressions 6, printer 2). `make
  check-clang` exit 0. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (the 3 deploy simulations ran). `git diff
  --exit-code -- test/circuit contracts/src/verifiers` exit 0.
  contracts/test/fixtures restored from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Review fixes (2026-10-16):
  - A neutral path case lost its endpoint metadata when fold_hit applied
    it. hit_case_eta now expands checked path cases through their field
    and interval binders, using core_papp to retain both checked ends.
    Native face checks cover Circle and Trunc with neutral path cases.
  - A constructor around a recursive boundary field folded that field
    twice and produced INTERNAL. eval_fold_hit_boundary now interprets
    constructor boundaries using the fields' existing images in B.
    A Nested fold computes 1, and an incoherent path case gives HIT_CASE.
  - An empty neutral hcomp tube normalized to a fold of Partial i0 D,
    which is not a valid scrutinee type. kan_fold_hcomp now gives that
    empty tube a false branch containing the folded base. The native
    regression checks equality with hcomp B i0 (fun i => [ i0 -> b ]) b.
  - Added seven native rows, including higher constructor computation
    and a parameterized recursive plain family. Native HIT: 20 passed.
- Open items:
  - `Path Self a b` fields (check_field) and k = 2 tests: s4.
  - Dependent `elim`: out of scope.

### s4: higher constructors and the set quotient (2026-10-16)

- Surface form: no new syntax. A field type of a constructor now sees
  the family parameters and the earlier fields of the same constructor
  (a dependent field), and a field of a HIT can be a Path in the family
  (`sqp : Path (Quot A R) sqx sqy`). `isSet` is a core name next to
  `isProp` and `isContr` (RULE_IS_SET, the BUILTINS row after isProp,
  core_is_set, rule_is_set): `isSet A = (x : A) -> (y : A) -> (p : Path A
  x y) -> (q : Path A x y) -> Path (Path A x y) p q`. A cube constructor
  with two binders (`<i j>`) goes through the s1 boundary code: s4 forced
  no s1 or s3 fix.
- Checker (src/front/check.c): FieldInfo (src/front/core.h) gains
  `dependent` (the type uses an earlier field), `ends[2]` (the ends of a
  Path field) and `recursive` 0, 1 or 2 (0 plain, 1 the whole family at
  its own parameters, 2 a Path in the family). check_field takes the
  count of the earlier fields: the field scope is the parameters, then
  the earlier fields (check_ctor pushes each field; check_boundary pushes
  the binders only), and self_applied takes the depth of that scope. The
  DOMAIN_FAMILY message is now "the recursive field %s must have the type
  %s applied to its own parameters, or a Path in it". case_type is
  deleted: fold_family uses hit_case_type for a point case too (kb = 0:
  the Pi binders, then the result type). case_field_type gives a Path
  field the case type `PathP (<_> res) a' b'`, where a' and b' are the
  folded ends (eval_fold_hit_boundary). hit_case_type nests one PathP per
  binder, from the last binder (inner) to the first (outer), so a square
  case has the type `(f' : ...) -> PathP (<i> PathP (<j> B) x' y') (<j> p'
  @ j) (<j> q' @ j)`, and the generic path check (check_plam) tests the
  inner ends. ctor_app and the new proj_type push the earlier field
  values before they evaluate a field type. boundary_core checks the cube
  boundary per binder: a square with three faces gives HIT_BOUNDARY "the
  path constructor sq does not cover the face j of its binder j: the
  boundary joins to i \/ ~i \/ ~j". A constructor in a boundary takes
  its parameters explicitly (`~i -> cls eqa` gives TYPE_ARITY "cls needs
  3 arguments, found 1").
- Evaluator (src/front/eval.c): fold_hit gives a recursive-2 field the
  image kan_fold_path_field. eval_fold_hit_boundary keeps a path field
  variable applied to an interval (`p @ j`: it tests the head of a
  VAL_PAPP) and folds a path field inside a boundary constructor
  pointwise.
- Kan (src/front/kan.c): kan_fold_path_field is the image of a field P
  that is a path in the family under the fold: `<j> fold RES CASES (P @
  j)`, a VAL_PLAM closure over res, the cases and p. field_line_transp
  returns NULL (stuck, no diagnostic) for a recursive-2 or a dependent
  field; hcomp_ctor uses a NULL field type for a dependent field.
- JSON (src/json.c): family_json evaluates a field type that is not
  recursive-1 in the environment params, then the earlier field values
  (env_push after each field).
- Domain (domain/domain.lang, line 26): `family Quot (A : Type 0) (R : A
  -> A -> Type 0) := cls (cla : A) | eqc (eqa : A) (eqb : A) (eqr : R eqa
  eqb) <i> [ ~i -> cls A R eqa, i -> cls A R eqb ] | squashc (sqx : Quot
  A R) (sqy : Quot A R) (sqp : Path (Quot A R) sqx sqy) (sqq : Path (Quot
  A R) sqx sqy) <i j> [ ~i -> sqp @ j, i -> sqq @ j, ~j -> sqx, j -> sqy
  ]` (D10). The field names cla, eqa, eqb, eqr, sqx, sqy, sqp and sqq are
  core names, checked free of every example, test, domain and src binder.
- Examples: examples/quot.lang is new (15 defs). natSame is `fun a => fun
  b => Path Nat a b` (`natEq` is a core name, REFUSE_NAME). quotZero `cls
  Nat natSame 0`; quotEnd `(eqc Nat natSame 0 0 refl) @ i0` (the boundary
  rule gives the cls form); quotIsSet `fun A => fun R => fun x => fun y
  => fun p => fun q => squashc A R x y p q` at the type `isSet (Quot A
  R)`, and quotNatIsSet. Four face refl rows: sqFaceI0 (`<j> squashc Nat
  natSame x y p q @ i0 @ j` is p), sqFaceI1 (q), sqFaceJ0 (`<i> ... @ i @
  i0` is `<i> x`), sqFaceJ1 (`<i> y`). quotRec is the eliminator into a
  set B (`sB : isSet B`, the squashc case `fun x => fun y => fun p => fun
  q => sB x y p q`). foldCls (`fold B f e s (cls Nat natSame a)` is `f
  a`, with `s : isSet B` passed directly as the squashc case) and foldEqc
  (`<i> fold B f e s (eqc Nat natSame a b r @ i)` is `e a b r`) are refl
  rows. quotId folds Quot into itself with the square case `quotIsSet Nat
  natSame`. quotRecOne abstracts over `sN : isSet Nat` (Nat has no isSet
  proof) and gives the eval row `quot quotRecOne => fun sN => 1`.
  test/json/quot.json holds the instances quotZero and quotEnd, both
  `{"tag":"cls","cla":0}`. Negative control: sqFaceI0 with the end `q`
  gives TYPE_REFL "the start normalizes to <j> p @ j, the end normalizes
  to q".
- Check rows (test/check/expect.txt, 3 new): hit-boundary-square.lang
  (`family Sq := pt | sq <i j> [ ~i -> pt, i -> pt, ~j -> pt ]`)
  HIT_BOUNDARY; hit-case-square.lang HIT_CASE, a fold of `Quot Nat
  natSame` into itself with the squashc case `fun x => fun y => fun p =>
  fun q => <i> <j> squashc Nat natSame x y p q @ i @ (j \/ (i /\ ~i))`:
  right on both i faces, but on the j = i0 face it is the stuck square
  at `i /\ ~i`, not x (`i /\ ~i` is not i0 in the De Morgan interval: the
  probe `<i> p @ (i /\ ~i)` against `<i> 1` gives TYPE_REFL); the message
  is "the case for squashc must match its boundary and have the type (_x0
  : Quot ...) -> ... -> Path (Path (Quot ...) _x0 _x1) (<j> _x2 @ j) (<j>
  _x3 @ j): the path <j> ... does not have the end points of its type: at
  i0 it normalizes to squashc Nat ..."; reserved-isSet.lang (`def isSet :
  Nat := 0`) REFUSE_NAME.
- Mutants (each with `make -B check`, source restored by the inverse
  edit, `git diff | md5` identical before and after): 3 KILLED, 0
  SURVIVED.

| Mutant | First failing row |
|---|---|
| boundary_core checks the first binder only (`j < k && j < 1u`, check.c) | hit-boundary-square.lang: the 3-face Sq family is accepted, so the row fails as REFUSE_DATA `Sq: a program cannot declare a family (rule R1)` (want exit 1 and HIT_BOUNDARY, got exit 1: REFUSE_DATA) |
| check_plam skips the end test of a path nested inside another path (a static depth counter: the inner j faces untested, the outer i faces tested; check.c) | hit-case-square.lang `want exit 1 and HIT_CASE, got exit 0`; every other row stays GREEN |
| rule_is_set returns core_is_prop (check.c) | `make check-clang`: `error: unused function 'core_is_set' [-Werror,-Wunused-function]`; the tcc build with `sh test/gate.sh` fails examples/quot.lang at quotIsSet (TYPE_MISMATCH `a function is not a term of type Path (Quot A R) x y`), then the eval row quotRecOne and the quot.json golden |

  Mutant 2 realization: the case check is the generic path check
  (check_plam) over the nested PathP type that hit_case_type builds. The
  binder loop of hit_case_type has no face test to drop (a dropped layer
  drops its fiber and breaks every square), so the mutant lives in
  check_plam. Only hit-case-square.lang kills it: no older row tests the
  inner ends of a square.
- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  20; examples 26, parse refusals 17, check refusals 99, evals 157, json
  22 builds, build refusals 7, circuits 3, same-ir 1, circuit refusals
  10, satcheck refusals 6; node regressions 6, printer 2). `make
  check-clang` exit 0. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (the 3 deploy simulations ran). `git diff
  --exit-code -- test/circuit contracts/src/verifiers` exit 0.
  contracts/test/fixtures restored from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Review fixes (2026-10-16): later independent fields retain the correct
  parameter scope during transport and hcomp. Dependency detection scans
  the normalized field type instead of counting unused closure captures.
  Boundary path fields fold pointwise with their existing images preserved;
  unused future case variables are excluded from the resulting closure.
  A plain field that depends on an earlier recursive field is refused with
  DOMAIN_FAMILY because the non-dependent fold cannot type its case.
- Review validation: seven native C regressions added (six fail against
  the original staged source, one checks supported dependent projections).
  The three independent-field controls also pass against HEAD. `make check`
  and `make check-clang` pass; native HIT regressions: 27 checked, 0 failed.
- Open items:
  - Transport and hcomp are stuck (NULL, no diagnostic) on a dependent
    field and on a Path field.
  - quotRecOne abstracts over `isSet Nat`: Nat has no isSet proof.
  - hit-case-square.lang is the only row that tests the inner ends of a
    square (the mutant 2 proof).

### s5: Direction 4 smoke and the quotient circuit (2026-10-16)

- Surface form: no new syntax. No src/ change. No evaluator rule was
  refuted, so kan.c and eval.c did not change.
- Domain (domain/domain.lang, lines 29 and 32): `family OTPSpec :=
  message | cipher | encrypt <i> [ ~i -> message, i -> cipher ]`, the
  Direction 4 specification (design/DESIGN.md lines 118-196). `family
  GQuot (A : Type 0) (R : A -> A -> Type 0) := gcls (gcla : A) | geqc
  (geqa : A) (geqb : A) (geqr : R geqa geqb) <i> [ ~i -> gcls A R geqa, i
  -> gcls A R geqb ]`, the graph quotient (Quot without squashc). Each
  family has a comment. The existing rows did not change.
- examples/otp.lang (new): the defs fill0 to isoToEquiv are a copy of
  examples/iso.lang (a .lang file has no import). `interp : (Impl : Type
  0) -> (m c : Impl) -> (enc : Path Impl m c) -> OTPSpec -> Impl` is
  `fold Impl m c enc x`. The design postulates Impl, m, c and enc, so
  they are arguments. The `refl` rows interpMessage, interpCipher,
  interpEncrypt (`Path (Path Impl m c) (<i> interp ... (encrypt @ i))
  enc`) and interpHcomp (the s3 commute rule) hold for each Impl. The
  Field instance is interpField = `interp Field 7 7 refl`: a path between
  two distinct numerals does not exist, so m = c = 7 and enc = refl.
  fieldMessage and fieldEncrypt use it.
- specImpl is a function of the iso data (Impl, f, g, s, t) to
  `isoToEquiv OTPSpec Impl f g s t`. A closed Equiv holds a function, so
  it has no JSON form (JSON_VALUE). specPath is `ua (specImpl ...)`.
  transportMessage, transportCipher and lawImpl (`Path (Path OTPSpec
  message cipher) (<j> lawAt j) encrypt`, the transport of the law) are
  `refl` rows at the identity iso (otpId, idSect). The Glue transp
  reduces here: eval prints `message` for specMessage and `fun j =>
  encrypt @ j` for lawAt.
- The specImpl gap: OTPSpec is the walking interval. An iso into a type
  other than OTPSpec needs the retraction `(x : OTPSpec) -> Path OTPSpec
  (g (f x)) x` for a variable x. The non-dependent fold cannot give it
  (D6). An iso into Unit also needs Unit eta, a type-directed rule
  (prohibited). Thus M4-PLAN section 4 row s5 holds for the identity iso
  at OTPSpec only, not for Impl in Field or Nat.
- examples/quotSq.glue (new): SqRel `fun a => fun b => Path Field (a * a)
  (b * b)`; `sq : GQuot Field SqRel -> Field := fun q => fold Field (fun
  a => a * a) (fun a => fun b => fun r => r) q`; `circuit square public
  (y : Field) private (x : Field) := sq (gcls Field SqRel x) == y`. The
  circuit name is square: the IR JSON holds the name, and the gate keys
  the goldens by the name (test/gate.sh line 153). Thus the rows use the
  square goldens, and no quotSq.circuit.json or quotSq.witness.json file
  exists. `langc circuit` and `langc witness ... y=9 x=3` give files
  equal to test/circuit/square.circuit.json and square.witness.json.
- Why GQuot: the squashc case of a fold out of Quot is a term of `isSet
  Field`. The hfill square `fun x y p q => <i> <j> hcomp Field (i \/ ~i
  \/ j \/ ~j) (fun k => [ i -> q @ j, ~i -> p @ j, j -> y, ~j -> x ]) (p
  @ j)` gives `HCOMP_BASE: squareCase: hcomp needs u i0 to be u0 on i,
  but u i0 is q @ j and u0 is p @ j`: an hfill square needs the square
  as its base. test/check/quot-square-hcomp.lang (new) records this.
  examples/quot.lang keeps squashc (quotRec takes `sB : isSet B`).
- CIRCUIT_LOWER: none of the probed circuits gives it. These constraints
  all lower to a Field normal form: `fold Field x (<i> x) (hcomp Circle i0 (fun k => [
  i0 -> loop @ k ]) base)`; `sq (transp (<_> GQuot Field SqRel) i0 (gcls
  Field SqRel x))`; `sq (transp (<_> GQuot Field SqRel) i0 (hcomp (GQuot
  Field SqRel) i0 (fun k => [ i0 -> gcls Field SqRel x ]) (gcls Field
  SqRel x)))`; `sq (hcomp (GQuot Field SqRel) i0 (fun k => [ i0 -> gcls
  Field SqRel x ]) (gcls Field SqRel x))`; `fold Field x (<i> x) (transp
  (<_> Circle) i0 (hcomp Circle i0 (fun k => [ i0 -> base ]) base))`. A
  circuit has only Field binders, but those binders can still occur in
  HIT terms, such as `gcls Field SqRel x`. These probes do not establish
  that every HIT term is closed or reduces. O4 stays open.
- The `transp (<i> Susp (L i)) i0 (merid a @ j)` row under a binder
  exists since s2 (examples/circle.lang meridT, eval row `circle
  meridT`). circle.lang did not change.
- Tests: test/eval/expect.txt +5 (`otp fieldMessage => 7`, `otp
  fieldEncrypt => <i> 7`, `otp interpField => fun x => fold Field 7 7 (<i>
  7) x`, `otp specMessage => message`, `otp lawAt => fun j => encrypt @
  j`). test/json/otp.json (new: the instances fieldMessage 7 and
  specMessage message). test/circuit/expect.txt `quotSq square y=9 x=3`,
  same-ir.txt `quotSq square square`, refuse.txt `1 WITNESS_FAIL witness
  quotSq square y=10 x=3`. test/check/expect.txt `quot-square-hcomp.lang
  HCOMP_BASE`.
- Mutants (each with `make -B check`, reverted, diff restored, final
  `make -B check` GREEN): 3 KILLED (and 1b), 0 SURVIVED.

| Mutant | First failing row |
|---|---|
| 1. examples/otp.lang interp `fold Impl c m enc x` | examples/otp.lang: `HIT_CASE: interp: the case for encrypt must match its boundary and have the type Path Impl c m` (also eval otp fieldMessage, fieldEncrypt) |
| 1b. examples/otp.lang interp `fold Impl c m (<i> enc @ ~i) x` (the points swapped, the case well typed) | the interpMessage `refl` row: `TYPE_REFL: interpMessage: refl cannot prove the path: the start normalizes to c, the end normalizes to m` |
| 2. test/circuit/same-ir.txt `quotSq square range` | `FAIL same-ir quotSq square: the output differs from test/circuit/range.circuit.json` |
| 3. src/front/kan.c transp_ctor gives `val_ivl_end(m, 0)` for `a->args[k]` (the interval arguments dropped to i0) | `FAIL eval circle meridT: ... got exit 0: fun L => fun a => <j> transp (<i> Susp (L i)) i0 (merid (L i0) a j)` |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  27; examples 28, parse refusals 17, check refusals 100, evals 162, json
  23 builds, build refusals 7, circuits 4, same-ir 2, circuit refusals
  11, satcheck refusals 6; node regressions 6, printer 2). `make
  check-clang` exit 0. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed, the 4 deploy simulations ran). `git diff
  --exit-code -- contracts/src/verifiers` exit 0. The test/circuit diff
  is only the 3 table rows above; no *.circuit.json changed.
  `make e2e` reads test/circuit/expect.txt, including the new quotSq row:
  it proves four rows and regenerates three distinct verifiers, with
  square processed twice. The verifier sources are byte-identical;
  the randomized proof fixtures change. contracts/test/fixtures restored
  from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Open items:
  - (a) specImpl into an Impl other than OTPSpec needs the retraction,
    thus a dependent eliminator (`elim`).
  - (b) O4 stays open: no probed circuit gives CIRCUIT_LOWER from a HIT
    term.
  - (c) No quotSq.circuit.json or quotSq.witness.json goldens: the gate
    keys the goldens by the circuit name.
  - (d) An empty system `[ ]` in a term gives PARSE_EXPECT.

### s6: close (2026-10-16)

The main loop built s6 by hand (sessions s6-a and s6-b). The two review
agents stopped on a rate limit, and the two Opus fallbacks stopped on a
model error. They wrote no file. s6 adds no op, no code and no test row.

- Docs (the s6 change): SPEC.md 2.4 (the JSON sentence: a constant path
  gives `null`, a path with content gives JSON_VALUE), 2.5 (the "Other
  formers" sentence points to 2.6), 2.6 (new: the HIT forms, the typing
  rules, the evaluator rules in order, conversion, the printer, JSON D9,
  `isProp` and `isSet` with their laws, the names, circuits, the codes
  HIT_BOUNDARY, HIT_CASE and HIT_CASES, and the codes with new causes), 9
  (O5 closed by D9; O8 narrowed; O12 new, items (a) to (f)). docs/PLAN.md:
  M4 DONE 2026-10-16 with the slice commits, and the sentence that the
  Direction 4 law holds only for the identity iso at OTPSpec.
  formers/FORMERS.md: no change. No glue-lang note and no matrix cell
  names the Kan values at families (the `O8` of the F4 cell is the O8 of
  the template column). The matrix has one row for each of the formers F1
  to F15, and a HIT is not one of them. docs/STATUS.md: the M4 summary
  and this section.

#### Mutant roll-up

Each mutant ran with `make -B check`, source restored after: 15 KILLED, 0
SURVIVED (3 in each of s1 to s5), and the s5 variant 1b KILLED. The slice
sections give the mutant forms and the failure counts.

| Slice | Mutant | Result | Killing row |
|---|---|---|---|
| s1 | Boundary coverage test off (check.c boundary_core) | KILLED | check hit-boundary.lang (REFUSE_DATA, not HIT_BOUNDARY) |
| s1 | Boundary rule reads the first branch only (eval.c pcon_boundary) | KILLED | examples/circle.lang (loopPath: INTERNAL) |
| s1 | Path constructor type takes the i1 body at both ends (check.c pcon_end) | KILLED | examples/circle.lang (`m`: TYPE_MISMATCH) |
| s2 | A HIT takes the closed-type hcomp rule (kan.c kan_hcomp) | KILLED | check hcomp-i0-circle.lang (exit 0, not TYPE_REFL) |
| s2 | The family closed-type transp rule applies with one parameter (kan.c kan_transp) | KILLED | examples/circle.lang (meridT: PATH_BOUNDARY) |
| s2 | The JSON constant-path test always writes `null` (json.c path_json) | KILLED | build path-content.lang (exit 0, not JSON_VALUE) |
| s3 | recode_case returns NULL without re-coding (check.c) | KILLED | native HIT `HIT fold cannot distinguish connected points` (TYPE_REFL, not HIT_CASE) |
| s3 | fold_hit at OP_PCON drops the papp_value loop (eval.c) | KILLED | native HIT `HIT fold computes on a path constructor interior` (TYPE_REFL) |
| s3 | kan_fold_hcomp base argument is `var(m, 0)` (kan.c) | KILLED | examples/circle.lang (foldHcomp: TYPE_REFL) |
| s4 | boundary_core checks the first binder only (check.c) | KILLED | check hit-boundary-square.lang (REFUSE_DATA, not HIT_BOUNDARY) |
| s4 | check_plam skips the end test of a nested path (check.c) | KILLED | check hit-case-square.lang (exit 0, not HIT_CASE) |
| s4 | rule_is_set returns core_is_prop (check.c) | KILLED | `make check-clang` (unused core_is_set); tcc gate examples/quot.lang (quotIsSet: TYPE_MISMATCH) |
| s5 | 1. examples/otp.lang interp `fold Impl c m enc x` | KILLED | examples/otp.lang (interp: HIT_CASE) |
| s5 | 1b. examples/otp.lang interp with the points swapped (`<i> enc @ ~i`) | KILLED | examples/otp.lang (interpMessage: TYPE_REFL) |
| s5 | 2. test/circuit/same-ir.txt `quotSq square range` | KILLED | same-ir quotSq square (the output differs from range.circuit.json) |
| s5 | 3. kan.c transp_ctor gives `val_ivl_end(m, 0)` for the interval arguments | KILLED | eval circle meridT (the transp stays stuck) |

#### Review

Review of the M4 diff (`git diff 9136426..HEAD -- src`, 1646 lines with
`-U2`). The review agents stopped on API errors (a Fable rate limit, then
the Opus fallback once), so the review is a hand review in the main
session (s6-a). Result: no defect found, no code change.

1. No conv rule reads a type. conv_values, conv_path, conv_sys and the
   other conv functions have no M4 hunk: OP_PCON, OP_FOLD_HIT and an
   `hcomp` at a HIT compare argument by argument (the M3 path). kan.c
   kan_hcomp: closed_type is Univ, Nat, Field, Unit and Flag only, and the
   data rule needs plain_family (path_ctors == 0). At a HIT only the face
   `i1` rule applies, then the canonical return (D3). There is no rule
   `hcomp A i0 u u0 = u0` (D8).
2. Switch cases: `make check-clang` uses `-Wswitch-enum -Werror`. The
   diff adds no `default:` line (rg count 0).
3. Globals: each added file-scope line is a prototype. There is no new
   static local.
4. NULL on a diagnostic: each added kan_fail, diag_fail, fail_at and
   refuse call, and each of the 16 added FAIL calls, returns NULL or 0
   (`nul(FAIL(..))`, or `return FAIL(..)` in an int function). The fold
   boundary helpers test `bv` and `fv` for NULL. quote_value and core_bind
   accept NULL.
5. IR: `git diff 9136426..HEAD --stat -- test/circuit
   contracts/src/verifiers` gives 3 added rows (expect, refuse and
   same-ir, each for quotSq) and no verifier change.

Scope: the review covered the checklist of the s6 plan (items 1 to 5). It
is not a line-by-line audit of each rule. The 16 KILLED mutants and the
guest rows cover the rules.

#### Gates

Gates in s6-b on HEAD c7090b1 with the s6 doc changes. s6 changes no code
or test file, and test/gate.sh and the Makefile read no doc file. The
counts are equal to the s5 counts. `make -B check` exit 0 (native circuit
regressions 9, native path regressions 42, native face regressions 35,
native HIT regressions 27, examples 28, parse refusals 17, check refusals
100, evals 162, json 23 builds, build refusals 7, circuits 4, same-ir 2,
circuit refusals 11, satcheck refusals 6, each with 0 failed; gate 0
failures; regressions 6 of 6; printer 2). `make check-clang` exit 0.
`forge build --root contracts --sizes` exit 0. `make e2e` exit 0 (33 Forge
tests passed, the 4 deploy simulations ran). IR check: `git diff
--exit-code -- test/circuit contracts/src/verifiers` exit 0.
contracts/test/fixtures is restored from HEAD, and `forge test --root
contracts` passes again (33 passed, 0 failed).

#### Open items

- O4 (M1, SPEC.md 9 O12 (f)): no probed circuit gives CIRCUIT_LOWER from
  a HIT term (s5). Milestone: none.
- O8 (M3, SPEC.md 9, narrowed in M4): `hcomp` stays stuck at a Sum,
  Option, List or plain family when `u0` and the branches do not have the
  same constructor, and at a stuck type. `transp` stays stuck at a neutral
  value of a Sum, Option or List. Milestone: the fold and unfold families
  in M6.
- O12 (M4, SPEC.md 9): (a) `fold B cases x` with a neutral scrutinee; (b)
  `transp` at a family with a neutral argument; (c) `transp` of a path
  constructor whose fieldwise transport does not agree with transport on
  a boundary face (no general boundary correction terms, s2); (d)
  `transp` and `hcomp` at a constructor with a dependent field or a Path
  field stay stuck with no diagnostic (s4); (e) no dependent eliminator
  (USER ruling 2026-10-15: `fold`, non-dependent, no `elim` in M4); (f)
  O4. Milestone: (e) is a USER question for the M5 kickoff.
- The s5 gap: specImpl into an Impl other than OTPSpec needs the
  retraction `(x : OTPSpec) -> Path OTPSpec (g (f x)) x` for a variable
  `x`, thus a dependent eliminator (O12 (e)).
- s4: quotRecOne abstracts over `isSet Nat`, because Nat has no isSet
  proof. hit-case-square.lang is the only row that tests the inner ends
  of a square.
- s5 (c): no quotSq.circuit.json or quotSq.witness.json goldens, because
  the gate keys the goldens by the circuit name.
- s5 (d): an empty system `[ ]` in a term gives PARSE_EXPECT.
- Closed in M4: O5 (D9, s2). The s1 items: the stuck-transp emit row (s2
  replaced it with test/emit/transp-hcomp.lang) and the `Path Self a b`
  fields (s4). The s2 item on dependent field types (s4). The s3 item on
  k = 2 tests (s4).

## M5

M5 adds quotient inductive types (QITs), the dependent eliminator `elim`
and an inductive-inductive block. docs/M5-PLAN.md is the plan, with the
USER rulings of its section 9 (2026-10-16: each recommendation). Summary:
s1 adds QITs as HITs with constructor equations (Bag, Int). s2 adds `elim`
with its point, path and hcomp rules. s3 adds the Kan rules at dependent
and Path fields (O12 (d) closed). s4 adds the `mutual` block of two
families (Ctx, Ty), and s5 adds its fold (OP_FOLD_BLOCK). s6 adds the
Direction 2 smoke (transportProp, and examples/intSq.glue in same-ir). M5
does not change the IR. M5 has six slice commits and the close (s7).

### s1: constructor equations with recursive ends (2026-10-16)

- Plan: docs/M5-PLAN.md (new) is in this commit (D12). s1 covers D1 and
  D2. No grammar change. No src/ change: each probe passed, so no fix
  site changed.
- Names (D11): `rg -w 'Bag|Int|bnil|bcons|bswap|pos|neg|zeq|bsize|negate'
  examples domain test` gave no hit before s1. The field names bx, bxs,
  sx, sy, sxs, pn and nn also gave no guest hit.
- Domain (domain/domain.lang, the section 5 text): `family Bag (A : Type
  0) := bnil | bcons (bx : A) (bxs : Bag A) | bswap (sx : A) (sy : A)
  (sxs : Bag A) <i> [ ~i -> bcons A sx (bcons A sy sxs), i -> bcons A sy
  (bcons A sx sxs) ]` and `family Int := pos (pn : Nat) | neg (nn : Nat)
  | zeq <i> [ ~i -> pos 0, i -> neg 0 ]`. The existing rows did not
  change: with only the domain change, `make -B check` gave the M4 counts.
- Probes (a scratch .lang before the tests):
  - (a) `bswap A x y xs @ i0` normalizes to `bcons A x (bcons A y xs)`,
    and `@ i1` to `bcons A y (bcons A x xs)` (the boundary rule). PASS.
  - (b) `bsize A (bswap A x y xs @ k)` normalizes to `natAdd (natAdd
    (bsize A xs) 1) 1`. The bswap case type has the folded nested ends
    (hit_case_end, eval_fold_hit_boundary). PASS.
  - (c) `transp (<i> Bag (L i)) i0 (bcons (L i0) a xs)` gives `bcons (L
    i1) (transp (<i> L i) i0 a) (transp (<i> Bag (L i)) i0 xs)`. At
    `bswap (L i0) a b xs @ r` it gives `bswap (L i1)` of the three
    transported fields `@ r`: transp_pcon_agrees holds. PASS.
  - (d) `negate (zeq @ k)` normalizes to `zeq @ ~k`. PASS.
- D2 result: the hypothesis holds for each s1 equation (bswap, zeq).
  `transp` commutes with each point constructor of an end, so the
  fieldwise rule agrees with the transported ends. No s1 equation stays
  stuck under O12 (c).
- `succ`: the plan (section 4) writes `succ n`. glue-lang has no `succ`
  core name, so bsize uses `natAdd n 1`, as `depth` in the domain:
  `fold Nat 0 (fun x => fun n => natAdd n 1) (fun x => fun y => fun n =>
  refl)`.
- examples/bag.lang (new): swapPath, swapStart, swapEnd (the boundary
  rule), bsize, sizeCons (the point rule), sizeSwap (the path rule, the
  ends fold the nested constructors), transpCons (the constructor rule),
  bagThree (a closed Bag value for the JSON golden), and the eval defs.
- examples/int.lang (new): zeqPath, zeqStart, zeqEnd, negate, negatePos,
  negateNeg, negateZeq (`<i> negate (zeq @ i) = <i> zeq @ ~i`),
  negateTwice, and the eval defs.
- Tests: test/eval/expect.txt +10 (`bag swapAt`, `sizeThree => 3`,
  `sizeSwapAt => fun k => 3`, `sizeOpen`, `transpConsAt`, `transpSwap`;
  `int negThree => neg 3`, `negZeqAt => fun k => zeq @ ~k`, `twiceAt =>
  fun k => zeq @ k`, `negHcomp` (the fold hcomp rule at Int)).
  test/check/expect.txt +4: hit-boundary-bag.lang HIT_BOUNDARY (a bswap
  equation with the face ~i only), hit-case-bag.lang HIT_CASE (a bswap
  case with the ends `natAdd n 1`), hit-int-end.lang TYPE_MISMATCH (`zeq
  @ i0` at the type Nat), hcomp-i0-int.lang TYPE_REFL (the guard: `hcomp
  Int i0 (fun i => [ i0 -> pos 0 ]) (pos 0)` does not convert to `pos
  0`). test/json/bag.json and test/json/int.json (new goldens: a Bag value
  and an Int value).
- Mutants (each with `make -B check`, reverted, final `make -B check`
  GREEN): 4 KILLED, 0 SURVIVED. `make check` stops at the first failing
  harness, so for mutants 2 and 3 the guest gate (test/gate.sh) also ran
  alone.

| Mutant | First failing rows |
|---|---|
| 1. eval.c eval_fold_hit_boundary keeps the inner constructor unfolded (`recursive == 1 ? x->args[p + i]`) | examples/bag.lang: `INTERNAL: bsize: the evaluator found a Nat operation on a value that is not a Nat`; hit-case-bag.lang; eval bag swapAt, sizeThree, sizeSwapAt. All 27 native HIT rows pass: only the s1 rows kill it. |
| 2. kan.c transp_pcon_agrees always true (`0 && !transp_pcon_agrees(..)`) | native HIT row `transport respects a path constructor application boundary: normalization changed endpoint i1` (M4, the FunH family). The guest gate alone passes: no Bag or Int equation makes the check false (D2). |
| 3. check.c hit_case_end takes the `~i` end twice (the second call with `0`) | 3 native HIT rows (M4); the guest gate alone: examples/int.lang `HIT_CASE: negate: the case for zeq must match its boundary and have the type Path Int (neg 0) (neg 0)`, also otp, quot, trunc, quotSq. |
| 4 (extra). kan.c transp_pcon_agrees always false (`(1 \|\| ..)`) | eval circle meridT (M4) and eval bag transpSwap: the transp stays stuck. |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  27; examples 30, parse refusals 17, check refusals 104, evals 172, json
  25 builds, build refusals 7, circuits 4, same-ir 2, circuit refusals
  11, satcheck refusals 6; node regressions 6, printer 2). `make
  check-clang` exit 0. `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed). `git diff --exit-code -- test/circuit
  contracts/src/verifiers` exit 0: no IR change. contracts/test/fixtures
  restored from HEAD, then `forge test --root contracts` 33 passed, 0
  failed.
- Open items:
  - (a) Mutant 2 has no guest row. A guest row needs a family with an
    application in a boundary (the native FunH family), and rule R1 keeps
    families in domain/domain.lang, which s1 changes only by the section 5
    text.
  - (b) The plan writes `succ` in section 4; the rows use `natAdd n 1`.
    s7 (SPEC) uses the `natAdd` form.

### s2: the dependent eliminator elim (2026-10-16)

- Plan: D3, D4, 3.4, 3.5, 7 and 8 of docs/M5-PLAN.md. `elim P cases x`:
  `P : (y : D params) -> Type n` is explicit, one case for each
  constructor in declaration order, the scrutinee last. A case takes each
  field;  after a recursive field `f` it takes the IH `P f`. A path case
  with k binders has the type `PathP (<i1> .. PathP (<ik> P (pc params
  fields @ i1 .. @ ik)) ..)`, with the ends `elim P cases t` of the
  boundary bodies t. `fold` does not change.
- src/front/core.h: the op OP_ELIM. src/front/check.c: the elim rule (the
  motive is a family of types over D, else TYPE_MISMATCH;  the case types,
  ELIM_CASES and ELIM_CASE;  `elim` is a core name, so `def elim` gives
  REFUSE_NAME). hit_case_end, case_field_type, hit_case_type and
  recode_case take the IHs. src/front/eval.c: eval_hit_case applies each
  case to the fields and, for OP_ELIM, the IHs;  the path rule applies the
  path case, then `@ r1 .. @ rk`;  a neutral scrutinee gives a stuck elim;
  the printer writes `elim P cases x`. src/front/kan.c, kan.h:
  kan_elim_hcomp (the D4 hcomp rule at a HIT, through gcomp). A plain
  family has no hcomp rule for elim. src/json.c: OP_ELIM is a stuck op
  (JSON_VALUE).
- Probes (s2-c): the recursive field type is `D params` (check_type,
  self_applied);  the constructor values are OP_CTOR and OP_PCON
  (reduce_pcon gives the same kind at fresh interval variables);  the
  printer writes `elim P cases x`;  `def elim` gives REFUSE_NAME. The
  index arithmetic of kan_elim_hcomp agrees with D4 binder by binder (the
  line, the tube, the base).
- The hcomp row (eval circle elimHcomp): derived by hand first (a scratch
  file), then run:  MATCH. One expected deviation:  the `k` branch is
  restricted to `k = i1`, so hfill at `i \/ j` prints as `loop @ (i \/ j)`
  (the face rule).
- Examples: examples/circle.lang elimBase (the point rule), elimLoop (the
  path rule at PathP), elimHcomp, elimStuck, stackSum and stackSumFive
  (elim at the plain family Stack, the IH of the recursive field rest).
  examples/int.lang negNeg (negate is an involution, the zeq case is the
  square `<i> <j> zeq @ i`), negNegZeq. examples/otp.lang flip, ret (the
  retraction of flip by elim), flipPath (`ua (isoToEquiv OTPSpec OTPSpec
  flip flip ret ret)`), flipMessage (`transport flipPath message =
  cipher` by refl), flipLaw (`<i> transport flipPath (encrypt @ i)`). A
  closed Equiv holds a function and has no JSON form, so flipPath holds
  the ua path and test/json/otp.json does not change. No golden changes.
- Tests: test/eval/expect.txt +6 (circle elimHcomp (exact text),
  elimStuck, stackSumFive `<i> 5`;  int negNegZeq;  otp flipMessage `<i>
  cipher`, flipLaw `<i> encrypt @ ~i`). test/check/expect.txt +6:
  elim-missing-case.lang ELIM_CASES, elim-no-ih.lang ELIM_CASES (a Stack
  push case without the IH), elim-path-ends.lang ELIM_CASE, elim-motive-
  nat.lang TYPE_MISMATCH, elim-def.lang REFUSE_NAME, elim-int-vars.lang
  TYPE_REFL (the guard: two variables `x y : Int` do not convert).
  test/emit/expect.txt +1: elim-instance.lang JSON_VALUE (see open item
  (c)).
- Mutants (each with `make -B check`, reverted, final `make -B check`
  GREEN): 3 KILLED, 0 SURVIVED.

| Mutant | First failing rows |
|---|---|
| 1. eval.c eval_hit_case: the IH of a recursive field is the field (`apply_value(m, acc, x->args[fam->param_count + i])`) | examples/circle.lang `INTERNAL: stackSumFive: the evaluator found a Nat operation on a value that is not a Nat`. Only the s2 row stackSumFive kills it:  no other elim row has a recursive field. |
| 2. eval.c eval_hit_case: the path rule drops `@ r` (`&& o != OP_ELIM`) | examples/circle.lang `TYPE_REFL: elimLoop: refl cannot prove the path`;  eval int negNegZeq. |
| 3. kan.c kan_elim_hcomp: the line is `<i> P u0` (`var(m, 2u)` for `fill`) | `make check` stops at the unused variable `fill` (-Werror);  the guest gate alone: eval circle elimHcomp (the line is `P base`). |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  27; examples 30, parse refusals 17, check refusals 110 (s1: 104), evals
  178 (s1: 172), json 25 builds, build refusals 8 (s1: 7), circuits 4,
  same-ir 2, circuit refusals 11, satcheck refusals 6; node regressions
  6, printer 2). `make check-clang` exit 0. `forge build --root contracts
  --sizes` exit 0. `make e2e` exit 0 (33 passed). `git diff --exit-code
  -- test/circuit contracts/src/verifiers` exit 0: no IR change.
  contracts/test/fixtures restored from HEAD, then `forge test --root
  contracts` 33 passed, 0 failed.
- Build record: s2-a had 2 agent deaths (fable/xhigh: Fable 429;  the
  opus/xhigh fallback: the reasoning_extraction classifier). s2-b, s2-c
  and s2-d are hand builds in the main loop.
- Open items:
  - (a) PRE-EXISTING printer quirk (not s2): the chain loop of eval.c
    print_value ends with `print_other(p, v, atom)`, which skips is_pcon.
    So a path constructor that is the last argument of a chain op prints
    as `loop i`, not `loop @ i` (seen in ELIM_CASE messages: `PathP (<i>
    Path Circle (loop @ i) (loop i)) ..`). s2 does not fix it (no golden
    changes). A USER ruling for a later slice.
  - (b) ELIM_CASES messages print the Pi binders as `_x0`, not the field
    names.
  - (c) A closed instance cannot hold a stuck elim, so the OP_ELIM case of
    json.c op_json has no closed witness (as OP_FOLD_HIT). A closed
    scrutinee is a constructor;  an hcomp at a plain family reduces before
    elim reads it (`hcomp Stack i0 (fun i => [ i0 -> push 1 bottom ])
    bottom` is `bottom`);  the elim of an empty-face hcomp at a HIT is a
    gcomp at `i0`, which is a transp;  an axiom gives REFUSE_AXIOM (R2).
    test/emit/elim-instance.lang holds the nearest form:  a stuck elim
    under a function in a pair. Its JSON_VALUE comes from the function.
  - (d) USER ruling: elim and fold differ on an empty-face hcomp at a
    HIT. `fold Circle base (<i> loop @ i) (hcomp Circle i0 (fun i => [ i0
    -> base ]) base)` stays stuck (kan_fold_hcomp keeps the empty tube),
    but `elim (fun y => Circle) base (<i> loop @ i)` of the same value
    gives `base`:  kan_elim_hcomp uses gcomp, and ghcomp at `phi = i0`
    gives u0 by the face i1 rule (M3 s5). No rule reads a type, and D8 is
    about hcomp. But in the CCHM model the comp at an empty face is an
    hcomp with the empty system, which is a different element at a HIT.
    Options: (i) keep;  (ii) on the empty face, use `hcomp (P h) i0 (fun
    i => [ i0 -> .. ]) (transp line i0 (elim P cases u0))`, as the M4
    fold keeps the empty tube.

### s3: transp and hcomp at dependent and Path fields (2026-10-16)

- Step 0 (s3-a), the USER ruling on s2 open item (d) = (ii). This closes
  s2 open item (d). src/front/kan.c kan_elim_hcomp on the empty face
  builds `hcomp (P h) i0 (fun i => [ i0 -> .. ]) (transp line i0 (elim P
  cases u0))`. The non-empty face keeps gcomp. Probe before:  `elim (fun
  y => Circle) base (<i> loop @ i) (hcomp Circle i0 (fun i => [ i0 ->
  base ]) base)` gives `base`, but the fold of the same value stays
  stuck. After:  the elim gives `hcomp Circle i0 (fun i => [ i0 -> base
  ]) base`, the same text as the fold. Row:  examples/circle.lang
  elimEmptyFold (`Path Circle (elim ..) (fold ..) := refl`) and its eval
  row.
- Baseline fix:  the native HIT regressions are 32 at de04000, not 27 (s2
  above gives 27). The s2 commit added rows to test/hit-regressions.c.
  The s3 baseline is native circuit 9, path 42, face 35, HIT 32.
- Code (s3-b), src/front/kan.c:
  - field_type_fun (new):  the field type as a closed function `fun
    params fields => T` (eval at val_var, quote at the level params + K,
    CORE_LAM binders).
  - fill_apps (new):  applies FN to `G n (R)` for each earlier field n.
  - field_fill (new):  the M3 Sigma method (pair_rule):  `fun i => transp
    (<j> T[DOM(L, i /\ j) / p, F n (i /\ j) / x n]) (phi \/ ~i) x`. At i1
    it is the field of the result. A Path field gets a PathP line, so
    pathp_rule gives the transported ends.
  - field_line_transp:  takes `fills`. At `recursive == 2` or `dependent`
    it gives `fills[k]` at i1 (before:  NULL).
  - transp_ctor:  computes the fills of all fields when a field is
    dependent or a Path field.
  - field_type_params (new), hcomp_fill (new):  the M3 comp method
    (pair_fill, hcomp_pair_rule). The fill of a non-dependent field is the
    hfill. The fill of a dependent field is `fun r => hcomp B[r] (phi \/
    ~r) (fun i => [ phi -> S (FIELD N (u (i /\ r))), ~r -> S x ]) (transp
    (<i> B[i /\ r]) i0 x)`, with S = `transp (<j> B[(i \/ j) /\ r]) i` and
    B[s] = the field type at the params and the earlier fills at s.
  - hcomp_ctor:  a dependent field is its fill at i1 (before:  NULL). A
    non-dependent field keeps the field_hcomp form, so the old rows do not
    change. A HIT stays canonical under hcomp (D3):  hcomp_ctor is the
    rule for plain families only.
  - No conversion change.
- domain/domain.lang (rule R1), after Int, with a `-- M5 s3 (D5)`
  comment. Before s3, no domain family had a dependent field (`dependent`
  = "the type uses an earlier field", core.h:180;  a parameter does not
  count).
  - `family DPair (A : Type 0) (B : A -> Type 0) := dpair (dfst : A)
    (dsnd : B dfst)`.
  - `family Web (A : Type 0) := wnode (wn : A) | wlink (wlx : Web A) (wly
    : Web A) (wlp : Path (Web A) wlx wly) | wloop (wa : A) <i> [ ~i ->
    wnode A wa, i -> wnode A wa ]`. wlink is a POINT constructor with a
    Path field, so transp_pcon_agrees does not apply.
  - `rg -w` shows that the names are free. No golden change.
- Probes before (HEAD + step 0) and after (s3-b). The full texts are the
  rows of test/eval/expect.txt.
  - quotEqcTransp (Quot eqc, the dependent field eqr):  before STUCK `fun
    R => fun r => fun k => transp (<i> Quot Nat (R i)) i0 (eqc Nat (R i0)
    1 2 r k)`. After `fun R => fun r => fun k => eqc Nat (R i1) 1 2
    (transp (<j> R j 1 2) i0 r) @ k`.
  - quotEqcLine (eqc along `<i> Quot (A i) (R i)`):  after `.. eqc (A i1)
    (R i1) (transp (<i> A i) i0 a) (transp (<i> A i) i0 b) (transp (<j> R
    j (transp (<_x0> A (j /\ _x0)) ~j a) (transp (<_x0> A (j /\ _x0)) ~j
    b)) i0 r) @ k`. The fills have the face `~j` (`phi \/ ~i` at phi =
    i0).
  - dpairTransp:  after `dpair (A i1) (B i1) (transp (<i> A i) i0 a)
    (transp (<j> B j (transp (<_x0> A (j /\ _x0)) ~j a)) i0 b)`.
  - dpairHcomp (DPair, hcomp at the same constructor):  before, no family
    had a dependent field (hcomp_ctor gave NULL at a dependent field).
    After `dpair Nat B (hcomp Nat k (fun i => [ k -> p @ i ]) 1) (hcomp (B
    (hcomp Nat k (fun i => [ k -> [ k -> p @ i ], i0 -> 1 ]) 1)) k (fun i
    => [ k -> [ k -> transp (<j> B (p @ (i \/ j))) i (q @ i) ], i0 -> ..
    ]) (transp (<i> B (hcomp Nat (k \/ ~i) ..)) i0 b))`.
  - webTransp:  after `wlink (L i1) (transp ..x) (transp ..y) (<j> hcomp
    (Web (L i1)) (j \/ ~j) (fun i => [ i0 -> .., ~j -> transp (<k> Web (L
    (i \/ k))) i (transp (<_x0> Web (L (i /\ _x0))) ~i x), j -> .. y ])
    (transp (<i> Web (L i)) i0 (p @ j)))`. The ends are the transported
    ends.
  - quotSquashcStuck (Quot squashc, the Path fields sqp and sqq):  before
    and after STUCK `.. transp (<i> Quot Nat (R i)) i0 (squashc Nat (R
    i0) x y p q k l)`. See the squashc finding.
  - `build/langc eval test/emit/elim-instance.lang e` gives `pair (fun c
    => elim (fun y => Nat) 0 (<i> 0) c) 0`:  s2 open item (c) does not
    change.
- The Trunc finding:  squash along `<i> Trunc (L i)` (tT) is ALREADY
  REDUCED at HEAD and does not change:  `squash (L i1) (transp (<i> Trunc
  (L i)) i0 x) (transp (<i> Trunc (L i)) i0 y) @ k`. The fields lo and hi
  are `recursive == 1`, not Path fields. So the plan row "a Path field
  under transp (a Trunc squash)" has no Path field.
- The squashc finding:  the cause is transp_pcon_agrees. On the face ~k
  the fieldwise value is `p' @ l`, with p' the pathp_rule hcomp at the
  HIT (canonical, D3). The expected value is the stuck `transp (<i> Quot
  Nat (R i)) i0 (p @ l)`. The two values do not convert. A correct transp
  at a path constructor needs the boundary correction (an hcomp with the
  faces of the transported boundary, as in CHM). This is an open item,
  not s3. So the Path-field row and mutant m2 use Web wlink (a point
  constructor).
- Rows (function form, no new JSON instance, `-- M5 s3` comments):
  examples/quot.lang quotEqcTransp, quotEqcLine, quotSquashcStuck;
  examples/circle.lang elimEmptyFold, dpairHcomp, dpairTransp, webTransp.
  test/eval/expect.txt +7 (s3-a: elimEmptyFold;  s3-b: the 6 other rows).
  No golden change.
- Mutants (runner run.sh, each with `make -B` and the gate, then
  reverted;  kan.c is equal to the backup;  final `make -B check` GREEN):
  5 KILLED, 0 SURVIVED. Each mutant compiles;  each make exits 2 at the
  gate.

| Mutant | First failing rows |
|---|---|
| m0. kan_elim_hcomp (step 0): `return empty ? kan_hcomp` -> `return 0 ? kan_hcomp` | examples/circle.lang `TYPE_REFL` elimEmptyFold (examples: 1 failed);  `gate: 22 failures`. |
| m1. field_fill: the face `phi \/ ~i` -> `phi` | eval quot quotEqcLine, circle dpairTransp, circle webTransp (the faces `~j` and `~i` become i0);  `gate: 3 failures`. |
| m2. field_fill: the Path field keeps its old ends (`fill_apps(.., r)` -> `recursive == 2 ? zero(m) : r`) | eval circle webTransp (1 FAIL). |
| m3. field_line_transp: NULL again at a dependent field | eval quot quotEqcTransp, quot quotEqcLine, circle dpairTransp, circle webTransp (wlp is a Path field AND a dependent field);  `gate: 4 failures`. |
| m4. (extra) hcomp_ctor: NULL again at a dependent field (`!c->fields[k].dependent` in the fills test) | eval circle dpairHcomp;  `gate: 1 failures`. |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  32 (see the baseline fix);  examples 30, parse refusals 17, check
  refusals 110, evals 185 (s2: 178), json 25 builds, build refusals 8,
  circuits 4, same-ir 2, circuit refusals 11, satcheck refusals 6;  node
  regressions 6, printer 2). `make check-clang` exit 0. `forge build
  --root contracts --sizes` exit 0. `make e2e` exit 0 (33 passed). `git
  diff --exit-code -- test/circuit contracts/src/verifiers` exit 0:  no
  IR change. contracts/test/fixtures restored from HEAD, then `forge test
  --root contracts` 33 passed, 0 failed.
- Build record:  s3-a, s3-b and s3-c are hand builds in the main loop (no
  builder or reviewer agents).
- Open items:
  - (a) transp at a path constructor with a Path field (Quot squashc)
    stays stuck. It needs the boundary correction (see the squashc
    finding).
  - (b) The fills at r = i1 keep an `i0 -> ..` branch in the system (the
    face ~r at r = i1), as the M3 hcomp_pair does (pair_fill). So the type
    of dsnd prints `B (hcomp Nat k (fun i => [ k -> [ k -> p @ i ], i0 ->
    1 ]) 1)`, not `B dfst'`. No conv rule removes an i0 branch. If
    necessary, s4 checks this with a Path row.
  - s2 open items (a), (b) and (c) do not change. s2 open item (d) is
    closed (step 0).

### s4: the mutual block of two families (2026-10-16)

- Start state:  HEAD 7f4a8cd, tree 00d57a83 (the staged s3 tree). The s4
  prompt expected the subject `M5 s3: compute transp and hcomp at
  dependent and Path fields`. The subject is `M5 s3: add Kan rules for
  dependent and Path fields`. The tree is correct, so s4 starts on it.
- Code:
  - src/front/lexer.h, lexer.c:  the keyword `mutual` (TOK_KW_MUTUAL, the
    last TokKind).
  - src/front/ast.h:  Decl `block` (0, or 1 and 2 for the first and the
    second family of a block).
  - src/front/parser.c:  parse_mutual. A family must follow `mutual`, else
    PARSE_EXPECT `expected 'family' (a mutual block holds two families)`.
    parse_source reads the second family when `family` follows the first
    family of a block. `mutual` is not an atom.
  - src/front/core.h:  FamilyInfo `partner` (the other family of the
    block, or the family itself). FieldInfo `recursive` 3 is a partner
    field.
  - src/front/check.c:
    - check_block (D6):  the checker checks the parameters of A and
      registers A, then checks the parameters of B and registers B. Then
      it checks the constructors of A, then the constructors of B. Thus
      both names are known before a constructor (rule (2), the one
      exception to rule R3), and the constructor table holds the
      constructors of A, then the constructors of B. MUTUAL_BLOCK:  the
      block holds one family;  a constructor is a path constructor (rule
      (6), O13);  the last parameter of B (the index) does not have the
      type A at the parameters of B before it (rule (1)).
    - check_family is now family_head + family_ctors. A family that is
      not in a block does not change.
    - partner_applied, check_field:  a field of A with the type `B params
      g`, where g is an earlier recursive field of the constructor, is a
      partner field (recursive 3). It is also a dependent field. A
      different use of B in a field of A gives DOMAIN_FAMILY (rule (3)).
      A field of B follows the M4 field rule.
    - check_decl:  a program block gets REFUSE_DATA `a program cannot
      declare a mutual block (rule R1)` after the block check.
    - rule_fold:  a block family gives HIT_CASES (the block fold is s5,
      D7). rule_elim:  a block family gives ELIM_CASES (O13).
  - src/front/kan.c:  a comment only. The `recursive` sites:  the fold and
    elim sites of eval.c and check.c do not get a block family, because
    the checker refuses fold and elim at it. json.c, field_line_transp,
    transp_ctor and hcomp_ctor read a partner field as a dependent field
    (its fill). `make check-clang` finds no missing switch case.
- domain/domain.lang (rule R1), after Web, with a `-- M5 s4 (D6)`
  comment:  `mutual family Ctx := cempty | ext (eg : Ctx) (et : Ty eg)`
  and `family Ty (tg : Ctx) := tbase | arrow (ta : Ty tg) (tb : Ty tg)`.
  The old goldens do not change.
- examples/ctx.lang (test/eval/expect.txt +8, test/json/ctx.json new):
  - c1, t1, c2:  constructor values. A constructor of Ty takes the index
    first (`tbase cempty`, `arrow cempty (tbase cempty) (tbase cempty)`).
  - tyTransp, tyArrowTransp:  transp along `<i> Ty (P @ i)` with `P :
    Path Ctx cempty c1` gives `tbase c1` and `arrow c1 (tbase c1) (tbase
    c1)` (printed in full). The index is a parameter of the line.
  - tyTranspRefl, ctxTranspRefl:  refl rows.
  - ctxTransp:  `transp (<_> Ctx) r (ext g t)` gives `ext g t`.
  - The Ctx finding:  Ctx has no parameters, so each line at Ctx is
    constant and the constant rule applies first. No row runs the fill of
    a partner field along a line that is not constant.
- Guest refusal rows. The test/check files are program files, so each
  block check runs before REFUSE_DATA:
  - mutual-one.lang MUTUAL_BLOCK (one family);  mutual-index.lang
    MUTUAL_BLOCK (the index has the type Nat);  mutual-path.lang
    MUTUAL_BLOCK (a path constructor).
  - mutual-partner-index.lang DOMAIN_FAMILY (`(mat : Mb maz)`, the index
    is not a field);  mutual-partner-type.lang DOMAIN_FAMILY (`(maf : Mb
    mag -> Nat)`, B in a different type).
  - mutual-program.lang REFUSE_DATA (a correct block in a program).
  - forward-family.lang REFUSE_REC (a family names a later family, no
    block:  rule R3 does not change).
  - test/parse/mutual-def.lang PARSE_EXPECT (`mutual def`).
- Probes (each is also a row):  fold at Ctx (fold-ctx.lang) and fold at
  Ty (fold-ty.lang) give HIT_CASES;  elim at Ctx (elim-ctx.lang) gives
  ELIM_CASES;  the partner field before its field, `(mat : Mb mag) (mag :
  Ma)` (mutual-partner-later.lang), gives NAME_UNKNOWN (mag is not in
  scope yet). No probe gives INTERNAL or a crash.
- Mutants (~/Documents/glue-lang-M5-s4-mutants/run.sh, from the s3
  runner;  each with `make -B` and the gate, then reverted;  check.c is
  equal to the backup;  final `make -B check` GREEN):  3 KILLED, 0
  SURVIVED. Each mutant compiles;  each make exits 2.

| Mutant | First failing rows |
|---|---|
| m1. the index rule (1) off (the test -> `if (0)`) | mutual-index.lang gives REFUSE_DATA, not MUTUAL_BLOCK;  `gate: 1 failures`. |
| m2. B is not registered while the constructors of A are checked (`m->family_count--` before family_ctors of A, `++` after) | the domain gives REFUSE_REC at `Ty eg`;  all rows fail;  `gate: 405 failures`. |
| m3. the partner field is `recursive` 0 (`partner ? 3 : 0` -> `partner ? 0 : 0`) | the domain gives DOMAIN_FAMILY (et depends on the recursive field eg);  `gate: 407 failures`. |

- Gates: `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  32;  examples 31 (s3: 30), parse refusals 18 (17), check refusals 121
  (110), evals 193 (185), json 26 builds (25), build refusals 8,
  circuits 4, same-ir 2, circuit refusals 11, satcheck refusals 6;  node
  regressions 6, printer 2). `make check-clang` exit 0. `forge build
  --root contracts --sizes` exit 0. `make e2e` exit 0 (33 passed). `git
  diff --exit-code -- test/circuit contracts/src/verifiers` exit 0:  no
  IR change. contracts/test/fixtures restored from HEAD, then `forge test
  --root contracts` 33 passed, 0 failed.
- Build record:  s4-a and s4-b are hand builds in the main loop (no
  builder or reviewer agents).
- Open items:
  - (a) The fill of a partner field along a line that is not constant has
    no row (the Ctx finding). A block whose first family has a parameter
    can give this row.
  - s3 (a) does not change. s3 (b) does not change:  no block row needs
    that type. s2 (a), (b) and (c) do not change.

#### s4 review follow-up (2026-10-16)

- HIGH, fixed: the first family did not inherit the final universe of a
  partner field. With `WideTy g` containing a `Type 0` field, `WideCtx`
  was accepted as `Type 0` and rejected as `Type 1`. check_block now raises
  the first family's universe and checks both declarations again with the
  final bounds. This also rejects parameter and field type arguments that
  no longer fit their required sorts. A phantom index leaves the second
  family's universe unchanged.
- test/hit-regressions.c adds seven C regressions. Four fail against the
  original staged checker and pass with the fix. The other three preserve
  the phantom-index sort, prove the dependent partner transport equation
  along a parameter line by refl, and check dependent uniform parameters
  across both families. The transport equation closes s4 open item (a).
- Validation: `make check` passes, including check-clang and native HIT
  39/39. The full e2e pipeline passes using the unchanged cached halo2
  binary, including 33/33 Forge tests and all deployment simulations.
  `forge build --sizes` passes. The generated circuit IR and verifier
  source match the staged tree. No CI or test weakening was found.

### s5: the fold of a mutual block (2026-10-16)

- Start state:  HEAD 5a223ef (USER commit `M5 s4: add mutual family
  blocks`). Baseline fix:  at 5a223ef the native HIT count is 39, not 32.
  The USER s4 commit adds 7 rows to test/hit-regressions.c.
- Decision (D7):  a NEW op OP_FOLD_BLOCK (src/front/core.h, after
  OP_FOLD_HIT). Reason:  each OP_FOLD_HIT site takes the arity
  ctor_count + 2 and reads args[0] as the one result type (eval.c
  dispatch `n - 2u`, varied_arity, kan.c `ncases + 2u`,
  eval_fold_hit_boundary, kan_fold_path_field). Two motives change the
  arity test at each site, thus one op with two arities is a hidden mode.
  A new op keeps each OP_FOLD_HIT site unchanged, and -Wswitch-enum finds
  each switch that must handle the new op (json.c only).
- Core form:  inst is the first family of the block. The args are MA, MB,
  the cases (the first family, then the second family), then the
  scrutinee. The arity is ctor_count A + ctor_count B + 3.
- Code:
  - src/front/eval.c fold_block:  a case of the first family gets the
    fields. The recursive fields and the partner fields are folded. A
    case of the second family gets `fold .. index` first (index =
    x->args[A.param_count]), then the fields.
  - src/front/check.c fold_block_rule and block_case_type:  MA and MB are
    explicit. Every MB, including an unannotated lambda, checks against
    `MA -> Type level(MA)` as required by D7. A wrong
    case count gives HIT_CASES. A case that does not match its type gives
    HIT_CASES (recode_case). A field of the second family with recursive
    0 that uses the index, or a path field, gives HIT_CASES. The result
    type is MA, or MB applied to the evaluated fold at the index.
  - src/json.c:  the switch handles OP_FOLD_BLOCK.
- Tests (guest language):
  - examples/ctx.lang:  length, lengthC1, lengthC2, lengthExt, tyRefl,
    tySize, tySizeT1, tySizeC1, lengthVia;  9 rows in test/eval/expect.txt.
    In each other example MB rg is Nat, the same type as MA. lengthVia
    has MB = `fun n => Path Nat n n` and its ext case uses `rt @ i0`, thus
    a case uses a partner field at the type MB rg (mutant m2).
  - test/check/fold-block-count.lang (the arrow case is missing, 6
    arguments) and test/check/fold-block-index.lang (the tbase case is
    `0`):  2 rows in test/check/expect.txt, both HIT_CASES.
  - test/json/ctx.json:  the new golden adds 3 instances (tySizeT1 0,
    tySizeC1 2, lengthVia 2). The s4 entries do not change (the first 392
    bytes are equal).
  - test/hit-regressions.c:  1 row `a block fold refuses a second family
    field that uses the index` (the prelude block XCtx and XTy, the field
    `xp : Path XCtx xi xi`). The fold gives HIT_CASES at block_case_type.
- Probes (no rows):  a wrong value gives TYPE_REFL. A wrong MB result
  gives TYPE_MISMATCH. Two folds with different cases do not convert
  (TYPE_REFL;  no proof irrelevance). MB = Nat gives TYPE_MISMATCH
  `Nat -> Type 0`. A fold with MA = Type 0 checks. The arrow law over
  variables (tySize of `arrow g t t`) checks by refl. The binder names
  `_x0 _x1` in the message for the ext case come from the printer
  (eval.c print_binder), not from a defect. Index field probes:  3
  preludes (`xp : Path XCtx xi xi`, `xp : (fun (u : XCtx) => Nat) xi`,
  `xp : Path XCtx xi xz`). The domain checker accepts each prelude, and
  the fold gives HIT_CASES with the D7 message for each.
- Mutants (~/Documents/glue-lang-M5-s5-mutants/run.sh), 4 of 4 KILLED:
  - m1 eval.c fold_block (a case of the second family gets the index,
    not its fold):  KILLED (5 gate failures, for example `ctx tyRefl`
    and `ctx tySizeT1`).
  - m2 check.c block_case_type (a partner field gets the type MA):
    KILLED (examples/ctx.lang:  PATH_EXPECTED at lengthVia).
  - m3 check.c fold_block_rule (the constructors of Ty first):  KILLED
    (19 gate failures;  HIT_CASES at length).
  - m4 check.c block_case_type (no index test):  KILLED (the native HIT
    row above gives ok, not HIT_CASES).
  - The runner restores eval.c and check.c (cmp). The final
    `make -B check` exits 0.
- Gates:  `make -B check` exit 0, `make check-clang` exit 0,
  `forge build` exit 0, `make e2e` exit 0 (33 passed), IR diff exit 0
  (test/circuit and contracts/src/verifiers do not change), `forge test`
  33 passed after the restore of contracts/test/fixtures. Counts against
  the s4 baseline (5a223ef):  native circuit 9, path 42, face 35, HIT 40
  (39 + 1);  examples 31, parse refusals 18, check refusals 123 (121 + 2),
  evals 202 (193 + 9), json 26 builds, build refusals 8, circuits 4,
  same-ir 2, circuit refusals 11, satcheck refusals 6;  node regressions
  6, printer 2.
- s4 (a) does not change.
- Open items:
  - The index field refusal now has a native row (closed in s5).
  - The path field branch of block_case_type is defensive:  check_block
    refuses a path constructor in a block (MUTUAL_BLOCK,
    test/check/mutual-path.lang), so no program gets to this branch.

- Staged review (2026-10-16):  one MEDIUM finding fixed. The inferred MB
  branch discarded the universe of its body, accepting mismatched motive
  universes that the annotated branch refused. All MB forms now check
  against `MA -> Type level(MA)`, enforcing D7.
  - Native C regressions:  the two mismatched inferred motives failed the
    refusal assertions before the fix (44 checked, 2 failed). Both now
    give TYPE_MISMATCH. Two matching higher-universe folds, with inferred
    and annotated motives, still normalize to Nat (44 checked, 0 failed).
  - `make check` passes, including check-clang and all guest regressions.
    The remaining counts above are unchanged. `make e2e` passes with the
    unchanged, current halo2 binary:  33 Forge tests and 4 deployment
    simulations. Generated circuit IR and verifier source are unchanged.

### s6: the Direction 2 smoke (2026-10-16)

- Start state:  HEAD 017d6ad (USER commit `M5 s5: add folds for mutual
  family blocks`). Baseline fix:  at 017d6ad the native HIT count is 44,
  not 40. The USER s5 commit adds 4 rows to test/hit-regressions.c (the
  staged review follow-up).
- Ruling (a):  s6 has no code. The slice adds test data only.
- Files:
  - examples/transportProp.lang:  the iso lemmas of examples/iso.lang
    (lines 4 to 21);  rot and rotInv on `Prod A (Prod A A)`;  rotSect,
    rotRetr, rotE;  natSame;  P (a fold out of `GQuot Nat natSame` into
    `Type 0`, with the geqc case `ua (rotE Field)`);  Pline;
    transportProp;  transportPropFwd (the row `= rot Field p`, by refl);
    tpOne. rot is not an involution, thus rot and rotInv are different
    and the uaβ row tests the direction of the map (mutant m1). The
    representative rows:  repFold (Quot, `fun s => 5`), repFoldG and
    repElimG (GQuot, 5), repIntPos (6), repIntNeg (3), repElimInt (6).
    7 rows in test/eval/expect.txt.
  - test/json/transportProp.json:  the golden from `langc build`.
  - examples/intSq.glue:  `isq x z = fold Field (fun n => x * x)
    (fun n => x * x) refl z`, and the circuit square :=
    `isq x (pos 0) == y`. test/circuit/same-ir.txt row
    `intSq square square` (the IR is equal to the square golden).
    test/circuit/refuse.txt row `1 WITNESS_FAIL witness intSq square
    y=10 x=3`.
  - test/circuit/expect.txt does not change:  the Makefile E2E_TABLE
    reads that file, thus a row there changes e2e.
- Deviation from the D8 text:  D8 writes the line in the transport
  argument, `transport (<i> P (geqc A R a b r @ i)) p`. Probe 2 refuses
  this form. The def Pline :
  `Path (Type 0) (P (gcls Nat natSame a)) (P (gcls Nat natSame b))`
  holds `<i> P (geqc Nat natSame a b r @ i)`, and transportProp is
  `transport (Pline a b r) p`. tpOne is `pair 2 (pair 3 1)`, that is
  rot (pair 1 (pair 2 3)). rotInv gives `pair 3 (pair 1 2)`.
- Probes (no rows):
  1. Section 6:  `Path Field (x + (y + s)) (y + (x + s))` by refl gives
     TYPE_REFL (the start is fieldAdd x (fieldAdd y s), the end is
     fieldAdd y (fieldAdd x s)). A Bag Field product fold gives HIT_CASE
     at the bswap case (fieldMul, for the same reason). Thus the circuit
     uses Int, not Bag.
  2. `transport (<i> P (geqc ...)) p` gives PATH_EXPECTED:  the term
     `<i> ...` needs a declared type, and glue-lang has no type
     ascription.
  3. A fold at Nat in a case (`fold x (fun m => fun r => r * x) n` and
     `fold x (fun r => r * x) n`) gives HIT_CASES (`a function is not a
     term of type Field`). Thus the pos case and the neg case of isq are
     equal.
  4. CIRCUIT_LOWER (O4):  no stuck `elim` gets to the lowering. A binder
     `z : Int` gives CIRCUIT_BINDER (M1 accepts Field binders only). An
     elim at `zeq @ k` with k := i0, at
     `hcomp Int i0 (fun i => [ i0 -> pos 0 ]) (pos 0)`, and at
     `hcomp Int k (fun i => [ k -> zeq @ i ]) (pos 0)` with k := i0 each
     reduce, and the IR is equal to square. CIRCUIT_LOWER occurs only at
     src/circuit.c:144 (no_circuit_form) and src/circuit.c:152 (depth).
- Mutants (test data only, ~/Documents/glue-lang-M5-s6-mutants/run.sh):
  3 of 3 KILLED.
  - m1 (the uaβ row with the inverse map, `rotInv Field p`):  KILLED.
    FAIL examples/transportProp.lang (TYPE_REFL at transportPropFwd),
    9 gate failures.
  - m2 (the same-ir golden set to range):  KILLED. FAIL same-ir intSq
    square, 1 gate failure.
  - m3 (the point cases of repIntPos swapped):  KILLED. FAIL eval
    transportProp repIntPos (3, not 6) and FAIL build
    examples/transportProp.lang, 2 gate failures.
  - The runner restores examples/transportProp.lang and
    test/circuit/same-ir.txt (cmp to the backups).
- Gates (build/s6-gates.sh):  `make -B check` exit 0, `make check-clang`
  exit 0, `forge build` exit 0, `make e2e` exit 0 (33 passed). IR check:
  s6 changes test/circuit/refuse.txt and test/circuit/same-ir.txt, thus
  the check compares the diff before e2e with the diff after e2e. The
  two diffs are equal, and contracts/src/verifiers does not change.
  `forge test` 33 passed after the restore of contracts/test/fixtures.
  Counts against the s5 baseline (017d6ad):  native circuit 9, path 42,
  face 35, HIT 44;  examples 33 (31 + 2), parse refusals 18, check
  refusals 123, evals 209 (202 + 7), json 27 builds (26 + 1), build
  refusals 8, circuits 4, same-ir 3 (2 + 1), circuit refusals 12
  (11 + 1), satcheck refusals 6;  node regressions 6, printer 2.
- Open items:
  - O4 stays open for M7:  no probe gets a stuck `elim` to the lowering,
    thus there is no CIRCUIT_LOWER row.
  - Optional:  give isq different pos and neg cases at `pos 1` (a Nat
    fold form of examples/functions.lang or examples/formers.lang), with
    the same IR.

### s7: close (2026-10-16)

The main loop built s7 by hand (sessions s7-a and s7-b). The builder agent
stopped on a rate limit and wrote no file. s7 adds no op, no code and no
test row.

- Docs (the s7 change): SPEC.md 2.6 (the `elim` sentence points to 2.7),
  2.7 (new: the constructor equations; `elim` and its rules, with the USER
  empty-face ruling (ii); the Kan rules at dependent and Path fields; the
  mutual block rules (1) to (6) and the universe rule; the block fold and
  the s5 choice OP_FOLD_BLOCK; neutral values; conversion; the printer and
  JSON; Direction 2; circuits; the codes ELIM_CASES, ELIM_CASE and
  MUTUAL_BLOCK, and the codes with new causes), 9 (O4 and O12 (f): the s6
  probe; O12 (a) and (b) go to the rules; (c) narrowed; (d) and (e)
  closed; O13 new). docs/PLAN.md: M5 DONE 2026-10-16 with the slice
  commits, and the sentence that the full quotient inductive-inductive
  form is SPEC.md 9 O13. formers/FORMERS.md: no change. The matrix has one
  row for each of the formers F1 to F15. A QIT, `elim` and the `mutual`
  block are not formers in it, and no glue-lang note names them. F6 and F7
  (fold and unfold) are the M6 scope. docs/STATUS.md: the M5 summary and
  this section.

#### Mutant roll-up

Each mutant ran with `make -B` and the gate, source restored after: 22
KILLED, 0 SURVIVED (the 18 planned, 3 in each of s1 to s6, and 4 extra
rows: s1 4, s3 m0 and m4, s5 m4). The slice sections give the mutant forms
and the failure counts.

| Slice | Mutant | Result | Killing row |
|---|---|---|---|
| s1 | 1. eval_fold_hit_boundary keeps the inner constructor unfolded (eval.c) | KILLED | examples/bag.lang (bsize: INTERNAL) |
| s1 | 2. transp_pcon_agrees always true (kan.c) | KILLED | native HIT `transport respects a path constructor application boundary` (no guest row, s1 open item (a)) |
| s1 | 3. hit_case_end takes the `~i` end twice (check.c) | KILLED | examples/int.lang (negate: HIT_CASE) |
| s1 | 4 (extra). transp_pcon_agrees always false (kan.c) | KILLED | eval circle meridT, bag transpSwap (stuck) |
| s2 | 1. the IH of a recursive field is the field (eval.c eval_hit_case) | KILLED | examples/circle.lang (stackSumFive: INTERNAL) |
| s2 | 2. the path rule drops `@ r` (eval.c eval_hit_case) | KILLED | examples/circle.lang (elimLoop: TYPE_REFL) |
| s2 | 3. kan_elim_hcomp: the line is `<i> P u0` (kan.c) | KILLED | `make check-clang` (unused `fill`); eval circle elimHcomp |
| s3 | m0 (extra). kan_elim_hcomp: the empty-face branch off (kan.c) | KILLED | examples/circle.lang (elimEmptyFold: TYPE_REFL) |
| s3 | m1. field_fill: the face `phi \/ ~i` -> `phi` (kan.c) | KILLED | eval quot quotEqcLine, circle dpairTransp, circle webTransp |
| s3 | m2. field_fill: the Path field keeps its old ends (kan.c) | KILLED | eval circle webTransp |
| s3 | m3. field_line_transp: NULL at a dependent field (kan.c) | KILLED | eval quot quotEqcTransp, quotEqcLine, circle dpairTransp, webTransp |
| s3 | m4 (extra). hcomp_ctor: NULL at a dependent field (kan.c) | KILLED | eval circle dpairHcomp |
| s4 | m1. the index rule (1) off (check.c) | KILLED | check mutual-index.lang (REFUSE_DATA, not MUTUAL_BLOCK) |
| s4 | m2. B is not registered while the constructors of A are checked (check.c) | KILLED | domain REFUSE_REC at `Ty eg` (405 gate failures) |
| s4 | m3. the partner field is `recursive` 0 (check.c) | KILLED | domain DOMAIN_FAMILY (407 gate failures) |
| s5 | m1. a case of B gets the index, not its fold (eval.c fold_block) | KILLED | eval ctx tyRefl, tySizeT1 (5 gate failures) |
| s5 | m2. a partner field gets the type MA (check.c block_case_type) | KILLED | examples/ctx.lang (lengthVia: PATH_EXPECTED) |
| s5 | m3. the constructors of Ty first (check.c fold_block_rule) | KILLED | examples/ctx.lang (length: HIT_CASES; 19 gate failures) |
| s5 | m4 (extra). no index test (check.c block_case_type) | KILLED | native HIT `a block fold refuses a second family field that uses the index` |
| s6 | m1. the uaβ row with the inverse map (test data) | KILLED | examples/transportProp.lang (transportPropFwd: TYPE_REFL) |
| s6 | m2. the same-ir golden set to range (test data) | KILLED | same-ir intSq square |
| s6 | m3. the point cases of repIntPos swapped (test data) | KILLED | eval transportProp repIntPos (3, not 6) |

#### Review

Review of the M5 diff (`git diff 5e76e43..HEAD -- src`, 1360 lines with
`-U2`). A read-only review agent ran in s7-a. It wrote its result to a
file and edited no file. Result: no defect found, no code change.

1. No conv rule reads a type. The conv functions (eval.c conv_fun,
   conv_binder, same_head, conv_pair, conv_path, conv_sys, conv_glue,
   conv_loop and conv_values) have no M5 hunk. The eval.c hunks are in
   varied_arity and reduce_op. OP_ELIM and OP_FOLD_BLOCK have no special
   conv or quote case: they take the generic VAL_OP and VAL_STUCK arm
   (same_head on the op, the instance, the field and the arity, then
   argument by argument). The one added line that names conv is in
   check.c rule_elim: a typing check of the motive domain against the
   scrutinee type, not a conv rule. conv_loop reads no type, family or
   constructor table.
2. There is no rule `hcomp A i0 u u0 = u0` (D8). kan_hcomp has the face
   `i1` rule, the closed-type rule (hcomp_constant), then the head rules,
   and no `i0` rule. hcomp_ctor always builds OP_CTOR with field hcomps or
   fills. On the empty face, kan_elim_hcomp returns `hcomp (P h) i0 u
   (transp line i0 base)`, not `u0`. Else it uses the gcomp.
3. `hcomp` at a HIT stays canonical (D3). hcomp_ctor has one caller,
   hcomp_data. hcomp_data has one caller, kan_hcomp, under `data_type a`
   or plain_family (path_ctors == 0). closed_type has no OP_FAMILY.
   hcomp_fill has one caller, hcomp_ctor, and field_fill has one caller,
   transp_ctor. check_block refuses a path constructor in a block, thus a
   block family is plain. eval.c calls kan_elim_hcomp only when
   path_ctors > 0 (else the `elim` is stuck).
4. Switches, globals and diagnostics. The diff adds no `default:` line. It
   adds no static local and no file-scope global: each column-0 line is a
   function or a prototype, a table entry (the builtin `elim`, a lexer
   keyword) or a struct field (FamilyInfo.partner, Checker.partner and
   Checker.partner_uses). Each new diagnostic path returns NULL or 0
   (`nul(FAIL(..))`, `FAIL(..); return NULL;`, or `return FAIL(..)` in the
   int functions check_field and check_block).
5. IR: `git diff 5e76e43..HEAD --stat -- test/circuit
   contracts/src/verifiers` gives 2 added rows (refuse.txt and
   same-ir.txt, each for intSq, s6) and no verifier change.

Scope: the review covered the checklist of the s7 plan (items 1 to 5). It
is not a line-by-line audit of each rule. The 22 KILLED mutants and the
guest rows cover the rules. The two review notes are open items (below).

#### Gates

Gates in s7-b on HEAD fb93222 with the s7 doc changes (build/s7-gates.sh).
s7 changes no code or test file. The M4 counts are from the M4 close on
5e76e43. `make -B check` exit 0 (native circuit regressions 9 (M4: 9),
native path regressions 42 (M4: 42), native face regressions 35 (M4: 35),
native HIT regressions 44 (M4: 27), examples 33 (M4: 28), parse refusals
18 (M4: 17), check refusals 123 (M4: 100), evals 209 (M4: 162), json 27
builds (M4: 23), build refusals 8 (M4: 7), circuits 4 (M4: 4), same-ir 3
(M4: 2), circuit refusals 12 (M4: 11), satcheck refusals 6 (M4: 6), each
with 0 failed; gate 0 failures; node regressions 6 (M4: 6); printer 2
(M4: 2)). `make check-clang` exit 0. `forge build --root contracts
--sizes` exit 0. `make e2e` exit 0 (33 Forge tests passed (M4: 33)). IR
check: `git diff --exit-code -- test/circuit contracts/src/verifiers`
exit 0. contracts/test/fixtures is restored from HEAD, and `forge test
--root contracts` passes again (33 passed, 0 failed).

#### Open items

- O4 (M1, SPEC.md 9 O12 (f)): no probe gets a stuck `elim` to the
  lowering, thus there is no CIRCUIT_LOWER row (s6). Milestone: M7.
- O8 (M3, SPEC.md 9): no change in M5. Milestone: M6 (the fold and
  unfold families).
- O12 (c) (M4, SPEC.md 9): open, narrowed. `transp` of `squashc` of Quot
  stays stuck, because it needs the boundary correction (s3).
- O13 (M5, SPEC.md 9): new. The full quotient inductive-inductive form
  (equations in a `mutual` block, non-uniform indices, `elim` at a block
  family) is not in M5.
- s1 (a): mutant 2 (transp_pcon_agrees always true) has no guest row. A
  native HIT row kills it.
- s2 (a): PRE-EXISTING printer quirk. A path constructor as the last
  argument of a chain op prints `loop i`, not `loop @ i`. This needs a
  USER ruling for a later slice.
- s2 (b): the ELIM_CASES messages print Pi binders as `_x0`.
- s2 (c): no closed instance holds a stuck `elim`.
- s3 (b): the fills at r = i1 keep an `i0 -> ..` branch (no conv rule
  removes it).
- s5: the path-field branch of block_case_type is defensive.
- s6 (optional): isq with different pos and neg cases.
- s7 review note (a): hcomp_constant is true on an empty system. With a
  closed-type motive (for example `P = fun _ => Nat`), the empty-face
  `elim` reduces to `transp line i0 base`, that is `elim P cases u0`.
  This is the allowed closed-type rule. The M4 kan_fold_hcomp does the
  same.
- s7 review note (b): kan_elim_hcomp gives the empty-face hcomp the type
  `P h`, but its base has the type `P (hfill .. 1)`. This needs `hfill ..
  1` to convert to `h` (USER ruling (ii) of s2 open item (d)). On the
  face `i0` the two systems can differ in form (s3 open item (b)). The
  review built no input that shows a failure. A candidate M6 probe.
- Closed in M5: O12 (a) and (b) (rules, D10), (d) (s3) and (e) (s2); s1
  (b) (SPEC.md 2.7 uses the natAdd form); s2 (d) (s3 step 0, USER ruling
  (ii)); s4 (a) (the s4 review follow-up); the s5 index field refusal (a
  native row).

## M6

M6 adds the M6 formers and laws of docs/M6-PLAN.md (USER commit ac8625c,
with the USER rulings of its section 9). The slice table of section 8 gives
the rows and the mutants of each slice. M6 must not change the IR of a
current circuit.

### s1: append and elim at the core carriers (2026-10-17)

- Plan: s1 covers D1 to D4 and D15. The USER ruling diff of
  docs/M6-PLAN.md is in this commit (D16).
- Probes (step 2, before the code;  scratch build/m6s1-probe/*.lang,
  `langc check`):  leftIdL, rightIdO/S/L, assocO/S/L and mapO/S/L, each a
  `refl` proof at a variable m, gave TYPE_REFL (as the kickoff table of
  section 1).  After D2, leftIdL gives `ok`.  The other 9 probes stay
  TYPE_REFL at `refl`:  they need the `elim` proofs of D4.
- D1:  a new op OP_APPEND (core.h after OP_BIND) and the core name
  `append` (check.c RULE_APPEND, rule_append;  eval.c append_values,
  reduce_append;  json.c op_json).  A nil end of xs gives the cells of xs
  over ys.  A neutral end gives xs when ys is nil (the right unit), else
  the cells over `append END ys`.  A trap end stays.
- D2:  bind_list goes through append from the last cell.  acc starts at
  nil, at the trap, or at `bind END f` for a neutral end;  then acc =
  `append (f x) acc` for each cell.  So `bind (cons a nil) f` is
  `append (f a) nil`, which is `f a`.  The loop is iterative.
- D3 tag:  a new op OP_ELIM_CORE (core.h after OP_ELIM;  inst = the
  Carrier;  args P, c1, c2, x;  arity 4), not a Carrier in the OP_ELIM
  inst.  Reason:  the OP_ELIM inst indexes m->families (eval.c fold_hit,
  the hcomp path, kan.c), so a Carrier there needs a guard at each site.
  The new op is the smaller and safer diff.
- D3 rules:  eval.c elim_core and elim_list.  Option:  none gives c1,
  `some a` gives `c2 a`.  Sum:  `inl e` gives `c1 e`, `inr a` gives
  `c2 a`.  List:  from the last cell, `cons a l` gives `c2 a l acc`;  acc
  starts at c1 for nil, at the stuck elim for a neutral end and at the
  trap for a trap end.  check.c:  elim_motive (cut out of rule_elim,
  shared by the two paths), carrier_case_type (section 3.2), elim_carrier.
  rule_elim calls carrier_of on the scrutinee type before the family
  test.  A scrutinee of another type gives TYPE_MISMATCH with a new text:
  `elim needs a last argument in a domain family, Option, Sum or List;
  found a last argument of type T`.
- Deviation:  at a carrier a wrong case type gives ELIM_CASE (plan 3.2 and
  the section 8 row).  At a family a point case keeps ELIM_CASES (M5
  recode_case;  the M5 rows do not change).
- The `_x0` fix (M5 s2 open item (b)):  the ELIM_CASES, ELIM_CASE,
  HIT_CASES and HIT_CASE messages print the case type with the field names
  of the constructor (eval.c value_print_case and the Printer field
  fields_free;  check.c show_case).  test/check/elim-no-ih.lang now prints
  `(top : Nat) -> (rest : Stack) -> (ih : Nat) -> Nat`.  value_print does
  not change, so no eval golden changed.
- The `_x0` print row:  no table of test/gate.sh compares message text.
  The gate had one message check (bad-refl.lang).  s1 adds a second check
  of the same form:  the elim-no-ih.lang message must contain the binder
  names.
- D4:  examples/monad-laws.lang (new).  leftIdO/S/L by refl.  rightIdO/S/L,
  assocO/S/L and mapBindO/S/L by `elim` on m;  at List also with `cong`,
  `sym`, `trans`, appendAssoc and bindAppend (each by `elim` at List).
  The same file has the defs of the eval rows.
- D15:  `def append` gives REFUSE_NAME (rule R4) with no new code.
- Examples (section 5):  monad.lang, laws.lang and algebra.lang check as
  before.  No printed form changed.
- JSON:  OP_APPEND and OP_ELIM_CORE are in the not-a-type-former group of
  op_json.  Each reduces at closed arguments, so a closed instance cannot
  hold one, and the group has no closed witness (as M5 s2 open item (c)).
- Rows:  evals +15 (monad-laws:  append at nil, at cons, at a neutral xs
  with nil, at a neutral pair, at a neutral xs with a cons;  bind at a
  neutral tail;  elim at Option (some, none, neutral), Sum (inl, inr,
  neutral) and List (nil, 3 cells, a cons over a neutral tail)).  Check
  refusals +9:  append-def REFUSE_NAME, append-mix TYPE_MISMATCH,
  elim-option-count, elim-sum-count and elim-list-count ELIM_CASES,
  elim-option-case, elim-sum-case and elim-list-case ELIM_CASE,
  elim-field TYPE_MISMATCH.  Examples +1, json +1
  (test/json/monad-laws.json), the elim-no-ih message check.
- Mutants (each with `make -B build/langc` and `make check`, reverted, the
  hash of eval.c compared, final `make -B check` GREEN):  3 KILLED,
  0 SURVIVED.

| Mutant | First failing rows |
|---|---|
| 1. eval.c append_values:  no right-unit rule (no `if (val_is(ys, OP_NIL)) return xs;`) | examples/monad-laws.lang `TYPE_REFL: leftIdL: ... the start normalizes to append (f a) nil, the end normalizes to f a`. |
| 2. eval.c bind_list:  a neutral end gives `bind xs f` for the whole term (cons bind without append) | examples/monad-laws.lang `ELIM_CASE: rightIdL: the case for cons must have the type ...`. |
| 3. eval.c elim_list:  the IH before l (`apply_value(m, apply_value(m, apply_value(m, a[2], cell->args[0]), acc), cell->args[1])`) | eval monad-laws lenThree `INTERNAL: lenThree: the evaluator found a Nat operation on a value that is not a Nat`. |

- Gates:  `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  44;  examples 34, parse refusals 18, check refusals 132, evals 224, json
  28 builds, build refusals 8, circuits 4, same-ir 3, circuit refusals 12,
  satcheck refusals 6;  node regressions 6, printer 2).  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed).  `git diff --exit-code -- test/circuit
  contracts/src/verifiers` exit 0:  no IR change.  contracts/test/fixtures
  restored from HEAD, then `forge test --root contracts` 33 passed,
  0 failed.
- Build record:  hand build in the main loop over s1-a to s1-d (no
  subagents;  the s1 builders died on 10-17:  Fable 429, then the opus
  fallback on the reasoning_extraction classifier).

#### s1 review correction (2026-10-17)

- MEDIUM: elim_list returned a trapped tail before applying its known cons
  cases. A head case that ignores its tail and IH normalized to 7 at a
  neutral input, but returned trap after substituting UINT64_MAX into an
  overflowing tail. The corresponding fold returned 7. Removing the early
  return lets blocked supply the trapped IH to the cons case, as for fold.
- Regression: elimTrapHead UINT64_MAX returned trap before the fix and 7
  after it. elimTrapHeadLaw checks by refl. elimTrapStrict and elimTrapBare
  still return trap. Added three eval rows; regenerated monad-laws.json is
  byte-identical.
- Validation on the corrected staged snapshot: make -B check passed (227
  evals, 28 JSON builds, all native and printer checks); dependent motives
  at polymorphic carriers and heterogeneous Sum payloads passed. End-to-end
  checks reused the unchanged halo2 binary and passed all 33 verifier tests.
  Generated circuit IR and verifier sources matched the original files.
  Forge build with sizes passed; all 33 tests passed with original fixtures.

### s2: the Nat successor view and elim at Nat (2026-10-17)

- Commits:  the USER commit df0d1e2 has the code (the s2-a hand build of
  core.h, json.c, eval.c and check.c, with the review correction below),
  examples/nat-elim.lang and 7 check refusals.  This follow-up adds
  examples/fold-laws.lang, 10 eval rows and this record.
- Probes before D5 (the s1 tree;  scratch build/m6s2-probe/*.lang,
  `langc check`):  (a) `fold plus1 0 (natAdd n 3)` checks.  (b) `refl` at
  `natAdd (natAdd n 1) 1 = natAdd n 2`, `natAdd n 0 = n`, `natAdd 0 n = n`
  and `natAdd 2 n = natAdd n 2` each gave TYPE_REFL (the start normalizes
  to the unchanged natAdd form).  (c) foldZero by `refl` checks;  foldSucc
  gave TYPE_REFL (`fold f z (natAdd n 1)` against `f (fold f z n)`).
  (d) foldCons at a variable l checks:  reduce_fold_list keeps the cells
  over a stuck fold, so it needs no new rule.  (e) `elim` at Nat gave
  TYPE_MISMATCH with the s1 text.  After D5 and D6, (a) to (e) check.
- Tag (a):  a new op OP_ELIM_NAT (core.h before OP_NAT_ADD;  args P, z, s,
  n;  arity 4;  name `elim`;  json.c case beside OP_ELIM_CORE;  eval.c
  reduce_op case).  No CARRIER_NAT:  Nat is not a monad carrier.
- D5 (eval.c):  nat_spine reads a stuck `natAdd x k` with a literal right
  operand as a spine.  nat_offset normalizes natAdd with a neutral
  operand:  it removes a zero offset, moves the literal to the right and
  adds nested literal offsets;  an overflow of j + k gives the trap.
  nat_add calls nat_offset when the result is stuck.  reduce_fold_nat at a
  spine `natAdd x k` applies f k times over the stuck `fold f z x`.
- D6 (eval.c elim_nat):  a literal k gives `s (k-1) (... (s 0 z))`;  a
  spine `natAdd x k` gives `s (natAdd x (k-1)) (... (s x (elim P z s
  x)))`;  a neutral n stays stuck;  a trap gives the trap.  check.c:
  nat_case_type (`z : P 0`, `s : (n : Nat) -> P n -> P (natAdd n 1)`,
  binders n and ih) and elim_at_nat (ELIM_CASES at a wrong count,
  ELIM_CASE at a wrong case;  the case names are `zero` and `succ`).
  rule_elim tests `val_is(st, OP_NAT)` before carrier_of.  The
  TYPE_MISMATCH text now ends `Option, Sum, List or Nat` (only
  test/check/elim-field.lang shows it;  no table compares text).
- Review correction (USER commit):  overflow is not equality evidence, so
  conversion does not equate two traps.  Without this rule, two successor
  laws proved by `refl` at a neutral n and applied at UINT64_MAX compose
  into a closed `Path Nat 0 1` (fold-overflow-proof.lang,
  elim-nat-overflow-proof.lang:  each gives TYPE_MISMATCH now).
  overflow-refl.lang (`refl` at `natAdd UINT64_MAX 1`) gives TYPE_REFL.
- D5 printed-form change:  one golden.  `bag sizeOpen` was `natAdd (natAdd
  (fold ... (<i> natAdd (natAdd _x2 1) 1) xs) 1) 1`;  it is now `natAdd
  (fold ... (<i> natAdd _x2 2) xs) 2`.  Reason:  nat_offset adds the
  nested literal offsets.  This is the D5 canonical form, thus the USER
  commit updates the row.  No other table changed.
- D7:  examples/nat-elim.lang (USER commit):  count and indices by `elim`;
  countSucc, countOffset and indicesSucc by `refl` at a spine;  foldId (the
  plan countUp) by `elim` with a dependent motive;  types (a `Type 0`
  motive);  the offset rows addZero, addLeft, zeroAdd and addOffsets (the
  plan offZeroR, offLeft, offZeroL and offNest);  the overflow rows.
  examples/fold-laws.lang (this follow-up):  foldZero, foldSucc, foldCons
  and foldPush (at Stack) by `refl`;  the defs of the eval rows;  countUp2
  and countUp2L (`elim` at the spines `natAdd n 2` and `natAdd 2 n`);
  foldNatUniq, foldListUniq and foldFamUniq (at Stack):  a function that
  satisfies the fold equations is the fold (funExt over `elim`, with
  trans and cong).  fold-laws.lang does not repeat the offset rows and
  countUp:  nat-elim.lang has them.
- Examples (section 5):  monad-laws.lang, monad.lang, laws.lang and
  algebra.lang check as before.
- Huge literal (the s2-a open item):  `dbl 18446744073709551615` and
  `fold plus1 0 18446744073709551615` each stop with `OOM: ...: the arena
  limit is reached` (the 1 GiB arena of main.c) in 1 to 3 s.  The two
  give the same result, thus this is the current behavior of a huge
  literal (the s1 reduce_fold_nat loop), not an s2 change.
- JSON:  test/json/nat-elim.json (USER commit) and test/json/fold-laws.json
  (the closed instance elimLit = 6).
- Rows:  USER commit:  evals +10 (nat-elim), check refusals +7
  (elim-nat-count ELIM_CASES;  elim-nat-zero, elim-nat-step and
  elim-nat-index ELIM_CASE;  fold-overflow-proof and
  elim-nat-overflow-proof TYPE_MISMATCH;  overflow-refl TYPE_REFL),
  examples +1, json +1.  This follow-up:  evals +10 (fold-laws foldSpine,
  elimLit, elimSpine, elimNeutral, idxSpine, offL, offZ, `dbl 5`, `idx 4`,
  `idx 0`), examples +1, json +1.  The plan refusals elim-nat-case and
  elim-nat-count are the USER rows elim-nat-step and elim-nat-count, so
  this follow-up adds no refusal file.
- Mutants (each with `make -B build/langc` and `make check`, reverted, the
  hash of eval.c compared, final `make -B check` GREEN):  4 KILLED,
  0 SURVIVED.

| Mutant | First failing rows |
|---|---|
| 1. eval.c reduce_fold_nat:  the spine at k, not k - 1 (`end[2] = a[2]`) | examples/fold-laws.lang `TYPE_REFL: foldSucc: ... the start normalizes to f (fold f z (natAdd n 1)), the end normalizes to f (fold f z n)`;  examples/nat-elim.lang `ELIM_CASE: foldId`. |
| 2. eval.c nat_spine:  the literal on the left only | examples/fold-laws.lang `TYPE_REFL: foldSucc: ... the start normalizes to fold f z (natAdd n 1), the end normalizes to f (fold f z n)`;  examples/nat-elim.lang `TYPE_REFL: countSucc`. |
| 3. eval.c elim_nat:  the IH dropped (`a[1]` for `acc`) | examples/nat-elim.lang `TYPE_REFL: countSucc: ... the start normalizes to 1, the end normalizes to natAdd (elim ... n) 1`. |
| 4. eval.c nat_add:  no offset normalization (`return out;`) | examples/nat-elim.lang `TYPE_REFL: countOffset: ... the start normalizes to elim ... (natAdd 2 n)`;  eval bag sizeOpen (the old nested form). |

- Gates:  `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  44;  examples 36, parse refusals 18, check refusals 139, evals 247, json
  30 builds, build refusals 8, circuits 4, same-ir 3, circuit refusals 12,
  satcheck refusals 6;  node regressions 6, printer 2).  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed).  `git diff --exit-code -- test/circuit
  contracts/src/verifiers` exit 0:  no IR change.  contracts/test/fixtures
  restored from HEAD, then `forge test --root contracts` 33 passed,
  0 failed.
- Open items:  (1) elim_at_nat repeats the case loop of elim_carrier (the
  Carrier switch has no Nat case);  a shared helper is a later cleanup.
  (2) The huge-literal OOM above is a resource limit, not a wrong value.
- Build record:  s2-a hand build in the main loop (claude3);  the USER
  commit df0d1e2;  this follow-up by hand in the main loop (no subagents).

### s3: the unfold step through option and unfold into Nat (2026-10-17)

- Commits:  none yet.  One USER commit has the code (s3-a:  core.h,
  eval.c, check.c and json.c), examples/unfold-laws.lang,
  test/json/unfold-laws.json, 8 eval rows, 1 check refusal and this
  record (s3-b).
- Step 0 (USER question):  build/m6s3-probe/s0.lang (f0 and p0 of
  test/check/fold-overflow-proof.lang and `def q : Path Nat (f0
  18446744073709551615) 0 := p0 18446744073709551615`) gives
  `TYPE_MISMATCH: q: expected a term of type Path Nat trap 0, found a
  term of type Path Nat trap 0`, before and after the code.  Thus q is
  refused:  conversion does not equate two traps (the s2 rule).  No code
  change.  See open item (1).
- Tag:  a new op OP_UNFOLD_NAT (core.h after OP_ELIM_NAT;  args g, n, s;
  arity 3;  name `unfold`), not an inst on OP_UNFOLD, as OP_ELIM_CORE
  (s1) and OP_ELIM_NAT (s2).  Reason:  the conversion head compares the
  op, the inst and the fields (`a->op == b->op && a->inst == b->inst
  ...`), thus a separate op keeps the two forms apart, and
  reduce_unfold, blocked and unfold_end read no new inst.  -Wswitch-enum
  lists each switch site:  only eval.c reduce_op and json.c need a case.
- D8 (eval.c):  unfold_step makes the stuck `option nil F (g s)` (`option
  0 F (g s)` in the Nat form).  F is a VAL_LAM closure over the env
  [limit, g]:  `fun p => cons (first p) (unfold g n' (second p))` at List
  and `fun r => natAdd (unfold g n' r) 1` at Nat (core_index and
  core_call make the Core nodes of the body).  unfold_end:  none gives
  nil (0 at Nat);  a trap gives the trap;  a neutral step result gives
  unfold_step;  `some p` with a neutral p continues the loop (the item
  and the seed by reduce_part OP_FIRST and OP_SECOND;  the Nat seed is
  p).  unfold_result:  build_list at List;  `natAdd TAIL STEPS` by
  nat_offset at Nat (a 0 tail gives the literal STEPS, a trap tail gives
  the trap).  reduce_unfold:  at a limit `natAdd x k` (nat_spine), k
  steps, then the stuck `unfold g x seed`;  a neutral limit stays stuck;
  a trap limit gives the trap.  reduce_op:  `case OP_UNFOLD: case
  OP_UNFOLD_NAT:` call reduce_unfold.
- check.c rule_unfold:  at a declared type Nat the step type is `Option
  S`;  at `List A` it is `Option (Prod A S)`;  else TYPE_INFER with the
  text `unfold needs a declared type List A or Nat here` (it was `List
  A`;  no table compares the text).
- Probes (build/m6s3-probe/*.lang, `langc check`), before -> after:  l1
  unfoldSomeL (the D8 List law at a neutral `g s`) TYPE_REFL -> ok;  l2
  unfoldZeroL ok -> ok;  l3 unfoldNoneL TYPE_REFL -> ok;  l4 unfoldSomeLc
  (a pair step) TYPE_REFL -> ok;  n1 to n5 (the Nat form `unfold g n s :
  Nat`, unfoldZeroN, unfoldNoneN, unfoldSomeN and unfoldSomeNc) each
  `TYPE_INFER: unfold needs a declared type List A here` -> ok.
- Printed forms:  `make -B check` with the code and no new rows (s3-a)
  exit 0, thus no golden changed.  No current row has an unfold at a
  neutral `g s`, at a `some` with a neutral payload or at a spine limit.
- examples/unfold-laws.lang:  unfoldZeroL, unfoldNoneL, unfoldSomeL,
  unfoldSomeLc, unfoldZeroN, unfoldNoneN, unfoldSomeN and unfoldSomeNc by
  `refl`;  the defs of the eval rows;  unfoldListUniq and unfoldNatUniq:
  a function h that satisfies the unfold equations is the unfold (`elim`
  at the limit, funExt, trans and cong).  consStep gives `nil` a declared
  type in the cong function of unfoldListUniq (an inline `fun t =>
  option nil ...` gives `TYPE_INFER: nil needs a declared type List A
  here`).
- Eval rows (8):  stepNeutral and countNeutral print 3 option layers, not
  1:  the printer reads back under the closure F, and the literal limit 2
  in F reduces again (the last tail is nil, or 1 at Nat).  This is the D8
  normal form.  countSpine has a List parameter (`natAdd (lenL xs) 2`):
  a type with Nat parameters only is an entry, and `langc eval` of an
  entry needs literal arguments (EVAL_ARGS).  listSomeNeutral, countNone
  (0), countDown (3), countSome (4) and countZero as in the plan.
- Check refusal:  test/check/unfold-nat-list-step.lang (`def bad : Nat :=
  unfold (fun (n : Nat) => some (pair n n)) 3 5`) gives TYPE_MISMATCH
  (`expected a term of type Nat, found a term of type Prod Nat Nat`).
- JSON:  test/json/unfold-laws.json (the closed instances countNone 0,
  countDown 3 and countSome 4).
- Huge literal limit (probe build/m6s3-probe/huge.lang, `unfold (fun (n :
  Nat) => some n) 18446744073709551615 0`):  `EVAL_FUEL: huge: the
  evaluation takes more than 20000000 steps` in 1.2 s.  The unfold loop
  stops at the step limit before the arena limit.  Not fixed (open item
  2).
- Mutants (each with `make -B build/langc` and `make check`, reverted,
  the hashes of eval.c and check.c compared, final `make -B build/langc`
  exit 0):  3 KILLED, 0 SURVIVED.

| Mutant | First failing rows |
|---|---|
| 1. eval.c unfold_step:  the step waits for `g s` (a stuck `unfold g limit (g s)`, not the option step) | examples/unfold-laws.lang `TYPE_REFL: unfoldSomeL: ... the start normalizes to unfold g n (g s), the end normalizes to option nil (fun p => cons (first p) (unfold g n (second p))) (g s)`;  the 8 unfold-laws eval rows. |
| 2. eval.c unfold_result:  the Nat form counts one step too many (`steps + 1u`) | examples/unfold-laws.lang `TYPE_REFL: unfoldZeroN: ... the start normalizes to 1, the end normalizes to 0`;  the 8 unfold-laws eval rows. |
| 3. check.c rule_unfold:  the List form at a Nat declared type (`at_nat = 0`) | examples/unfold-laws.lang `TYPE_INFER: unfoldZeroN: unfold needs a declared type List A or Nat here`;  unfold-nat-list-step.lang (TYPE_INFER, not TYPE_MISMATCH);  the unfold-laws eval rows. |

- Gates:  `make -B check` exit 0 (native circuit 9, path 42, face 35, HIT
  44;  examples 37, parse refusals 18, check refusals 140, evals 255, json
  31 builds, build refusals 8, circuits 4, same-ir 3, circuit refusals 12,
  satcheck refusals 6;  node regressions 6, printer 2).  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed).  `git diff --exit-code -- test/circuit
  contracts/src/verifiers` exit 0:  no IR change.  contracts/test/fixtures
  restored from HEAD, then `forge test --root contracts` 33 passed,
  0 failed.

- Open items:  (1) The step 0 message prints the same text for the two
  types (a trap prints as `trap`), thus it reads as a contradiction.  A
  USER ruling can change the text.  (2) The huge-limit fuel stop is a
  resource limit, not a wrong value.
- Build record:  s3-a and s3-b by hand in the main loop (claude3, no
  subagents).

#### s3 review correction (2026-10-17)

- MEDIUM: the Nat unfold inspected a `some` payload for traps before
  counting the step. With `g n = some (natAdd n 1)`, depth 1 and seed
  18446744073709551615, it returned `trap` instead of 1. The equivalent
  `option` step returned 1 because the recursive unfold at depth 0 does
  not use the successor seed. The closed successor law also failed with
  TYPE_REFL.
- Fix: at Nat, count the outer `some` constructor and pass its payload
  only as the next seed. A trap in the Option result still propagates.
- Four C regressions in test/path-regressions.c cover the closed law,
  one step, a two-step prefix, and a trapped Option result. Before the
  fix, the first three failed. After the fix, native path regressions
  report 46 checked, 0 failed.
- Validation: `make check` exit 0 on the staged snapshot plus the fix,
  including Clang, native regressions, guest checks, evals, JSON and
  circuit gates. No CI or test weakening, and no remaining review
  blockers. No commit created.

### s4: O8 closes as Kan rules at closed data (2026-10-17)

- Commits:  none yet.  One USER commit has the code (s4-a:  kan.c and
  eval.c), examples/o8-laws.lang, test/json/o8-laws.json, 7 eval rows,
  1 check refusal, 1 changed golden row and this record (s4-b).
- D10 transp (kan.c closed_line, 321):  kan_transp (1246) tests the
  line head with closed_line, not closed_type.  closed_line is true at a
  closed type, and at Sum, Option, List, Prod and a family when each
  argument is a closed line or (at a family) a Nat literal.  Thus
  `transp (<_> T) phi a` gives `a` at each such T, a neutral `a` too.
  The rule reads the line, not the type of `a`.
- D10 hcomp (kan.c hcomp_closed, 341):  kan_hcomp (1600) calls
  hcomp_closed before the Pi, Sigma, Prod and data rules, then
  hcomp_constant gives `u0`.  The old test was `closed_type(a) &&
  a->kind != VAL_UNIV`.  hcomp_closed is a separate recursive check:
  Nat, Field, Unit and Flag;  Sum, Option, List and Prod with eligible
  fields;  a plain family when each constructor field at the parameters
  is eligible (a recursive field is the family itself).  Type n, a HIT,
  a dependent field, a partner field and an open field are not
  eligible, at each depth.  The walk stops at depth 64.  A nested
  universe keeps the universe rule (b11).  D3 of M4 and D8 of M3 stay:
  a HIT is never eligible, and the rule needs a constant system.
- Review note (a) (an empty system):  b20 `hcomp (Option Nat) i0 (fun
  i => [ i0 -> a ]) a` TYPE_REFL -> ok.  Each face is i0, thus
  hcomp_constant is true and the new rule gives `u0`.  This is the
  closed-type rule of note (a) at more types, not a wrong value:  at a
  constructor `u0` the fieldwise rule gives the same value, and D8 of M3
  forbids the rule only at a general A.
- D11:  no code.  d01, d03 and d04 stay neutral (the table below).
- D12:  skipped (no code, no mutant).  f01 (the empty-face `elim` hcomp
  at a dependent motive), f02 (`hfill Circle k .. base i1` against the
  hcomp), f03 (the same at Nat) and f04 (the M5 s3 (b) fill) give ok
  before the code.  No input fails.  See open item (1).
- Printer (eval.c print_value, 2781 and 2797):  the chain loop stops at
  a path constructor, and the tail prints by print_pcon.  Root cause of
  the M5 s2 quirk:  the loop read the path constructor as a chain op,
  thus its last interval argument printed without `@`.  `rg -n 'loop i'
  test` has no match.
- Probes (build/m6s4-probe/*.lang, `langc check`), before -> after:

| Probes | Before | After |
|---|---|---|
| (a) transp, closed line:  a01 to a05 (Nat, Field, Unit, Flag, Type 0), a09 (Prod Nat Field, eta), a16 (Item, no parameters), a13 (`some 3`), a14 (`makeLot 3 7`) | ok | ok |
| (a) transp, closed line, neutral `a`:  a06 Sum Nat Flag, a07 Option Nat, a08 List Nat, a10 Lot 3, a11 Option (Prod (Type 0) (List Flag)), a12 List (Option Nat) at face k, a15 Susp Nat | TYPE_REFL (`the start normalizes to transp (<_> T) i0 a, the end normalizes to a`) | ok |
| (b) hcomp, constant system, neutral `u0`:  b01 Nat, b02 Flag, b06 Prod Nat Field (fieldwise and eta) | ok | ok |
| (b) the same:  b03 Sum Nat Flag, b04 Option Nat, b05 List Nat, b07 Item, b08 Stack (recursive field), b09 Lot 3, b10 List (Option Unit) | TYPE_REFL | ok |
| (b) not eligible:  b11 Type 0 (`Glue a [ k -> pack a (lineToEquiv (<i> a)) ]`), b12 Circle, b13 Option Circle, b14 List Circle, b15 DPair Circle (fun c => Nat), b16 Option A (open), b17 Option (Type 0), b18 DPair Nat (fun n => Nat) (dependent field), b19 Prod Nat Circle | TYPE_REFL | TYPE_REFL, the same text |
| Review note (a):  b20 (only i0 faces) | TYPE_REFL | ok |
| (c) substitution:  c01 (fieldwise at Option A, then A := Circle, against Option Circle), c03 (= `some (hcomp Circle k (fun i => [ k -> base ]) base)`) | ok | ok |
| (c) c02 (= `some base`) | TYPE_REFL | TYPE_REFL |
| (d) D11:  d01 `transp (<i> P n) i0 u`, d03 `transp (<_> Option A) i0 a`, d04 hcomp (Option Nat) with `q @ i` | TYPE_REFL (neutral) | TYPE_REFL (neutral) |
| (d) d02 | ok | ok |
| (e) e01 (`none` against `some 3`), e02 (`none` against a neutral `a`), e03 (a neutral `a` against `none`) | HCOMP_BASE | HCOMP_BASE |
| (f) D12:  f01 to f04 | ok | ok |
| (g) g01 (`<i> pair base (loop @ i)`) | TYPE_REFL, prints `(loop i)` | TYPE_REFL, prints `(loop @ i)` |

- Classification (step 0 (e)), hcomp at a non-HIT data type, phi not
  i1.  HCOMP_BASE is a check refusal (check.c 2698):  u i0 must convert
  to `u0` on each face.  A neutral branch that converts to `u0` at i0
  passes the check;  then the hcomp stays stuck unless the system is
  constant at an eligible type.

| u0 (base) | u i0 (branch) | Type | Result |
|---|---|---|---|
| any | constant system (the same value at each i) | eligible | `u0` (D10, new) |
| constructor c | constructor c | data | fieldwise:  c with an hcomp at each field (M3, no change) |
| constructor c | constructor d, d not c | data | HCOMP_BASE (e01) |
| neutral | constructor | data | HCOMP_BASE (e02) |
| constructor | neutral | data | HCOMP_BASE (e03) |
| neutral | neutral, not constant | data | a stuck hcomp (D11, d04) |
| neutral | constant system | not eligible:  a HIT field, an open field, a dependent field, Type n in a field | a stuck hcomp (b13 to b18) |
| any | any | Type n | hcomp_univ gives a Glue type (b11) |
| any | any | a HIT | a canonical hcomp (D3 of M4, b12) |

- Printed forms:  `make -B check` with s4-a and no new rows gave exit 2
  with 1 failure.  test/eval/expect.txt 177 `quot quotSquashcStuck`:
  `(squashc Nat (R i0) x y p q k l)` -> `(squashc Nat (R i0) x y p q @ k
  @ l)`.  Reason:  the printer fix (squashc has 2 interval arguments).
  D10 changed no golden row.
- examples/o8-laws.lang:  the refl rows of a06 to a12, a15 (transpSum
  to transpSusp), b03 to b10 (hcompSum to hcompNested), c01
  (substCircle) and c03 (substCircleField, with the helper optionFill);
  the defs of the eval rows.  test/json/o8-laws.json is
  `{"glue-lang":1,"instances":[]}`:  the example has no closed instance
  (as universes.json and kanpath.json).
- Eval rows (7):  transpClosed (`fun a => a`), hcompConst (`fun k =>
  fun a => a`), hcompCircle, hcompOptionCircle and hcompListCircle (the
  stuck hcomp), transpOpen (`transp (<i> P n) i0 u`) and loopPair (`<i>
  pair base (loop @ i)`).
- Check refusal:  test/check/hcomp-base-data.lang (e01) gives
  HCOMP_BASE (`u i0 is none and u0 is some 3`).  hcomp-base.lang covers
  Nat only.
- Mutants (each with `make -B build/langc` and `make check`, reverted,
  the hashes of kan.c and eval.c compared, final `make -B build/langc`
  exit 0):  4 KILLED, 0 SURVIVED.  `make check` stops at the first
  failing native harness:  for mutants 1 and 3 a second run (gate.sh
  only) lists the guest rows.

| Mutant | First failing rows |
|---|---|
| 1. kan.c closed_line:  a head that is not data, Prod or a family gives 1 (an open line passes) | native HIT regressions `parameterized mutual partner transport computes its dependent fill: expected ok, got TYPE_REFL`;  gate.sh:  examples bag.lang (transpCons), circle.lang (meridT), gluekan.lang, iso.lang, otp.lang, transportProp.lang;  eval `kan stuckP` (`fun P => fun n => fun u => u`), `equality moved`, `kan piLine`, `kan sigmaLine`, `kan meetLine`. |
| 2. kan.c hcomp_closed:  `val_is(a, OP_FAMILY)` for `plain_family` (a HIT passes) | hcomp-i0-circle.lang and hcomp-i0-int.lang (exit 0, not TYPE_REFL);  eval `circle elimEmptyFold` (`<i> base`);  o8-laws hcompCircle, hcompOptionCircle and hcompListCircle (`fun k => fun a => a`);  build transp-hcomp.lang and hcomp-instance.lang (exit 0, not JSON_VALUE).  8 failures. |
| 3. kan.c hcomp_closed:  Sum, Option, List and Prod do not test their fields (a HIT is excluded only at the outer head) | native path regressions, 4 failed (`Sum hcomp projects on the branch face`:  `inl x`, not `inl (hcomp A k ..)`;  an open field passes too);  gate.sh:  examples/o8-laws.lang `TYPE_REFL: substCircleField: ... the start normalizes to some base, the end normalizes to some (hcomp Circle k (fun i => [ k -> base ]) base)`, thus the 7 o8-laws eval rows and the o8-laws build. |
| 4. eval.c print_value:  the chain loop does not stop at a path constructor | eval `quot quotSquashcStuck` (`q k l`) and `o8-laws loopPair` (`<i> pair base (loop i)`). |

- Gates:  `make -B check` exit 0 (native circuit 9, path 46, face 35, HIT
  44;  examples 38, parse refusals 18, check refusals 141, evals 262, json
  32 builds, build refusals 8, circuits 4, same-ir 3, circuit refusals 12,
  satcheck refusals 6;  node regressions 6, printer 2).  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0 (33 passed).  `git diff --exit-code -- test/circuit
  contracts/src/verifiers` exit 0:  no IR change.  contracts/test/fixtures
  restored from HEAD, then `forge test --root contracts` 33 passed,
  0 failed.

- Open items:  (1) D12 is skipped:  no input fails (f01 to f04 ok).  A
  USER ruling can ask for the i0 branch drop for a different reason.
  (2) Susp Nat under D10:  Susp is a HIT.  closed_line accepts a family
  with closed parameters, a HIT family too (a15 TYPE_REFL -> ok).
  Circle (no parameters) has the rule since M4 s2 (D4).  The USER can
  confirm that D10 covers a HIT with closed parameters.  (3) A family
  with a dependent field (b18 `DPair Nat (fun n => Nat)`) is not
  eligible, also when the field type does not use the earlier field.
  (4) b19 prints a nested system `[ k -> [ k -> second a ] ]`.  This
  print is from before s4.
- Build record:  s4-a and s4-b by hand in the main loop (claude3, no
  subagents).

### s5: add the hint binder and the core op inv (2026-10-17)

- Commits:  none yet.  One USER commit has the code (s5-a:  the front
  end;  s5-b:  circuit.c and the rows), the docs and the contracts
  (s5-c), and this record (s5-d).
- D14 inv (fr.c fr_inv):  square and multiply over fr_mul from the top
  bit of r - 2.  `inv 0` is 0 because r - 2 is not 0.  fr.h declares
  it.  core.h:  OP_FIELD_INV after OP_FIELD_MUL.  eval.c:  OP_NAME
  "inv", OP_ARITY 1, field_inv (at a neutral value blocked() gives the
  stuck op, at a literal fr_inv) and the reduce case.  json.c op_json:
  the case is with the other value ops.  -Wswitch-enum found no other
  site.
- D15 (check.c):  the builtin `{"inv", 1, RULE_FIELD1, OP_FIELD_INV,
  OP_FIELD, 0}` and rule_field1 (Field -> Field).  field_type moved up
  next to nat_type (its definition was after its first use).  `def inv`
  gives REFUSE_NAME from the BUILTINS table (rule R4):  no new code.
- D14 hint (front end):  lexer TOK_KW_HINT after TOK_KW_PRIVATE.
  parser.c:  `case TOK_KW_HINT:` at the 3 switch sites, and parse_hints
  (`(hint NAME := TERM)*`;  the Field `type` of each hint holds the term
  e).  parse_decl sets d->hints and d->hint_count to zero.  ast.h Decl:
  hints, hint_count.  core.h CircuitInfo:  hint_count, hints (the Core
  of each e).  check.c circuit_hints:  e checks at Field (check(),
  TYPE_MISMATCH) over the earlier binders, then push(h : Field).
  names[] holds the public, private and hint names in that order.
- D14 lowering (circuit.c lower_circuit):  n = public + private +
  hint_count.  A hint row is a ROW_INPUT after the private rows.
  fixed_at gives 0 on an input row, and put_copies copies the public
  rows only:  thus a hint row has zero selectors and no instance copy.
  lower_value:  VAL_VAR uses bound + hint_count.  A residual `inv x` is
  a VAL_STUCK OP_FIELD_INV:  the arith test sends it to no_circuit_form
  (CIRCUIT_LOWER, the text does not change).
- D14 witness (circuit.c):  witness_args matches the public, private
  and hint names.  A hint name gives WITNESS_ARGS (exit 2).  The new
  static witness_hints (after witness_checks) makes an env of val_field
  literals for binders 0 to i - 1, then calls eval_core on
  ci->hints[j].  A result that is not OP_FIELD_LIT gives the new code
  WITNESS_HINT (exit 1).  eval.c val_field is not static now;  core.h
  declares it after val_nat.
- D13:  no code.  The current lowering is the constrained monad.
- O4 closes (ruling (e) of docs/M6-PLAN.md, RULED 2026-10-17:  the
  recommendation).  test/check/hint-residual.glue (`x * inv x == 1`)
  and test/check/inv-helper.glue (`h x == x`, with the guest helper
  `def h : Field -> Field := fun y => inv y`) give CIRCUIT_LOWER.  These
  are the first gate rows of CIRCUIT_LOWER.
- Docs:  SPEC.md 2.1 (`inv`), 2.2 (the hint form, `hint` in the keyword
  list, the hint paragraph and the nonzero example), 2.3 (the 3 rows
  below), 7.1 (hint rows;  no row for `inv`;  a closed `inv k` computes
  first) and O4 in section 9.  docs/IR.md:  no change.  `rg -n
  'private|binder|hint' docs/IR.md` has no match:  the text names no
  binder kinds.
- Rows:  examples/nonzero.glue (circuit nonzero `private (x : Field)
  hint v := inv x := x * v == 1`;  circuit invOne `x * inv 1 == x`,
  which lowers).  examples/inv.lang (three, zero, one `3 * (inv 3)`,
  one2 `3 * inv 3`, stuck, big) with test/json/inv.json and 6
  test/eval/expect.txt rows.  test/check:  inv-def.lang REFUSE_NAME,
  hint-residual.glue and inv-helper.glue CIRCUIT_LOWER, hint-unused.glue
  CIRCUIT_UNCONSTRAINED, hint-type.glue TYPE_MISMATCH.
  test/circuit/expect.txt `nonzero nonzero x=3` and the goldens (k 5,
  4 rows:  the x input row, the v hint row, the mul row, the const 1
  row;  public 0;  the witness instance is `[[]]`;  satcheck ok).
  test/circuit/refuse.txt:  `2 WITNESS_ARGS witness nonzero nonzero x=3
  v=1` and `1 WITNESS_FAIL witness nonzero nonzero x=0`.
- Plan deviation:  `fieldMul` is not a surface name (NAME_UNKNOWN).
  The plan row `fieldMul 3 (inv 3)` is the eval row `3 * (inv 3)`
  (= 1).
- Probes after s5-a (build/m6s5-probe/, `langc check` and `eval`):

| Probe | Before | After s5-a |
|---|---|---|
| inv3.lang `def three : Field := inv 3` | NAME_UNKNOWN `three: unknown name inv` | ok;  eval three = 14592161914559516814830937163504850059032242933610689562465469457717205663745, zero (`inv 0`) = 0, one (`3 * (inv 3)`) = 1, stuck (`fun x => inv x`) = `fun x => inv x`, big (`inv` of r - 1) = r - 1 |
| definv.lang `def inv : Field -> Field := fun x => x` | ok | REFUSE_NAME `inv: inv is a core name (rule R4)` |
| nonzero.glue `circuit nonzero private (x : Field) hint v := inv x := x * v == 1` | PARSE_EXPECT `nonzero.glue:1:37: expected ':=', found a name` | INTERNAL `nonzero: the evaluator found a variable outside its scope` (circuit.c was not done) |
| hinttype.glue (`hint v := flagYes`) | (not run) | TYPE_MISMATCH `e: expected a term of type Field, found a term of type Flag` |
| `rg -n '\bhint\b\|\binv\b' examples test domain src` | no `inv`;  `hint` only as a C identifier in check.c and in the comment of test/check/glue-no-hint.lang (and its expect.txt row name) | (the plan said one comment hit:  the C identifier hits are not names) |

- Probes after s5-b:

| Probe | After s5-a | After s5-b |
|---|---|---|
| nonzero.glue check | INTERNAL `the evaluator found a variable outside its scope` | ok;  circuit ok, witness x=3 ok (v = inv 3), satcheck ok |
| witness x=3 v=1 | (not run) | WITNESS_ARGS (exit 2) `v is a hint: witness computes its value from its term` |
| witness x=0 | (not run) | WITNESS_FAIL `1:56: the constraint fails: the left side is 0, the right side is 1` |
| residual.glue (`x * inv x == 1`) | (not run) | CIRCUIT_LOWER `a constraint side normalizes to a stuck operation that is not + - *, ...` |
| unused.glue | (not run) | CIRCUIT_UNCONSTRAINED `the hint v is in no constraint, ...` |
| helper.glue (`h x == x`) | (not run) | CIRCUIT_LOWER (the same text) |
| invone.glue (`x * inv 1 == x`) | (not run) | circuit ok |
| hintaxiom.glue (`axiom a : Field`, `hint v := a`) | (not run) | REFUSE_AXIOM `a: an axiom has no proof (rule R2)` |
| hint-o3.glue (`hint v := x`, `x * x == 9; v == v`) | (not run) | circuit ok (s5-d;  open item (2)) |

- Printed forms:  no current golden row changed.  The 4 tables
  (test/check/expect.txt, test/eval/expect.txt, test/circuit/expect.txt
  and refuse.txt) have new rows only.  The new texts, with reasons:
  (1) CIRCUIT_UNCONSTRAINED at a hint:  `the hint %s is in no
  constraint, so the proof does not constrain it`.  The private text
  does not change.  Reason:  a hint is not a private binder, thus the
  private text is not true for it.
  (2) WITNESS_ARGS:  `v is a hint: witness computes its value from its
  term`.  Reason:  the command line gives no value for a hint (D14).
  (3) WITNESS_HINT (a new code, exit 1):  `the hint %s does not compute
  to a Field literal`.  Reason:  a defensive diagnostic, so that a user
  input does not give INTERNAL.  SPEC.md 2.3 has the 3 rows.
- e2e probe (s5-c, build/m6s5-probe/e2e/, sandbox on):  the halo2 chain
  for nonzero by hand (circuit, witness x=3, satcheck, keygen
  --dev-srs, prove, verify `ok: the proof of nonzero verifies`,
  solidity).  Each step exit 0.  proof.json `.instances` is `[]`.
  halo2 accepts 0 public inputs, thus no IR change and no halo2/ change
  (ruling a).
- Contracts (s5-c):  Verifiers.t.sol imports NonzeroVerifier and has
  NonzeroVerifierTest (testThereIsNoPublicInput:  `instances.length ==
  0`).  VerifierCase has two guards for 0 instances.  (1)
  testRedirectedInstancePointerIsRejected:  an empty `bad` becomes an
  array of one value.  Reason:  `bad[0]` reverts at 0 instances, and an
  array of one value has the wrong count, so the verifier must refuse
  it.  (2) testWrongInstanceCountIsRejected:  `fewer` has length 0 at 0
  instances, and the "too few" check is `instances.length == 0 ||
  !accepts(proof, fewer)`.  Reason:  `instances.length - 1` underflows
  at 0, and at 0 there is no shorter array.  The "too many" check does
  not change.  The 3 current circuits run the same checks as before.
  DeployVerifier.s.sol:  the import, a nonzero branch, the doc and the
  revert text (`make e2e` runs the deploy script for each name of
  test/circuit/expect.txt).  contracts/README.md:  the name list, the
  Numbers row and the Replace line.
- contracts/src/verifiers/nonzero.sol and
  contracts/test/fixtures/nonzero.proof.json are new.  The e2e probe
  wrote the first copies:  forge build runs before e2e, and
  Verifiers.t.sol imports nonzero.sol.  `make e2e` writes nonzero.sol
  byte-identical (the dev SRS keygen).
- Numbers (contracts/README.md):  `| nonzero | 5,387 | 1,216,448 |
  262,661 |`.  The size is the runtime size of `forge build --sizes`.
  The gas report gives a deployment size of 5,419 = runtime + 32 (the
  same offset as square, 5,647 and 5,615).  The deployment cost is
  1,216,448.  verifyProof is the max column (as in the other rows).
- Mutants (s5-c, a mutants.py in the session scratchpad, over fr.c,
  check.c and circuit.c;  each
  string matches once;  each mutant with `make -B`, reverted, the hashes
  compared;  final rebuild exit 0, sha ok):  5 KILLED, 0 SURVIVED.  Each
  kill is a guest gate row, thus no `gate N` run.  Mutant (2):  check.c
  has no generic Term or Core walker (self_applied, about 3585, matches
  one shape only), thus a full source-level ban is more than 2 lines.
  The mutant is a one-line shallow ban after the rhs line of the
  constraint loop (about 3976):  `if (rhs != NULL && lhs->kind ==
  CORE_OP && lhs->argc == 2 && lhs->args[1]->kind == CORE_OP &&
  lhs->args[1]->op == OP_FIELD_INV) return FAIL(c, "CIRCUIT_LOWER", "%s:
  a constraint names inv", d->name);`.  It refuses a left side `E * inv
  F` before normalization.  Only invOne (`x * inv 1 == x`) can kill it:
  hint-residual gives the same code, and inv-helper (`h x == x`) does
  not have the shallow form.

| Mutant | Verdict | Killed by |
|---|---|---|
| (1) fr_inv:  the power r - 1 | KILLED | eval rows `inv three` (got 1), `inv one` and `inv one2` (got 3), `inv big` (got 1);  build examples/inv.lang (json differs);  witness nonzero (golden differs) |
| (2) check:  the shallow source-level ban `E * inv F` at the left side | KILLED | examples/nonzero.glue `CIRCUIT_LOWER: invOne: a constraint names inv` (the intended row).  The other nonzero rows (circuit, witness, the 2 refuse rows) fail with it, because langc checks the whole file first |
| (3) lower_value:  a residual `inv x` lowers as x | KILLED | hint-residual.glue (got CIRCUIT_UNCONSTRAINED:  v is then unused), inv-helper.glue (got exit 0) |
| (4) witness_hints:  the hint gets 0 | KILLED | witness nonzero (golden differs) |
| (5) lower_circuit:  an unused hint is accepted | KILLED | hint-unused.glue (got exit 0) |

- Gates (s5-c):  `make -B check` exit 0 (native circuit 9, path 46,
  face 35, HIT 44;  examples 40, parse refusals 18, check refusals 146,
  evals 268, json 33 builds, build refusals 8, circuits 5, same-ir 3,
  circuit refusals 14, satcheck refusals 6;  node regressions 6, printer
  2).  `make check-clang` exit 0.  `forge build --root contracts
  --sizes` exit 0 (Halo2Verifier runtime 5,387, DeployVerifier 24,473).
  `make e2e` exit 0:  the e2e rows square, range, poly, square
  (quotSq) and nonzero;  forge test 44 passed (was 33:  4 contracts,
  11 tests each);  the deploy script ran for each name.  The hashes of
  test/circuit/expect.txt, refuse.txt, same-ir.txt, nonzero.sol and
  Verifiers.t.sol are the same before and after e2e.  `git diff
  --exit-code -- 'test/circuit/*.json' contracts/src/verifiers` exit 0:
  no IR change at the 4 current circuits.  contracts/test/fixtures
  restored from HEAD (the nonzero fixture is untracked and stays), then
  `forge test --root contracts` 44 passed, 0 failed.
- Open items:  (1) No guest row reaches WITNESS_HINT.  `axiom a :
  Field` gives REFUSE_AXIOM (rule R2), and a closed Field term computes
  to a literal.  (2) O3 also covers a hint:  CIRCUIT_UNCONSTRAINED is a
  syntactic check, thus a hint that is only in `v == v` passes
  (hint-o3.glue, circuit ok).  (3) The plan s5 mutant column (`inv 0`
  is not 0;  a hint row with an instance copy;  the constraint context
  sees e) did not run.  The 5 mutants above came from the s5 prompt.
  The s7 mutant roll-up can add the 3 plan mutants.
- Trap:  `langc circuit` and `langc witness` without -o write
  NAME.circuit.json and NAME.witness.json in the cwd (the repository
  root).  Give -o in a probe.
- Build record:  s5-a to s5-d by hand in the main loop (claude3, no
  subagents).

### s6: smoke the fold and unfold circuits (2026-10-17)

- Commits:  none yet.  The s6 files are staged on top of the staged s5
  (tree 95a1dddc).  s6 has no code change.
- New circuit sources (plan section 6):  examples/foldPow.glue has
  square (`pow x 1`, the fold of `t => x * t` from x at Nat), poly
  (`cube x`, the fold of `t => t * x` from x at depth 2) and nonzero
  (the fold at depth 1 gives `x * v == 1`, the hint term stays
  `inv x`).  examples/unfoldPow.glue has square (the unfold into Nat
  `stepCount` counts down from the seed 1 through option to none, and
  gives the static depth 1 of the fold) and poly (the unfold into
  List Field with the limit 2 gives `cons x (cons x nil)`, and the
  List fold gives `x * x * x`).
- The IR of each of the 5 circuits is byte-identical to the golden of
  its name.  Thus each circuit has a same-ir row and no new golden,
  verifier or fixture:  test/circuit/same-ir.txt +5 rows (3 to 8).
  The IR has no hint term (`langc witness` computes the hint), thus
  the nonzero row compares the constraint `x * v == 1` only.
- Probe (build/m6s6-probe, not a gate row):  `langc witness` for each
  of the 5 circuits with the values of test/circuit/expect.txt is
  byte-identical to the witness golden of its name.  A same-ir row
  compares the circuit only, and an expect.txt row adds an e2e run.
- Trap:  `depth` is a domain definition, thus `def depth` gives
  REFUSE_NAME (rule R4).  The unfold definition is `stepCount`.
- Mutants (plan section 8, s6 column), 3/3 KILLED:

| Mutant | Edit | Result |
|---|---|---|
| M1 the unroll one step short | eval.c reduce_fold_nat:  `k + 1 < count` | KILLED:  same-ir foldPow square, poly, nonzero and unfoldPow square FAIL;  foldPow.glue gives CIRCUIT_UNCONSTRAINED (nonzero);  31 FAIL lines |
| M2 a static depth left blocked | eval.c reduce_unfold:  OP_UNFOLD_NAT stays stuck at a literal limit | KILLED:  4 native Nat unfold regressions FAIL, then make stops before test/gate.sh.  Direct probe:  unfoldPow square gives CIRCUIT_LOWER (the depth is stuck), unfoldPow poly (List) is the same as before |
| M3 a same-ir row against the wrong circuit | same-ir.txt:  `foldPow square square` to `foldPow square poly` | KILLED:  same-ir foldPow square and unfoldPow square FAIL (the pattern also matched the unfoldPow row) |

- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face
  35, HIT 44;  examples 42 (was 40);  parse refusals 18, check
  refusals 146, evals 268, json 33 builds, build refusals 8, circuits
  5, same-ir 8 (was 3), circuit refusals 14, satcheck refusals 6;
  node regressions 6, printer 2.  `make check-clang` exit 0.  `forge
  build --root contracts --sizes` exit 0.  `make e2e` exit 0, forge
  test 44 passed.  The hash of `git diff -- test/circuit
  contracts/src/verifiers` is the same before and after e2e (only
  same-ir.txt, +5).  `git diff --exit-code -- 'test/circuit/*.json'
  contracts/src/verifiers` exit 0:  no IR change.
  contracts/test/fixtures restored from the index, then `forge test
  --root contracts` 44 passed, 0 failed.
- Open items:  (1) The 3 plan s5 mutants (s5 open item 3) are still
  for the s7 roll-up.  (2) M2 has no gate row that runs after the
  native regressions:  the new same-ir row kills it only in the direct
  probe.
- Build record:  s6 by hand in the main loop (claude3, no subagents).

### s7: close (2026-10-18)

- Commits:  plan ac8625c;  s1 f521e4e;  s2 df0d1e2 and 36595d0;  s3
  fcb1902;  s4 1b9f817;  s5 and s6 4f0e083 (one USER commit).  s7 has no
  compiler code.
- Docs:  SPEC.md 2.8 (new:  append and List bind, `elim` at the carriers
  and at Nat, the Nat successor view, the fold laws and F6 uniqueness,
  `unfold` into List and into Nat, the Kan rules at closed data, the
  overflow rule, conversion, printer, circuits and codes), 4 (the monad,
  the algebra and the coalgebra) and 9 (O3:  a hint too;  O8 closed in
  s4;  O12 (f):  O4 closed in s5;  O14 to O18 new).  docs/PLAN.md:  the
  M6 row DONE 2026-10-18 with the slice commits;  its O14 cite holds.
  formers/FORMERS.md and formers/tcc-json.md:  no change.  The F5 to F7
  cells give the status of the host kit (DONE, with the examples of the
  kit).  M6 adds the laws and the Nat form of F7 in this language:
  SPEC.md 2.8 and 4 record them.  docs/STATUS.md:  this section.
- Build record:  the s7-a agents (4 Fable, then 4 opus/xhigh with the
  marker once) died on Fable 429 and on the reasoning_extraction
  classifier, and wrote nothing.  s7-a ran the 3 plan s5 mutants by hand
  (build/m6s7-mutants.sh, output build/m6s7-mutants.out, logs
  build/m6s7-probe/).  s7-b did the rest by hand in the main loop.

#### Mutant roll-up

The plan section 8 mutant columns (s1 to s6, 3 each = 18), then each
mutant that a slice added.  Each was KILLED or not applicable.  No mutant
SURVIVED.

| Slice | Mutant | Verdict | Killed by |
|---|---|---|---|
| s1 | plan:  no right-unit rule (append_values) | KILLED | monad-laws.lang TYPE_REFL leftIdL |
| s1 | plan:  cons bind without append (bind_list) | KILLED | monad-laws.lang ELIM_CASE rightIdL |
| s1 | plan:  the IH before l in ccons (elim_list) | KILLED | eval monad-laws lenThree INTERNAL |
| s2 | plan:  the spine at k, not k - 1 (reduce_fold_nat) | KILLED | fold-laws.lang TYPE_REFL foldSucc;  nat-elim.lang ELIM_CASE foldId |
| s2 | plan:  the literal on the left only (nat_spine) | KILLED | fold-laws.lang TYPE_REFL foldSucc;  nat-elim.lang countSucc |
| s2 | plan:  `elim` Nat with the IH dropped (elim_nat) | KILLED | nat-elim.lang TYPE_REFL countSucc |
| s2 | added (obligation):  no offset normalization (nat_add) | KILLED | nat-elim.lang TYPE_REFL countOffset;  eval bag sizeOpen |
| s3 | plan:  the step waits for `g s` (unfold_step) | KILLED | unfold-laws.lang TYPE_REFL unfoldSomeL;  8 eval rows |
| s3 | plan:  the Nat form counts one step too many (unfold_result) | KILLED | unfold-laws.lang TYPE_REFL unfoldZeroN;  8 eval rows |
| s3 | plan:  the List form at a Nat declared type (rule_unfold) | KILLED | unfold-laws.lang TYPE_INFER unfoldZeroN;  unfold-nat-list-step.lang |
| s4 | plan:  the closed rule at an open line (closed_line) | KILLED | native HIT regressions;  gate.sh:  6 examples and 5 eval rows |
| s4 | plan:  the hcomp rule at a HIT (hcomp_closed) | KILLED | hcomp-i0-circle.lang, hcomp-i0-int.lang;  o8-laws hcompCircle rows;  2 builds (8 failures) |
| s4 | plan:  no i0 drop | not applicable | D12 has no code (s4 skipped it:  no probe input fails), thus there is no rule to remove |
| s4 | added (obligation):  a HIT excluded only at the outer head (hcomp_closed) | KILLED | native path regressions (4);  o8-laws.lang TYPE_REFL substCircleField |
| s4 | added:  the print chain does not stop at a path constructor (print_value) | KILLED | eval quot quotSquashcStuck, o8-laws loopPair |
| s5 | plan (run in s7-a):  `inv 0` is not 0 (fr.c fr_inv gives 1 at 0) | KILLED | eval `inv zero` (want 0, got 1);  build examples/inv.lang (json differs) |
| s5 | plan (run in s7-a):  a hint row with an instance copy (circuit.c put_copies) | KILLED | circuit nonzero golden;  same-ir foldPow nonzero |
| s5 | plan (run in s7-a):  the constraint context sees e | KILLED | foldPow.glue and nonzero.glue CIRCUIT_LOWER;  circuit and witness nonzero goldens;  same-ir foldPow nonzero;  witness rows `x=3 v=1` and `x=0` |
| s5 | added:  fr_inv with the power r - 1 | KILLED | eval `inv three`, `inv one`, `inv big`;  build inv.lang;  witness nonzero |
| s5 | added (obligation):  a shallow source-level ban on `E * inv F` | KILLED | nonzero.glue CIRCUIT_LOWER invOne |
| s5 | added:  a residual `inv x` lowers as x (lower_value) | KILLED | hint-residual.glue, inv-helper.glue |
| s5 | added:  the hint gets 0 (witness_hints) | KILLED | witness nonzero golden |
| s5 | added:  an unused hint is accepted (lower_circuit) | KILLED | hint-unused.glue |
| s6 | plan:  the unroll one step short (reduce_fold_nat) | KILLED | same-ir foldPow square, poly, nonzero and unfoldPow square;  31 FAIL lines |
| s6 | plan:  a static depth left blocked (reduce_unfold) | KILLED | 4 native Nat unfold regressions;  direct probe:  unfoldPow square CIRCUIT_LOWER |
| s6 | plan:  a same-ir row against the wrong circuit (same-ir.txt) | KILLED | same-ir foldPow square and unfoldPow square |

- Totals:  18 plan mutants:  17 KILLED, 1 not applicable (s4 no i0
  drop).  8 added mutants:  8 KILLED.  0 SURVIVED.
- The 3 plan s5 mutants (s5 open item 3, s6 open item 1):  one runner
  script (backup, `sd` with the hit count, `make -B check`, restore from
  the backup), each pattern hit 1, src/ clean against HEAD after the
  run.  M3 is at the lowering env, not at the checker:  check.c 3938
  circuit_hints pushes h as a Field variable and has no let binder, thus
  the mutant binds each hint row in circuit.c lower_circuit (243) to
  `eval_core(m, env, ci->hints[..])`, not to `val_var(m, i)`.  The clean
  `make -B check` after the run (build/m6s7-probe/final.log) gave the s6
  close counts.

#### Review

- (i) No conv rule reads a type.  The conv functions (eval.c conv_fun
  1941 to conv_values 2226) do not name OP_APPEND, OP_ELIM_CORE,
  OP_ELIM_NAT, OP_UNFOLD_NAT or OP_FIELD_INV, and read no type:  the M6
  ops convert by the stuck-head rule (op, inst, then each argument).  The
  reducers of OP_ELIM_CORE, OP_ELIM_NAT, OP_FOLD_NAT, OP_UNFOLD and
  OP_UNFOLD_NAT (eval.c reduce_op 1703 to 1730;  elim_core 676 and 690,
  reduce_fold_nat 753, elim_nat 781, the unfold step 1189 to 1282) test
  the head of the scrutinee or the limit (blocked, val_is), not a type.
  The D10 rules read the line or the type argument (kan.c closed_line,
  hcomp_closed), not the type of a value.
- (ii) No rule `hcomp A i0 u u0 = u0`.  hcomp-i0-int.lang and
  hcomp-i0-circle.lang give TYPE_REFL (gate rows;  the s4 mutant 2 makes
  them exit 0).  The s4 rule at an eligible closed type with only i0
  faces (b20) is the closed-type rule of a constant system, not a
  general i0 rule (D8 of M3 forbids it at a general A).
- (iii) `hcomp` at a HIT stays canonical (D3 of M4).  hcomp_closed
  refuses a HIT at each depth (the s4 mutants 2 and 3 KILLED);  the eval
  rows hcompCircle, hcompOptionCircle and hcompListCircle keep the stuck
  hcomp.
- (iv) A trap is not conversion evidence (s2).  conv_loop (eval.c 2176)
  has the VAL_TRAP case with this rule.  fold-overflow-proof.lang and
  elim-nat-overflow-proof.lang give TYPE_MISMATCH;  overflow-refl.lang
  gives TYPE_REFL.
- (v) A residual `inv` in a constraint gives CIRCUIT_LOWER
  (hint-residual.glue, inv-helper.glue), and `inv 0` is 0 (the eval row
  `inv zero`;  the s5 plan mutant 1 KILLED).  A closed `inv 1` computes
  before the lowering (invOne in examples/nonzero.glue).
- Result:  no compiler defect found in (i) to (v).  The documentation
  corrections from the staged review are recorded below.  No change to src/.

#### Gates

- `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT 44;
  examples 42, parse refusals 18, check refusals 146, evals 268, json 33
  builds, build refusals 8, circuits 5, same-ir 8, circuit refusals 14,
  satcheck refusals 6;  node regressions 6, printer 2.  Each count is
  the same as the s6 close (s7 adds no row).  Against the M5 close:
  path +4, examples +9, check refusals +23, evals +59, json +6, circuits
  +1, same-ir +5, circuit refusals +2.
- `make check-clang` exit 0.  `forge build --root contracts --sizes`
  exit 0.  `make e2e` exit 0 (5 rows, forge test 44 passed).  `git diff
  --exit-code -- test/circuit contracts/src/verifiers` exit 0:  no IR
  change.  contracts/test/fixtures restored, then `forge test --root
  contracts` 44 passed, 0 failed.  Logs:  build/m6s7-gates/.

#### Open items

- SPEC.md 9 O14:  F6 uniqueness at a HIT (the path cases need squares).
- SPEC.md 9 O15:  s2 `dbl` and `fold plus1 0` at 18446744073709551615
  give OOM (the 1 GiB arena);  s3 the huge `unfold` limit gives
  EVAL_FUEL.  Resource limits, not wrong values.
- SPEC.md 9 O16:  s3 TYPE_MISMATCH `Path Nat trap 0` against the same
  text (USER open item).
- SPEC.md 9 O17:  automated refusal coverage for WITNESS_HINT is missing;
  the overflow probe below reaches it.
- SPEC.md 9 O3:  s5 (2) O3 also covers a hint.
- SPEC.md 9 O18:  s4 (2) the closed-line rule at a HIT family with
  closed parameters (`Susp Nat`, USER can confirm);  s4 (3) a dependent
  field is not eligible for the hcomp rule.
- s4 (1):  D12 (the i0 branch drop) is not in M6.  A USER ruling can ask
  for it for a different reason.
- s4 (4):  b19 prints a nested system `[ k -> [ k -> second a ] ]` (from
  before s4).
- s2 (1):  elim_at_nat repeats the case loop of elim_carrier (a later
  cleanup).
- s6 (2):  the s6 M2 mutant (unfold depth stuck) fails the native
  regressions first, thus `make check` stops before the new same-ir row.
  The direct probe kills it.
- O12 (c) and O13 stay open (ruling (e) of docs/M6-PLAN.md).
- M7:  docs/PLAN.md M7 row (Direction 2:  circuits quotiented by
  relation equivalence).  The M7 kickoff prompt has not been written yet.

#### Staged review corrections (2026-10-18)

Four documentation contracts were corrected against the evaluator and
guest probes. Each finding is MEDIUM; no CI or test was weakened.

- WITNESS_HINT is reachable from a well-typed hint. The assumption in
  s5 open item 1 that every closed Field term is a literal was incorrect.
  This guest circuit checks, but `witness hintTrap x=0` gives WITNESS_HINT
  and exit 1, with no witness file:

  ```text
  def badField : Field := fold (fun (y : Field) => y) 0 (natAdd 18446744073709551615 1)
  circuit hintTrap private (x : Field) hint v := badField := v == x
  ```

  SPEC.md O17 now distinguishes missing automated coverage from
  reachability. No compiler change is needed.
- A trapped List unfold step preserves cells produced before it. For
  `listStep 0 = some (pair 7 1)` and `listStep 1 = trap`,
  `unfold listStep 2 0 : List Nat` evaluates to `cons 7 trap`, rather than
  discarding the first cell. The corresponding Nat unfold gives a trap.
  SPEC.md 2.8 now gives separate List and Nat rules and allows a successor
  spine to stop before its literal offset is exhausted.
- `natAdd` of two neutral operands stays stuck. The probe
  `fun xs => fun ys => natAdd (length xs) (length ys)`, where `length` is
  the List fold, retains both neutral folds. SPEC.md 2.8 now requires a
  positive literal operand for the offset normal form.
- The `hcomp` table omitted the full-face rule. At `i1`,
  `hcomp (Type 0) i1 (fun i => [ i1 -> Nat ]) Nat` evaluates to `Nat`,
  and `hcomp Circle i1 (fun i => [ i1 -> base ]) base` evaluates to `base`,
  before the universe or HIT rule can apply. SPEC.md 2.8 now states this
  precedence. O8 also drops the old claims that closed-data transport at a
  neutral value stays stuck and incompatible constructor heads stay stuck.
  The neutral-data rows exclude Pi, Sigma, Prod and Path, which retain
  their fieldwise rules: transport of a neutral function at `A -> A`
  produces a lambda, rather than a neutral transport of the function.

Validation: a fresh TinyCC build from HEAD 4f0e083 and `make check` passed
(including check-clang, all native and guest regressions, and printer
checks). All probes above were rerun with that fresh binary. These fixes
change documentation only; the counts in the s7 gates above are unchanged.

## M7

Plan:  docs/M7-PLAN.md (RULED 2026-10-18 (USER):  questions (a) to (i),
each recommendation).  M7 is Direction 2 of design/DESIGN.md.  The s1
commit includes the plan.

### s1: add quotient circuits and obligation refusals (2026-10-18)

- Scope:  D1, D5, D6 and D7 of docs/M7-PLAN.md.  No change in src/
  (section 3:  none).
- Table formats:  test/check/expect.txt is `FILE CODE`, and `langc check`
  reads a .glue file there (circuit-binder.glue).  Thus CIRCUIT_BINDER is
  a check refusal, and the s1 delta of section 10 keeps check refusals +4.
  test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`, and test/gate.sh
  reads examples/FILE.lang only.  test/circuit/same-ir.txt is `PROG NAME
  GOLDEN`.
- Examples (D5, D6), each with one circuit `square` and the quotSq header
  (`SqRel`, `sq`):  examples/quotEnd0.glue (A1), quotEnd1.glue (A2),
  quotHcomp.glue (A3), quotElimHcomp.glue (A4, `sqE` by `elim`) and
  quotProof.glue (A8, the `Lk` def form).  `langc circuit examples/F.glue
  square -o $TMPDIR/F.json` of each:  exit 0, IR SAME (`cmp` against
  test/circuit/square.circuit.json).  5 same-ir rows `F square square`.
- Refusals (D1, D7):  test/check/geqc-case.glue (A5, HIT_CASE),
  elim-geqc-case.glue (A4 with the `geqc` case `fun a => fun b => fun r
  => refl`;  probe:  ELIM_CASE, as the plan expects), squashc-field.glue
  (A7, HIT_CASE) and isset-binder.glue (A6, CIRCUIT_BINDER).  4 check
  refusal rows.
- evHcomp (section 4):  the eval table reads .lang files only.  Thus the
  new file examples/evHcomp.lang has `SqRel`, `sq`, the law `evHcompLaw :
  (x : Field) -> Path Field (sq (hcomp ..)) (x * x) := fun x => refl` and
  the closed def `evHcomp` (x = 3).  Eval row `evHcomp evHcomp => 9`.  The
  new .lang file also adds 1 json build:  test/json/evHcomp.json has one
  instance, `evHcomp` with the value "9".  Delta change against section
  10:  examples +6 (not +5), json +1 (not in the plan).
- SPEC.md 2.9 (new):  Direction 2 (M7):  D1, D5, D6, D7 and the s1 files.
  The later slices add to it.
- Mutants:  a scratch runner did each mutant with `sd -F` (1 hit for each
  pattern, checked first), `make -B build/langc`, the new rows probed
  directly with `langc check`, then `sh test/gate.sh`, then `git restore
  src/front/check.c`.  The path case type of fold_hit_rule comes from
  hit_case_type (check.c line 972), and the path case type of rule_elim
  comes from elim_case_type (check.c line 1480).  Each mutant is at one of
  these two lines.
  1. fold_hit_rule does not test a path case:  hit_case_type gives the i0
     end at the two ends of a 1-dimensional path case (`.., i - 1u, 1)` to
     `.., i - 1u, kb > 1u)`).  KILLED:  geqc-case.glue exit 0, so its row
     fails.  Old rows also kill it (examples quot.lang, int.lang,
     otp.lang, transportProp.lang, trunc.lang:  HIT_CASE).
  2. rule_elim does not test a path case:  elim_case_type gives the i0 end
     at the two ends (`.., i - 1u, 1)` to `.., i - 1u, 0)`).  KILLED:
     elim-geqc-case.glue exit 0, so its row fails.  Old rows also kill it
     (int.lang negNeg, otp.lang ret, transportProp.lang repElimG:
     ELIM_CASE;  3 otp eval rows).
  3. fold_hit_rule does not test the `squashc` case:  hit_case_type gives
     the i0 end at the two ends of the outer binder of a 2-dimensional
     case (`kb > 1u && i == 1u ? 0 : 1`).  KILLED:  squashc-field.glue
     exit 0, so its row fails.  Old rows also kill it (quot.lang quotRec,
     transportProp.lang repFold:  HIT_CASE;  4 quot eval rows).
  No mutant SURVIVED, thus no new row.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 48, parse refusals 18, check refusals 150, evals 269,
  json 34 builds, build refusals 8, circuits 5, same-ir 13, circuit
  refusals 14, satcheck refusals 6;  node regressions 6, printer 2.
  Against the M6 close:  examples +6, check refusals +4, evals +1, json
  +1, same-ir +5.  `make check-clang` exit 0.  `forge build --root
  contracts --sizes` exit 0.  `make e2e` exit 0.  The diff of
  test/circuit and contracts/src/verifiers is the same before and after
  e2e:  only the 5 new rows of test/circuit/same-ir.txt.  No IR change.
  contracts/test/fixtures restored, then `forge test --root contracts`
  44 passed, 0 failed.
- Open items:  none new.  Next:  s2 (docs/M7-PLAN.md section 10).

### s2: add the circuit syntax family Circ (2026-10-18)

- Scope:  D2 of docs/M7-PLAN.md (the deep form, rulings (b) and (h)):  one
  public binder `cpub` and one private binder `cpriv`.  The indexed form is
  an M8 open item.  The only change outside the guest tables, examples and
  docs is the `family Circ` line in domain/domain.lang (rule R1).  No
  change in src/.
- Name check:  `rg -n -w` of the 14 names (`Circ`, the 6 constructors and
  the 7 fields) over examples, test, domain and src at efd430b:  no hit.
- Domain probe (COMMON M7 kickoff trap):  a scratch copy of the repository
  (rsync without .git, halo2, contracts and build) with the `family Circ`
  line appended, then `make -B -C <copy> check`:  each count equals the s1
  count.  Thus no old golden lists the domain families.
- Domain:  domain/domain.lang gets the family `Circ := cpub | cpriv | clit
  (clv : Field) | cadd (cal : Circ) (car : Circ) | cmul (cml : Circ) (cmr :
  Circ) | cpow (cpb : Circ) (cpn : Nat)`, after the mutual block, with a
  comment.  `make -B check`:  the s1 counts.
- Runner trap:  the first runner guarded the append with `rg -q -F 'family
  Circ'`, and that pattern also matches `family Circle`.  Thus the append
  did not occur, and each probe gave NAME_UNKNOWN (`unknown name Circ`).
  The runner stopped before the install.  The fixed guard is `family Circ
  :=`.
- examples/circSyntax.glue (D2, D1):  `ev`, `CRel` and `evQ` of the kickoff
  probe (D0), and one circuit `square` with the D1 constraint `evQ y x (gcls
  Circ CRel (cmul cpriv cpriv)) == y`.  `langc circuit examples/circSyntax.glue
  square -o ..`:  exit 0, IR SAME.  The D1 form gives IR SAME, thus the
  fallback `ev y x (cmul cpriv cpriv) == y` is not necessary.  `CRel` and
  `evQ` are local defs of the .glue program, thus they do not collide with
  examples/circRel.lang of s3.  Same-ir row `circSyntax square square`.
- examples/circEval.lang (eval rows at each constructor, closed binders y =
  9 and x = 3):  `evPub` 9, `evPriv` 3, `evLit` (`clit 5`) 5, `evAdd` (`cadd
  cpub cpriv`) 12, `evMul` (`cmul cpriv cpriv`) 9, `evPow` (`cpow cpriv 2`)
  27.  The cpow case `fold (fun (t : Field) => hb * t) hb n` gives hb to the
  power n + 1.  Each probe gave the planned value.  6 eval rows `circEval
  NAME => VALUE`.  test/json/circEval.json (`langc build`) has the 6
  instances.  Delta change against section 10:  examples +2 (not +1), json
  +1 (the eval table reads examples/FILE.lang only, s1 trap).
- Refusal:  test/check/domain-cpow.lang (`def cpow : Field := 1`):
  REFUSE_NAME (`cpow is a domain constructor (rule R4)`).  Row
  `domain-cpow.lang REFUSE_NAME`.
- SPEC.md:  2.9 gets the D2 paragraph (the `Circ` family, its constructors
  and fields, `ev` as a guest `elim (fun d => Field)`, the M6 Nat fold in the
  `cpow` case, the binders, the M8 open item, the s2 files).  The M4 list of
  the smoke families (section 2.6, Names) gets the sentence "M7 adds the
  family Circ (section 2.9)".  That list names the M4 families only (it has
  no M5 family), thus Circ is a cross reference there.
- Mutants:  scratch copies of the new examples, `sd -F`, the rows probed
  directly with the real build/langc (no rebuild:  the examples change
  only).
  1. Swap the `cadd` and `cmul` cases of `ev` in circSyntax.glue (1 hit):
     KILLED, `langc circuit` exit 0, IR DIFF (the same-ir row fails).
  2. `hb * t` to `hb + t` in circEval.lang:  KILLED, evPow gives 9, not
     27.  The pattern has 2 hits:  the cpow case and the header comment.
     The comment hit does not change the program.
  3. Swap the `cpub` and `cpriv` cases (`y x` to `x y`) in circEval.lang (1
     hit):  KILLED, evPub gives 3 and evPriv gives 9.
  No mutant SURVIVED, thus no new row.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 50, parse refusals 18, check refusals 151, evals 275,
  json 35 builds, build refusals 8, circuits 5, same-ir 14, circuit
  refusals 14, satcheck refusals 6;  node regressions 6, printer 2.
  Against s1:  examples +2, check refusals +1, evals +6, json +1, same-ir
  +1.  `make check-clang` exit 0.  `forge build --root contracts --sizes`
  exit 0.  `make e2e` exit 0.  The diff of test/circuit and
  contracts/src/verifiers is the same before and after e2e:  only the new
  row of test/circuit/same-ir.txt.  The verifiers did not change, thus the
  domain change gives no IR change.  contracts/test/fixtures restored, then
  `forge test --root contracts` 44 passed, 0 failed.
- Open items:  the indexed binder form of `Circ` (`cvar (n : Nat)` with an
  environment) for M8 (question (h)).  Next:  s3 (docs/M7-PLAN.md section
  10).

### s3: add the circuit relations and their laws (2026-10-18)

- Scope:  D3, D4 and D8 of docs/M7-PLAN.md (rulings (b) and (i)):  the
  pointwise `CRel` for the lowering, `CEqv` for properties only, no ring
  conv rule.  No change in src/ and in domain/domain.lang.  No circuit and
  no test/circuit change.
- examples/circRel.lang:  `ev`, `CRel` and `evQ` (as circSyntax.glue),
  `Sat`, `CEqv`, `SatQ` (the `geqc` case `ua (e y x)`), `SatP`, `QLine`,
  and the 5 laws of section 4 as defs with Pi binders and the body `refl`:
  evPoint, evEnd0, evEnd1, evPath and satQFwd.  `langc check` gives ok.
  The term `<i> evQ y x (geqc Circ CRel c1 c2 r @ i)` as an argument of
  `Path` in the type of evPath checks:  no `EvLine` def.  satQFwd holds by
  refl (the computation rule of `ua`, as transportPropFwd):  no TYPE_REFL.
  No def name gave REFUSE_NAME.  `QLine` is after evPath in the file, thus
  a boundary mutant fails at an `ev` fold or law first.  `langc build`
  exit 0 with 0 instances (a def with a Pi type is not a JSON instance):
  test/json/circRel.json is `{"glue-lang":1,"instances":[]}`.
- test/check/ring-refl.lang and the row `ring-refl.lang TYPE_REFL` (D8):
  `langc: TYPE_REFL: ringRefl: refl cannot prove the path: the start
  normalizes to fieldMul x x, the end normalizes to fieldAdd (fieldMul x
  x) 0`.
- Delta against section 10 (examples +1, check refusals +1):  also json +1
  (a .lang example adds a json build, s1 trap).  The plan does not list
  it.  No eval row (section 8 lists none for s3).
- SPEC.md 2.9:  4 paragraphs after **The circuit syntax.**:  **The circuit
  relations.** (D3, D4), **The laws of the relations.**, **The gap of
  CEqv.** (D4, O19) and **No ring normal form.** (D8).  The SPEC.md
  section 9 record of O19 is s5.
- Mutants:  a scratch copy for the guest mutant;  for each src mutant a
  scratch rsync copy of the repository and `make -B -C <copy> build/langc`,
  run on the REAL new files.  Each `sd -F` pattern has 1 hit.
  1. Guest:  `hl + hr` to `hl` in a copy of ring-refl.lang:  `langc
     check` exit 0.  KILLED by the row `ring-refl.lang TYPE_REFL`.
  2. src:  field_op (src/front/eval.c) gives the left operand for a Field
     add with the literal 0 on the right (2 added lines after the trap
     test).  The copy's check of test/check/ring-refl.lang gives exit 0.
     KILLED by the ring-refl row.  It is the only failing row:  the 4
     native harnesses pass, and test/gate.sh gives `gate: 1 failures`.
  3. src:  pcon_boundary (eval.c line 1535) gives the body of the first
     branch (`sys->args[1u]`) at the first face that is i1, thus `geqc ..
     @ i1` gives the i0 end.  KILLED by the examples row of circRel.lang.
     Difference from the prompt:  the first error is HIT_CASE at `evQ`,
     not TYPE_REFL at evEnd1, because the checker gets the boundary of the
     `geqc` case type with the same rule.  Old rows also kill it:  16 of 51
     examples fail (quotEnd0.glue and quotEnd1.glue are 2 of them), many
     eval rows fail (circle.lang:  TYPE_MISMATCH at `m`), and 5 of 44
     native HIT rows fail.
  No mutant SURVIVED, thus no new row.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 51, parse refusals 18, check refusals 152, evals 275,
  json 36 builds, build refusals 8, circuits 5, same-ir 14, circuit
  refusals 14, satcheck refusals 6;  node regressions 6, printer 2.
  Against s2:  examples +1, check refusals +1, json +1.  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0.  The diff of test/circuit and contracts/src/verifiers
  is clean before and after e2e.  contracts/test/fixtures restored, then
  `forge test --root contracts` 44 passed, 0 failed.
- Open items:  O19 (new, D4):  an `Equiv` of the two satisfaction types
  does not give `Path Field (ev y x c1) (ev y x c2)`, because `y` is also
  an input of `ev`.  Thus `CEqv` gives no fold of `GQuot Circ CEqv` into
  Field, and `GQuot Circ CEqv` carries properties only.  The SPEC.md
  section 9 record is s5.  The indexed binder form of `Circ` stays an M8
  item (question (h)).  Next:  s4 (docs/M7-PLAN.md section 10).

### s4: add the transport smoke example (2026-10-18)

- Scope:  D5, D6, D7 and D9 of docs/M7-PLAN.md (rulings (d) and (e)):
  `opt` by refl, the circuits through `geqc .. opt` and through `cpow`,
  and `moveSat` through the `SLine` def.  No change in src/ and in
  domain/domain.lang.
- examples/circOpt.glue:  `ev`, `CRel` and `evQ` (as circSyntax.glue),
  `opt : CRel (cmul cpriv cpriv) (cpow cpriv 1) := fun y => fun x =>
  refl`, and the circuit `square` on `geqc Circ CRel (cmul cpriv cpriv)
  (cpow cpriv 1) opt @ i1`.  `langc check` gives ok, and the IR equals
  test/circuit/square.circuit.json.
- examples/circPow.glue:  `ev`, `CRel` and `evQ`, and the circuit `square`
  on `gcls Circ CRel (cpow cpriv 1)`.  It has no `opt`, because the
  circuit does not use it.  `langc check` gives ok, and the IR equals the
  square golden.
- examples/circTransport.lang:  `ev`, `CRel`, `evQ`, `Sat` and `SatP` (as
  circRel.lang), `opt`, `SLine`, `moveSat` (a declared Pi type), the law
  moveSatRefl `Path (Path Field 9 9) (moveSat 9 3 refl) refl := refl`,
  and the closed def `m3 : Path Field 9 9 := moveSat 9 3 refl`.  `langc
  check` gives ok:  no TYPE_REFL at moveSatRefl.  `langc eval` gives `<j>
  9` for m3 and `<i> <j> 9` for moveSatRefl.  No def name gave
  REFUSE_NAME.  `langc build` exit 0 with 0 instances:
  test/json/circTransport.json is `{"glue-lang":1,"instances":[]}`.
  Difference from the prompt plan note (a closed def is an instance):
  moveSatRefl and m3 are proofs, and src/json.c line 349 says that
  functions, types, families and proofs are not instances.
- Rows:  `circOpt square square` and `circPow square square` in
  test/circuit/same-ir.txt;  `1 WITNESS_FAIL witness circOpt square y=10
  x=3` in test/circuit/refuse.txt (`langc: WITNESS_FAIL: square: 15:58:
  the constraint fails: the left side is 9, the right side is 10`, and no
  file at -o);  `circTransport m3 => <j> 9` in test/eval/expect.txt.
- Delta against section 10 (examples +3, same-ir +2, circuit refusals +1,
  evals +1):  also json +1 (circTransport.lang is a .lang example, s1
  trap).  The plan does not list it.
- SPEC.md 2.9:  1 paragraph after **No ring normal form.**:  **The
  transport smoke example.** (D5, D6, D7, D9).
- Mutants:  scratch copies of the new files, run with the real
  build/langc (no src mutant).  Each `sd -F` match count is as planned:
  the header comments do not match.
  1. Section 8:  `cpow cpriv 1` to `cpow cpriv 2` in a copy of
     circOpt.glue (2 matches:  `opt` and the circuit).  `langc: TYPE_REFL:
     opt: refl cannot prove the path: the start normalizes to fieldMul x
     x, the end normalizes to fieldMul x (fieldMul x x)`.  KILLED by the
     examples row and the same-ir row (`langc circuit` gives the same
     TYPE_REFL).  The refuse row also kills it (TYPE_REFL, not
     WITNESS_FAIL).
  2. The circuit of a copy of circPow.glue at `cpow cpriv 0` (1 match):
     check ok, IR DIFF.  KILLED by the same-ir row.
  3. `def m3 : Path Field 9 9 := moveSat 9 3 refl` to `def m3 : Path
     Field 4 4 := moveSat 4 2 refl` in a copy of circTransport.lang (1
     match):  check ok, and `langc eval` gives `<j> 4`.  KILLED by the
     eval row.
  No mutant SURVIVED, thus no new row.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 54, parse refusals 18, check refusals 152, evals 276,
  json 37 builds, build refusals 8, circuits 5, same-ir 16, circuit
  refusals 15, satcheck refusals 6;  node regressions 6, printer 2.
  Against s3:  examples +3, evals +1, json +1, same-ir +2, circuit
  refusals +1.  `make check-clang` exit 0.  `forge build --root contracts
  --sizes` exit 0.  `make e2e` exit 0.  Before e2e, the diff of
  test/circuit and contracts/src/verifiers holds only the 3 new rows, and
  after e2e it is the same.  contracts/src/verifiers did not change.
  contracts/test/fixtures restored, then `forge test --root contracts` 44
  passed, 0 failed.
- Open items:  none new.  s5 records O19 and the O4 record in SPEC.md
  section 9, and closes O17 (hintTrap.glue and the WITNESS_HINT row).
  Next:  s5 (docs/M7-PLAN.md section 10).

### s5: close O17 and record O19 (2026-10-18)

- Scope:  D11 of docs/M7-PLAN.md and ruling (f):  O17 closes with an
  automated row;  SPEC.md section 9 gets the O4 record, O17 closed and
  O19 new;  the review of the s1 to s4 mutants.  No change in src/ and in
  domain/domain.lang.
- examples/hintTrap.glue:  the circuit of the M6 staged review
  corrections, `def badField : Field := fold (fun (y : Field) => y) 0
  (natAdd 18446744073709551615 1)` and `circuit hintTrap private (x :
  Field) hint v := badField := v == x`.  `langc check` gives ok.  No def
  name gave REFUSE_NAME.  `langc circuit .. hintTrap -o` exit 0 (559
  bytes), because the lowering does not evaluate a hint.  The examples
  gate takes a .glue example with no golden (test/gate.sh line 6 runs
  `langc check` only).  No json build (a .glue example, s1 trap).
- Row:  `1 WITNESS_HINT witness hintTrap hintTrap x=0` in
  test/circuit/refuse.txt.  The probe gives exit 1 and `langc:
  WITNESS_HINT: hintTrap: the hint v does not compute to a Field
  literal`, and no file at -o.
- SPEC.md 9:  O4 (the record text "O4 stays open for M7" gets the M7
  result:  no quotient value gets to the lowering stuck, A1 to A4);  O17
  closed in M7 (the file and the row;  the M6 record stays);  O19 new
  (the gap of `CEqv`, D4, as SPEC.md 2.9 **The gap of CEqv.**).
- Mutant review:  each killing row of the s1 to s4 mutants is still in
  its table (`rg -n -x -F`).  s1 1 to 3:  test/check/expect.txt lines
  147, 148 and 149 (geqc-case.glue HIT_CASE, elim-geqc-case.glue
  ELIM_CASE, squashc-field.glue HIT_CASE).  s2 1:  test/circuit/same-ir.txt
  line 14 (circSyntax).  s2 2 and 3:  test/eval/expect.txt line 274
  (`circEval evPow => 27`), lines 269 and 270 (`evPub => 9`, `evPriv =>
  3`).  s3 1 and 2:  check line 152 (ring-refl.lang TYPE_REFL).  s3 3:
  the examples row of circRel.lang (also same-ir lines 9 and 10, quotEnd0
  and quotEnd1).  s4 1:  the examples row of circOpt.glue, same-ir line
  15 and refuse.txt line 15.  s4 2:  same-ir line 16 (circPow).  s4 3:
  eval line 275 (`circTransport m3 => <j> 9`).  Each file of these rows
  is present.  No row is missing, thus no mutant rerun.
- Mutants of the new row:  scratch copies of examples/hintTrap.glue, run
  with the real build/langc.  Each `sd -F` pattern has 1 match:  the
  header comment does not match.
  1. The hint at a value with no trap (`hint v := badField` to `hint v :=
     0`):  check ok, circuit exit 0, `witness .. x=0` exit 0 and a file
     at -o.  KILLED by the refuse row.  The examples row passes.
  2. No overflow (`18446744073709551615 1` to `18446744073709551614 1`):
     check ok, circuit exit 0, `witness .. x=0` exit 1 with `langc:
     EVAL_FUEL: hintTrap: the evaluation takes more than 20000000 steps`
     and no file.  KILLED by the refuse row on the code (EVAL_FUEL, not
     WITNESS_HINT).  The fold of 18446744073709551615 steps is the O15
     resource limit.  The examples row passes.
  No mutant SURVIVED, thus no new row.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 55, parse refusals 18, check refusals 152, evals 276,
  json 37 builds, build refusals 8, circuits 5, same-ir 16, circuit
  refusals 16, satcheck refusals 6;  node regressions 6, printer 2.
  Against s4:  examples +1, circuit refusals +1 (as section 10).  `make
  check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.
  `make e2e` exit 0.  Before e2e, the diff of test/circuit and
  contracts/src/verifiers holds only the new row, and after e2e it is the
  same.  contracts/src/verifiers did not change.  contracts/test/fixtures
  restored, then `forge test --root contracts` 44 passed, 0 failed.
- Open items:  none new.  O18:  ruling (f) confirmed it (USER,
  2026-10-18);  the SPEC.md 9 O18 text is s6 work (SPEC.md 4 and 9).
  Next:  s6, the M7 close (docs/M7-PLAN.md section 10).

### s6: close (2026-10-18)

- Commits:  s1 efd430b (with the plan docs/M7-PLAN.md);  s2 aa2d240;  s3
  4aefb85;  s4 0ddc6bd;  s5 b21aaf7.  s6 changes docs only:  no change in
  src/, in domain/domain.lang, in test/ and in examples/.
- What M7 added:  Direction 2 of design/DESIGN.md (circuits quotiented by
  relation equivalence), with no lowering code and no IR change (rulings
  (a) and (d)).  The domain family `Circ` (one public binder `cpub`, one
  private binder `cpriv`, `clit`, `cadd`, `cmul` and `cpow`) and the guest
  interpreter `ev`;  the pointwise relation `CRel`, the quotient `GQuot
  Circ CRel` and its fold `evQ` into Field (the lowering quotient);
  `CEqv`, `SatQ` and `SatP` (properties only, O19);  the laws of
  docs/M7-PLAN.md section 4, each by refl;  the transport smoke example
  (`opt`, `moveSat`, and circOpt.glue and circPow.glue with the IR of
  square);  the obligation refusals of the `geqc` and `squashc` cases;
  the TYPE_REFL row of D8 (no ring normal form);  the automated
  WITNESS_HINT row (O17 closed).  The only change outside the examples,
  the guest tables and the docs is the `family Circ` line in
  domain/domain.lang (s2).  `git diff --numstat c8c1d5e -- src` is empty.
- Docs:  SPEC.md 2.9 (new in s1;  s2 to s4 add to it), 4 (s6:  the
  circuit quotient) and 9 (s5:  the O4 record, O17 closed, O19 new;  s6:
  the O18 ruling, O20 new).  docs/PLAN.md:  the M7 row DONE 2026-10-18
  with the slice commits.  docs/STATUS.md:  this section.

#### Mutant roll-up

The mutants of the `Mutants` items of s1 to s5 (s1 3, s2 3, s3 3, s4 3,
s5 2:  14).  The s5 review found each killing row of s1 to s4 still in
its table.

| Slice | Mutant | Verdict | Killed by |
|---|---|---|---|
| s1 | 1. fold_hit_rule does not test a path case (check.c hit_case_type) | KILLED | check geqc-case.glue HIT_CASE;  examples quot, int, otp, transportProp, trunc (HIT_CASE) |
| s1 | 2. rule_elim does not test a path case (check.c elim_case_type) | KILLED | check elim-geqc-case.glue ELIM_CASE;  int negNeg, otp ret, transportProp repElimG (ELIM_CASE);  3 otp eval rows |
| s1 | 3. fold_hit_rule does not test the `squashc` case (check.c hit_case_type) | KILLED | check squashc-field.glue HIT_CASE;  quot quotRec, transportProp repFold (HIT_CASE);  4 quot eval rows |
| s2 | 1. the `cadd` and `cmul` cases of `ev` swapped (circSyntax.glue) | KILLED | same-ir circSyntax square (IR DIFF) |
| s2 | 2. `hb * t` to `hb + t` in the `cpow` case (circEval.lang) | KILLED | eval circEval evPow (9, not 27) |
| s2 | 3. the `cpub` and `cpriv` cases swapped (circEval.lang) | KILLED | eval circEval evPub (3) and evPriv (9) |
| s3 | 1. `hl + hr` to `hl` (ring-refl.lang) | KILLED | check ring-refl.lang TYPE_REFL |
| s3 | 2. field_op gives the left operand at `+ 0` (eval.c) | KILLED | check ring-refl.lang TYPE_REFL (the only failing row) |
| s3 | 3. pcon_boundary gives the first branch at i1 (eval.c) | KILLED | examples circRel.lang (HIT_CASE at `evQ`);  same-ir quotEnd0, quotEnd1;  16 examples, 5 native HIT rows |
| s4 | 1. `cpow cpriv 1` to `cpow cpriv 2` (circOpt.glue) | KILLED | examples circOpt.glue TYPE_REFL;  same-ir circOpt square;  refuse circOpt (TYPE_REFL, not WITNESS_FAIL) |
| s4 | 2. the circuit at `cpow cpriv 0` (circPow.glue) | KILLED | same-ir circPow square (IR DIFF) |
| s4 | 3. m3 at `moveSat 4 2 refl` (circTransport.lang) | KILLED | eval circTransport m3 (`<j> 4`) |
| s5 | 1. the hint at 0, no trap (hintTrap.glue) | KILLED | refuse hintTrap WITNESS_HINT (exit 0) |
| s5 | 2. no overflow:  18446744073709551614 (hintTrap.glue) | KILLED | refuse hintTrap on the code (EVAL_FUEL, not WITNESS_HINT) |

- Totals:  14 mutants:  14 KILLED, 0 SURVIVED.  5 mutants are in src/
  (s1 1 to 3 in check.c, s3 2 and 3 in eval.c), 9 are in guest files.  No
  mutant added a row.

#### Review

- (i) No conv rule reads a type.  M7 has no change in src/, thus the M6
  close review (i) holds.  The ring-refl.lang row shows that M7 adds no
  ring conv rule (D8, ruling (i)).
- (ii) No rule `hcomp A i0 u u0 = u0` (D8 of M3):  no change in src/,
  and the hcomp-i0-int.lang and hcomp-i0-circle.lang rows stay.
- (iii) `hcomp` at a HIT stays a canonical value (D3 of M4):  no change
  in src/.  evHcomp (s1) folds such a value with the eliminator `hcomp`
  rule.
- (iv) No IR change:  contracts/src/verifiers did not change in a slice.
  Each new circuit through a quotient has a same-ir row against square
  (8 rows:  5 in s1, circSyntax, circOpt and circPow).
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35,
  HIT 44;  examples 55, parse refusals 18, check refusals 152, evals 276,
  json 37 builds, build refusals 8, circuits 5, same-ir 16, circuit
  refusals 16, satcheck refusals 6;  node regressions 6, printer 2.
  Against s5:  no change (section 10:  none).  Against the M6 close:
  examples +13, check refusals +6, evals +8, json +4, same-ir +8, circuit
  refusals +2.  `make check-clang` exit 0.  `forge build --root contracts
  --sizes` exit 0.  `make e2e` exit 0.  The diff of test/circuit and
  contracts/src/verifiers is empty before and after e2e.
  contracts/test/fixtures restored, then `forge test --root contracts` 44
  passed, 0 failed.
- Open items at the M7 close (SPEC.md 9):  O19 (new in M7:  the gap of
  `CEqv`) and O20 (new in M7:  the indexed binder form of `Circ`, ruling
  (h)) for M8.  O18:  RULED 2026-10-18 (USER), ruling (f).  O17 closed in
  s5.  O3, O12 (c), O13 and O14 stay open (ruling (f));  O15 and O16
  stay open.  Next:  the M8 kickoff (docs/PLAN.md, the M8 row).

## M8

Plan:  docs/M8-PLAN.md (RULED 2026-10-18 (USER):  each recommendation of
section 9, (a) to (h)).  Direction 4:  the indexed binder form (O20), the
specification HIT, the Poseidon hash and the transfer circuit.

### s1

- What changed:  O20, the indexed binder form.  domain/domain.lang:  the
  family `CircV` (`vvar (vvn : Nat)`, `vlit`, `vadd`, `vmul`, `vpow`)
  after `family Circ`, with a comment line.  examples/circVar.lang:  `evV`
  (an `elim` at `CircV`;  the `vpow` case is the Nat fold, as the `cpow`
  case of `ev`), the environments `envSq y x` and `envP a y x` (each an
  `elim` at Nat), `CRelV`, `evQV`, the laws evVPoint, evVSq and optV (each
  `refl`) and the closed defs `evVsq` (9) and `evVidx` (3).
  examples/circVarSq.glue (circuit square through `GQuot CircV CRelV`) and
  examples/circVarPoly.glue (circuit poly:  `a` at index 0, `y` at index 1,
  `x` at index 2).  test/json/circVar.json (2 instances:  evVsq, evVidx).
  test/check/domain-vvar.lang (REFUSE_NAME).  Rows:  2 in
  test/circuit/same-ir.txt, 2 in test/eval/expect.txt, 1 in
  test/check/expect.txt.  SPEC.md:  section 2.10 (new:  Direction 4 (M8),
  **The indexed binder form.**);  the section 4 bullet **Circuit
  quotient** points to `CircV`;  section 9 O20 is closed.  No src/ change.
  This commit includes docs/M8-PLAN.md.
- Probes (real build/langc after `make -B build/langc`):  the name guard
  `rg -w` of the 14 new names gave no hit before the append.  `langc check`
  gives ok for the 3 examples.  `langc eval` gives 9 (evVsq) and 3
  (evVidx).  `langc circuit .. -o` gives an IR byte-identical to
  test/circuit/square.circuit.json and test/circuit/poly.circuit.json
  (poly:  9 rows).  domain-vvar.lang gives REFUSE_NAME (rule R4).
- Mutants (scratch copies, real build/langc;  each `sd -F` pattern had 1
  hit):  (m1) the envSq index swap (`fun k => fun ih => x` to `y`):
  KILLED.  TYPE_REFL at evVSq (the start normalizes to `fieldMul y y`).
  `langc eval` checks the whole file first, thus the eval row `circVar
  evVsq => 9` fails with TYPE_REFL (not with 81), and the examples gate
  also fails.  (m2) `vvar 2` to `vvar 0` in circVarPoly.glue:  KILLED.
  CIRCUIT_UNCONSTRAINED (x is in no constraint), by the same-ir row
  `circVarPoly poly poly` only (the examples gate runs `langc check`, and
  check gives ok).  (m3) the vpow case `hb * t` to `hb + t`:  KILLED.
  TYPE_REFL at optV, by the examples gate and the json build of circVar.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT
  44;  examples 58, parse refusals 18, check refusals 153, evals 278, json
  38 builds, build refusals 8, circuits 5, same-ir 18, circuit refusals
  16, satcheck refusals 6;  node regressions 6, printer 2.  Against the M7
  close:  examples +3, check refusals +1, evals +2, json +1, same-ir +2
  (docs/M8-PLAN.md section 10, row s1).  `make check-clang` exit 0.
  `forge build --root contracts --sizes` exit 0.  `make e2e` exit 0.  The
  diff of test/circuit and contracts/src/verifiers is the 2 same-ir.txt
  rows only, the same before and after e2e.  contracts/test/fixtures
  restored, then `forge test --root contracts` 44 passed, 0 failed.
- Open items:  O20 closed (SPEC.md 9).  O19, O3, O12 (c), O13, O14, O15
  and O16 stay open.  Next:  s2 (the specification HIT in the SqSpec
  form).

### s2

- What changed:  the specification HIT (D2).  domain/domain.lang:  the
  family `SqSpec := smul | spow | slaw <i> [ ~i -> smul, i -> spow ]`
  after `family CircV`, with a comment line.  examples/spec.lang:  `ev`,
  `CRel`, `evQ`, `Sat`, `SatP` and `opt` (as in
  examples/circTransport.lang), the iso defs (as in examples/otp.lang),
  `lawQ`, `interpS` (the fold of `SqSpec` into `GQuot Circ CRel`), the
  laws interpEnd0 and interpEnd1 (each `refl`), `SpLine`, `moveSpec`,
  `idSat` (`isoToEquiv`, both maps `fun p => p`), `SpecSat` (the fold into
  `Type 0` with the law case `ua (idSat y x)`), `UaLine`, the law uaFwd
  (`refl`, uaBeta) and the closed defs `m3s` and `m4s`.
  examples/specSq.glue (circuit square with `evQ y x (interpS (slaw @
  i1)) == y`).  test/json/spec.json (`{"glue-lang":1,"instances":[]}`:
  the closed defs have Path types).  test/check/domain-slaw.lang
  (REFUSE_NAME).  Rows:  1 in test/circuit/same-ir.txt, 2 in
  test/eval/expect.txt, 1 in test/check/expect.txt.  SPEC.md:  section
  2.10 **The specification HIT.** (new paragraph);  the section 4 bullet
  **Circuit quotient** points to `SqSpec`;  section 9 O19 has an M8 s2
  line.  No src/ change.
- Probes (real build/langc after `make -B build/langc`):  the name guard
  `rg -w 'SqSpec|smul|spow|slaw'` over domain, examples, test and src gave
  no hit before the append.  `langc check` gives ok for spec.lang and
  specSq.glue.  `langc eval` gives `<j> 9` for m3s and for m4s.  `langc
  circuit .. -o` gives 3 rows and an IR byte-identical to
  test/circuit/square.circuit.json.  domain-slaw.lang gives REFUSE_NAME
  (rule R4).
- Mutants (scratch copies, real build/langc;  each `sd -F` pattern had 1
  hit):  (m1) the two point cases of interpS swapped:  KILLED.  HIT_CASE
  at interpS (the case for slaw must match its boundary), before
  interpEnd0;  by the examples gate, the json build and the 2 eval rows
  of spec.  (m2) `slaw @ i1` to `slaw @ i0` in specSq.glue:  SURVIVED.
  Both points normalize to `x * x` (the precondition of `opt` by refl),
  thus check gives ok and the IR stays byte-identical.  The mutant is
  equivalent:  no row can see it (the same-ir row compares the IR only).
  (m3) first form, the idSat forward map to `<j> hcomp Field (j \/ ~j)
  (fun k => [ ~j -> p @ i0, j -> p @ i1 ]) (p @ j)`:  check gives ok.
  This mutant is also equivalent:  Field is a closed type, and the hcomp
  rule (hcomp_constant) reduces a constant tube to the base `p @ j`.
  (m3b) the uaFwd right side to the hcomp with the tube `j -> p @ (k \/
  ~k)`:  KILLED.  TYPE_REFL at uaFwd (the end normalizes to the stuck
  hcomp).  (m3c) the same hcomp as the idSat forward map:  KILLED.
  TYPE_REFL at idSat (the iso law `fun q => refl`).  m3b and m3c are
  killed by the examples gate, the json build and the 2 eval rows of
  spec.  Two different terms of `Path Field (x * x) y` do not convert:  no
  proof irrelevance.
- Gates:  `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT
  44;  examples 60, parse refusals 18, check refusals 154, evals 280, json
  39 builds, build refusals 8, circuits 5, same-ir 19, circuit refusals
  16, satcheck refusals 6;  node regressions 6, printer 2.  Against s1:
  examples +2, check refusals +1, evals +2, json +1, same-ir +1
  (docs/M8-PLAN.md section 10, row s2).  `make check-clang` exit 0.
  `forge build --root contracts --sizes` exit 0.  `make e2e` exit 0.  The
  diff of test/circuit and contracts/src/verifiers is the same-ir.txt row
  `specSq square square` only, the same before and after e2e.
  contracts/test/fixtures restored, then `forge test --root contracts` 44
  passed, 0 failed.
- Open items:  O19, O3, O12 (c), O13, O14, O15 and O16 stay open.  Next:
  s3 (poseidon.lang:  the constants, the round defs, wrap, poseidonVec).
  s3 reads the circomlib t = 3 constants, the MDS matrix and the test
  vector from the circomlib source first (ruling (e)).

### s3

The Poseidon permutation (docs/M8-PLAN.md D4 to D6, section 10 row s3,
ruling (e)). No src/ change, no IR change, no new circuit.

What changed:
- examples/poseidon.lang (new): a header comment (the sources, the form,
  t, RF, RP), 195 round constants `rc0` to `rc194` (C[1] of the source)
  and 9 MDS entries `mds00` to `mds22` (M[1][I][J]), each a closed Field
  def with one literal (D5), then `sbox5`, `mdsMix`, `fullRd`, `partRd`,
  `fullStep`, `partStep`, `perm3` (8 full and 57 partial rounds), `wrap`,
  `poseidonVec` and `fullOne`. The state is `Prod Field (Prod Field
  Field)`.
- test/eval/expect.txt: `poseidon wrap => 0`, `poseidon poseidonVec =>
  7853200120776062878684798364095072458815029376092732009249414926327459813530`
  and `poseidon fullOne => pair M00 (pair M10 M20)` (column 0 of M). The
  plan names no third row, thus the third row is one round def at a small
  closed input: `fullOne` is `fullRd 0 0 0 1 0 0`. `langc eval` takes
  arguments only for an entry (EVAL_ARGS), thus fullOne is a closed def.
- test/json/poseidon.json (new): 207 instances (204 constants, wrap,
  poseidonVec and fullOne;  fullOne is a Prod instance).
- SPEC.md section 2.10: the paragraph **The Poseidon hash.** Section 9
  gets no line: s3 opens no question.

Sources (fetched with curl on 2026-10-18;  commits from the GitHub API
`commits/HEAD`):
- circomlibjs, commit 48b3ab37013c5ed21e9ff8a80a5b010795c97094
  (2025-04-03): https://raw.githubusercontent.com/iden3/circomlibjs/main/src/poseidon_reference.js
  (the rounds), .../src/poseidon_constants.js and
  .../src/poseidon_constants.json (C and M;  the two files have the same
  12638 hex values), .../test/poseidon.js (the vector, also as the hex
  value 0x115cc0f5e7d690413df64c6b9662e9cf2a3617f2743245519e19607a4417189a).
- circomlib, commit 35e54ea21da3e8762557234298dbb553c175ea8d
  (2025-04-03): https://raw.githubusercontent.com/iden3/circomlib/master/test/poseidoncircuit.js
  (the t = 3 vector), .../circuits/poseidon.circom and
  .../circuits/poseidon_constants.circom (the optimized form C, S, M, P).

Parameters (from the fetched text, not from D6): t = 3 (the capacity word
0, then 2 inputs), N_ROUNDS_F = 8, N_ROUNDS_P[t - 2] = 57, the S-box
pow5 (x^5), C[1] with 195 values (index r * t + i), M[1] 3 x 3 (word i of
the mix is the sum of M[i][j] * word j). The rounds 0 to 3 and 61 to 64
are full. Each round mixes, also the last. The hash is word 0. D6 is
correct. The vector is the same in 3 places (the circomlib test, the
circomlibjs test, and the hex test value as a decimal).

Form: the plain form (C and M). circomlib poseidon.circom uses the
optimized form;  the two forms give the same hash. POSEIDON_M(3) of
poseidon_constants.circom is the transpose of M[1] of the json file.

Probes: each hex value to decimal with bc;  204 of 204 decimals below r
(src/front/fr.h, 77 digits). Name guard (214 names): one hit, `wrap` is a
C local in src/front/eval.c (not a domain name;  `langc check` gives ok).
`langc check` ok;  each eval 0 s (no EVAL_FUEL, no EVAL_PRINT).

Rows of form (i) (probe circuits in $TMPDIR, `-o`;  one hint for each
S-box output and each state word, one constraint for each hint): a base
circuit `a + b + c == y` 6 rows (k 5);  one full round 78 rows (k 7);  one
partial round 60 rows (k 7);  both 132 rows (k 8). Thus a full round is 72
rows (D4 estimate 48) and a partial round is 54 rows (estimate 46). The
permutation estimate is 8 x 72 + 57 x 54 = 3654 rows (D4: 3006), k 12.

Mutants (scratch copies, 1 line each, `sd -F` hit count 1):
- m1 (one digit of rc100): KILLED by the poseidonVec row.
- m2 (one digit of mds12): KILLED by the poseidonVec row.
- m3 (the partial round S-box on word 1): KILLED by the poseidonVec row.
The wrap and fullOne rows pass for each mutant.

Gates: `make -B check` exit 0 (native circuit 9, path 46, face 35, HIT
44;  examples 61, parse refusals 18, check refusals 154, evals 283, json
40 builds, build refusals 8, circuits 5, same-ir 19, circuit refusals 16,
satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang`
exit 0;  `forge build` exit 0;  `make e2e` exit 0;  the IR diff of
test/circuit and contracts/src/verifiers is empty before and after e2e;
`forge test` 44 passed.

Open items: s4 (examples/poseidon.glue, the circuit `poseidon` with 276
hints of form (i), a golden, an e2e row, a verifier file and its Forge
tests, a WITNESS_FAIL row with a wrong hash) measures the real rows and k.

### s4

The circuit `poseidon` (docs/M8-PLAN.md D4 to D6, section 10 row s4,
ruling (d):  form (i), hints on the current arith rows).  No src/ change,
no new IR form, no verifier generator change.  The IR and the verifier of
each current circuit are byte-identical.

What changed:
- examples/poseidon.glue (new):  8 header comment lines, the 204 constant
  defs and `sbox5` (copies from examples/poseidon.lang with
  `rg -N '^def (rc|mds|sbox5)'`), then one circuit line
  `circuit poseidon public (y : Field) private (a : Field) (b : Field)`
  with 276 hints (`tRkJ` = the S-box outputs, `sRkI` = the state words;
  8 x 6 + 57 x 4) and 277 constraints (one for each hint, then
  `s64k0 == y`).  A runner wrote the circuit line (the round r takes
  rc(3r), rc(3r+1) and rc(3r+2);  the rounds 0 to 3 and 61 to 64 are
  full).
- test/circuit/poseidon.circuit.json (538883 B) and
  poseidon.witness.json (625627 B):  the new goldens.
  test/circuit/expect.txt:  the row `poseidon poseidon` at y = the vector
  of test/eval/expect.txt, a = 1, b = 2.  test/circuit/refuse.txt:  a
  WITNESS_FAIL row at y + 1.
- contracts/src/verifiers/poseidon.sol (new, the current generator) and
  contracts/test/fixtures/poseidon.proof.json (new).
- contracts/test/Verifiers.t.sol:  the poseidon verifier import and the
  contract PoseidonVerifierTest (the pattern of the current tests, and
  testThePublicInputIsTheHash):  11 tests.
- contracts/script/DeployVerifier.s.sol (ruling below):  no verifier
  imports.  `run(string)` gets the creation code with
  `VM.getCode(string.concat(name, ".sol:Halo2Verifier"))` and deploys it
  with a CREATE (`createFrom`) under `startBroadcast`;  a zero address
  stops the script.  The interface does not change:  the e2e
  `forge script` loop and the README deploy lines are the same.
  contracts/test/Vm.sol:  `getCode`.  No foundry.toml change (`getCode`
  needs no fs_permissions).
- SPEC.md 2.10 (**The Poseidon hash.**):  the measured circuit line
  replaces the estimate.
- contracts/README.md:  the 3 circuit lists name poseidon ("five
  files"), the numbers row `| poseidon | 5,801 | 1,305,911 | 277,871 |`
  (runtime size = gas report Deployment Size 5833 - 32;  verifyProof =
  the gas report Max) and the script line (getCode, no imports).

Ruling (USER 2026-10-18, s4 turn 2):  getCode + create, option 1 of 4
(getCode + create / one script for each verifier / change the size gate /
stop).  Cause:  the first gate run gave `forge build --root contracts
--sizes` exit 1:  DeployVerifier runtime 30,380 B, margin -5,804 against
EIP-170 (24,576 B).  The old script imported each verifier and held its
creation code (approximately 5.8 KB each);  with 4 verifiers it was
approximately 29 B below the limit.  After the ruling:  DeployVerifier
runtime 1,445 B.  A new verifier (s5) adds no size to the script.

Probes ($TMPDIR, `-o`):  `langc check` ok;  `langc circuit`:  k 12, 3644
rows (s3 estimate 3654), advice 3, fixed 7, instance 1, one gate arith;
`langc witness` at the vector:  exit 0, `satcheck` ok;  at y + 1:
WITNESS_FAIL at 215:34520.  Prover (debug glue-halo2, k 12, sandbox off
for `/usr/bin/time -l`):  keygen 1.51 s, max RSS 25149440 B;  prove
3.20 s, max RSS 35340288 B (the limit is 4 GB).  Two `solidity` runs give
the same verifier file.

Mutants (scratch copies, `sd -F` hit count 1 each):
- m1a (the s64k2 constraint removed):  CIRCUIT_UNCONSTRAINED at check,
  circuit and witness:  KILLED by the examples row and the circuits row.
- m1b (the t31k0 S-box constraint removed):  check ok, 3630 rows, the IR
  is not the golden, witness exit 0 (`langc witness` does not see a free
  hint), satcheck of the golden witness gives SAT_IR:  KILLED by the
  circuits row (the IR golden).
- m2 (the last digit of rc100):  WITNESS_FAIL at `s64k0 == y`:  KILLED by
  the circuits row.
- m3 (`mds12 * t64k2 == s64k1` to `mds12 * t64k1 == s64k1`):
  WITNESS_FAIL at 215:34406:  KILLED by the circuits row.

Gates (after the ruling;  the slice files were staged before the gates):
`make -B check` exit 0 (native circuit 9, path 46, face 35, HIT 44;
examples 62, parse refusals 18, check refusals 154, evals 283, json 40
builds, build refusals 8, circuits 6, same-ir 19, circuit refusals 17,
satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang`
exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0
(6 rows, 27 s);  the IR diff of test/circuit and contracts/src/verifiers
is empty before and after e2e;  `forge test` 55 passed (44 + 11).  The
delta against s3 is the section 10 row s4:  examples +1, circuits +1,
circuit refusals +1, e2e +1 row, forge tests +11.

Open items:  s5 (`TransferSpec` in domain/domain.lang;  transfer.glue,
the circuit `transfer`, its golden, e2e row, verifier file and Forge
tests;  a WITNESS_FAIL row with a wrong nullifier).  The script reads each
artifact by name, thus the transfer verifier changes only the doc comment
list of DeployVerifier.

### s5

The circuit `transfer` and the family `TransferSpec` (docs/M8-PLAN.md
D7 and D8, section 10 row s5), and ruling (a):  a JSON input limit for
`langc satcheck`.  No new IR form, no verifier generator change.  The IR
and the verifier of each current circuit are byte-identical.

Ruling (a) (USER 2026-10-18, s5 turn 2):  `langc satcheck` reads its 2
JSON files (the circuit and the witness) with a separate limit
JSON_MAX_BYTES (16 MiB);  SOURCE_MAX_BYTES (1 MiB) stays the limit for
each source file.  Cause:  the s5 turn 1 probe gave IO_SIZE at
satcheck:  the transfer circuit JSON is 1,080,055 B and its witness is
1,251,199 B (larger than 1 MiB).  The circuits gate row (test/gate.sh
line 168) and e2e (Makefile line 75) run satcheck on each row, thus both
gates would fail.  The value 16 MiB:  a later Merkle circuit of about 21
permutations gives about 13 MB of witness.  IO_SIZE stays the code above
the limit (no new check code).

What changed:
- src/front/front.h:  `#define JSON_MAX_BYTES ((size_t)1 << 24)` after
  SOURCE_MAX_BYTES, with a 2-line comment.  src/main.c:  `read_source`
  takes `size_t max_bytes`;  run_satcheck passes JSON_MAX_BYTES for the
  circuit and the witness;  the program read passes SOURCE_MAX_BYTES.
  11 insertions, 7 deletions;  `make -B build/langc` exit 0, 0 warnings.
  This is the only src/ change of s5.
- domain/domain.lang:  after SqSpec, a blank line, 4 comment lines and
  the family `TransferSpec`:  the points note, nullifier and commitment;
  the law `noDoubleSpend` from nullifier (i0) to note (i1) and the law
  `balanceCons` from note (i0) to commitment (i1).
- test/check/domain-noDoubleSpend.lang (new) and test/check/expect.txt
  line 155:  `def noDoubleSpend` gives REFUSE_NAME (rule R4).
- examples/transfer.glue (new, 217 lines, 93,792 B):  10 header comment
  lines, the 206 defs of examples/poseidon.glue (an rg copy, identical),
  then one circuit line `circuit transfer public (nf : Field) (cm :
  Field) private (v : Field) (sk : Field) (rho : Field) (rho2 : Field)`
  with 552 hints (the prefix n for the nullifier hash, c for the
  commitment hash) and 554 constraints (one for each hint, then
  `ns64k0 == nf` and `cs64k0 == cm`).  The nullifier hash takes
  (0, sk, rho) and the commitment hash takes (0, v, rho2):  D7
  `nf == H(sk, rho)` and `cm == H(v, rho2)`.
- test/circuit/transfer.circuit.json (1,080,055 B) and
  transfer.witness.json (1,251,199 B):  the new goldens.
  test/circuit/expect.txt:  the row `transfer transfer` at sk 1, rho 2,
  v 3, rho2 4 (nf = the s3 vector of test/eval/expect.txt;  cm =
  14763215145315200506921711489642608356394854266165572616578112107564877678998).
  test/circuit/refuse.txt:  a WITNESS_FAIL row at nf + 1.
- contracts/src/verifiers/transfer.sol (new, the current generator, 923
  lines) and contracts/test/fixtures/transfer.proof.json (new, 6928 B,
  from `make e2e`).
- contracts/test/Verifiers.t.sol:  the transfer verifier import after
  the square import and the contract TransferVerifierTest (the pattern
  of the current tests, and testThePublicInputsAreTheHashes:  2
  instances, instances[0] = nf, instances[1] = cm):  11 tests.
- contracts/script/DeployVerifier.s.sol:  the doc comment list names
  transfer.  No code change (the script reads each artifact by name).
- SPEC.md:  **The transfer circuit.** (line 1090), a paragraph on the
  input limits (1 MiB for a source file, 16 MiB for each JSON file of
  satcheck, IO_SIZE above the limit) and **The transfer specification.**
  (line 1116, the family in a code block).
- contracts/README.md:  the 3 circuit lists name transfer ("six files")
  and the numbers row `| transfer | 5,802 | 1,306,127 | 278,592 |`
  (runtime size = gas report Deployment Size 5834 - 32;  verifyProof =
  the gas report Max).

Probes ($TMPDIR, `-o`):  name guard (examples, test, src, domain):
TransferSpec, note, nullifier, commitment, noDoubleSpend, balanceCons,
transfer, nf, cm, sk, rho, rho2 and the hint names `[nc][ts]Rk J`:  0
files each.  The family on a scratch copy:  build ok, `noDoubleSpend @
i1` gives note, 0 examples with a different check result.  `langc
check` ok;  `langc circuit`:  k 13, 7288 rows (D7 estimate:  about 6000
rows), advice 3, fixed 7, instance 1, public 2, one gate arith;  `langc
witness` exit 0, satcheck ok;  at nf + 1:  WITNESS_FAIL at 217:72780.
Limit probes (after the src change):  a source file of 1,100,027 B gives
IO_SIZE at `check` and `eval` (larger than 1048576 bytes);  a JSON file
of 17,000,000 B gives IO_SIZE at satcheck as the circuit and as the
witness (larger than 16777216 bytes);  satcheck of transfer and of the
5 current goldens:  ok.  Prover (debug glue-halo2, k 13, sandbox off for
`/usr/bin/time -l`):  probe keygen 3.21 s, max RSS 43,368,448 B;  prove
4.52 s, max RSS 59,736,064 B;  verify ok (3.26 s);  gate run:  keygen
2.61 s, max RSS 44,630,016 B;  prove 4.88 s, max RSS 62,980,096 B (the
limit is 4 GB).  Two `solidity` runs give the same verifier file.

Mutants (scratch copies;  `make -B` for each rebuild):
- m1 (the constraint of the commitment hint cs30k1 removed, 553
  constraints):  check ok, circuit exit 0, the IR is not the golden,
  witness exit 0 (a free hint):  KILLED by the circuits row (the IR
  golden).
- m2 (`sbox5 (sk + rc1)` to `sbox5 (rho + rc1)`, 2 hits:  the hint
  nt0k1 and its constraint):  CIRCUIT_UNCONSTRAINED at check, circuit
  and witness (sk is in no constraint):  KILLED by the examples row (and
  the circuits row).
- m2b (`sbox5 (sk + rc1)` to `sbox5 (sk + rc2)`, 2 hits):  check ok, the
  IR is not the golden, WITNESS_FAIL at 217:72780 at the D7 inputs:
  KILLED by the circuits row (the IR golden and the witness).
- m3 (`| noDoubleSpend <i>` to `| noDoubleSpent <i>` in
  domain/domain.lang, 1 hit, build ok):  `def noDoubleSpend` gives ok:
  KILLED by the check refusal row domain-noDoubleSpend.lang.
- m4 (`JSON_MAX_BYTES, &` to `SOURCE_MAX_BYTES, &` in src/main.c, 2
  hits, build ok):  the circuits row gives `FAIL satcheck transfer`
  (IO_SIZE, larger than 1048576 bytes);  the poseidon row ok:  KILLED by
  the circuits row.

Gates (the slice files were staged before the gates):  `make -B check`
exit 0 (native circuit 9, path 46, face 35, HIT 44;  examples 63, parse
refusals 18, check refusals 155, evals 283, json 40 builds, build
refusals 8, circuits 7, same-ir 19, circuit refusals 18, satcheck
refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;
`forge build --root contracts --sizes` exit 0 (DeployVerifier 1,445 B);
`make e2e` exit 0 (7 rows, 30 s);  the IR diff of test/circuit and
contracts/src/verifiers is the same before and after e2e (empty);
`forge test` 66 passed (55 + 11).  The delta against s4 is the section
10 row s5:  examples +1, circuits +1, circuit refusals +1, check
refusals +1, e2e +1 row, forge tests +11.

Open items:  s6 (examples/transferSpec.lang:  transferLaw, the transport
of one TransferSpec law along `ua` to a `Sat` proof of the transfer
circuit (D8);  the eval row;  the s1 to s5 mutant review).

### s6

The example examples/transferSpec.lang (docs/M8-PLAN.md section 4 row
transferLaw, D8 and section 10 row s6):  the transport of the
TransferSpec law `noDoubleSpend` along `ua` to a satisfaction proof of
the transfer circuit, its eval row, and the s1 to s5 mutant review.  No
src/ change, no domain change, no new IR form, no new golden and no
verifier change.

What changed:
- examples/transferSpec.lang (new, 259 lines, 30,628 B):  the Poseidon
  defs of examples/poseidon.lang without wrap, poseidonVec and fullOne
  (211 defs, an `rg -N '^def '` copy), the 6 iso defs of
  examples/spec.lang (fill0, fill2, sq, sq1, lemIso, isoToEquiv), then
  20 new defs (237 defs in all).  pos2 is the hash of the circuit
  (`first (perm3 (pair 0 (pair a b)))`).  `Sat2 a b nf cm` is `Prod
  (Path Field a nf) (Path Field b cm)`:  the satisfaction type of the 2
  hash constraints (`a` is the nullifier hash, `b` is the commitment
  hash).  In NullSat the nullifier path starts at nf;  in CommSat the
  commitment path starts at cm.  `TSat nf cm v sk rho rho2` is `Sat2
  (pos2 sk rho) (pos2 v rho2) nf cm`.  nullTo, toNull, idNull and
  commTo, toComm, idComm are the 2 isos (the path inverse on one
  component, the 2 laws by refl) and their `Equiv` (isoToEquiv).
  TransferSat is the fold of TransferSpec into `Type 0` (the cases in
  declaration order:  note, nullifier, commitment, `ua (idNull ..)`,
  `ua (idComm ..)`).  NdsLine is `<i> TransferSat a b nf cm
  (noDoubleSpend @ i)`, a path from NullSat to Sat2;  transferLaw is
  `transport` along NdsLine;  transferFwd:  transferLaw is the forward
  map of idNull (uaBeta, refl).  nfD7 and cmD7 are `pos2 1 2` and `pos2
  3 4`;  nfD7Vec and cmD7Vec are refl against the 2 values of
  test/circuit/expect.txt line 7;  transferD7 is a proof of `TSat nfD7
  cmD7 3 1 2 4`:  `transferLaw (pos2 1 2) (pos2 3 4) nfD7 cmD7 (pair
  refl refl)`.
- test/eval/expect.txt line 283:  the row `transferSpec transferD7`
  (a pair of 2 constant paths at nf and cm of the D7 inputs, the values
  of test/circuit/expect.txt line 7).
- test/json/transferSpec.json (new, 24,935 B, from `langc build`):  207
  instances:  rc0 to rc194, mds00 to mds22, nfD7 and cmD7 (Field), and
  transferD7 (a closed def of a Prod of Path types is an instance;  the
  Path defs nfD7Vec and cmD7Vec are not).
- SPEC.md:  one paragraph at the end of section 2 (after `types (slice
  s6).`, before `## 3. Type formers`):  the example, Sat2, TSat, NullSat
  and CommSat, the isos, transferLaw, the eval row, the hash values as
  arguments and the EVAL_FUEL limit.

Probes (s6 turn 1, ~/Documents/glue-lang-M8-s6-probe/, the real
build/langc, no domain change):  the name guard of the new def names:
ok (no domain name).  v1 (s6probe.sh:  the hash is an argument `h` of
the Sat types, `h sk rho` in the types):  check 0.57 s, but `eval
transferD7` 31.7 s and `build` 42.2 s (the same output):  a hash in a
type family is computed again at each step of the Glue transport.  v2
(s6probe2.sh, the installed design:  the hash values are arguments):
check 0.27 s, eval 0.36 s, build 0.36 s (calibration:  eval
poseidonVec 0.07 s).  A law for all inputs with pos2 in its type
(`transferSat : .. -> NullSat (pos2 sk rho) (pos2 v rho2) nf cm -> TSat
nf cm v sk rho rho2`) gives EVAL_FUEL (20,000,000 steps, 2.1 s):  it is
not in the example (open item).  Install (s6-b):  `langc check` ok,
`langc eval transferD7` gives the probe output and `langc build` gives
the probe bytes (cmp).

Mutants (scratch copies of the v2 draft, `sd -F` 1 hit each, the real
build/langc):  each mutant stops `langc check`, thus the examples row,
the json row and the eval row `transferSpec transferD7` kill it.
- m1 (`(noDoubleSpend @ i)` to `(balanceCons @ i)` in NdsLine):
  PATH_BOUNDARY at NdsLine:  KILLED.
- m1b (the 2 path cases of TransferSat swapped):  HIT_CASE at
  TransferSat:  KILLED.
- m1c (the point cases note and nullifier swapped):  HIT_CASE at
  TransferSat:  KILLED.
- m2 (cmD7 `pos2 3 4` to `pos2 4 3`):  TYPE_REFL at cmD7Vec:  KILLED.
- m2b (`(pos2 1 2)` to `(pos2 2 1)` in transferD7):  TYPE_REFL at
  transferD7:  KILLED.
- m3 (the path inverse of nullTo removed, `@ ~i` to `@ i`):
  PATH_BOUNDARY at nullTo:  KILLED.

The s1 to s5 mutant review (`### s1` to `### s5` of `## M8`):  20
mutants, 18 KILLED, 2 EQUIVALENT;  no new gate row, thus the delta does
not change.

| slice | mutant | result | gate row |
|---|---|---|---|
| s1 | m1 envSq index swap | KILLED (TYPE_REFL at evVSq) | examples, eval circVar evVsq |
| s1 | m2 `vvar 2` to `vvar 0` (circVarPoly.glue) | KILLED (CIRCUIT_UNCONSTRAINED) | same-ir circVarPoly only |
| s1 | m3 vpow `hb * t` to `hb + t` | KILLED (TYPE_REFL at optV) | examples, json circVar |
| s2 | m1 interpS point cases swapped | KILLED (HIT_CASE at interpS) | examples, json, 2 spec eval rows |
| s2 | m2 `slaw @ i1` to `slaw @ i0` (specSq.glue) | SURVIVED, EQUIVALENT:  both points normalize to x * x, one IR | none can see it |
| s2 | m3 idSat forward map to a hcomp with constant tubes | EQUIVALENT:  hcomp_constant reduces it to `p @ j` (Field is a closed type) | none can see it |
| s2 | m3b uaFwd tube `j -> p @ (k \/ ~k)` | KILLED (TYPE_REFL at uaFwd) | examples, json, 2 spec eval rows |
| s2 | m3c the m3 hcomp in idSat | KILLED (TYPE_REFL at idSat) | examples, json, 2 spec eval rows |
| s3 | m1 one digit of rc100 | KILLED | eval poseidon poseidonVec only |
| s3 | m2 one digit of mds12 | KILLED | eval poseidonVec only |
| s3 | m3 partial round S-box on word 1 | KILLED | eval poseidonVec only |
| s4 | m1a s64k2 constraint removed | KILLED (CIRCUIT_UNCONSTRAINED) | examples, circuits |
| s4 | m1b t31k0 S-box constraint removed | KILLED (IR golden;  witness exit 0, satcheck SAT_IR) | circuits only |
| s4 | m2 last digit of rc100 | KILLED (WITNESS_FAIL) | circuits |
| s4 | m3 `mds12 * t64k2` to `mds12 * t64k1` | KILLED (WITNESS_FAIL at 215:34406) | circuits |
| s5 | m1 cs30k1 constraint removed | KILLED (IR golden;  a free hint) | circuits only |
| s5 | m2 `sbox5 (sk + rc1)` to `sbox5 (rho + rc1)` | KILLED (CIRCUIT_UNCONSTRAINED) | examples, circuits |
| s5 | m2b `sk + rc1` to `sk + rc2` | KILLED (IR golden, WITNESS_FAIL at 217:72780) | circuits |
| s5 | m3 domain `noDoubleSpend` to `noDoubleSpent` | KILLED (`def noDoubleSpend` gives ok) | check refusal domain-noDoubleSpend.lang |
| s5 | m4 `JSON_MAX_BYTES, &` to `SOURCE_MAX_BYTES, &` (src/main.c) | KILLED (IO_SIZE at satcheck transfer) | circuits |

Review notes:  the 2 EQUIVALENT mutants change no normal form and no
IR, thus no gate row can kill them (a reason, not a new row).  6
mutants have one killing row only (s1 m2, s3 m1 to m3, s4 m1b, s5 m1):
the free-hint rows (s4 m1b, s5 m1) depend on the IR golden, because
`langc witness` does not see a free hint.

Gates (the slice files were staged before the gates):  `make -B check`
exit 0 (native circuit 9, path 46, face 35, HIT 44;  examples 64, parse
refusals 18, check refusals 155, evals 284, json 41 builds, build
refusals 8, circuits 7, same-ir 19, circuit refusals 18, satcheck
refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;
`forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (7
rows, 66 s);  the IR diff of test/circuit and
contracts/src/verifiers is empty before and after e2e;  `forge test` 66
passed.  The delta against s5 is the section 10 row s6:  examples +1,
evals +1, json +1.

Open items:  s7 (the M8 close:  `### s7: close`, the PLAN M8 row DONE,
SPEC.md 4 and 9, the mutant roll-up, the M9 kickoff prompt).  The hash
of open inputs:  a law for all inputs with pos2 in its type gives
EVAL_FUEL (20,000,000 steps);  the s6 law takes the hash values as
arguments.

### s7: close (2026-10-18)

- Commits:  s1 0b82a04 (with the plan docs/M8-PLAN.md:  0b82a04);  s2
  c6d952f;  s3 5062dda;  s4 cded21d;  s5 0224aee;  s6 dcbac52.  s7
  changes docs only:  no change in src/, in domain/domain.lang, in test/
  and in examples/.
- The slices:
  - s1 0b82a04:  O20 closed:  the family `CircV` (the binder `vvar n`,
    an environment `Nat -> Field`), circVar.lang, circVarSq.glue and
    circVarPoly.glue (same-ir against square and poly), the REFUSE_NAME
    row.
  - s2 c6d952f:  the specification HIT `SqSpec`, its fold `interpS` into
    `GQuot Circ CRel`, the fold into `Type 0` with `ua (idSat y x)`
    (uaFwd by refl), spec.lang and specSq.glue.
  - s3 5062dda:  examples/poseidon.lang:  circomlib BN254 Poseidon (t =
    3, 8 full and 57 partial rounds), the constants and the test vector
    from the circomlib source, the row count of form (i).
  - s4 cded21d:  the circuit `poseidon` (3644 rows, k 12), its golden,
    e2e row, verifier and 11 Forge tests, the WITNESS_FAIL row;  the
    USER ruling on contracts/script/DeployVerifier.s.sol (getCode and
    CREATE, no imports, 1,445 B).
  - s5 0224aee:  the family `TransferSpec`, the circuit `transfer` (7288
    rows, k 13), its golden, e2e row, verifier and 11 Forge tests, the
    WITNESS_FAIL row and the REFUSE_NAME row;  ruling (a) of s5 (USER):
    `langc satcheck` reads its JSON files with JSON_MAX_BYTES (16 MiB).
  - s6 dcbac52:  examples/transferSpec.lang:  the transport of the law
    `noDoubleSpend` along `ua` to a satisfaction proof of `transfer`;
    the s1 to s5 mutant review.
- What M8 added outside the examples, the guest tables and the docs:
  domain/domain.lang +14 lines (`CircV` s1, `SqSpec` s2, `TransferSpec`
  s5);  src/main.c and src/front/front.h (s5, the satcheck input limit
  only);  contracts/src/verifiers/poseidon.sol (s4) and transfer.sol
  (s5);  contracts/script/DeployVerifier.s.sol (s4).  No new IR form, no
  verifier generator change and no conv rule.
- Docs:  SPEC.md 2.10 (new in s1;  s2 to s6 add to it), 4 (s7:
  Direction 4) and 9 (s1:  O20 closed;  s2:  the O19 note;  s7:  O21
  new).  docs/PLAN.md:  the M8 row DONE 2026-10-18 with the slice
  commits.  docs/STATUS.md:  this section.

#### Rulings

The USER ruled each recommendation of docs/M8-PLAN.md section 9 on
2026-10-18.

| Ruling | Content | Where it landed |
|---|---|---|
| (a) | the OTPSpec form (points, path constructors between points) | s2 `SqSpec`, s5 `TransferSpec`;  no law needed a field at a path constructor |
| (b) | the satisfaction type at a representative:  D2 (b) with `ua`, D2 (a) for the circuit side | s2 (`ua (idSat y x)`, uaFwd), s6 (`ua (idNull ..)`, `ua (idComm ..)`, transferLaw) |
| (c) | the new family `CircV` | s1 (O20 closed) |
| (d) | Poseidon form (i):  hints on the current arith rows | s3 (the round defs), s4 and s5 (no IR form, no verifier generator change);  forms (ii) and (iii) go to M9 or later |
| (e) | circomlib BN254 Poseidon, t = 3, 8 full and 57 partial rounds, the D6 vector | s3 (checked against the circomlib source;  the URL is in `### s3`) |
| (f) | D7:  one input note, one output note, `nf` and `cm` | s5;  Merkle membership goes to M9 or later |
| (g) | D10:  O20 closes in s1;  O19, O3, O12 (c), O13, O14 and O15 stay open | s1 (O20), SPEC.md 9;  s7 adds O21 |
| (h) | slices s1 to s7 | s1 to s7 (s4, s5 and s6 each used more than one session) |

Two more USER rulings came from the slices:  the s4 ruling
(DeployVerifier by getCode and CREATE:  importing 5 verifiers made the
old script exceed EIP-170) and ruling (a) of s5 (JSON_MAX_BYTES for
satcheck:  the transfer circuit JSON is 1,080,055 B, over SOURCE_MAX_BYTES).

#### Mutant roll-up

The mutants of the `Mutants` items of s1 to s6.  The table of each
mutant of s1 to s5 is in `### s6` (the s1 to s5 review);  the s6 mutants
are in the `### s6` Mutants item.

| Slice | Mutants | KILLED | EQUIVALENT | SURVIVED |
|---|---|---|---|---|
| s1 | 3 | 3 | 0 | 0 |
| s2 | 5 | 3 | 2 | 0 |
| s3 | 3 | 3 | 0 | 0 |
| s4 | 4 | 4 | 0 | 0 |
| s5 | 5 | 5 | 0 | 0 |
| s6 | 6 | 6 | 0 | 0 |
| M8 | 26 | 24 | 2 | 0 |

- Totals:  26 mutants:  24 KILLED, 2 EQUIVALENT (s2 m2 and s2 m3:  they
  change no normal form and no IR, thus no gate row can see them), 0
  SURVIVED that a gate row can see.  1 mutant is in src/ (s5 m4,
  src/main.c), 1 is in domain/domain.lang (s5 m3), 24 are in guest
  files.  6 mutants have one killing row only (s1 m2, s3 m1 to m3, s4
  m1b, s5 m1).  No mutant added a row.

#### Review

- (i) No conv rule reads a type.  The only src/ change of M8 is the
  satcheck input limit (s5):  no change in the checker, the evaluator or
  the conv rules, thus the M7 close review (i) holds.
- (ii) No rule `hcomp A i0 u u0 = u0` (D8 of M3):  no change in the
  evaluator, and the hcomp-i0-int.lang and hcomp-i0-circle.lang rows
  stay.  s2 m3 shows the closed-type rule hcomp_constant at Field (a
  hcomp with constant tubes), not an i0 rule.
- (iii) `hcomp` at a HIT stays a canonical value (D3 of M4):  no change
  in the evaluator.
- (iv) No IR change:  M8 adds no IR form (ruling (d)).  The IR and the
  verifier of each M7 circuit stayed byte-identical (the IR check after
  each e2e).  The new circuits `poseidon` and `transfer` each added a
  golden and a verifier file.  3 new same-ir rows (circVarSq,
  circVarPoly, specSq).
- Gates (s7, the s7 files staged first):  `make -B check` exit 0:
  native circuit 9, path 46, face 35, HIT 44;  examples 64, parse
  refusals 18, check refusals 155, evals 284, json 41 builds, build
  refusals 8, circuits 7, same-ir 19, circuit refusals 18, satcheck
  refusals 6;  node regressions 6, printer 2.  Against s6:  no change
  (section 10:  none).  Against the M7 close:  examples +9, check
  refusals +3, evals +8, json +4, circuits +2, same-ir +3, circuit
  refusals +2.  `make check-clang` exit 0.  `forge build --root
  contracts --sizes` exit 0 (DeployVerifier 1,445 B).  `make e2e` exit 0
  (7 rows;  5 at the M7 close).  The diff of test/circuit and
  contracts/src/verifiers is empty before and after e2e.
  contracts/test/fixtures restored, then `forge test --root contracts`
  66 passed (44 at the M7 close), 0 failed.
- Open items at the M8 close (SPEC.md 9):  O21 (new in M8:  the hash of
  open inputs;  a law for all inputs with pos2 in its type gives
  EVAL_FUEL, 20,000,000 steps;  the s6 law takes the hash values as
  arguments).  The transport of `balanceCons`:  its iso (idComm) and its
  fold case (`ua (idComm ..)`) are in examples/transferSpec.lang, but no
  def moves it (O21).  Merkle membership (ruling (f)) and the Poseidon
  forms (ii) and (iii) (ruling (d)) go to M9 or later.  O19, O3, O12
  (c), O13, O14 and O15 stay open (ruling (g));  O16 stays open.  Next:
  the M9 kickoff (docs/PLAN.md, the M9 row).

## M9

Plan:  docs/M9-PLAN.md (RULED 2026-10-18 (USER):  each recommendation of
section 9, (a) to (j)).  The M9 row of
docs/PLAN.md:  a ZK-KYC credential circuit, ceremony SRS ingestion and
deployment to a Telcoin network (the USER runs the broadcast).  From M8:
O21 (SPEC.md 9), Merkle membership (ruling (f) of M8) and the Poseidon
forms (ii) and (iii) (ruling (d) of M8).

### kickoff (2026-10-18)

- What changed:  docs/M9-PLAN.md (new, untracked) and this `## M9`
  section.  No src/, domain/, examples/, test/, halo2/ or contracts/
  change.  Nothing is staged:  docs/M9-PLAN.md and this section go into
  the M9 s1 commit, after the USER rulings (as docs/M8-PLAN.md went into
  the M8 s1 commit).
- Start:  HEAD fa38310 (`M8 s7: close M8`, USER), tree d8ad7f30.  The
  USER reworded 3 lines of `#### Rulings` of the M8 `### s7` before the
  commit, thus the tree is not the staged s7 tree dc57ff1c.  The kickoff
  accepted it.
- Probes:  ~/Documents/glue-lang-M9-kickoff-probe/run.sh and run.out
  (the real build/langc, no new family;  each `langc circuit FILE NAME -o
  OUT` takes 0.05 to 0.11 s).

| Probe | Shape | Result |
|---|---|---|
| (a) kyc1 | 1 Poseidon permutation, `a == want` | 3645 rows, k 12 |
| (a) kyc2 | 2 permutations (the transfer shape), `v == want` | 7289 rows, k 13, public 3 |
| (a) kyc2r | kyc2, `d + want == v`, `range8 d` | 7291 rows, k 13, lookups 1 |
| (b) merkle1 | 1 permutation, 1 bit, 2 selects | 3654 rows, k 12 |
| (b) merkle2 | 2 levels | 7307 rows, k 13 (3653 rows for each level) |
| (c) O21 | the hash as an opaque argument `h` | check 0.41 s;  eval 0.38 to 0.43 s;  no EVAL_FUEL |
| (d) SRS | the setup path (read only) | `keygen --srs FILE` or `--dev-srs`;  largest e2e k 13 |

- (c) O21, the probe-proven form (docs/M9-PLAN.md D4):  `TSatH h nf cm v
  sk rho rho2 = Sat2 (h sk rho) (h v rho2) nf cm` with `h : Field ->
  Field -> Field` opaque, and `transferSatH` (for all `h` and all inputs,
  through `transferLaw`).  `langc check` of o21.lang:  0.41 s
  (examples/transferSpec.lang alone:  0.23 s).  `langc eval transferD7H`
  (h := pos2, the D7 inputs) gives the pair of `transferD7` in 0.42 s
  (transferD7:  0.47 s).  `langc eval transferSatH` gives `fun h => .. =>
  fun p => pair (<j> (first p) @ ~j) (<j> (second p) @ j)` in 0.38 s.  No
  EVAL_FUEL:  a neutral `h` stops the hash in the Glue transport.  The
  transport of `balanceCons`:  `BalLine a b nf cm = <i> TransferSat a b
  nf cm (balanceCons @ i)` from `Sat2` to `CommSat`, then `balLaw` and
  `balD7`:  check 0.36 s, and `balD7` gives the pair `nf`, `cm` in 0.43
  s.  The other orientation (from `CommSat` to `Sat2`) gives
  PATH_BOUNDARY.  Thus both parts of O21 have a form.
- (d) The SRS:  `glue-halo2 keygen --srs FILE` reads a halo2 KZG params
  file (ParamsKZG::read) and downsizes it to the circuit k;  a smaller k
  is refused (halo2/tests/backend.rs, srs_smaller_than_k_is_refused).
  `--dev-srs` is a fixed seed (DEV_SEED, INSECURE).  `make e2e` uses
  `--dev-srs` for each of its 7 rows (Makefile line 76);  the largest k is
  13 (transfer).  prove, verify and solidity read srs.bin from the keys
  dir (halo2/src/backend.rs lines 367 and 438).
  SolidityGenerator::new(&srs, vk, Bdfg21, public) (backend.rs line 349):
  the verifier holds G2 points of the SRS, thus another SRS changes each
  verifier file (D5:  `make e2e` stays on the development SRS).
- Questions for the USER (docs/M9-PLAN.md section 9;  each has a
  recommendation):  (a) the credential format and the attribute check
  (D1:  `cm = pos2 attr sk`, `nf = pos2 sk scope`, `range8 d` with `d +
  min == attr`);  (b) Merkle membership and its depth (kycMerkle in s4,
  depth 4, k 15);  (c) the ceremony and its file (a halo2 KZG params file
  from the Perpetual Powers of Tau ceremony, k >= 15, pinned by SHA-256;
  the USER names the file and its source);  (d) the consumer contract
  (KycGate, D7, in s6);  (e) the deployment target (USER facts:  the
  network, the chain id, the RPC URL, the explorer, the keystore account
  name, the circuits;  the testnet first);  (f) O21 (closed in s1 with
  D4);  (g) the specification HIT `KycSpec` (D3, in s3);  (h) the
  Poseidon forms (ii) and (iii) (not in M9, D8);  (i) O3, O12 (c), O13,
  O14, O15, O16 and O19 (they stay open);  (j) the slices s1 to s7 of
  section 10.
- Gates:  none (no code change).  The M8 close counts stay the baseline
  (~/Documents/glue-lang-M9-COMMON.md).
- Rulings (USER, 2026-10-18):  each recommendation of section 9, (a) to
  (j).  The USER facts of (c) (the ceremony file and its source) and of
  (e) (the network, the chain id, the RPC URL, the explorer and the
  keystore account name) stay open:  s5 and s6 ask for them first.
- Staged:  nothing.  Next:  M9 s1 (this plan;  O21 closed by D4).

### s1

The M9 plan commit and O21 closed (docs/M9-PLAN.md D4, ruling (f);
section 10 row s1).  No src/ change, no domain change, no new IR form,
no new golden and no verifier change.

What changed:
- docs/M9-PLAN.md (new, RULED 2026-10-18) and this `## M9`
  section go into this commit.
- examples/transferSpec.lang (266 lines, 31,881 B, 243 defs):  one
  comment line and the 6 defs of D4, copied from the kickoff probes
  (o21.lang and balF.lang lines 260 to 262).  `TSatH h nf cm v sk rho
  rho2` is `Sat2 (h sk rho) (h v rho2) nf cm`, with the hash `h : Field
  -> Field -> Field` as an argument.  transferSatH is the law of
  noDoubleSpend for all `h` and all inputs (through transferLaw).
  transferD7H is its instance at pos2 with the D7 inputs (3 1 2 4).
  BalLine is `<i> TransferSat a b nf cm (balanceCons @ i)`, a path from
  Sat2 to CommSat.  balLaw is `transport` along BalLine.  balD7 is
  `balLaw (pos2 1 2) (pos2 3 4) nfD7 cmD7 (pair refl refl)`.
- test/eval/expect.txt rows 284 and 285:  `transferSpec transferD7H`
  and `transferSpec balD7`.  Each gives the pair of row 283
  (transferD7).  The output of transferD7H is byte-identical to the
  right side of row 283 (cmp).
- test/check/bal-orientation.lang (new, 18 lines:  the 6 iso defs,
  Sat2, NullSat, CommSat, the 2 isos and their Equiv, TransferSat, and
  BalLine from CommSat to Sat2) and its row 156 `bal-orientation.lang
  PATH_BOUNDARY`.  The first diagnostic:  `PATH_BOUNDARY: BalLine: ..
  at i0 it normalizes to Prod (Path Field a nf) (Path Field b cm), the
  type needs Prod (Path Field a nf) (Path Field cm b)`.  A control file
  with the other orientation (from Sat2 to CommSat) in the same defs
  checks (`ok`).
- test/json/transferSpec.json:  rebuilt, 207 to 209 instances (+2:
  transferD7H and balD7, closed defs of a Prod of Path types).  TSatH,
  transferSatH, BalLine and balLaw are Pi defs, thus not instances.
  The json builds stay 41.
- SPEC.md:  9 O21 is `Closed in M9 (slice s1)` with the D4 form (the M8
  text stays as the record).  One paragraph after the slice s6
  paragraph of **The transfer specification.** (line 1134).  The M8
  milestone text (line 1212) says that O21 is closed in M9 s1.

Probes:  ~/Documents/glue-lang-M9-s1-probe/probe.sh and probe.out (the
real build/langc after `make -B build/langc`).  The name guard (`rg -w`
of the 6 names over domain/, examples/, test/ and SPEC.md, and of the
file name bal-orientation over test/check):  no hit.  The draft:
`langc check` ok (1.17 s);  `langc eval` transferD7H 0.91 s, balD7
0.79 s, transferD7 0.80 s (each time includes the check of the whole
file);  no EVAL_FUEL.

Mutants (~/Documents/glue-lang-M9-s1-probe/install.sh and install.out;
scratch copies, `sd -F` 1 hit each, no binary change):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | `Sat2 (h sk rho)` to `Sat2 (h rho sk)` (TSatH only) | TYPE_MISMATCH at transferSatH | KILLED (examples gate, `langc check`) |
| m2 | `TSatH pos2 nfD7 cmD7 3 1 2 4` to `.. 3 1 4 2` | TYPE_MISMATCH at transferD7H | KILLED (examples gate) |
| m3 | `CommSat (pos2 1 2) ..` to `CommSat (pos2 2 1) ..` (balD7) | TYPE_MISMATCH at balD7 | KILLED (examples gate) |
| m4 | the last digit of the balD7 eval row value | the eval loop of test/gate.sh on a scratch expect:  1 failure (the base:  0) | KILLED (eval row 285) |

The prompt expected TYPE_REFL for m2 and m3.  The real code is
TYPE_MISMATCH (the declared type of the def and the type of its body
do not convert).  KILLED all the same.

Gates (~/Documents/glue-lang-M9-s1-probe/s1gates.sh and gates.out,
sandbox off, on the staged slice files):  `make -B check` exit 0:
native circuit 9, path 46, face 35, HIT 44;  examples 64, parse
refusals 18, check refusals 156, evals 286, json 41 builds, build
refusals 8, circuits 7, same-ir 19, circuit refusals 18, satcheck
refusals 6, `gate: 0 failures`;  node regressions 6, printer 2.  `make
check-clang` exit 0.  `forge build --root contracts --sizes` exit 0
(DeployVerifier 1,445 B).  `make e2e` exit 0 (7 rows, 16 s).  The IR
diff of test/circuit and contracts/src/verifiers:  0 B before and after
e2e.  `git restore contracts/test/fixtures`, then `forge test --root
contracts`:  66 passed.

Open items:  s2 (the credential circuit kyc, D1).  The USER facts of
questions (c) and (e) stay open (ask before s5 and s6).

Staged:  the 8 slice paths.  Next:  M9 s2.

#### Review corrections

- HIGH, D7:  KycGate must authenticate the issuer commitment or root and
  bind the public scope and threshold to its configured policy.  Merely
  verifying a proof and remembering its nullifier accepts a caller's
  self-issued credential and permits a fresh nullifier for each chosen
  scope.  The plan now requires those checks and negative Forge tests.
- MEDIUM, D1:  `range8 d; d + min == attr` is modular.  The guest
  counterexample `min = r - 1`, `attr = 0`, `d = 1` witnesses and
  satchecks successfully.  The claimed integer inequality requires
  `min <= r - 256`;  the consumer configuration must enforce that bound.
  The plan now states the precondition and requires boundary tests.
- MEDIUM, D2 and D8:  depth 16 and depth 20 need k 17, above the
  satchecker's SAT_K_MAX 16.  A circuit document with k 17 gives SAT_IR
  at `k` even with a valid witness.  The plan now separates the file
  estimates from supported gate sizes;  the selected depth stays 4.

No language implementation change was needed.  The six new definitions
check, the open forms of transferSatH and balLaw evaluate, and the JSON
output matches its staged golden.

Review validation used a temporary snapshot of the index:  `make -B
check` (including check-clang), `forge build --root contracts --sizes`,
`make e2e` (all 7 rows), and all 66 Forge tests passed.  The unchanged
Rust backend binary was reused.  The circuit goldens and regenerated
verifiers remain byte-identical.  No CI or validation weakening found.

### s2

The credential circuit `kyc` (docs/M9-PLAN.md D1, ruling (a);  section
10 row s2).  No src/ change, no domain change, no new IR form and no
verifier generator change.  The IR and the verifier of each current
circuit are byte-identical.

What changed:
- examples/kyc.glue (new, 221 lines, 94,128 B):  14 header comment
  lines (D1, the modular caveat, the hint names), the 206 defs of the
  kickoff probe kyc2r.glue (byte-identical:  the Poseidon constants and
  sbox5 of examples/poseidon.lang), then one circuit line `circuit kyc
  public (cm : Field) (nf : Field) (scope : Field) (min : Field)
  private (attr : Field) (sk : Field) (d : Field)` with 552 hints
  (the prefix n for the nullifier hash, c for the commitment hash) and
  556 constraints (one for each hint, then `ns64k0 == nf`,
  `cs64k0 == cm`, `d + min == attr` and `range8 d`).  The nullifier
  hash takes (0, sk, scope) and the commitment hash takes (0, attr,
  sk):  D1 `nf = pos2 sk scope` and `cm = pos2 attr sk`.
- test/circuit/kyc.circuit.json (1,080,977 B) and kyc.witness.json
  (1,251,701 B):  the new goldens (k 13, 7290 rows, advice 3, fixed 7,
  instance 1, public 4).  test/circuit/expect.txt row 8:  `kyc kyc` at
  scope 9, min 18, attr 30, sk 5, d 12 (cm = pos2 30 5 =
  5967774456880520545986641262024216432454924249289189870195314151569515620696,
  nf = pos2 5 9 =
  2046061026757605145648934375530781408765546393983200800628234446163090055163).
  This row is also the e2e row (Makefile:  `done <$(E2E_TABLE)`).
  test/circuit/refuse.txt row 19:  a WITNESS_FAIL row at attr 17 (cm =
  pos2 17 5) with d = r - 1 (the only d with d + 18 == 17):  `range8
  fails`.
- contracts/src/verifiers/kyc.sol (new, the current generator, 988
  lines, 49,890 B) and contracts/test/fixtures/kyc.proof.json (new,
  8,252 B, from `make e2e`;  4 instances in the order cm, nf, scope
  0x9, min 0x12).
- contracts/test/Verifiers.t.sol:  the kyc verifier import after the
  transfer import and the contract KycVerifierTest (the pattern of the
  current tests, and testThePublicInputsAreTheCredential:  4
  instances, cm, nf, 9, 18):  11 tests (254 to 279 lines).
- contracts/script/DeployVerifier.s.sol:  the doc comment list names
  kyc.  No code change (the script reads each artifact by name).
- contracts/README.md:  the 3 circuit lists name kyc ("seven files")
  and the numbers row `| kyc | 6,280 | 1,409,428 | 315,797 |` (runtime
  size = gas report Deployment Size 6312 - 32;  verifyProof = the gas
  report Max of 20 calls).

Probes (~/Documents/glue-lang-M9-s2-probe/probe.sh and probe.out, the
real build/langc, `-o` for each output):  name guard (`rg -w kyc` over
domain/, examples/, test/, contracts/src, contracts/test and the
Makefile):  0 hits;  attr, scope and min are not in domain/domain.lang;
`min` is not a reserved name (a probe file with `def min` checks).
`langc check` ok;  `langc circuit`:  k 13, 7290 rows (section 6
estimate:  about 7291;  the kickoff probe kyc2r had 7291 with a
separate binder for the second hash input:  the shared private binder
sk saves one row), 0.23 s;  `langc witness` at the circuits row 0.31 s,
satcheck ok.  Refuse probes at attr 17, min 18:  d = r - 1 gives
WITNESS_FAIL at 221:72843 (`range8 fails`, the refuse row);  d = 0
gives WITNESS_FAIL at 221:72826 (`the left side is 18, the right side
is 17`).

The modular counterexample (D1, section 8 row s2):  the relation `d +
min == attr` with `range8 d` holds in the field.  It gives the integer
bound `min <= attr <= min + 255` only when `min <= r - 256`.  The
witness at min = r - 1, attr = 0, d = 1 (cm = pos2 0 5) gives exit 0
and satcheck ok:  the circuit accepts it.  The boundary min = r - 256,
attr = 0, d = 256 gives WITNESS_FAIL (`range8 fails: the value is
256`):  the bound holds at r - 256.  The integer predicate is a
precondition of the consumer:  KycGate must refuse a policy with min >
r - 256 (D7, s6).  r is the BN254 scalar modulus of src/front/fr.h
(`rg -o -N '[0-9]{76,78}' src/front/fr.h`;  r - 1 and r - 256 by bc).

Mutants (~/Documents/glue-lang-M9-s2-probe/m1.glue to m4.glue;
scratch copies, `sd -F` at the circuit site, no binary change;  the IR
against kyc.circuit.json, the witness at the circuits row):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | `d + min == attr` to `d + attr == min` (2 hits:  the header comment and the circuit) | IR DIFF;  WITNESS_FAIL (`the left side is 42, the right side is 18`) | KILLED (circuits row:  the IR golden and the witness golden) |
| m2 | `; range8 d` removed (1 hit) | IR DIFF (no lookup);  the circuits row exit 0;  the refuse row inputs exit 0 | KILLED (circuits row:  the IR golden;  refuse row:  WITNESS_FAIL expected, exit 0) |
| m3 | `pos2 sk scope` to `pos2 scope sk` (the nt0k1 and nt0k2 inputs swapped, 1 hit each) | IR SAME (both inputs are private advice:  the IR is symmetric);  WITNESS_FAIL at 221:37317 | KILLED (circuits row:  the witness golden only) |
| m4 | the min binder private (1 hit) | IR DIFF (`"public":3`);  the circuits row exit 0 | KILLED (circuits row:  the IR golden) |

Gates (~/Documents/glue-lang-M9-s2-probe/s2gates.sh and gates.out,
sandbox off, on the staged slice files):  `make -B check` exit 0:
native circuit 9, path 46, face 35, HIT 44;  examples 65, parse
refusals 18, check refusals 156, evals 286, json 41 builds, build
refusals 8, circuits 8, same-ir 19, circuit refusals 19, satcheck
refusals 6, `gate: 0 failures`;  node regressions 6, printer 2.  `make
check-clang` exit 0.  `forge build --root contracts --sizes` exit 0
(one Halo2Verifier row, 5,802 B, the last compiled file, COMMON M8
s4;  the kyc runtime 6,280 B of the gas report is under EIP-170;
DeployVerifier 1,445 B).  `make
e2e` exit 0 (8 rows;  30 s at the first run, 26 s at the gate).  The
IR diff of test/circuit and contracts/src/verifiers:  0 B before and
after e2e (the kyc golden and verifier are staged before the gates;
the verifier is deterministic, the proof is random).  `git restore
contracts/test/fixtures`, then `forge test --root contracts`:  77
passed (66 + 11).

Open items:  s3 (the specification HIT KycSpec, D3).  The USER facts of
questions (c) and (e) stay open (ask before s5 and s6).

Staged:  the 11 slice paths.  Next:  M9 s3.

### s3

The credential specification HIT KycSpec (docs/M9-PLAN.md D3, ruling
(g);  section 10 row s3).  One domain change (the family KycSpec) and
one rebuild of build/langc.  No src/ change, no new IR form, no new
golden and no verifier change.

What changed:
- domain/domain.lang:  the family KycSpec after TransferSpec, with 4
  comment lines.  The points credential, kycNullifier and predicate name
  the representatives of the credential circuit (examples/kyc.glue).
  noReplay is the law from kycNullifier to credential (`<i> [ ~i ->
  kycNullifier, i -> credential ]`), disclosure is the law from
  credential to predicate.  build/langc is rebuilt (`make -B
  build/langc`;  domain/domain.lang is compiled into langc).
- examples/kycSpec.lang (new, 256 lines, 32,640 B, 238 defs):  18 header
  comment lines, the 218 Poseidon and iso defs of
  examples/transferSpec.lang (its lines 23 to 240), and 20 new defs.
  The satisfaction types CredSat, NfSat and PredSat take the hash values
  as arguments (`a` for `h attr sk`, `b` for `h sk scope`) with nf, cm,
  min, attr and d:  CredSat is the triple of `Path Field a cm`, `Path
  Field b nf` and `Path Field (d + min) attr`;  NfSat and PredSat are
  the same triple in another order.  The isos nfTo, toNf, predTo and
  toPred and their Equiv idNf and idPred;  the fold KycSat into Type 0
  through ua (credential to CredSat, kycNullifier to NfSat, predicate to
  PredSat, each law to the ua path);  NrLine is `<i> KycSat a b nf cm
  min attr d (noReplay @ i)`, a path from NfSat to CredSat, and kycLaw
  is `transport` along it;  DiscLine and discLaw are the same for
  disclosure.  CredSatH and kycLawH take the hash `h : Field -> Field ->
  Field` as an argument (D4):  `CredSatH h cm nf scope min attr sk d` is
  `CredSat (h attr sk) (h sk scope) nf cm min attr d`, and kycLawH is the
  law of noReplay for all h and all inputs.  The instances cmK (`pos2 30
  5`), nfK (`pos2 5 9`), kycDH (`CredSatH pos2 cmK nfK 9 18 30 5 12`)
  and discD (`PredSat (pos2 30 5) (pos2 5 9) nfK cmK 18 30 12`) are at
  the values of the circuits row kyc (scope 9, min 18, attr 30, sk 5, d
  12).
- test/eval/expect.txt rows 286 to 289:  `kycSpec cmK`, `kycSpec nfK`,
  `kycSpec kycDH` and `kycSpec discD`.  cmK and nfK are the cm and nf
  values of the circuits row kyc (test/circuit/expect.txt row 8).  kycDH
  and discD each give a pair of constant paths:  `<j> cm`, then the pair
  of `<j> nf` and `<j> 30`.
- test/json/kycSpec.json (new, 25,427 B):  208 instances (the 206
  Poseidon and iso instances of transferSpec.json, then cmK, nfK, kycDH
  and discD;  a closed def of a Prod of Path types is an instance, the Pi
  defs and the Path defs are not).  The json builds go 41 to 42.
- test/check/domain-noReplay.lang (new, 3 lines, as
  domain-noDoubleSpend.lang) and its row 157 `domain-noReplay.lang
  REFUSE_NAME`.  The diagnostic:  `REFUSE_NAME: noReplay: noReplay is a
  domain constructor (rule R4)`.
- SPEC.md:  one paragraph **The credential specification.** after the
  O21 paragraph of **The transfer specification.** (after line 1134,
  before `## 3. Type formers`), with the family in a code block.

#### Deviation

The D3 text names the point `nullifier`.  The domain constructor
namespace is flat:  src/front/check.c name_owner scans the constructors
of each family by name (rule R4).  With `family KycSpec := credential |
nullifier | ..` the build passes, but every langc run gives
`REFUSE_NAME: KycSpec: nullifier is a domain constructor (rule R4)`
(transferSpec.lang, transfer.glue, kyc.glue and the draft are all
refused), because `nullifier` is a constructor of TransferSpec.  The
point is `kycNullifier`.  The USER rules:  keep kycNullifier or name the
point.  The rename cost is the domain line and its comment, the SPEC.md
paragraph, the kycSpec.lang header comment and this section.

Probes:  ~/Documents/glue-lang-M9-s3-probe/probe.sh, probe.out and
baseline.out (the baseline `make -B check` on HEAD, exit 0, the s2
counts;  a scratch copy of the repository with the family appended,
`make -B -C <copy> build/langc` 0.14 s with tcc;  two trials, nullifier
and then kycNullifier, with domain.orig restored before each append).
The name guard (`rg -w` of KycSpec, credential, predicate, noReplay,
disclosure, KycSat, kycLaw and kycSpec over domain/, examples/, test/
and SPEC.md):  only the word credential in the comment of
examples/kyc.glue line 1.  The draft with the copy's langc:  `langc
check` ok (0.54 s);  `langc eval` cmK 0.29 s, nfK 0.29 s, kycDH 1.30 s,
discD 1.12 s (each a check of the whole file and two Poseidon hashes in
the refl checks), kycLawH with open h and inputs 1.91 s, no EVAL_FUEL.
The normal form of kycLawH:  `fun h => fun cm => fun nf => fun scope =>
fun min => fun attr => fun sk => fun d => fun p => pair (<j> (first p) @
j) (pair (<j> (first (second p)) @ ~j) (<j> (second (second p)) @ j))`.
The install (install.sh and install.out):  the real build/langc after
`make -B build/langc`;  test/json/kycSpec.json from the real langc is
byte-identical to the probe json (cmp).

Mutants (install.sh and install.out;  scratch copies of
examples/kycSpec.lang, `sd -F` 1 hit each, the real build/langc, no
binary change):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | `Path (Type 0) (NfSat ..) (CredSat ..)` to `Path (Type 0) (CredSat ..) (NfSat ..)` (NrLine only) | PATH_BOUNDARY at NrLine (`at i0 it normalizes to Prod (Path Field a cm) (Prod (Path Field nf b) (Path Field (fieldAdd d min) attr))`, as bal-orientation.lang) | KILLED (examples gate, `langc check`) |
| m2 | `CredSat (h attr sk)` to `CredSat (h sk attr)` (CredSatH only) | TYPE_MISMATCH at kycLawH (`expected a term of type Prod (Path Field (h sk attr) cm) ..`, found the type with `h attr sk`) | KILLED (examples gate) |
| m3 | `kycDH : CredSatH pos2 cmK nfK ..` to `.. pos2 nfK cmK ..` | TYPE_MISMATCH at kycDH (the expected type holds the two hash values in the swapped order) | KILLED (examples gate) |
| m4 | the last digit of the kycDH eval row 288 (`30))` to `31))`) | the eval loop of test/gate.sh (lines 58 to 75) on a scratch expect:  the base 0 failures (289 evals, 32.2 s), the mutant 1 failure (`FAIL eval kycSpec kycDH`, 25.1 s) | KILLED (eval row 288) |

Gates (~/Documents/glue-lang-M9-s3-probe/s3gates.sh and gates.out,
sandbox off, on the staged slice files):  `make -B check` exit 0:
native circuit 9, path 46, face 35, HIT 44;  examples 66, parse
refusals 18, check refusals 157, evals 290, json 42 builds, build
refusals 8, circuits 8, same-ir 19, circuit refusals 19, satcheck
refusals 6, `gate: 0 failures`;  node regressions 6, printer 2.  `make
check-clang` exit 0.  `forge build --root contracts --sizes` exit 0
(DeployVerifier 1,445 B).  `make e2e` exit 0 (8 rows, 30 s).  The IR
diff of test/circuit and contracts/src/verifiers:  0 B before and after
e2e.  `git restore contracts/test/fixtures`, then `forge test --root
contracts`:  77 passed.

Open items:  the point name (kycNullifier, or a USER name);  s4 (the
Merkle credential circuit kycMerkle, D2, depth 4).  The USER facts of
questions (c) and (e) stay open (ask before s5 and s6).

Staged:  the 8 slice paths.  Next:  M9 s4.

### s4

The Merkle credential circuit `kycMerkle` (docs/M9-PLAN.md D2, ruling
(f) of M8, depth 4 (question (b));  section 10 row s4).  No src/
change, no domain change, no new IR form and no verifier generator
change.  The IR and the verifier of each current circuit are
byte-identical.

What changed:
- examples/kycMerkle.glue (new, 225 lines, 248,048 B):  18 header
  comment lines, the 206 defs of examples/kyc.glue (its lines 15 to
  220, byte-identical:  the Poseidon constants and sbox5), then one
  circuit line `circuit kycMerkle public (root : Field) (nf : Field)
  (scope : Field) (min : Field) private (attr : Field) (sk : Field)
  (d : Field) (b0 : Field) .. (b3 : Field) (s0 : Field) .. (s3 :
  Field)` with 1664 hints and 1672 constraints.  The hints:  the n
  block (the nullifier hash) and the c block (the commitment hash) of
  kyc, then for each level i the selects `li := cur + bi * (si -
  cur)` and `ri := si + bi * (cur - si)` and the cm permutation block
  of kyc with the binders renamed (ct and cs to mit and mis;  the
  inputs li and ri;  cur is cs64k0 at level 0 and m(i-1)s64k0 above).
  The constraints:  the n and c constraints, for each level `bi * bi
  == bi`, the two select equalities and the renamed block, then the
  tail `ns64k0 == nf`, `m3s64k0 == root`, `d + min == attr` and
  `range8 d`.  There is no `cs64k0 == cm`:  the leaf cm is private (a
  public cm links each proof of one holder, D2).
- test/circuit/kycMerkle.circuit.json (3,279,740 B) and
  kycMerkle.witness.json (3,758,915 B):  the new goldens (k 15,
  21,898 rows, advice 3, fixed 7, instance 1, public 4, 1 lookup).
  test/circuit/expect.txt row 9:  `kycMerkle kycMerkle` at the kyc
  values scope 9, min 18, attr 30, sk 5, d 12 with the path bits b0
  0, b1 1, b2 1, b3 0 and the siblings s0 11, s1 22, s2 33, s3 44;
  nf is the kyc value (pos2 5 9, `### s2`) and root is the depth 4
  root over the leaf cm = pos2 30 5
  (~/Documents/glue-lang-M9-s4-probe/vals.txt).  This row is also the
  e2e row.  test/circuit/refuse.txt row 20:  a WITNESS_FAIL row at
  root + 1 with the same private values (the root equality fails at
  225:226406).
- contracts/src/verifiers/kycMerkle.sol (new, the current generator,
  988 lines, 49,890 B:  the size of kyc.sol, the verifier size depends
  on the column count and not on k) and
  contracts/test/fixtures/kycMerkle.proof.json (new, 8,252 B, from
  `make e2e`;  4 instances in the order root, nf, scope 0x9, min
  0x12).
- contracts/test/Verifiers.t.sol:  the kycMerkle verifier import
  after the kyc import and the contract KycMerkleVerifierTest (the
  pattern of the current tests, and
  testThePublicInputsAreTheRootAndTheNullifier:  4 instances, root,
  nf, 9, 18):  11 tests (279 to 306 lines;  8 test contracts).
- contracts/script/DeployVerifier.s.sol:  the doc comment list names
  kycMerkle.  No code change (the script reads each artifact by name).
- contracts/README.md:  the 3 circuit lists name kycMerkle ("eight
  files") and the numbers row `| kycMerkle | 6,222 | 1,396,947 |
  315,922 |` (runtime size = gas report Deployment Size 6254 - 32;
  verifyProof = the gas report Max of 20 calls).

Probes (~/Documents/glue-lang-M9-s4-probe/probe.sh and probe.out, the
real build/langc, `-o` for each output):  name guard (`rg -w -l
kycMerkle` over domain/, examples/, test/, contracts/src,
contracts/test and the Makefile, and the binder names root, b0, s0,
l0, r0 and m0t0k0 against domain/domain.lang):  0 hits.  `langc check`
ok (2.48 s);  `langc circuit`:  k 15, 21,898 rows (D2 estimate:  about
21,903), advice 3, fixed 7, instance 1, 1 lookup (range8), 1.08 s;
the root by `langc eval rootvals.lang h3M` (a closed chain of 5
Poseidon hashes:  the leaf pos2 30 5, then one hash for each level
with the sibling in bit order), 0.98 s;  `langc witness` at the
circuits row 1.58 s;  `langc satcheck` ok (4.97 s);  the wrong root
gives WITNESS_FAIL at 225:226406 (the refuse row).  The sizes are
under the limits:  the source 248,048 B (SOURCE_MAX_BYTES 1 MiB), the
circuit JSON 3,279,740 B and the witness 3,758,915 B (JSON_MAX_BYTES
16 MiB), k 15 (the satchecker accepts k <= 16).

Mutants (~/Documents/glue-lang-M9-s4-probe/m1.glue to m3.glue and the
refuse row;  scratch copies, `sd -F` at the circuit site, no binary
change;  the IR against kycMerkle.circuit.json, the witness at the
circuits row):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | the level 0 select l0 with cur and s0 swapped (2 hits:  the hint and its constraint) | IR DIFF;  WITNESS_FAIL at 225:226398 | KILLED (circuits row:  the IR golden and the witness golden) |
| m2 | ` ; b0 * b0 == b0` removed (1 hit) | IR DIFF (21,897 rows);  the circuits row exit 0 | KILLED (circuits row:  the IR golden) |
| m3 | ` ; m3s64k0 == root` removed (1 hit) | CIRCUIT_UNUSED_PUBLIC at `langc check` (`the public binder root is in no constraint`), not CIRCUIT_UNCONSTRAINED | KILLED (examples gate) |
| m4 | the refuse row:  a wrong public root (root + 1) at the circuits row values | WITNESS_FAIL at 225:226406 | KILLED (circuit refusals row 20) |

Gates (~/Documents/glue-lang-M9-s4-probe/s4gates.sh and gates.out,
sandbox off, on the staged slice files):  `make -B check` exit 0:
native circuit 9, path 46, face 35, HIT 44;  examples 67, parse
refusals 18, check refusals 157, evals 290, json 42 builds, build
refusals 8, circuits 9, same-ir 19, circuit refusals 20, satcheck
refusals 6, `gate: 0 failures`;  node regressions 6, printer 2.  `make
check-clang` exit 0.  `forge build --root contracts --sizes` exit 0
(DeployVerifier 1,445 B).  `make e2e` exit 0 (9 rows;  62 s at the
gate;  the first e2e 78 s under `/usr/bin/time -l` with a cold forge
cache, max RSS 274,350,080 B:  the kycMerkle proof at k 15 is about
48 s of the run).  The IR diff of test/circuit and
contracts/src/verifiers:  0 B before and after e2e (the kycMerkle
golden and verifier are staged before the gates).  `git restore
contracts/test/fixtures`, then `forge test --root contracts`:  88
passed (77 + 11).

Open items:  s5 (the ceremony SRS, D5):  the USER fact of question
(c), the ceremony file and its source, comes before s5.  The USER
fact of question (e) stays open (ask before s6).  The point name
kycNullifier (`### s3`) stays until the USER rules.

Staged:  the 11 slice paths.  Next:  M9 s5.

### s5

The ceremony SRS rule (docs/M9-PLAN.md D5;  section 10 row s5).  The
halo2 back end reads a pinned KZG params file, the Makefile builds the
ceremony keys, proofs and verifiers under build/, and the
documentation names the rule.  No src/ change, no domain change, no
new IR form and no verifier generator change.  The IR and the
development verifier of each current circuit are byte-identical.

The question (c) answer (the USER;  COMMON line 3):  the format is a
halo2 ParamsKZG params file;  the ceremony verifiers stay under
build/ and are not committed.  The file:
~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs, 4,194,564 B,
k 15, SHA-256
4cdb1688ec4457351582cc99a31e073279bb3abe1ee6fe04c401cc9e390ab67b.
The USER ran the curl in the session (a Claude-chosen download host
is denied at the permission gate).  The source is
https://axiom-crypto.s3.amazonaws.com/challenge_0085/kzg_bn254_15.srs,
the Axiom copy of the perpetual powers of tau challenge 0085 in the
halo2 format.

What changed:
- halo2/Cargo.toml:  `sha2 = "0.10"` after hex.  halo2/Cargo.lock:
  the sha2 0.10.9 package block and its dependency line (12
  insertions, 0 deletions, 234 to 235 packages;  digest, cpufeatures,
  block-buffer and crypto-common were in the lock before).
- halo2/src/backend.rs:  `pub struct Digest([u8; 32])` (Clone, Copy,
  Debug, PartialEq, Eq;  `parse` = hex::decode then
  `<[u8; 32]>::try_from`, a wrong length or a non-hex char is
  `Error::Usage("--sha256 HEX needs 64 hex digits")`;  `of` =
  Sha256::digest;  Display = lowercase hex).  `Srs::read_pinned(path,
  k, &pin)`:  fs::read, the digest check BEFORE the parse,
  `Error::Pin { path, expected, actual }` on a mismatch, then
  `ParamsKZG::read` on an io::Cursor.  The k check moved into `fn
  sized(params, k)`, shared by `read` and `read_pinned` (`read` is
  unchanged in behavior).
- halo2/src/error.rs:  the variant `Pin { path: PathBuf, expected:
  Digest, actual: Digest }`, Display `the SRS file {path} has SHA-256
  {actual}, the pin is {expected}`.  The Display match stays
  exhaustive (the only match on Error).
- halo2/src/main.rs:  `SrsSource::Pinned(PathBuf, Digest)`;
  `Flags::value` (a &str accessor;  `path` is value + PathBuf::from)
  and `Flags::need_hex` (value, else `missing NAME HEX`, then
  Digest::parse);  the keygen flag list adds `--sha256`;  the
  `match ()` chain in order:  `--sha256` without `--srs` is
  `Error::Usage("--sha256 HEX needs --srs FILE")`, then the old `give
  one of` arm, then Dev, then Pinned, then File;  the USAGE line
  `keygen --circuit C (--srs FILE [--sha256 HEX] | --dev-srs) --keys
  DIR`;  execute `Pinned(path, digest) => Srs::read_pinned(&path, k,
  &digest)`.  halo2/src/lib.rs re-exports Digest.
- halo2/tests/backend.rs:
  `pinned_srs_refuses_a_wrong_pin_and_a_changed_file` (a temp dir, a
  dev k 4 file;  read_pinned at k 3 ok;  k 5 is Shape;  the flipped
  first hex char is Pin with expected == wrong and actual == pin;  the
  file + one byte is Pin;  `parse("abc")` and 64 'g' are Usage;  the
  hex round trip).  14 tests.
- halo2/ceremony.sha256 (new, one line, the sha256sum format):
  `4cdb1688...  kzg_bn254_15.srs`.
- Makefile:  `CEREMONY = build/e2e-ceremony`, `CEREMONY_PIN =
  halo2/ceremony.sha256`, `.PHONY` adds e2e-ceremony, the rule
  `e2e-ceremony` after the e2e rule (`SRS=FILE` required, exit 2 with
  a message when absent;  the pin read by awk from $(CEREMONY_PIN);
  the e2e chain with `--srs $(SRS) --sha256 $$pin`;  the verifiers to
  $(CEREMONY)/verifiers/;  no forge step).
- contracts/README.md:  the keygen lines name `--srs FILE --sha256
  HEX` and the rule;  the new section `## The ceremony SRS` after
  them.  halo2/README.md:  the keygen usage line and the `--sha256
  HEX` sentence.  README.md:  `--srs FILE --sha256 HEX` and the rule.

Probes (~/Documents/glue-lang-M9-s5-probe/probe.sh, probe.out and
p1/, on HEAD 0208087 before the build):  the pin MATCHES the file;
`square` keygen `--srs` ok (the format is accepted);  kycMerkle
keygen 5.5 s / 190 MB, prove 12.5 s / 203 MB, verify and solidity
ok;  the srs.bin of the keys dir round trips byte-identical to the
input file;  the dev fixture proof is refused by the ceremony keys;
the verifier diff against contracts/src/verifiers/kycMerkle.sol is
56 lines = the whole vk (the G2 point at mstore 0x05a0 and the fixed
and permutation commitments from 0x07c0), not G2 only:  the
commitments are G1 points of the SRS.

Mutants (~/Documents/glue-lang-M9-s5-probe/mutants.sh, mutants.out
and m/;  F = srs/kzg_bn254_15.srs;  the built binary, no source
change):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | a wrong pin (the first digit 4 to 5) | the back end:  exit 1, `the SRS file .../kzg_bn254_15.srs has SHA-256 4cdb1688..., the pin is 5cdb1688...`;  the rule with CEREMONY_PIN=m/wrong.sha256:  stops at row 1, make exit 2 | KILLED (Error::Pin;  the rule) |
| m2 | the file + one byte (m/plus.srs), the right pin | exit 1, Pin, actual 89825ddd... | KILLED (Error::Pin) |
| m3 | the downsized file:  the k 13 srs.bin of a kyc keys dir (1,048,836 B, pin ff7e044c...) with its own pin, kycMerkle keygen | exit 1, `srs: k 13 is less than the circuit k 15` (the pin passes, the k check refuses:  the order holds) | KILLED (Error::Shape) |

The flag probes:  `make e2e-ceremony` with no SRS exit 2
`e2e-ceremony: give SRS=FILE (the ceremony KZG params file)`;
`--sha256` with `--dev-srs` exit 1 `usage: --sha256 HEX needs --srs
FILE`;  `--sha256 abc` exit 1 `usage: --sha256 HEX needs 64 hex
digits`;  `--sha256` with neither exit 1 `needs --srs FILE`;  the
pin in UPPERCASE is accepted (hex::decode takes either case;  Display
is lowercase).

The lock (~/Documents/glue-lang-M9-s5-probe/s5lock.sh and lock.out):
the s5-b resolution ran with `--offline` and left a lock with 143
insertions and 844 deletions (74 unreachable rows pruned).  The HEAD
lock restored, then one online `cargocho build -- -j 2`:  12
insertions, 0 deletions, names removed 0, names added 1 (sha2),
`version = 4` unchanged.  That minimal lock is the staged one.
clippy `--all-targets` on it:  0 errors, 0 warnings (gateledger green
2d366bd4).

Gates (~/Documents/glue-lang-M9-s5-probe/s5gates.sh and gates.out,
sandbox off, on the staged slice files):  `make -B check` exit 0:
native circuit 9, path 46, face 35, HIT 44;  examples 67, parse
refusals 18, check refusals 157, evals 290, json 42 builds, build
refusals 8, circuits 9, same-ir 19, circuit refusals 20, satcheck
refusals 6, `gate: 0 failures`;  node regressions 6, printer 2.
`make check-clang` exit 0.  `forge build --root contracts --sizes`
exit 0 (DeployVerifier 1,445 B).  `cargo test` (testledger):  14
passed (the new test runs).  `make e2e` exit 0 (9 rows, 41 s).  The
IR diff of test/circuit and contracts/src/verifiers:  0 B before and
after e2e.  `git restore contracts/test/fixtures`, then `forge test
--root contracts`:  88 passed.  `make e2e-ceremony SRS=<the probe
file>` exit 0 (9 `e2e-ceremony:` rows, 44 s;  the IR diff 0 B after
it;  8 verifier files under build/e2e-ceremony/verifiers, the 8
distinct names of the 9 expect.txt rows;
build/e2e-ceremony/verifiers/kycMerkle.sol is byte-identical to the
probe verifier p1/kycMerkle.sol).

Open items:  s6 (the deploy, D6 and D7):  the deploy script reads the
development verifier contracts/src/verifiers/NAME.sol;  a real deploy
needs the ceremony verifier under build/e2e-ceremony/verifiers/ (the
s6 probe rules the route).  The USER fact of question (e) (the
network, the chain id, the RPC URL, the explorer, the keystore
account name) comes before s6.  The pinned k 15 file supports circuits
with k <= 15, including growth of kycMerkle within that limit.  A
circuit requiring k > 15 needs a larger SRS file and a new pin
(halo2/ceremony.sha256).  The Axiom file is a derived copy in the
halo2 format, not the ceremony transcript:  its own check against the
powers of tau response is a USER item.  The point name kycNullifier
(`### s3`) stays until the USER rules.

Staged:  the 12 slice paths and docs/STATUS.md.  Next:  M9 s6.

### s6

- The deployment slice of D6 and D7 (~/Documents/glue-lang-M9-s6-b-PROMPT.md;
  the route probe and the runners in ~/Documents/glue-lang-M9-s6-probe/).
  No change in src/, in the IR, in the verifier generator or in
  test/circuit:  the 88 forge tests of s4 and `make e2e` are unchanged.
- The (e) record:  the five target fields (network, chain id, RPC URL,
  explorer with its verify endpoint, keystore account name) came as the
  literal word VALUE.  s6 takes the target as parameters:  the make
  variables NAME, CHAIN_ID, SCOPE, MIN, PORT, RPC_URL, VERIFIER, GATE and
  the shell variables of the runbook.  No value is in a repository file.
- New files:  contracts/src/KycGate.sol (D7), contracts/test/KycGate.t.sol
  (11 tests), contracts/script/DeployKycGate.s.sol,
  contracts/script/rehearsal.sh, contracts/script/check.sh.  Changed:
  contracts/foundry.toml (the `ceremony` profile), contracts/test/Vm.sol
  (expectRevert, prank), Makefile (rehearsal, deploy-check),
  contracts/README.md (`## KycGate`;  `## The deployment runbook`
  replaces `## Deploy on Telcoin`), README.md (`## Deploy`).
- KycGate:  verifier, scope and min are immutable;  the issuer approves
  an anchor (cm for kyc, the root for kycMerkle);  admit(proof, instances)
  checks the count 4, the anchor, scope, min and the nullifier in that
  order, then staticcall verifyProof;  a bad proof makes the generated
  verifier revert with empty data, so the gate reverts with ProofRejected;
  an accepted proof records nf and emits Admitted(nf, anchor, sender).
  The D1 bound in the constructor:  scope < r, min <= r - 256.
- The route (ii) facts:  the ceremony profile sets src, test (a dir that
  does not exist), out, cache_path and broadcast under build/e2e-ceremony/
  and inherits solc, the optimizer, evm_version paris and fs_permissions;
  contracts/out stays the development build.  The --sizes rows:  the
  ceremony kycMerkle runtime 5,802 B, KycGate 2,176 B, DeployKycGate
  3,702 B.
- Rehearsal (`make rehearsal`:  anvil chain id 31337, port 8577, anvil
  account 0 unlocked in the node, no key anywhere):  the kycMerkle
  verifier at 0x5FbDB2315678afecb367f032d93F642f64180aa3, 1815422 gas;
  KycGate with the approve at 0xe7f1725E7734CE288F8367e1Bb143E90bb3F0512,
  785056 gas;  check.sh:  the chain id, the verifier code against the
  ceremony artifact, the verifier on the ceremony proof, the gate
  verifier, scope, min, the nullifier unused, admit as an eth_call:
  9 ok lines, exit 0, 1 s.
- Mutants (KycGate.sol;  ~/Documents/glue-lang-M9-s6-probe/install.sh and
  install.out):

| Mutant | Change | Result | Status |
|---|---|---|---|
| m1 | `used[nf] = true;` removed | 3 failed:  testRefusesTheSameNullifierTwice, testAdmitsTheKycMerkleFixture, testAdmitsTheKycFixtureWithAnApprovedCommitment | KILLED |
| m2 | the scope compare removed | testRefusesAnotherScope failed | KILLED |
| m3 | the record moved above the staticcall | 11 passed | EQUIVALENT:  the ProofRejected revert rolls the record back |
| m3b | the ProofRejected revert removed | testRefusesATamperedProofAndRecordsNothing failed | KILLED |
| m4 | `R - 256` to `R - 255` | testRefusesMinAboveTheBound failed | KILLED |

- Gates (~/Documents/glue-lang-M9-s6-probe/s6gates.sh, gates.out):
  `make -B check`:  native 9/46/35/44, examples 67, parse refusals 18,
  check refusals 157, evals 290, json 42, build refusals 8, circuits 9,
  same-ir 19, circuit refusals 20, satcheck refusals 6, gate 0 failures,
  node regressions 6, printer 2;  check-clang 0;  forge build 0
  warnings;  forge test 99 (88 + 11);  `make e2e` 9 rows (38 s);  IR
  diff 0 B before and after;  `make e2e-ceremony` 9 rows (34 s), IR
  diff 0 B, 8 verifiers;  the ceremony build;  `make rehearsal` exit 0.
- Open items:  the five (e) VALUE fields (the USER fills them at the
  broadcast);  msg.sender is not bound by the proof (a relayer can
  spend the nf first;  a binding needs a public input, not M9);  k 15,
  the Axiom file check and kycNullifier stay open from s4 and s5.
- The next slice:  s7, the close (STATUS, SPEC.md 4 and 9, the PLAN M9
  row, the mutant roll-up).
- Staged-change review fixes (2026-10-19):
  - MEDIUM: the rehearsal could use an existing RPC on PORT while its own
    anvil failed to bind. Reproduced with an existing chain 31338 and a
    requested chain 31337: the existing sender nonce advanced from 0 to 3
    before the final chain check failed. All rehearsal RPC now uses a
    unique IPC socket owned by the spawned anvil, with the chain checked
    before deployment and the process and socket cleaned up on exit.
  - MEDIUM: check.sh accepted a gate whose issuer had changed to 0xBEEF.
    Gate checks now require ISSUER and compare it with issuer(); the make
    target, rehearsal and runbook pass the expected issuer.
  - contracts/test/deployment.sh and make deployment-test exercise the
    occupied port, a correct issuer, a missing issuer, an unexpected
    issuer and read-only checks. The original scripts failed three
    assertions; the fixed scripts pass all checks.
  - Validation: make check (including check-clang), forge test (99 passed),
    ceremony forge build --sizes, the kyc and kycMerkle rehearsals on
    fresh local anvils, shellcheck and git diff --check all passed.
    No compiler, IR, generated verifier or proof fixture was changed.
    No default test or CI check was weakened. Twelve paths are staged.

### s7: close (2026-10-19)

- Commits:  s1 674c32f (with the plan docs/M9-PLAN.md);  s2 be5a3f6;
  s3 d409eab;  s4 0208087;  s5 becd33c;  s6 0b0c136.  s7 changes docs
  only:  no change in src/, in domain/domain.lang, in halo2/, in
  contracts/, in the Makefile, in test/ and in examples/.
- The slices:
  - s1 674c32f:  the plan (docs/M9-PLAN.md, RULED 2026-10-18) and O21
    closed (D4, ruling (f)):  `TSatH`, `transferSatH`, `transferD7H`,
    `BalLine`, `balLaw` and `balD7` in examples/transferSpec.lang, the
    PATH_BOUNDARY row bal-orientation.lang, 2 eval rows.
  - s2 be5a3f6:  the credential circuit `kyc` (D1, ruling (a):  `cm =
    pos2 attr sk`, `nf = pos2 sk scope`, `d + min == attr` with `range8
    d`;  7290 rows, k 13), its golden, e2e row, verifier and 11 Forge
    tests, the WITNESS_FAIL row.
  - s3 d409eab:  the family `KycSpec` (D3, ruling (g);  the points
    `credential`, `kycNullifier` and `predicate`, the laws `noReplay`
    and `disclosure`), examples/kycSpec.lang (`CredSat`, `NfSat`,
    `PredSat`, the fold `KycSat` into `Type 0`, `kycLaw`, `discLaw`,
    `kycLawH`, the instances `kycDH` and `discD`), the REFUSE_NAME row.
  - s4 0208087:  the Merkle credential circuit `kycMerkle` (D2, ruling
    (b):  a public issuer root, depth 4, k 15), its golden, e2e row,
    verifier and 11 Forge tests, the circuit refusal row (a wrong
    public root).
  - s5 becd33c:  the ceremony SRS rule (D5, ruling (c)):  the halo2 back
    end reads a KZG params file pinned by SHA-256 (`Digest`,
    `SrsSource::Pinned`, `Error::Pin`;  a file below the circuit k is
    refused), halo2/ceremony.sha256 (kzg_bn254_15.srs, k 15, the Axiom
    copy of the Perpetual Powers of Tau challenge 0085), `make
    e2e-ceremony` (keys, proofs and verifiers under build/e2e-ceremony/,
    not committed), cargo test 14.
  - s6 0b0c136:  the deployment (D6 and D7, rulings (d) and (e)):
    contracts/src/KycGate.sol and its 11 Forge tests,
    DeployKycGate.s.sol, the `ceremony` profile of contracts/foundry.toml
    (route (ii)), the anvil rehearsal (contracts/script/rehearsal.sh,
    chain id 31337, no key), the read-only check
    (contracts/script/check.sh), the deployment test
    (contracts/test/deployment.sh) and the runbook (contracts/README.md).
    The five (e) fields are parameters.
- What M9 added outside the examples, the guest tables and the docs:
  domain/domain.lang +6 lines (`KycSpec`, s3);  halo2/ (s5:  backend.rs,
  error.rs, main.rs, lib.rs, tests/backend.rs, Cargo.toml and Cargo.lock
  for sha2, ceremony.sha256);  contracts/src/verifiers/kyc.sol (s2) and
  kycMerkle.sol (s4) with their proof fixtures and 22 tests in
  Verifiers.t.sol;  contracts/src/KycGate.sol, KycGate.t.sol, Vm.sol,
  foundry.toml, the scripts and deployment.sh (s6);  the doc comment
  list of DeployVerifier.s.sol;  the Makefile (s5 e2e-ceremony;  s6
  rehearsal, deploy-check, deployment-test).  No src/ change, no new IR
  form, no verifier generator change and no conv rule.
- Docs:  SPEC.md 2.10 (s1:  the O21 defs;  s3:  `KycSpec`), 4 (s7:  the
  M9 bullet) and 9 (s1:  O21 closed;  s7:  O22 and O23 new).
  docs/PLAN.md:  the M9 row DONE 2026-10-19 with the slice commits.
  docs/M9-PLAN.md:  unchanged (docs/M8-PLAN.md has no result section).
  docs/STATUS.md:  this section.

#### Rulings

The USER ruled each recommendation of docs/M9-PLAN.md section 9 on
2026-10-18.  The USER facts came later:  (c) on 2026-10-19 (the
ceremony file and its source), (e) on 2026-10-19 (the literal word
VALUE for each field).

| Ruling | Content | Where it landed |
|---|---|---|
| (a) | D1:  `cm = pos2 attr sk`, `nf = pos2 sk scope`, `range8 d` with `d + min == attr` | s2 `kyc` |
| (b) | D2:  Merkle membership, depth 4 | s4 `kycMerkle` (k 15) |
| (c) | D5:  a halo2 KZG params file from the Perpetual Powers of Tau ceremony, k >= 15, pinned by SHA-256;  the USER named the file and its source on 2026-10-19 | s5 (halo2/ceremony.sha256, `make e2e-ceremony`) |
| (d) | D7:  the consumer contract `KycGate` | s6 |
| (e) | D6:  the deployment target;  the five USER facts came as the literal word VALUE | s6 (the make variables, the runbook, the rehearsal at 31337);  O23 |
| (f) | D4:  O21 closed with the hash as an argument | s1 |
| (g) | D3:  the specification HIT `KycSpec` | s3 |
| (h) | D8:  the Poseidon forms (ii) and (iii) are not in M9 | none (M10 or later) |
| (i) | O3, O12 (c), O13, O14, O15, O16 and O19 stay open | SPEC.md 9 |
| (j) | the slices s1 to s7 of section 10 | s1 to s7 (s2 to s6 each used more than one session) |

No further USER ruling came from the slices.  The point name
`kycNullifier` (s3) waits for a USER ruling.

#### Mutant roll-up

The mutants of the `Mutants` items of s1 to s6 (the table of each slice
is in its section).

| Slice | Mutants | KILLED | EQUIVALENT | SURVIVED |
|---|---|---|---|---|
| s1 | 4 | 4 | 0 | 0 |
| s2 | 4 | 4 | 0 | 0 |
| s3 | 4 | 4 | 0 | 0 |
| s4 | 4 | 4 | 0 | 0 |
| s5 | 3 | 3 | 0 | 0 |
| s6 | 5 | 4 | 1 | 0 |
| M9 | 24 | 23 | 1 | 0 |

- Totals:  24 mutants:  23 KILLED, 1 EQUIVALENT (s6 m3:  the record
  before the staticcall;  the ProofRejected revert rolls the record
  back, thus no test can see it;  m3b, the revert removed, is the mutant
  that the tests see), 0 SURVIVED.  16 mutants are in guest files (s1 to
  s4:  the examples, the eval rows and the refuse rows);  3 are inputs
  of the back end (s5:  the pin, the file and the downsized file, killed
  by Error::Pin and Error::Shape);  5 are in contracts/src/KycGate.sol
  (s6, killed by the Forge tests).  No mutant is in src/ (M9 changed no
  src/ file).  No mutant added a row.

#### Review

- (i) No conv rule reads a type.  M9 changed no src/ file (the diff of
  fa38310 to 0b0c136 holds no src/ path):  the checker, the evaluator
  and the conv rules are the M8 close state, thus the M8 close review
  (i) holds.
- (ii) No rule `hcomp A i0 u u0 = u0` (D8 of M3):  no change in the
  evaluator, and the hcomp-i0-int.lang and hcomp-i0-circle.lang rows
  stay.
- (iii) `hcomp` at a HIT stays a canonical value (D3 of M4):  no change
  in the evaluator.  `KycSpec` has the form of `TransferSpec`, and its
  fold `KycSat` into `Type 0` goes through `ua` (s3).
- (iv) No IR change:  M9 adds no IR form and no verifier generator
  change (ruling (h)).  The IR and the development verifier of each M8
  circuit stayed byte-identical (the IR check after each e2e and each
  e2e-ceremony).  The new circuits `kyc` and `kycMerkle` each added a
  golden and a verifier file.  The ceremony verifiers stay under
  build/e2e-ceremony/ (D5).
- (v) Deployment (D6):  no key, no keystore and no live RPC was read or
  used;  the rehearsal ran on a local anvil (chain id 31337) only;  no
  broadcast.  No value of the five (e) fields is in a repository file.
- Gates (s7, the s7 files staged first;  the sandbox on, thus no forge,
  no anvil and no e2e in s7):  `make -B check` exit 0:  native circuit
  9, path 46, face 35, HIT 44;  examples 67, parse refusals 18, check
  refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9,
  same-ir 19, circuit refusals 20, satcheck refusals 6;  node regressions
  6, printer 2.  Against s6:  no change (section 10:  none).  Against the
  M8 close:  examples +3, check refusals +2, evals +6, json +1, circuits
  +2, circuit refusals +2.  The s6 sandbox-off gates stand (s7 changes
  docs only):  `make check-clang` exit 0, `forge build --root contracts
  --sizes` exit 0, `forge test --root contracts` 99 passed (66 at the M8
  close), `make e2e` 9 rows (7 at the M8 close), `make e2e-ceremony` 9
  rows, the IR diff of test/circuit and contracts/src/verifiers empty
  before and after each, `make rehearsal` exit 0.
- Open items at the M9 close (SPEC.md 9):  O22 (new in M9:  the sender
  binding;  the proof does not bind msg.sender, thus a relayer can spend
  the nullifier first;  a binding needs a public input and a gate
  check);  O23 (new in M9:  the deployment target;  the five (e) fields
  came as the literal word VALUE and the USER fills them at the
  broadcast;  the check of the Axiom copy of the ceremony file against
  the Perpetual Powers of Tau response;  a circuit requiring k > 15
  needs a larger SRS file and a new pin, while growth within k <= 15
  fits the pinned file).  The point name
  `kycNullifier` (s3) waits for a USER ruling.  The Poseidon forms (ii)
  and (iii) (ruling (h)) go to M10 or later.  O3, O12 (c), O13, O14,
  O15, O16 and O19 stay open (ruling (i)).  Next:  none in M9;  an M10
  starts only when the USER opens it.

## M10

### kickoff (2026-10-19)

- Start condition MET:  HEAD f1012a8 `M9 s7: close` (USER), status empty.  The HEAD tree 089602a5 is not the staged s7 tree 55c48ad7:  the diff is SPEC.md (6 lines) and docs/STATUS.md (10 lines), a USER rewording of the O23 text (the pinned SRS supports circuits with k <= 15;  k > 15 needs a larger file and a new pin).  Accepted (the M9 kickoff rule:  docs-only lines).
- Plan:  docs/M10-PLAN.md DRAFT (sections 1 to 10;  not staged:  it goes with the M10 s1 commit after the rulings).  Scope candidates from the M9 close:  O22 the sender binding, the point name `kycNullifier`, the Poseidon forms (ii) and (iii) as an estimate, the open items.
- Probe (~/Documents/glue-lang-M10-kickoff-probe/run.sh, scratch copies of examples/kyc.glue and examples/kycMerkle.glue, the real build/langc of HEAD;  the header edit `(min : Field) (sender : Field) private`, one binding row appended after `range8 d`;  sender = 1390849295786071768276380950238675083608645509734, the anvil account 0 address 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266 as an integer):

| Probe | Circuit | Row | rows (golden) | copies (golden) | public | k | check | witness | satcheck |
|---|---|---|---|---|---|---|---|---|---|
| kycA | kyc | `sender * 1 == sender` | 7293 (7290) | 8713 (8709) | 5 | 13 | ok | ok | ok |
| kycB | kyc | `sender == sender` | 7291 (7290) | 8710 (8709) | 5 | 13 | ok | ok | ok |
| kycC | kyc | `sender + 0 == sender` | 7293 (7290) | 8713 (8709) | 5 | 13 | ok | ok | ok |
| kycE | kyc | `0 * sender == 0` | 7294 (7290) | 8713 (8709) | 5 | 13 | ok | ok | ok |
| kycU | kyc | none (the binder only) | | | | | CIRCUIT_UNUSED_PUBLIC (rc 1) | the same | |
| kmA | kycMerkle | `sender * 1 == sender` | 21901 (21898) | 26181 (26177) | 5 | 15 | ok | ok | ok |
| kmB | kycMerkle | `sender == sender` | 21899 (21898) | 26178 (26177) | 5 | 15 | ok | ok | ok |

- Probe findings:  the other IR fields stay (advice 3, fixed 7, instance 1, gates 1, lookups 1, fixed_values 7);  k stays 13 and 15 (within the pinned SRS);  `witness` with `sender + 1` also exits 0 in each form (the row does not restrict the value:  the gate does, and the transcript binds the instance);  `witness` without `sender=` gives rc 2 `WITNESS_ARGS: kyc: the binder sender has no value`;  the name `sender` is free (no REFUSE_NAME).  The gate side (a read of KycGate.sol, no forge):  the count check at line 89 becomes 5, a new check of `instances[4]` against `uint256(uint160(msg.sender))` and an error `WrongSender`;  both verifiers hold `num_instances` 4 (kyc.sol line 220) and regenerate through e2e.
- Questions for the USER (docs/M10-PLAN.md section 9):  (a) the scope (O22 in s1 and s2, the `kycNullifier` ruling in s3, the Poseidon estimate in s4, the close in s5);  (b) the raw address as the fifth public binder;  (c) form B `sender == sender`;  (d) kyc and kycMerkle in place (the IR rule ruling);  (e) the gate design and the fixture sender;  (f) keep `kycNullifier`;  (g) the Poseidon estimate only;  (h) the open items stay;  (i) the slices;  (j) the SPEC.md 2.10 sentences.
- Gates:  none ran (the kickoff edits no tracked file;  docs/M10-PLAN.md is untracked).
- Next:  the USER rules on section 9;  then a fresh session follows ~/Documents/glue-lang-M10-s1-PROMPT.md.

### s1 (2026-10-20)

- Rulings (USER 2026-10-20, the first message of the s1 session):  `(a) .. (j) ..`, each recommendation of docs/M10-PLAN.md section 9 taken (the M9 form, no deviation named):  (a) the scope O22 in s1 and s2, the `kycNullifier` ruling in s3, the Poseidon estimate in s4, the close in s5;  (b) the raw address as the fifth public binder;  (c) form B `sender == sender`;  (d) kyc and kycMerkle in place;  (e) the gate count 5, `WrongSender` after `WrongMin`, the fixture sender = the anvil account 0, `vm.prank` in the tests, check.sh `--from`;  (f) keep `kycNullifier`;  (g) the Poseidon estimate only;  (h) the open items stay;  (i) the slices s1 to s5;  (j) the SPEC.md 2.10 sentences.  Recorded in the plan Status line.
- Start condition MET:  HEAD f1012a8, ` M docs/STATUS.md` and `?? docs/M10-PLAN.md` only.
- Changed (the install runner ~/Documents/glue-lang-M10-s1-probe/install.sh, the log install.out, rc 0):  examples/kyc.glue (224 lines) and examples/kycMerkle.glue (228 lines) get the binder `(sender : Field)` after `min`, the row `; sender == sender` at the end of the circuit line and a 3-line header sentence (O22);  the four goldens regenerate (kyc 7291 rows, 8710 copies, public 5, k 13;  kycMerkle 21899 rows, 26178 copies, public 5, k 15;  satcheck ok on each pair);  test/circuit/expect.txt rows 8 and 9 and refuse.txt rows 19 and 20 take `sender=1390849295786071768276380950238675083608645509734` (the anvil account 0 address as an integer);  refuse.txt row 21 is new (`2 WITNESS_ARGS witness kyc kyc` with the row 8 args without `sender=`;  21 rows);  contracts/src/KycGate.sol (105 lines):  the count check 5, `error WrongSender(uint256 sender)`, the check of `instances[4]` against `uint256(uint160(msg.sender))` after the `WrongMin` check, the doc comments name five inputs;  contracts/test/KycGate.t.sol (172 lines):  the constant `SENDER`, `vm.prank(SENDER)` before each of the 10 admits, `testRefusesAnotherSender` (no prank, `WrongSender` with the fixture value), `testRefusesAWrongInstanceCount` keeps `new uint256[](3)`;  contracts/test/Verifiers.t.sol (314 lines):  `SENDER` in `VerifierCase`, `instances.length == 5` and `instances[4] == uint256(uint160(SENDER))` in the two kyc tests;  `make e2e` regenerated contracts/src/verifiers/kyc.sol and kycMerkle.sol (48 lines each;  `num_instances` 5) and the fixtures contracts/test/fixtures/kyc.proof.json and kycMerkle.proof.json (five instances;  the fifth is 0x000000000000000000000000f39fd6e51aad88f6f4ce6ab8827279cfffb92266).
- Mutants (langc on scratch copies, ~/Documents/glue-lang-M10-s1-probe/mutants.out;  forge on the staged gate, mutants45.out):  m1 the binding row removed:  KILLED (CIRCUIT_UNUSED_PUBLIC rc 1 at check, circuit and witness;  kycMerkle the same);  m2 the header binder removed with the row kept:  KILLED (NAME_UNKNOWN `unknown name sender` rc 1 at check and circuit);  m3 the circuits row without `sender=`:  KILLED (WITNESS_ARGS rc 2, no witness file;  kycMerkle the same);  m4 the gate check of `instances[4]` removed:  KILLED (`testRefusesAnotherSender` FAIL, `next call did not revert as expected`);  m5 the count check kept at 4:  KILLED (9 of the 12 KycGateTest tests FAIL with `WrongInstanceCount(5)`, `testRefusesAWrongInstanceCount` passes);  the control goldens byte-identical, the restored gate 12 passed.
- Gates (~/Documents/glue-lang-M10-s1-probe/gates.sh, sandbox off, the log gates.out):  `make -B check` exit 0 (51 s):  native circuit 9, path 46, face 35, HIT 44;  examples 67, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 21 (20 at the M9 close), satcheck refusals 6;  node regressions 6, printer 2;  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (73 s, 9 rows, 100 tests passed inside);  `forge test --root contracts` 100 passed (99 at the M9 close;  KycGateTest 12);  `make e2e-ceremony SRS=<the pinned file>` exit 0 (68 s, 9 rows, 8 verifier files under build/e2e-ceremony/verifiers, not committed).
- IR check:  before e2e (the index against HEAD) test/circuit holds the kyc and kycMerkle goldens, expect.txt and refuse.txt, contracts/src/verifiers nothing;  after e2e (the worktree against the index) test/circuit empty, contracts/src/verifiers kyc.sol and kycMerkle.sol only (the 6 other verifiers byte-identical), the fixtures kyc and kycMerkle (10 lines each, the fifth instance) and the 6 other fixtures (4 lines each, the proof bytes of a fresh run, restored from the index as at each M9 e2e).  No new IR form, no verifier generator change.
- Docs:  SPEC.md 2.10 (the sender binder sentence, the flat constructor namespace sentence of ruling (j) and D6) and 9 (O22:  the circuit and the gate side done in s1, the operations side in s2;  open until s2);  docs/PLAN.md M10 row (PLANNED 2026-10-20, the scope of ruling (a));  docs/M10-PLAN.md Status line (RULED 2026-10-20, s1 BUILT).
- Open items:  O22 stays open until s2 (contracts/script/check.sh `--from`, contracts/script/rehearsal.sh, contracts/test/deployment.sh, the runbook in contracts/README.md).
- Staged-change review (2026-10-20): one MEDIUM finding fixed. The new
  sender check made the admission simulation in contracts/script/check.sh
  revert with WrongSender because cast call supplied no sender. The
  original `make deployment-test` reproduced the failure. The check now
  decodes the address from the proof's fifth instance and passes `--from`;
  a failed admission call also retains its exit status before output parsing.
  The deployment regression covers a holder distinct from the issuer and
  checks that simulations send no transaction and leave the nullifier unused.
  contracts/README.md now documents five inputs and the simulation sender.
  These review fixes bring the check.sh and runbook work forward from s2;
  O22 remains open for the rest of that slice.
- Review validation: `make check` passed, including regenerated circuit
  and witness golden comparisons; `forge test --root contracts --gas-report`
  passed all 100 tests; `make deployment-test rehearsal NAME=kyc` and
  `make rehearsal NAME=kycMerkle` passed against local Anvil nodes. Both
  staged verifiers regenerated byte-for-byte from the staged circuits and
  development keys; both staged proof fixtures verified natively and their
  instances matched the witness goldens. No CI weakening or remaining
  correctness finding was identified. Full command captures are retained
  locally under build/staged-review/.kanon-exec/ (original failure:
  run-ci4EkP; compiler: run-hBiEo3; Solidity: run-WUEcMF; deployment and
  kyc rehearsal: run-n4ZZms; kycMerkle rehearsal: run-zGQYgM).
- Next:  s2, the operations side of O22 (~/Documents/glue-lang-M10-s2-PROMPT.md).

### s2 (2026-10-20)

- Rulings:  none new (M10 (a) to (j) RULED 2026-10-20;  (e) names check.sh `--from`).
- Start condition MET:  HEAD 54cc43b `M10 s1: add the sender binder` (USER), status empty.  The commit holds 22 paths against the 19 staged at the s1 close (tree c7a6c7be):  the USER staged-change review added contracts/script/check.sh (`--from` from `.instances[4]` with `cast parse-bytes32-address`, +6/-2), contracts/test/deployment.sh (the nullifier-unused checks and the changed-issuer check, +16), contracts/README.md (five public inputs, 12 tests, the runbook step 5 `--from` sentences, +17/-13), SPEC.md O22 (+4/-3) and this file (+22);  5 files, +65/-18.  Accepted:  the s2 install shrinks to what is left.
- Probe (~/Documents/glue-lang-M10-s2-probe/probe.sh, the log probe.out):  `make rehearsal` exit 0 (3 s) and `make deployment-test` exit 0 (2 s) as is;  the rehearsal runs check.sh (rehearsal.sh line 90) and the admission eth_call passes with `--from`.  The WrongSender failure of the s1 review (no `--from`) is the mutant m1 below.
- Changed (install.sh, the log install.out, rc 0, one hit before and after each site):  contracts/script/check.sh (the report line names the derived sender:  `check: gate admits the $NAME proof from $sender (eth_call, nothing sent) ok`);  contracts/script/rehearsal.sh (the header:  the e2e proofs bind the anvil account 0 as the holder;  the report line ends `gate $GATE, sender $SENDER)`);  contracts/test/deployment.sh (3 comment lines before `SENDER=`:  the deployer, the issuer and the holder;  the report line `the configured issuer passes (admission from the proof sender $SENDER)`);  contracts/README.md (the runbook step 7 `Admit a holder.`:  the fifth public input is the holder's address, `cast call --from $HOLDER` then `cast send --account`, WrongSender for any other sender, the nullifier recorded;  the runtime table rows of the five-instance verifiers from `forge test --gas-report`:  `| kyc | 6,280 | 1,409,440 | 316,454 |` and `| kycMerkle | 6,222 | 1,396,947 | 316,579 |`;  the runtime size = the deployment size minus the 32-byte constructor, unchanged;  the `verifyProof` number = the max column, the valid proof).  No guest row, no src/, no IR form, no verifier change.
- Mutants (gates.sh, the logs m1-*.out and m2-*.out;  `git restore` from the index after each):  m1 check.sh without `--from`:  KILLED (`make deployment-test` rc 2 with `FAIL the configured issuer was refused` and `FAIL the proof sender was confused with the issuer`;  `make rehearsal` rc 2;  the cast error `execution reverted: custom error 0xe5d1f3bb: <the fifth instance>` = WrongSender, then `check: gate admit (eth_call): FAILED`);  m2 `--from` the anvil account 1:  KILLED (the same two rc 2, the same error).  The control rehearsal after the restores:  exit 0.
- Gates (gates.sh, sandbox off, the log gates.out):  `make -B check` exit 0 (49 s;  parse refusals 18, check refusals 157, build refusals 8, circuit refusals 21, satcheck refusals 6, the other counts as at s1), `make check-clang` exit 0, `forge build --root contracts --sizes` exit 0, `make e2e` exit 0 (58 s, 9 rows, 100 tests inside), `forge test --root contracts` 100 passed (9 suites), `make rehearsal` exit 0 (the check line names the sender 0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266), `make deployment-test` exit 0 (5 check lines).
- IR check:  before e2e (the index against HEAD) test/circuit and contracts/src/verifiers empty;  after e2e (the worktree against the index) empty too (the 8 verifiers byte-identical);  the 8 fixtures churned (the proof bytes, 4 lines each) and were restored from the index.
- Docs:  SPEC.md 9 (O22 CLOSED in M10, slices s1 and s2);  docs/M10-PLAN.md Status line (s2 BUILT 2026-10-20);  contracts/README.md (step 7, the two table rows).
- Open items:  O22 CLOSED.  O23 (the deployment target) stays with the USER.
- Next:  s3, the `kycNullifier` ruling (f), option (i), docs only (~/Documents/glue-lang-M10-s3-PROMPT.md).

### s3 (2026-10-20)

- Rulings:  none new (M10 (a) to (j) RULED 2026-10-20;  (f) = option (i) of D6:  keep `kycNullifier`).
- Start condition MET:  HEAD 6a17d6f `M10 s2: document sender-bound admission` (USER;  the subject of the commit message file was `M10 s2: bind the sender in the scripts`, the USER reworded it), tree a81b9151 = the s2 index tree (`git diff a81b9151 HEAD --stat` empty), status empty.  No USER staged-change review this time:  the slice is as planned.
- The question of the s3 prompt ((i) two commits, or (ii) one docs commit for s3 and s4) came back without an answer a second time (the slot of the first message held the question text);  the session took the recommendation (ii):  s3 and s4 are one docs commit, this file gets both sections, and no s4 prompt exists.
- Ruling (f), option (i):  the point name `kycNullifier` stays.  The three sites of domain/domain.lang are unchanged:  line 64 and line 66 (the comment of M9 s3) and line 68 (the family line of `KycSpec`).  SPEC.md 2.10 states the rule in one sentence (lines 1149 to 1153, written in s1):  the constructor namespace of the domain families is flat (src/front/check.c name_owner, rule R4), thus `TransferSpec` has `nullifier` and `KycSpec` has `kycNullifier`;  no edit.  No code, no rebuild, no guest row.
- Control (~/Documents/glue-lang-M10-s4-probe/control.sh, the log control.out):  `make -B check` exit 0 (native circuit 9, path 46, face 35, HIT 44;  examples 67, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 21, satcheck refusals 6;  node regressions 6, printer 2;  the s2 counts).  No install, no gate delta.
- Docs:  docs/M10-PLAN.md Status line (s3 DONE 2026-10-20).

### s4 (2026-10-20)

- Rulings:  none new ((g) = the Poseidon estimate only, no Poseidon code;  D7).  No D7 input is missing.
- Method:  the lowering rules of SPEC.md 7.1 (one row for each `+`, `-` and `*`, one constant row for each Field literal, one input row for each binder and hint, a copy for `==`) applied to form (i) (examples/poseidon.glue:  `hint tRkJ := sbox5 (s + rc)` with `sbox5 x = x * x * x * x * x`, `hint sRkI := mds * t + mds * t + mds * t`), then checked against the golden test/circuit/poseidon.circuit.json with jq (the selector pattern ql qr qo qm qc of the rows 279 to 345:  the hint `t0k1` is `c + c + * c + * c + * c + *`, 14 rows;  a mix hint is `c * c * + c * +`, 8 rows).  The lowering has no sharing:  each occurrence of a subterm gets its own rows.
- Form (i) counts:  an S-box on a binder is 14 rows (the sum `s + rc` 5 times = 5 constant rows + 5 add rows, then 4 mul rows);  a full round is 72 rows = 6 hint rows + 3 x 14 + 3 x 8;  a partial round is 54 rows = 4 hint rows + 14 + 3 x 12 (each mix hint repeats the two sums `s + rc` of the words without an S-box, 2 rows each).  The model gives each golden exactly:  poseidon 279 input rows + 53 (round 0:  the S-box of word 0 is closed, `sbox5 (0 + rc0)`, one constant row) + 3 x 66 + 57 x 50 + 4 x 66 = 3644;  transfer 558 + 2 x 3365 = 7288;  kyc 560 + 6730 + 1 = 7291;  kycMerkle 1680 + 20190 + 4 x 7 + 1 = 21899.  Each hash has 80 S-boxes on binders and 57 partial rounds.
- The savings for each hash:  form (ii), the degree-5 gate, 11 rows for each S-box on a binder (14 to 3:  the sum once, then the gate row), or 13 if the gate holds the round constant in a fixed column (14 to 1);  form (iii), the memo table, 8 rows for each S-box on a binder (the sum once:  14 to 6) and 8 rows for each partial round (the two repeated sums:  3 uses to 1, 2 words), and 585 x hashes - 9 more rows if the memo also shares literal rows (the 9 MDS constants of each of the 65 rounds), plus 195 x (hashes - 1) for the round constants across hashes.
- The table (rows, then k by the rule rows + 16 <= 2^k):

| circuit | hashes | form (i) | (ii) wire | (ii) constant | (iii) binders | (iii) + literals |
|---|---|---|---|---|---|---|
| poseidon | 1 | 3644, k 12 | 2764, k 12 | 2604, k 12 | 2548, k 12 | 1972, k 11 |
| transfer | 2 | 7288, k 13 | 5528, k 13 | 5208, k 13 | 5096, k 13 | 3740, k 12 |
| kyc | 2 | 7291, k 13 | 5531, k 13 | 5211, k 13 | 5099, k 13 | 3743, k 12 |
| kycMerkle | 6 | 21899, k 15 | 16619, k 15 | 15659, k 14 | 15323, k 14 | 10847, k 14 |

- Result:  the D7 hypothesis (neither form changes k) holds for poseidon, transfer and kyc under form (ii) and under form (iii) over binder subterms, and fails for kycMerkle:  form (iii) gives k 14 under each memo scope, form (ii) gives k 14 only with the constant in the gate.  The literal memo gives k 11 for poseidon and k 12 for transfer and kyc.  Both forms together (the gate on the wire, the binder memo) give poseidon 2308 (k 12), kyc 4619 (k 13) and kycMerkle 13883 (k 14).  The forms stay OPEN as a milestone candidate:  SPEC.md 9 O24 (new).  The pinned SRS (k <= 15) covers each k of the table.  No code, no gate delta, no guest row.
- Docs:  SPEC.md 9 (O24);  docs/M10-PLAN.md Status line (s4 DONE 2026-10-20);  this file (`### s3`, `### s4`).
- Next:  s5, the M10 close (~/Documents/glue-lang-M10-s5-PROMPT.md).

### s5: close (2026-10-20)

- Start condition MET:  HEAD c4b79d1 `M10 s4: record the kycNullifier ruling and Poseidon estimates` (the USER reworded the subject of the s4 commit message file), status empty.  HEAD^{tree} c32e2f91 is not the s4 staged tree 60d319b6:  `git diff 60d319b6 HEAD` names SPEC.md only (+6/-2), in 9 O24.  The USER change replaces the sentence on the literal memo (576 more rows for each hash) with the formula `585 * H - 9 + 195 * (H - 1)` for a memo over all H hashes (the 9 MDS constants over the 65 rounds of each hash, plus the 195 other literal values across hashes):  576 rows for one hash, 1356 for two and 4476 for six.  These are the differences of the two form (iii) columns of the `### s4` table (2548 to 1972, 5096 to 3740, 5099 to 3743, 15323 to 10847);  the table and the `### s4` text stay as they are.  The slice is not smaller:  the change is in O24 only.
- USER answer to the s5 question (2026-10-20, the first message of the s5 session):  (i).  O24 stays open, and this session does not start M11.
- Commits:  s1 54cc43b (with the plan docs/M10-PLAN.md);  s2 6a17d6f;  s3 and s4 c4b79d1 (one docs commit).  s5 changes docs only:  no change in src/, in domain/domain.lang, in halo2/, in contracts/, in the Makefile, in test/ and in examples/.
- The slices:
  - s1 54cc43b:  the circuit and gate side of O22 (rulings (b) to (e)):  the fifth public binder `sender` and the row `sender == sender` (form B) of `kyc` and `kycMerkle`, the four goldens (kyc 7291 rows, k 13;  kycMerkle 21899 rows, k 15;  public 5), the circuit refusal row 21, the error `WrongSender` and the check of `instances[4]` against `msg.sender` in `KycGate.admit`, the five-instance verifiers and fixtures, `testRefusesAnotherSender`;  22 paths;  m1 to m5 KILLED.
  - s2 6a17d6f:  the operations side of O22 (O22 CLOSED):  check.sh derives the `--from` address from the fifth instance and names it, the rehearsal and the deployment regression name the sender, the deployment regression admits with an issuer that differs from the holder, the runbook has the admission step;  7 paths;  m1 and m2 KILLED.
  - s3 and s4 c4b79d1:  ruling (f) option (i) (`kycNullifier` stays, no code) and the Poseidon estimate of D7 (ruling (g):  the lowering has no sharing, an S-box on a binder is 14 rows, a full round 72, a partial round 54;  the hypothesis fails for `kycMerkle`;  SPEC.md 9 O24 new);  3 paths;  no mutants.
- Mutants:  7 of 7 KILLED (s1 m1 to m5, s2 m1 and m2).
- Gates at the close (the control ~/Documents/glue-lang-M10-s5-probe/control.sh, the log control.out, sandbox on):  `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT 44;  examples 67, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 21 (20 at the M9 close), satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  The s5 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s2 (`make check-clang`, `forge build --root contracts --sizes`, `make e2e` 9 rows, `forge test --root contracts` 100 passed, `make rehearsal`, `make deployment-test`, each exit 0).
- The IR rule kept:  `git diff f1012a8 HEAD --stat -- src domain halo2 Makefile test/circuit contracts/src/verifiers` names 8 paths only:  the goldens and witnesses of `kyc` and `kycMerkle`, test/circuit/expect.txt and refuse.txt, and the verifiers kyc.sol and kycMerkle.sol (`num_instances` 5).  M10 changed the IR of `kyc` and `kycMerkle` under ruling (d) only (`public` 4 to 5, one arith row and one copy);  the 6 other circuits (square, range, poly, nonzero, poseidon, transfer) and their 6 verifiers are byte-identical to the M9 close;  the 8 verifiers are as at s2.  No new IR form, no conv rule, no src/ change, no change to the verifier generator.
- Docs:  SPEC.md 4 (the bullet `The sender binding`) and 9 (O24:  the close sentence);  docs/PLAN.md (the M10 row DONE);  docs/M10-PLAN.md (the Status line, CLOSED);  this file (`### s5: close`).  SPEC.md 10 does not change:  its table has the rows M0 to M3 only (the template), and docs/PLAN.md holds the rows of each milestone up to M10.
- Open after M10:  O23 (the deployment target:  the five USER facts of M9 question (e) and the check of the Axiom copy of the ceremony file;  USER items) and O24 (the Poseidon forms (ii) and (iii):  a milestone candidate that needs a USER ruling first;  answer (i) does not start it).
- Next:  none.  A new milestone starts only when the USER names one (O24 or another topic).

## M11

### kickoff (2026-10-20)

- Start condition MET:  HEAD 1f02fe5 `M10 s5: close` (USER), HEAD^{tree} edb256bd = the s5 staged tree, status empty.
- Topic:  the topic slot of the kickoff message came as the literal placeholder `<O24 the Poseidon forms (ii)/(iii) | another topic, name it>`;  the kickoff took its first option, O24 (the Poseidon forms), and said so.
- Plan:  docs/M11-PLAN.md DRAFT (sections 1 to 10;  not staged:  it goes with the M11 s1 commit after the rulings, with the docs/PLAN.md M11 row and this section).
- Probe P1, form (iii) by hand in the source (~/Documents/glue-lang-M11-kickoff-probe/run.sh and rewrite.awk, the log run.out, scratch copies of the four Poseidon examples, the real build/langc of HEAD;  repository status empty before and after):  rewrite.awk names each `(A + B)` of two names in the constraint list with a new hint `zzmN` and the constraint `A + B == zzmN`.  Mode sbox:  the argument of `sbox5` only.  Mode all:  each such sum (also the partial-round sums that 3 mix constraints repeat).  circuit, witness and satcheck rc 0 for all 8 runs;  instance[0] = the golden value each time;  advice 3, fixed 7, gates 1 as before.

| Circuit | golden:  rows, k | sbox:  named, rows, k | all:  named, rows, k |
|---|---|---|---|
| poseidon | 3644, 12 | 80, 3084, 12 | 194, 2742, 12 |
| transfer | 7288, 13 | 160, 6168, 13 | 388, 5484, 13 |
| kyc | 7291, 13 | 160, 6171, 13 | 388, 5487, 13 |
| kycMerkle | 21899, 15 | 480, 18539, 15 | 1164, 16487, 15 |

- P1 findings:  a memo has no hint row, so the memo rows = the P1 rows - named.  all - named = 2548, 5096, 5099, 15323 = the `(iii) binders` column of `### s4` exactly (the s4 model holds on the real lowering).  sbox - named = 3004 (k 12), 6008 (k 13), 6011 (k 13), 18059 (k 15) = a pointer memo inside one constraint.
- Probe P2, form (ii) on the back end (the real halo2/target/debug/glue-halo2 of 2026-10-19, `--dev-srs`;  jq on a copy of test/circuit/square.circuit.json, k 5, rows 3, no lookup):  fixed 7 to 8 (q5 = fixed column 7), the gate `pow5` with the poly `q5 * (a^5 - c)` (degree 6).  base (as is):  check, keygen, prove, verify, solidity rc 0;  the verifier 46893 bytes (923 lines), proof.json 6714 bytes.  off (the gate, q5 = 0) and on (row 3:  a = 3, c = 243, q5 = 1):  all rc 0;  the verifier 41815 bytes (840 lines), proof.json 5690 bytes.  bad (c = 242):  check rc 1 and prove rc 1 (`Constraint 0 in gate 1 ('pow5') is not satisfied .. at offset 3`);  keygen and solidity rc 0 (they do not read the witness).  The generator sets the permutation chunks and the quotient count from `cs.degree()`;  not measured on a real circuit.
- Survey (one read-only agent):  `lower_value` (src/circuit.c 148 to 187) has no memo and the values are not hash-consed;  `lower_check` runs `eval_core` once for each constraint (line 197);  satcheck, halo2/ and the verifier generator have no degree limit and no fixed-column limit, so form (ii) is a lowering peephole and docs/IR.md section 4 only;  a structural row memo is about 40 to 60 lines (the model:  kycMerkle 10847 rows, k 14;  the depth at k 15:  form (i) 6, the structural memo 16).  New form (iv):  a literal folded into ql, qr or qc of the arith gate (not estimated).
- Questions for the USER (docs/M11-PLAN.md section 9):  (a) the scope (form (iii) as the structural memo;  (ii) and (iv) stay open in O24);  (b) the structural key, not the pointer key;  (c) the IR rule ruling for each golden that changes;  (d) the Merkle depth stays 4, the depth table into SPEC.md 9 O24;  (e) form (iv) recorded in O24, no build;  (f) no new guest example (the goldens only);  (g) the open items stay;  (h) the slices (s1 in two sessions, s2 the close).
- Gates:  none ran (no code file changed).
- Next:  the USER rules on (a) to (h);  then s1 (~/Documents/glue-lang-M11-s1-PROMPT.md).

### s1 (2026-10-20)

- Rulings (USER 2026-10-20, the first message of the s1 session:  "Take all recommendations for (a) through (h)"):  each recommendation of docs/M11-PLAN.md section 9 taken, no ruling differs.  The m2 ruling (USER 2026-10-20, the first message of the s1-b session):  (r1), the recommendation:  a refusal row for the op in the memo key.  It adds one guest example to ruling (f), with no e2e row, no verifier and no fixture.
- Start condition MET.  s1:  HEAD 1f02fe5, the 3 kickoff docs unstaged.  s1-b:  HEAD 1f02fe5, the index tree c1321854 = the s1 staged tree, `git diff c1321854 HEAD --stat` = the s1 install only (9 files).
- Probe (~/Documents/glue-lang-M11-s1-probe/probe.sh and refuse.sh, the logs probe.log and refuse.log, scratch copies from `git archive HEAD`, the repository status unchanged):  the D3 inference is true.  In poseidon the 5 factors of each of the 80 sbox5 chains are one `const Value *` (905 products and 80 squares on one pointer;  the rows stay 3644).  The memo copy:  circuit, witness and satcheck rc 0 for each of the 9 expect rows;  instance, advice, fixed, public, gates, lookups and the fixed column count the same.

| Circuit | golden:  rows, k, copies | memo:  rows, k, copies | model (plan section 6):  rows, k |
|---|---|---|---|
| poseidon | 3644, 12, 4352 | 1972, 11, 3256 | 1972, 11 |
| transfer | 7288, 13, 8704 | 3740, 12, 6512 | 3740, 12 |
| kyc | 7291, 13, 8710 | 3743, 12, 6518 | 3743, 12 |
| kycMerkle | 21899, 15, 26178 | 10847, 14, 19602 | 10847, 14 |

- The model held exactly.  square, poly, range and nonzero (and quotSq) do not change.  The 19 same-ir rows keep their verdicts;  the 21 refuse rows keep their codes.
- Install (~/Documents/glue-lang-M11-s1-probe/install.sh, install.log):  src/circuit.c +88/-2.  `Lower` gets `memo`, `memo_cap` and `memo_count`.  New:  `memo_key` (an exhaustive switch on RowKind, no `default:`), `memo_hash` (FNV-1a over 9 key words), `memo_slot` (a linear probe) and `memo_row` (the probe, else `add_row` and an insert;  the table doubles at half load;  `oom` on a failed alloc).  `lower_value` calls `memo_row` at the CONST and the ARITH site.  The 4 goldens regenerated with build/langc:  test/circuit/{poseidon,transfer,kyc,kycMerkle}.{circuit,witness}.json.
- Mutants (~/Documents/glue-lang-M11-s1-probe/mutants.sh, mutants.log;  `make -B check` each, `git restore` from the index after each):  m1 (the probe removed) KILLED (8 FAIL lines:  circuit and witness of the 4 changed goldens);  m2 (the ARITH key without the op) SURVIVED in s1, because no current circuit has two ARITH rows with the same operand cells and different ops;  m3 (the ARITH key without the second operand) KILLED (8 FAIL lines);  m4 (the CONST key without the value) KILLED (8 FAIL lines);  the control GREEN.
- The m2 ruling (r1) in s1-b (~/Documents/glue-lang-M11-s1-b-probe/step0.sh, step0.log):  examples/memoKey.glue has one circuit, `memoKey public (q : Field) (s : Field) private (a : Field) (b : Field) := a * b == q; a + b == s` (6 rows, k 5, 8 copies).  test/circuit/refuse.txt row 22 is `1 WITNESS_FAIL witness memoKey memoKey q=6 s=6 a=2 b=3` (the binder order of the circuit:  the public binders first).  With the op in the key, the witness is refused (`the left side is 5, the right side is 6`);  q=6 s=5 a=2 b=3 passes satcheck.  m2 again:  KILLED (2 FAIL lines at row 22:  exit 0, and a file written).  The control `make -B check` rc 0 (examples 68, circuit refusals 22).  `make e2e` does not read refuse.txt.
- Gates (~/Documents/glue-lang-M11-s1-b-probe/gates.sh, sandbox off, the log gates.log, 164 s in all):  `make -B check` exit 0 (47 s):  native circuit 9, path 46, face 35, HIT 44;  examples 68, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 22, satcheck refusals 6;  node regressions 6, printer 2.  `make check-clang` exit 0.  `forge build --root contracts --sizes` exit 0.  `make e2e` exit 0 (42 s, 9 rows).  `forge test --root contracts` 100 passed.  `make e2e-ceremony SRS=~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs` exit 0 (60 s, 9 rows;  kycMerkle at k 14 with the k 15 file).  `make rehearsal` exit 0 (anvil 31337).  `make deployment-test` exit 0.
- The count delta against the M10 close:  examples 67 to 68, circuit refusals 21 to 22;  the other counts stay.
- IR check:  before e2e (the index against HEAD):  src/circuit.c, the 8 goldens, examples/memoKey.glue and test/circuit/refuse.txt.  After e2e (the worktree against the index, test/circuit and contracts/src/verifiers):  contracts/src/verifiers/{poseidon,transfer,kyc,kycMerkle}.sol only.  The fixtures of square, poly, range and nonzero changed (4 lines each, a fresh proof) and went back with `git restore`;  the fixtures of the 4 changed circuits are staged.  No IR form changed, and docs/IR.md does not change.
- Docs:  SPEC.md 7.1 (the memo rule) and 9 O24 (form (iii) DONE with the measured rows and k;  forms (ii) and (iv) open;  the depth table at s2);  docs/M11-PLAN.md (the Status line RULED, D3 tested, section 6 measured, sections 5 and 8 the m2 ruling);  docs/PLAN.md M11 row (s1 STAGED).
- Open items:  none new.
- Next:  the USER commits s1;  then s2, the close (~/Documents/glue-lang-M11-s2-PROMPT.md).

### s2: close (2026-10-20)

- Start condition MET:  HEAD d0b78f1 `M11 s1: share rows in the lowering (structural memo)` (USER), its parent 1f02fe5 (`M10 s5: close`), HEAD^{tree} ddd82a8a = the s1-b staged tree (23 paths), status empty.  The USER did not change the staged tree.
- Commits:  s1 d0b78f1 (with the plan docs/M11-PLAN.md, the docs/PLAN.md M11 row and `### kickoff`).  s2 changes docs only:  no change in src/, in domain/domain.lang, in halo2/, in contracts/, in the Makefile, in test/ and in examples/.
- The slice:
  - s1 d0b78f1:  form (iii) of O24 as the structural row memo of SPEC.md 7.1 (rulings (a) to (h)):  `memo_key`, `memo_hash`, `memo_slot` and `memo_row` in src/circuit.c, called at the CONST and the ARITH site of `lower_value`;  the 4 Poseidon goldens (poseidon 1972 rows, k 11;  transfer 3740, k 12;  kyc 3743, k 12;  kycMerkle 10847, k 14), their verifiers and fixtures;  the m2 ruling (r1):  examples/memoKey.glue and refuse.txt row 22;  23 paths.
- Mutants:  4 of 4 KILLED (s1 m1, m3 and m4;  m2 at row 22 after the m2 ruling (r1)).
- Gates at the close (the log ~/Documents/glue-lang-M11-s2-probe/check.log, sandbox on):  `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT 44;  examples 68, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 22, satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  These are the s1 counts.  The s2 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s1-b (`make check-clang`, `forge build --root contracts --sizes`, `make e2e` 9 rows, `forge test --root contracts` 100 passed, `make e2e-ceremony` 9 rows, `make rehearsal`, `make deployment-test`, each exit 0).
- The IR rule kept:  `git diff 1f02fe5 HEAD --stat -- src domain halo2 Makefile test examples contracts/src docs/IR.md` names 15 paths only:  src/circuit.c, examples/memoKey.glue, test/circuit/refuse.txt, the goldens and witnesses of poseidon, transfer, kyc and kycMerkle, and their 4 verifiers.  The other 4 goldens and verifiers (square, poly, range, nonzero) do not change.  No new IR form, no conv rule, no new check code, no change to the verifier generator, and docs/IR.md does not change.
- Docs:  SPEC.md 4 (the bullet `The structural row memo`) and 9 O24 (the depth table of docs/M11-PLAN.md D1 and the close sentence);  docs/PLAN.md (the M11 row DONE);  docs/M11-PLAN.md (the Status line, CLOSED);  this file (`### s2: close`).
- Open after M11:  O24 (forms (ii), the degree-5 gate, and (iv), the literal fold:  each needs a USER ruling first;  no milestone named);  O23 (the deployment target and the check of the Axiom copy of the ceremony file;  USER items);  O3, O12 (c), O13, O14, O15, O16 and O19 do not change (D7).
- Next:  the USER picks M12 (O24 form (ii) or (iv), O23, or another topic).

## M12

### kickoff (2026-10-20)

- Start condition MET:  HEAD 1025aef `M11 s2: close` (USER), its parent d0b78f1, HEAD^{tree} 0f21f9a9 = the s2 staged tree, status empty.
- Topic:  form (iv) of O24, the literal fold (the USER filled the topic slot:  "Topic: form (iv)").  The kickoff question:  docs/M11-PLAN.md D5 says that the fold needs no new IR form and no back-end change;  M11 COMMON line 5 calls it "a new row kind".
- Plan:  docs/M12-PLAN.md DRAFT (sections 1 to 10;  not staged:  it goes with the M12 s1 commit after the rulings, with the docs/PLAN.md M12 row and this section).
- Probe (~/Documents/glue-lang-M12-kickoff-probe/run.sh, model.awk, flatten.jq, fold-circuit.jq, fold-witness.jq;  the log run.out;  scratch copies from `git archive HEAD`, the real build/langc and halo2/target/debug/glue-halo2;  repository status empty before and after):  build/langc reproduces the poseidon golden (the memo baseline).  model.awk models two scopes on each golden:  T1, the literal fold (a literal operand into ql, qr, qm or qc;  a CONST row goes when each use is an ARITH operand), and T2, T1 and the affine fold (an ARITH row with only ARITH operand uses goes into each consumer whose form still fits the gate).

| Circuit | memo:  rows, k, copies | T1:  rows, k, copies | T2:  rows, k, copies |
|---|---|---|---|
| poseidon | 1972, 11, 3256 | 1769, 11, 2477 | 990, 10, 1698 |
| transfer | 3740, 12, 6512 | 3537, 12, 4954 | 1979, 11, 3396 |
| kyc | 3743, 12, 6518 | 3540, 12, 4960 | 1982, 11, 3402 |
| kycMerkle | 10847, 14, 19602 | 10644, 14, 14928 | 5970, 13, 10254 |
| poly | 9, 5, 13 | 8, 5, 12 | 7, 5, 11 |
| range | 4, 9, 4 | 3, 9, 3 | 3, 9, 3 |

- Counts:  square and nonzero do not change.  Each Poseidon golden has 204 CONST rows (203 with ARITH operand uses only).  The ARITH rows with a literal operand:  poseidon ADD 194 and MUL 585 (779 literal operand copies), transfer and kyc ADD 388 and MUL 1170, kycMerkle ADD 1164 and MUL 3510;  no ARITH row has two literal operands.  T2 removes each of them (poseidon:  194 ADD and 585 MUL rows).
- Depth at k 15 (level rows = (kycMerkle - kyc) / 4):  memo 1776, max depth 16, depth 20 39263 rows k 16;  memo + T1 1776, 16, 39060 k 16;  memo + T2 997, 30, 21922 rows k 15 (a depth-20 tree fits the pinned SRS).
- The folded copy (jq on a copy of the poseidon golden):  row 293 (a MUL by a literal:  qr = m, qm = 0), row 281 (an ADD of a literal:  qr = 0, qc = rc), row 296 (an ADD of two MUL-by-literal rows:  ql = m0, qr = m1, a and b copied from rows 3 and 4);  copies 3256 to 3254.  satcheck, check, keygen, prove, verify and solidity rc 0 (the verifier 46893 bytes).  The bad witness (c of row 296 changed):  satcheck rc 1 (`SAT_GATE ... on row 296`), check rc 1 and prove rc 1 (`Constraint 0 in gate 0 ('arith') is not satisfied ... at offset 296`).
- The answer:  D5 of M11 is true for the IR, the back end and the verifier generator (no new IR form;  docs/IR.md has no row kinds).  "A new row kind" is true for src/circuit.c only:  `Row` must hold the coefficients (docs/M12-PLAN.md D1, question (c)).
- Questions for the USER (docs/M12-PLAN.md section 9):  (a) the scope T1 and T2, T2 after T1;  (b) the IR rule ruling of D7;  (c) the coefficients on `Row` (r1) against a new `RowKind` value (r2);  (d) the refusal row for the key mutant (examples/foldKey.glue, refuse.txt row 23);  (e) the Merkle depth stays 4, the depth table into SPEC.md 9 O24;  (f) docs/IR.md does not change;  (g) the open items stay;  (h) the slices (s1 T1 in two sessions, s2 T2 in two sessions, s3 the close).
- Gates:  none ran (no code file changed).
- Next:  the USER rules on (a) to (h);  then s1 (~/Documents/glue-lang-M12-s1-PROMPT.md).

### s1 (2026-10-20)

- Start condition MET:  HEAD 1025aef `M11 s2: close`, status ` M docs/PLAN.md`, ` M docs/STATUS.md`, `?? docs/M12-PLAN.md`.
- Rulings (the USER, before s1):  "Take all recommendations for (a) through (h)."  Thus (a) T1 and T2 in M12, T2 after T1;  (b) the IR rule of D7;  (c) (r1), the 4 coefficients on `Row`;  (d) examples/foldKey.glue and test/circuit/refuse.txt row 23;  (e) the Merkle depth stays 4, the D4 table goes into SPEC.md 9 O24;  (f) docs/IR.md does not change;  (g) the open items of D8 stay open;  (h) the slices of section 10.  docs/M12-PLAN.md Status RULED 2026-10-20;  docs/PLAN.md M12 row IN PROGRESS.
- Build (src/circuit.c only, ruling (c) (r1)):  `Row` holds ql, qr, qm and qc (Fr) in place of the op and k;  a CONST row holds k in qc.  `lower_operand` folds a Field literal argument of `+`, `-` or `*` into the coefficients (`arith_coeffs`:  each operand is S * cell + C;  S = 1, C = 0 for a wire;  S = 0, C = k for a literal);  a literal that is a constraint side keeps its CONST row.  `uses_a` and `uses_b` give the operand cells (ql or qm not 0;  qr or qm not 0).  `memo_key` holds the kind, the 4 coefficients and the operand cells (37 words;  a cell that is not an operand is 0).  `fixed_at` writes the coefficients;  `put_copies` writes a copy only for an operand cell;  `fill_advice` computes c = ql*a + qr*b + qm*a*b + qc (a cell that is not an operand holds 0).  No new `default:`, no new globals.
- Goldens (rows, k, copies = the D3 T1 column, exact):  poseidon 1769, 11, 2477;  transfer 3537, 12, 4954;  kyc 3540, 12, 4960;  kycMerkle 10644, 14, 14928;  poly 8, 5, 12;  range 3, 9, 3.  square and nonzero do not change.  satcheck gives ok on each golden.  The scratch build (git archive HEAD + the working-tree src/circuit.c;  ~/Documents/glue-lang-M12-s1-scratch/run.sh) and build/langc give the same 12 files.
- A fixture outside the plan list:  test/satcheck/lookup.witness.json is a range witness with b = 256 (the refusal row expects SAT_LOOKUP).  It had the 4 advice rows of the memo range circuit, so after T1 satcheck gave SAT_IR (`advice: a column has the wrong length`).  s1 wrote it again from the new range witness (next = 257, b = 256;  3 rows).  The refusal row keeps SAT_LOOKUP.
- `make -B check` exit 0 (it also runs `make check-clang`):  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 23, satcheck refusals 6;  node regressions 6, printer 2.
- Mutants (`make -B check` each, `git restore` from the index;  ~/Documents/glue-lang-M12-s1-scratch/mutants.sh):  f1 (the fold removed) KILLED, 15 FAIL lines (the goldens of range, poly, poseidon first);  f2 (the coefficients out of the memo key) KILLED, 12 FAIL lines (the goldens of the 4 Poseidon circuits, memoKey row 22, foldKey row 23);  f2q (only qc out of the key) KILLED, 6 FAIL lines (the goldens of kyc and kycMerkle, foldKey row 23);  f3 (the qc of an ADD fold set to 0) KILLED, 15 FAIL lines (the goldens).  The first f2 run did not build (tcc -Werror, the unused `i`);  the second run voids `q` and `i`.  The control `make -B check` after the mutants:  exit 0.
- Staged (tree 96bec8f4 on 1025aef, 16 files):  src/circuit.c, examples/foldKey.glue, test/circuit/refuse.txt, test/satcheck/lookup.witness.json, the circuit and witness goldens of poseidon, transfer, kyc, kycMerkle, poly and range.  docs/PLAN.md, docs/STATUS.md and docs/M12-PLAN.md stay unstaged until s1-b.
- Gates not run in s1:  `forge build`, `make e2e`, the IR check, `forge test`, `make e2e-ceremony`, `make rehearsal`, `make deployment-test` (s1-b, the sandbox off).
- Next:  s1-b (~/Documents/glue-lang-M12-s1-b-PROMPT.md).

### s1-b (2026-10-20)

- Start condition MET:  HEAD 1025aef, tree 96bec8f4 (the 16 staged files of s1), ` M docs/PLAN.md`, ` M docs/STATUS.md`, `?? docs/M12-PLAN.md`.
- Docs:  SPEC.md 7.1 (the fold rule:  item 2, a Field literal that is a constraint side keeps its constant row;  item 3, the coefficients ql, qr, qm, qc of a row with a literal operand, with no constant row;  the memo key is the row kind, the 4 coefficients and the operand cells;  a copy only for an operand cell) and 9 O24 (form (iv) T1 DONE, the D3 T1 table;  T2 at s2;  form (ii) open).  docs/PLAN.md M12 row:  the gates GREEN in s1-b.
- Gates (~/Documents/glue-lang-M12-s1-b/gates.sh, the sandbox off, 187 s:  check 40 s, e2e 59 s, ceremony 77 s):  `make -B check` exit 0 (the s1 counts:  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 23, satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (9 rows);  `forge test --root contracts` 100 passed;  `make e2e-ceremony` exit 0 (9 rows);  `make rehearsal` exit 0 (anvil 31337);  `make deployment-test` exit 0.
- IR check before e2e (the index against HEAD, test/circuit and contracts/src/verifiers):  the 12 goldens and refuse.txt only.  After e2e (the worktree against the index):  the verifiers of poseidon, transfer, kyc, kycMerkle, poly and range, and the fixtures of all 8 circuits.  The fixtures of square and nonzero (4 lines each) went back with `git restore`.
- The range verifier is smaller:  contracts/src/verifiers/range.sol 49890 to 45802 bytes.  The 5 other changed verifiers keep their size (kyc and kycMerkle 49890;  poseidon, transfer and poly 46893).  Cause:  halo2/src/circuit.rs enables equality only on a column that a copy names.  The T1 range circuit has no copy on advice b (copies 4 to 3:  the add row takes its literal from qc, so b is not an operand), so the permutation argument has one column less.  No IR form, back-end or generator change;  `forge test` (100), e2e, e2e-ceremony and the rehearsal pass with the new verifier.  The s1-b prompt said that the verifier sizes do not change:  that is true only when the set of copied columns stays.
- `forge build --sizes` prints one `Halo2Verifier` row for the 8 verifiers with the same contract name (runtime 5,802 to 5,801 bytes):  compare the verifier sizes with `wc -c` on the sources.
- Staged on 1025aef (32 files):  the 16 files of s1, the verifiers and the fixtures of the 6 changed circuits (12 files), SPEC.md, docs/M12-PLAN.md, docs/PLAN.md and docs/STATUS.md.  Commit message:  ~/Documents/glue-lang-M12-s1-commit-msg.txt.
- Next:  the USER commits s1;  then s2 (T2, ~/Documents/glue-lang-M12-s2-PROMPT.md).

### s2 (2026-10-20)

- The build (T2, D5):  `fold_affine` in src/circuit.c, a pass at the end of `lower_circuit` (184 lines added).  It counts the uses of each row (the ARITH operands;  a range row and each side of a check pin a row), computes the form of each ARITH row bottom-up (a sum of monomials of degree 2 at most), and lets each consumer take its operand rows (both, else x, else y) when the form fits the gate (at most two cells, degree 2 at most, no square of one cell when two cells occur).  A row goes when each consumer took it.  The rows above the binder rows get new numbers, and so do the cells of the rows and of the checks.  The memo of s1 stays.
- The scratch build (HEAD and the working-tree src/circuit.c;  ~/Documents/glue-lang-M12-s2-scratch/run.sh):  rows, k and copies equal the D3 T2 column:  poseidon 990, 10, 1698;  transfer 1979, 11, 3396;  kyc 1982, 11, 3402;  kycMerkle 5970, 13, 10254;  poly 7, 5, 11;  range 3, 9, 3.  model.awk on the memo goldens of 1025aef gives the same values.  satcheck rc 0 on each golden.  The goldens of square, nonzero and range do not change (byte-identical), so test/satcheck/lookup.witness.json stays.
- The copied columns (Traps (M12 s1-b)):  the set of each regenerated golden equals HEAD (advice0, advice1, advice2, instance0).  k changes for the 4 Poseidon circuits (poseidon 11 to 10, transfer 12 to 11, kyc 12 to 11, kycMerkle 14 to 13):  their verifiers can change size at e2e.  poly keeps k 5 and its columns.
- `make -B check` exit 0 with the s1 counts (examples 69, circuits 9, same-ir 19, circuit refusals 23;  the other counts as the M11 close).  No new refusal row.
- Mutants (section 8;  `make -B check` each, `git restore` from the index after each):  f4 the use check removed:  KILLED (rc 2, 13 FAIL lines);  f5 the fit check removed:  KILLED (rc 2, 13 FAIL lines);  f6 the renumbering of the check cells removed:  KILLED (rc 2, 13 FAIL lines).  The golden diffs of poly and the 4 Poseidon circuits come first in each log.
- Staged before e2e:  src/circuit.c, docs/STATUS.md and the circuit and witness goldens of poly, poseidon, transfer, kyc and kycMerkle (12 files).  The gates of COMMON line 5 go to s2-b (~/Documents/glue-lang-M12-s2-b-PROMPT.md).

### s2-b (2026-10-20)

- Start condition MET:  HEAD 152c6d4, tree 7e0c2699 (the 12 staged files of s2), no other change.
- Docs:  SPEC.md 7.1 (the affine fold:  an arith row whose uses are all operands of arith rows goes into each consumer when the form of the consumer still fits the gate (at most two cells, degree 2 at most, no square of one cell when two cells occur);  the row goes when each consumer takes it;  a range row and each side of a check also use a row) and 9 O24 (form (iv) T2 DONE, the D3 T2 column;  the depth table of D4 comes at s3;  form (ii) open).  docs/PLAN.md M12 row:  s1 DONE (152c6d4), s2 DONE, the gates GREEN in s2-b.
- Gates (~/Documents/glue-lang-M12-s2-b/gates.sh, the sandbox off, 204 s:  check 56 s, check-clang 1 s, forge build 0 s, e2e 75 s, forge test 0 s, ceremony 63 s, rehearsal 5 s, deployment-test 3 s):  `make -B check` exit 0 (the s1 counts:  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 23, satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (9 rows);  `forge test --root contracts` 100 passed;  `make e2e-ceremony` exit 0 (9 rows);  `make rehearsal` exit 0 (anvil 31337);  `make deployment-test` exit 0.
- IR check before e2e (the index against HEAD, test/circuit and contracts/src/verifiers):  the 10 goldens of poly, poseidon, transfer, kyc and kycMerkle only.  After e2e (the worktree against the index):  the verifiers of the same 5 circuits, and the fixtures of all 8 circuits.  The fixtures of square, nonzero and range (4 lines each) went back with `git restore`.  No verifier and no golden of square, nonzero or range changed.
- The verifier sizes (`wc -c` before and after e2e) do not change:  kyc.sol and kycMerkle.sol 49890;  poseidon.sol, transfer.sol and poly.sol 46893.  The copied columns stay (advice0, advice1, advice2, instance0).  In the 4 Poseidon verifiers the vk digest, k, n_inv, the omega words and the fixed commitments change (k:  poseidon 11 to 10, transfer 12 to 11, kyc 12 to 11, kycMerkle 14 to 13).  Each of them is one 32-byte word, thus a change of k does not change the size.  In poly.sol only the vk digest and the fixed commitments change.  `forge build --sizes`:  the `Halo2Verifier` row stays 5,801 bytes runtime.
- Staged on 152c6d4 (24 files):  the 12 files of s2, the verifiers and the fixtures of the 5 changed circuits (10 files), SPEC.md and docs/PLAN.md.  Commit message:  ~/Documents/glue-lang-M12-s2-commit-msg.txt.
- Next:  the USER commits s2;  then s3 (the close, ~/Documents/glue-lang-M12-s3-PROMPT.md).

### s3: close (2026-10-20)

- Start condition MET:  HEAD 54cda59 `M12 s2: the affine fold (T2)` (USER), its parent 152c6d4 (`M12 s1: the literal fold (T1)`), HEAD^{tree} c4ae9f99 = the s2-b staged tree (24 paths), status empty.  The USER did not change the staged tree.
- Commits:  s1 152c6d4 (T1, with the plan docs/M12-PLAN.md, the docs/PLAN.md M12 row and `### kickoff`);  s2 54cda59 (T2).  s3 changes docs only:  no change in src/, in domain/domain.lang, in halo2/, in contracts/, in the Makefile, in test/ and in examples/.
- The slices:
  - s1 152c6d4:  T1 of O24 form (iv) (SPEC.md 7.1 item 3, rulings (a) to (h)):  the coefficient rows (ruling (c) (r1)), a literal operand into ql, qr, qm and qc of its arith row, the memo key with the coefficients;  examples/foldKey.glue and refuse.txt row 23 (ruling (d));  the 6 goldens of the D3 T1 column, their verifiers and fixtures;  test/satcheck/lookup.witness.json (the range witness, 4 to 3 rows);  range.sol 49890 to 45802 bytes (no copy on advice b).
  - s2 54cda59:  T2, the affine fold (`fold_affine` in src/circuit.c, a pass at the end of `lower_circuit`);  the 5 changed goldens of the D3 T2 column (poseidon 990 rows, k 10;  transfer 1979, k 11;  kyc 1982, k 11;  kycMerkle 5970, k 13;  poly 7, k 5), their verifiers and fixtures.
- Mutants:  7 of 7 KILLED (s1 f1, f2, f2q (qc only) and f3;  s2 f4 to f6).
- Gates at the close (the log ~/Documents/glue-lang-M12-s3/check.log, sandbox on):  `make -B check` exit 0:  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 23, satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  These are the s2-b counts.  The s3 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s2-b (`make check-clang`, `forge build --root contracts --sizes`, `make e2e` 9 rows, `forge test --root contracts` 100 passed, `make e2e-ceremony` 9 rows, `make rehearsal`, `make deployment-test`, each exit 0).  After the control the status names only the 3 docs of s3 that had changed then (no change in test/circuit or contracts).
- The IR rule kept (D7, ruling (b)):  no new IR form, no new `RowKind` value, no conv rule, no new check code, no change to the verifier generator, and docs/IR.md does not change (ruling (f)).  The goldens and verifiers of square and nonzero do not change in M12;  range changes in s1 only.
- Docs:  SPEC.md 4 (the bullet `The literal fold`) and 9 O24 (form (iv) DONE, the depth table of docs/M12-PLAN.md D4 and the close sentence);  docs/PLAN.md (the M12 row DONE);  docs/M12-PLAN.md (the Status line, CLOSED);  this file (`### s3: close`).
- The depth (D4):  under T2 a level is 997 rows and the Merkle depth at k 15 is 30;  a depth-20 tree is 21922 rows, k 15, and fits the pinned SRS.  Ruling (e):  the depth stays 4.
- Open after M12:  O24 (form (ii), the degree-5 gate:  it needs a new IR form, the pow5 gate, and a USER ruling first;  no milestone named);  O23 (the deployment target and the check of the Axiom copy of the ceremony file;  USER items);  O3, O12 (c), O13, O14, O15, O16 and O19 do not change (D8, ruling (g)).
- Next:  the USER commits s3;  then the USER picks the next milestone (O24 form (ii), a Merkle depth change, O23, or another topic).

## M13

### kickoff (2026-10-20)

- Start condition MET:  HEAD 932ca87 `M12 s3: close` (USER), its parent 54cda59, HEAD^{tree} 4c75526b, status empty.
- Topic:  (ii), a Merkle depth change for kycMerkle (the USER answer in kickoff attempt 5:  "(ii) kycMerkle depth").  The probe ran by hand in the main loop:  in attempt 5 the probe agents died (a rate limit, then a safeguard stop).
- Plan:  docs/M13-PLAN.md DRAFT (sections 1 to 10;  not staged:  it goes with the M13 s1 commit after the rulings, with the docs/PLAN.md M13 row and this section).
- Probe (~/Documents/glue-lang-M13-kickoff-probe/run.sh, the log run.out, REPORT.md;  scratch copies from `git archive HEAD`, the real build/langc and glue-halo2 of HEAD;  status empty before and after):  a depth-D generator on the current examples/kycMerkle.glue (the level-3 block repeated with the M9 s4 renames).  `gen 4` = examples/kycMerkle.glue byte for byte;  build/langc reproduces the depth-4 golden circuit and witness.
- The table (rows, k, copies):  depth 4:  5,970, 13, 10,254;  depth 8:  9,958, 14, 17,106;  depth 20:  21,922, 15, 37,662.  The model of M12 s3 exactly.  Depth 20:  satcheck, check, keygen (the development SRS and the ceremony file), prove, verify and solidity rc 0;  the verifier 49,890 B (58 constant lines change);  a bad witness (the binder s5) refused by satcheck (SAT_COPY) and by prove.
- Counts:  no gate ran;  the M12 close counts stay (examples 69, circuit refusals 23, forge test 100, e2e 9 rows).
- The answer:  a depth change is a new circuit text, not a new IR form (the gate set, the columns and the lookup stay).  The generator of kycMerkle.glue is outside the repository (~/Documents/glue-lang-M9-s4-probe/probe.sh).  "SPEC.md section 8" of M12 ruling (e) names no depth text:  section 8 is the empty template;  the depth sites are SPEC.md sections 4 and 9 O24.
- Questions:  docs/M13-PLAN.md section 9, (a) to (k) (the depth value 20, the reversal of M12 ruling (e), the generator, the circuit name, the SPEC.md sites, the e2e row and the fixture, a new refuse row, the contracts tests and README, the header, the open items, the slices).
- Gates:  none ran.
- Next:  the USER rules section 9, then ~/Documents/glue-lang-M13-s1-PROMPT.md.

### s1 (2026-10-20)

- Rulings:  the USER ruled docs/M13-PLAN.md section 9, (a) to (k), on 2026-10-20:  each question takes its recommendation (the plan Status line and section 9 say `RULED 2026-10-20`;  the docs/PLAN.md M13 row is IN PROGRESS).
- Start condition MET:  HEAD 932ca87 `M12 s3: close`;  status ` M docs/PLAN.md`, ` M docs/STATUS.md`, `?? docs/M13-PLAN.md`.
- Runner:  ~/Documents/glue-lang-M13-s1-probe/run.sh (a copy of the kickoff generator with D = 20;  the base is `git show HEAD:examples/kycMerkle.glue`), the log run.out.  `gen 4` = the HEAD file (cmp);  `gen 20` = the kickoff scratch/d20.glue (cmp);  lines 22 to 228 of the output = `gen 20`.
- examples/kycMerkle.glue:  228 lines, 881,808 B (248,297 B at depth 4);  line 228 has 859,915 chars and 6,112 hints;  the root compare is `m19s64k0 == root`.  The header (ruling (i), lines 1 to 21):  depth 20, 2^20 leaves, b0 to b19, s0 to s19, the hash of level 19, the generator path.
- The goldens:  test/circuit/kycMerkle.circuit.json 5,792,505 B (rows 21,922, k 15, copies 37,662, public 5, fixed 7, lookups 1:  the model);  kycMerkle.witness.json 4,268,021 B.  Both are equal to the kickoff output (cmp).  The times (sandbox on):  langc check 39.2 s, circuit 36.8 s, witness 43.4 s, satcheck (rc 0) 5.3 s.
- The rows:  test/circuit/expect.txt row 9 (the D5 values;  the root 5733471641131741361167289734777355811818729985824207359272481037795046878975 from `langc eval`;  `sender=` at the end);  test/circuit/refuse.txt row 20 (the root + 1, from `bc`) and the new row 24 (ruling (g):  b7=2, WITNESS_FAIL at the bit check of level 7:  `the left side is 4, the right side is 2`).
- contracts/test/Verifiers.t.sol:  the comment (the depth-20 path bits, the siblings 11 (i + 1)), the root literal (the D5 root) and the message (`the depth 20 tree`).  `forge test` runs in s1-b, after `make e2e` writes the fixture.  SPEC.md section 4 line 1238:  `depth 20; k 15`.
- `make -B check` (~/Documents/glue-lang-M13-s1-probe/check.log):  rc 0;  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 24 (23 to 24, ruling (g)), satcheck refusals 6;  node regressions 6, printer 2.
- Mutants (section 8;  ~/Documents/glue-lang-M13-s1-probe/mutants.sh, mutants.out;  `make -B check` each, good.glue back after each, cmp):  n1 (the bit check `b12 * b12 == b12` removed) KILLED (rc 2, 2 FAIL lines:  the circuit and witness goldens);  n2 (the select of level 10 swapped, the hint to match) KILLED (rc 2, 2 FAIL lines, the goldens);  n3 (the root compare on level 18) KILLED (rc 2, 2 FAIL lines, the goldens);  n4 (depth 19, `run.sh gen 19`) KILLED (rc 2, 4 FAIL lines:  the goldens and refuse.txt rows 20 and 24).  4 of 4 KILLED.
- Files (UNSTAGED):  examples/kycMerkle.glue, the 2 kycMerkle goldens, test/circuit/expect.txt, test/circuit/refuse.txt, contracts/test/Verifiers.t.sol, SPEC.md, docs/M13-PLAN.md, docs/PLAN.md, docs/STATUS.md.  No src/, halo2/ or docs/IR.md change.
- Next:  ~/Documents/glue-lang-M13-s1-b-PROMPT.md (the gates, the IR check, the fixture, contracts/README.md line 58, the RSS with the sandbox off, then stage).

### s1-b (2026-10-20)

- Start condition MET:  HEAD 932ca87 `M12 s3: close`;  the 10 paths of s1 UNSTAGED (SPEC.md, contracts/test/Verifiers.t.sol, docs/PLAN.md, docs/STATUS.md, examples/kycMerkle.glue, test/circuit/expect.txt, the 2 kycMerkle goldens, test/circuit/refuse.txt;  `?? docs/M13-PLAN.md`);  examples/kycMerkle.glue = ~/Documents/glue-lang-M13-s1-probe/good.glue (cmp).
- Gates (~/Documents/glue-lang-M13-s1-b/gates.sh, the sandbox off, 786 s:  check 463 s, e2e 149 s, ceremony 122 s):  `make -B check` exit 0 (native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 24, satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (9 rows);  `forge test --root contracts` 100 passed;  `make e2e-ceremony` exit 0 (9 rows);  `make rehearsal` exit 0 (anvil 31337, the kycMerkle gate admits the proof from the anvil account 0);  `make deployment-test` exit 0.
- The time of `make -B check` is 463 s (M12 s1-b:  40 s).  The D8 estimate (about 60 s more) is too low:  langc check, circuit and witness on the depth-20 example take about 40 s each (s1), and the gate runs them more than once.
- IR check before e2e (the worktree against HEAD, test/circuit and contracts/src/verifiers):  the 2 kycMerkle goldens, expect.txt and refuse.txt only.  After e2e:  contracts/src/verifiers/kycMerkle.sol (49,890 B, 988 lines, 29 lines out and 29 lines in;  all 58 are `mstore` constant lines) and the 8 fixtures.  The 7 other fixtures (4 lines each) went back with `git restore`.  The repository has 8 fixtures (the e2e table has 9 rows, but quotSq writes the square fixture):  the "9 fixtures" and "the 8 other fixtures" of the s1-b prompt and COMMON are a count error.
- contracts/test/fixtures/kycMerkle.proof.json:  8,466 B;  the first instance is 0x0cad07aa..6eff = 5733471641131741361167289734777355811818729985824207359272481037795046878975, the D5 root (`bc`).
- contracts/README.md line 58 (`forge test --root contracts --gas-report`, the log ~/Documents/glue-lang-M13-s1-b/gas-report.log;  the runtime size = the deployment size 6,254 minus the 32-byte constructor;  the `verifyProof` number = the max column, the valid proof):  `| kycMerkle | 6,222 | 1,396,935 | 316,579 |` (the deployment gas 1,396,947 to 1,396,935;  the runtime size and `verifyProof` do not change).  `forge build --sizes` prints one `Halo2Verifier` row for the 8 verifiers (runtime 5,801 B).
- The README rows of the other circuits are not in the M13 scope.  The same gas report gives other `verifyProof` max values for 4 of them (kyc 316,319, range 296,819, poseidon 277,741, transfer 278,462;  the README:  316,454, 313,566, 277,871, 278,592):  these rows did not change after the M12 verifier changes.  A USER item.
- RSS (ruling (j), `/usr/bin/time -l`, the sandbox off, halo2/target/debug/glue-halo2 on test/circuit/kycMerkle.circuit.json and kycMerkle.witness.json, the keys and the proof under ~/Documents/glue-lang-M13-s1-b/):  `keygen --dev-srs` 138,641,408 B max RSS (139 MB), 19.7 s;  `prove` 272,515,072 B max RSS (273 MB), 17.0 s.  Both are far below the 4 GB limit.
- Staged on 932ca87 (13 files):  the 10 files of s1 (SPEC.md, contracts/test/Verifiers.t.sol, examples/kycMerkle.glue, test/circuit/expect.txt, the 2 kycMerkle goldens, test/circuit/refuse.txt, docs/M13-PLAN.md, docs/PLAN.md, docs/STATUS.md;  the 3 M13 docs), contracts/src/verifiers/kycMerkle.sol, contracts/test/fixtures/kycMerkle.proof.json and contracts/README.md.  Commit message:  ~/Documents/glue-lang-M13-s1-commit-msg.txt.
- Next:  the USER commits s1;  then s2 (the close, ~/Documents/glue-lang-M13-s2-PROMPT.md).

### s2: close (2026-10-20)

- Start condition MET:  HEAD 79fe06d `M13 s1: kycMerkle at depth 20` (USER), its parent 932ca87 (`M12 s3: close`), HEAD^{tree} 3a1236ae = the s1-b staged tree (13 paths), status empty.  The USER did not change the staged tree.
- Commits:  s1 79fe06d (the depth-20 kycMerkle, with the plan docs/M13-PLAN.md, the docs/PLAN.md M13 row and the sections `### kickoff`, `### s1` and `### s1-b`).  s2 changes docs only:  no change in src/, in domain/domain.lang, in halo2/, in contracts/, in the Makefile, in test/ and in examples/.
- The slice s1 79fe06d (built in s1, the gates in s1-b):  examples/kycMerkle.glue at depth 20 (2^20 leaves;  the generator stays outside the repository, ruling (c));  the 2 kycMerkle goldens (21,922 rows, k 15, 37,662 copies);  test/circuit/expect.txt row 9 and test/circuit/refuse.txt rows 20 and 24 (ruling (g):  b7=2, WITNESS_FAIL;  circuit refusals 23 to 24);  contracts/test/Verifiers.t.sol (the D5 root);  SPEC.md section 4 (`depth 20; k 15`);  contracts/src/verifiers/kycMerkle.sol (58 `mstore` constant lines);  contracts/test/fixtures/kycMerkle.proof.json;  contracts/README.md line 58.
- Mutants:  4 of 4 KILLED (s1 n1 to n4).
- Gates at the close (~/Documents/glue-lang-M13-s2/gates.sh, the log check.log and gates.out there, the sandbox off):  `make -B check` exit 0, 278 s (s1-b:  463 s):  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 24, satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  These are the s1-b counts.  The s2 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s1-b (`make check-clang`, `forge build --root contracts --sizes`, `make e2e` 9 rows, `forge test --root contracts` 100 passed, `make e2e-ceremony` 9 rows, `make rehearsal`, `make deployment-test`, each exit 0).  After the control the status names only the 4 docs of s2.
- The IR rule kept (D2):  no new IR form, no new `RowKind` value, no conv rule, no new check code, no change to the verifier generator, no change in src/ or halo2/, and docs/IR.md does not change.  Only the kycMerkle circuit, its verifier (49,890 B, 58 `mstore` constant lines) and its fixture change in M13.
- Docs:  SPEC.md 9 O24 (the reversal of M12 ruling (e), USER ruling (b);  the table `Measured, M13`;  the close sentence);  docs/PLAN.md (the M13 row DONE);  docs/M13-PLAN.md (the Status line, CLOSED);  this file (`### s2: close`).
- The measured values (s1 and s1-b):  kycMerkle depth 20, 21,922 rows, k 15, 37,662 copies;  the verifier 49,890 B;  the proof 8,466 B;  keygen `--dev-srs` 139 MB max RSS, 19.7 s;  prove 273 MB max RSS, 17.0 s;  `make -B check` 463 s in s1-b and 278 s in s2 (M12:  40 s).
- Open after M13:  O24 (form (ii), the degree-5 gate:  it needs a new IR form, the pow5 gate, and a USER ruling first;  no milestone named);  O23 (the deployment target and the target fields, the literal word VALUE:  USER items);  the contracts/README.md rows of kyc, range, poseidon and transfer do not agree with the s1-b gas report (~/Documents/glue-lang-M13-s1-b/gas-report.log;  `verifyProof` max kyc 316,319, range 296,819, poseidon 277,741, transfer 278,462;  the README:  316,454, 313,566, 277,871, 278,592):  a USER item (the recommendation:  refresh the 8 rows from one `forge test --root contracts --gas-report` in a separate slice after M13).  The other open items of section 9 do not change.
- Next:  the USER commits s2;  then the USER picks the next milestone (O24 form (ii), the README rows, O23, or another topic).

### readme rows (2026-10-20)

- Start condition MET:  HEAD e305568 `M13 s2: close` (USER) on 79fe06d;  `git status --short` empty.
- USER ruling:  refresh all 8 rows of contracts/README.md from one gas report.
- Gate (~/Documents/glue-lang-M13-readme/gates.sh, the sandbox off):  `forge test --root contracts --gas-report` exit 0, 100 passed (9 suites);  `git status --short` is empty before and after.  The log:  ~/Documents/glue-lang-M13-readme/gas-report.log.
- The rows (~/Documents/glue-lang-M13-readme/rows.awk on the log;  the runtime size = Deployment Size minus 32;  the deployment gas = Deployment Cost;  `verifyProof` = the max column, the valid proof):  5 rows change.  range 6,217 | 1,395,783 | 313,566 to 5,646 | 1,272,518 | 296,819;  poly 5,802 | 1,306,103 to 5,801 | 1,305,875 (`verifyProof` 278,072 does not change);  poseidon 1,305,911 | 277,871 to 1,305,851 | 277,741 (the size 5,801 does not change);  transfer 5,802 | 1,306,127 | 278,592 to 5,801 | 1,305,899 | 278,462;  kyc 6,280 | 1,409,440 | 316,454 to 6,222 | 1,396,899 | 316,319.  The square, nonzero and kycMerkle rows do not change (kycMerkle `6,222 | 1,396,935 | 316,579`, as in s1-b).
- The new `verifyProof` values of kyc, range, poseidon and transfer are the values of the s1-b gas report.  This closes the README item of `### s2: close`.
- Only contracts/README.md and this file change.  No code, verifier, fixture or IR change.
- Next:  the USER commits;  then the USER picks the next milestone (O24 form (ii), O23, or another topic).

## M14

### kickoff (2026-10-20)

- Start condition MET:  HEAD 5bf112c `M13 readme: refresh the 8 gas rows` (USER), its parent e305568, HEAD^{tree} 7fd4bb88, status empty.
- Topic:  O24 form (ii), the degree-5 S-box gate (pow5).  The kickoff text came with the pick placeholder;  the USER answer (AskUserQuestion):  "O24 form (ii) pow5".
- Plan:  docs/M14-PLAN.md DRAFT (sections 1 to 10).  At the USER request, the draft plan, the docs/PLAN.md M14 row and this section go in a separate kickoff commit before the section 9 rulings.
- Probe (~/Documents/glue-lang-M14-kickoff-probe/run.sh, the log run.out, rows.jq, model.py, REPORT.md;  one read-only agent;  no build, no make;  status empty before and after):  the halo2_proofs v0.3.0 degree rules, jq on the goldens, a proof-size model.
- The answer:  the S-box is 4 arith rows and 9 copies now (poseidon rows 280 to 283);  each hash has 80 S-boxes on binders;  kycMerkle has 1,760 (7,040 of its 21,922 rows).  The degree is 3 for square, nonzero, poly, poseidon and transfer, and 5 for range, kyc and kycMerkle (the lookup).  The proof model gives the proof size of each of the 8 fixtures exactly.
- The variants (MODEL;  kycMerkle rows, k, copies;  the degree):  (ii-a) 16,642, 15, 25,342, degree 6;  (ii-b) 16,642, 15, 25,342, degree 5 (poseidon and transfer 4);  (ii-c) 18,402, 15, 27,102, degree 6.  No variant changes k.  The halo2 back end and the verifier generator need no code change;  per-circuit emission changes 4 verifiers.
- Questions:  docs/M14-PLAN.md section 9, (a) to (k) (the variant, the IR ruling, the emission, the verifiers, the lowering pattern, the refuse rows, the doc sites, the O24 status, the fixtures and the README, the open items, the slices).
- Gates:  none ran;  the M13 close counts stay (examples 69, circuit refusals 24, forge test 100, e2e 9 rows).
- Next:  the USER rules section 9, then ~/Documents/glue-lang-M14-s1-PROMPT.md.


### s1 (2026-10-20)

- Complete: the USER accepted section 9(a) to (k), including IR ruling
  (b);  variant (ii-b), emitted per circuit.  The USER also approved the
  existing test/gate.sh extension;  the plan records that file-list change.
- Built src/circuit.c and regenerated four circuit/witness pairs.
  Model rows/k/copies: poseidon 750/10/1,138;  transfer 1,499/11/2,276;
  kyc 1,502/11/2,282;  kycMerkle 16,642/15/25,342.  All matched.
  The four other circuit/witness pairs match HEAD byte for byte.
- Smoke check, keygen with --dev-srs, prove and verify all exited 0 for
  poseidon and kycMerkle;  proofs matched 1,344 B and 1,824 B.
- Added two SAT_GATE refusals for c + 1 and b + 1 on a pow5 row, through
  the existing runner and guest table.  No new harness was added.
- make -B check exited 0;  circuit refusals 26, satcheck refusals 6,
  examples 69, circuits 9, same-ir 19, all other M13 counts unchanged.
- Final mutants, each with 26 circuit refusals: m1 KILLED (4 FAIL lines);  m2 KILLED (4 FAIL lines);  m3 KILLED (4 FAIL lines);  m4 KILLED (25 FAIL lines).
  The good source was restored and make -B exited 0.
- Evidence: ~/Documents/glue-lang-M14-s1-probe/REPORT.md, model.json,
  smoke.json, refusals.json, check.log, run.sh, run.out and mutants.tsv.
  No Halo2 source, verifier, fixture, commit or push changed.
- Next: ~/Documents/glue-lang-M14-s1-b-PROMPT.md for the remaining gates;
  M14 is still IN PROGRESS, with O24 close assigned to s2.

### s1-b (2026-10-21)

- Start condition MET:  HEAD 083b93d `M14 s1: lower S-boxes with a per-circuit pow5 gate` (committed and pushed at the USER request), origin/main = HEAD, status empty;  src/circuit.c = ~/Documents/glue-lang-M14-s1-probe/circuit.good.c (SHA-256 86762116..a567b2).
- Gates (~/Documents/glue-lang-M14-s1-b/phase1.sh and phase2.sh, the sandbox off, 984 s:  check 437 s, e2e 253 s, ceremony 226 s):  `make -B check` exit 0 (native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 26, satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (9 rows);  `forge test --root contracts` 100 passed;  `make e2e-ceremony` exit 0 (9 rows);  `make rehearsal` exit 0 (anvil 31337);  `make deployment-test` exit 0.  The counts are the s1 counts;  only the circuit refusals (24 to 26) differ from the M13 close.
- IR check:  before e2e, the worktree against HEAD is empty.  After e2e:  the 4 verifiers of section 6 (poseidon.sol and transfer.sol 120 lines in and 128 out;  kyc.sol and kycMerkle.sol 131 in and 93 out) and the 8 fixtures.  The verifiers of square, nonzero, poly and range do not change.  halo2/, src/, test/ and the docs do not change:  the generator is the same (D6).
- The verifier changes (poseidon.sol:  244 changed lines;  kyc.sol:  220):  44 VK `mstore` constant lines, the pointer constants of the proof layout, the proof read offsets, the quotient evaluation (the gate expression and the permutation chunks) and the pairing terms.  poseidon and transfer go from degree 3 to 4:  3 quotient chunks (2 before) and fewer permutation chunks.
- Proof sizes (build/e2e and the fixtures;  the D5 MODEL is exact):  poseidon 1,344 B, transfer 1,344 B, kyc 1,824 B, kycMerkle 1,824 B;  square 1,536 B, nonzero 1,344 B, poly 1,536 B, range 1,568 B (no change).
- The fixtures of square, nonzero, poly and range went back with `git restore` (4 lines each);  then `forge test --root contracts` 100 passed.
- e2e-ceremony:  the SHA-256 of ~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs = the pin halo2/ceremony.sha256 (4cdb1688..0ab67b);  k:  kycMerkle 15, kyc 11, transfer 11, poseidon 10, range 9, square, nonzero and poly 5 (at most 15).
- rehearsal:  kycMerkle at chain id 31337 on anvil;  the gate admits the kycMerkle proof from the anvil account 0 (eth_call, nothing sent).  deployment-test:  all checks passed.  No broadcast, no key, no live RPC.
- RSS (`/usr/bin/time -l`, the sandbox off, child exit code 0 for each, halo2/target/debug/glue-halo2 on test/circuit/kycMerkle.circuit.json and kycMerkle.witness.json, one sample each):  `keygen --dev-srs` 188,481,536 B max RSS (188 MB), 20.1 s;  `prove` 201,768,960 B max RSS (202 MB), 27.5 s, proof 1,824 B.  M13 s1-b:  keygen 139 MB, 19.7 s;  prove 273 MB, 17.0 s.  The keygen RSS and the prove time are more than the D8 MODEL (no growth);  both measurements are far below the 4 GB limit.
- contracts/README.md rows (one `forge test --root contracts --gas-report`, the log ~/Documents/glue-lang-M14-s1-b/gas-report.log;  the runtime size = the deployment size minus 32;  `verifyProof` = the max column):  poseidon 5,643 | 1,271,834 | 283,534 (was 5,801 | 1,305,851 | 277,741);  transfer 5,644 | 1,272,062 | 284,256 (was 5,801 | 1,305,899 | 278,462);  kyc 6,610 | 1,480,728 | 330,104 (was 6,222 | 1,396,899 | 316,319);  kycMerkle 6,612 | 1,481,220 | 330,359 (was 6,222 | 1,396,935 | 316,579).  The same report gives the 4 other README rows exactly (8 of 8 rows = the report).  All runtime sizes are below 20,000 B.
- The `verifyProof` gas goes up for the 4 circuits (poseidon +5,793, transfer +5,794, kyc +13,785, kycMerkle +13,780):  the verifier cost does not depend on the row count.  The pow5 gate adds 2 polys and 2 fixed columns (q5 and c5:  more evaluations and commitments), and poseidon and transfer get one more quotient chunk.  The fewer rows and copies help the prover, not the verifier.
- Staged on 083b93d (11 files):  the 4 verifiers, the 4 fixtures, contracts/README.md, docs/M14-PLAN.md (the status line) and docs/STATUS.md.  The s1 paths (src/circuit.c, the 8 goldens, refuse.txt, test/gate.sh, docs/IR.md, SPEC.md) are in 083b93d and have no diff.  Commit message:  ~/Documents/glue-lang-M14-s1-b-commit-msg.txt.
- Next:  the USER commits s1-b;  then s2 (the close, ~/Documents/glue-lang-M14-s2-PROMPT.md).

### s2: close (2026-10-21)

- Start condition MET:  HEAD 00155c7 `M14 s1-b: regenerate the 4 pow5 verifiers and fixtures` (USER), its parent 083b93d `M14 s1: lower S-boxes with a per-circuit pow5 gate`, status empty;  src/circuit.c = ~/Documents/glue-lang-M14-s1-probe/circuit.good.c.
- Commits:  the kickoff 122b00f (the plan, the docs/PLAN.md M14 row, `### kickoff`);  s1 083b93d (src/circuit.c, the 8 goldens of the 4 circuits, refuse.txt, test/gate.sh, docs/IR.md, SPEC.md 7.1);  s1-b 00155c7 (the 4 verifiers, the 4 fixtures, contracts/README.md).  s2 changes docs only:  no change in src/, domain/, halo2/, contracts/, the Makefile, test/ and examples/.
- The rows, k and copies at HEAD (jq with ~/Documents/glue-lang-M14-kickoff-probe/rows.jq on the 4 goldens):  poseidon 750, 10, 1,138;  transfer 1,499, 11, 2,276;  kyc 1,502, 11, 2,282;  kycMerkle 16,642, 15, 25,342 (= docs/M14-PLAN.md section 6, exact).  The proof sizes of the 4 fixtures at HEAD:  1,344, 1,344, 1,824 and 1,824 B (= D5).
- Gate (the control, ~/Documents/glue-lang-M14-s2/check.sh, the log check.log there):  `make -B check` exit 0, 259 s (s1-b:  437 s):  native circuit 9, path 46, face 35, HIT 44;  examples 69, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 26, satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  These are the s1-b counts.  The s2 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s1-b (each exit 0).  After the control the status names only the 4 docs of s2.
- The IR rule kept (D2):  M14 adds one IR form, the gate pow5 with `ROW_POW5` and the fixed columns q5 and c5, only in a circuit with a pow5 row (ruling (b)).  The gate arith, the lookup range8, the 3 advice columns and the instance column do not change.  No conv rule, no new check code, no change in halo2/ or in the verifier generator.  4 verifiers change (poseidon, transfer, kyc, kycMerkle);  square, nonzero, poly and range stay byte for byte.
- Docs:  SPEC.md 9 O24 (`Closed in M14`, form (ii) DONE, the table `Measured, M14`, the gas and D8 facts, the close sentence);  docs/PLAN.md (the M14 row DONE 2026-10-21);  docs/M14-PLAN.md (the Status line, DONE);  this file (`### s2: close`).
- The measured facts (s1-b):  the `verifyProof` gas goes up by 5,793 (poseidon), 5,794 (transfer), 13,785 (kyc) and 13,780 (kycMerkle);  the deployment gas goes down for poseidon and transfer and goes up for kyc and kycMerkle.  The kycMerkle keygen RSS (139 to 188 MB) and prove time (17.0 to 27.5 s) are more than the D8 MODEL (no growth);  the keygen time (19.7 to 20.1 s) and the prove RSS (273 to 202 MB) agree with it.  One sample each;  far below the 4 GB limit.
- Open after M14:  O24 is CLOSED (ruling (h);  the forms (i) to (iv) are DONE).  O23 stays open (the deployment target, the target fields with the literal word VALUE, the PPoT check of the ceremony file:  USER items).  The hint-row form (kycMerkle 14,882 rows, k 14) and a k 14 goal for kycMerkle are not open items;  each needs a USER request (rulings (h) and (j)).  The other open items of section 9 do not change.
- Next:  the USER commits s2;  then the USER picks M15 (O23 USER fields and the PPoT check, k 14 or the hint-row form, or another topic):  ~/Documents/glue-lang-M15-kickoff-PROMPT.md.

## M15

### kickoff (2026-10-21)

- Start condition MET:  HEAD 8a09680 (the M14 close), git status empty.
- Topic (USER pick, 2026-10-21):  k 14 for kycMerkle.  The Merkle depth stays 20 (M13).
- Plan:  docs/M15-PLAN.md, DRAFT 2026-10-21.
- Probe (~/Documents/glue-lang-M15-kickoff-probe/, read only;  no src/ change, no build):  the census of the 4 pow5 circuits from the goldens and the sources, the designs (MODEL), the soundness and the code sites with line anchors (REPORT.md).
- The answer:  at HEAD kycMerkle has 16,642 rows (k 15);  k 14 needs 16,368 rows or fewer.  6,112 of its 6,160 input rows are hint rows;  1,760 hints have a pow5 row as their only check.  The hint merge (H1) puts each such hint in the c cell of its pow5 row:  14,882 rows, k 14, 23,582 copies (MODEL).  H1 adds no gate, no column and no IR form;  it changes the lowering rule D14 (SPEC.md 7.1 item 1).
- The variants (MODEL;  kycMerkle rows and k, then the max depth at k 14):  now 16,642, 15, 19;  H1 14,882, 14, 22;  H2 (the pow5 gate on the hint row, a new gate form) 14,882, 14, 22;  H1-mds 12,352, 14, 27;  H1g (each hint) 10,530, 14, 32 (poseidon, transfer and kyc change k too).  Under H1:  poseidon 750 to 670 rows, transfer 1,499 to 1,339, kyc 1,502 to 1,342, with the same k.  No lowering without a rule change gives k 14.
- The pinned SRS (k 15) serves k 14 (halo2/src/backend.rs, `sized` and `downsized`).  The proof sizes do not change (MODEL).
- Questions:  docs/M15-PLAN.md section 9, (a) to (k);  (b) is the ruling on the lowering rule D14.
- Gates:  none ran;  the M14 close counts stay (circuit refusals 26, satcheck 6, examples 69, circuits 9, same-ir 19, forge test 100, e2e 9 rows).
- Next:  the USER commits the kickoff and rules section 9;  then s1 (~/Documents/glue-lang-M15-s1-PROMPT.md).

### staged review (2026-10-21)

- Fixed M15-PLAN.md D3 (5):  every active row use of a merged hint must be strictly after its producing pow5 row.  The producing row itself must not read the hint.
- Reproducer:  `hint t := 1` checked by `sbox5 t == t` passes check, witness generation and satcheck at HEAD.  A C reproducer using fill_advice gives the pow5 output 1 before the merge and 0 after a self-read merge.
- The plan now requires the selfHint regression in the existing test/circuit-regressions.c (9 to 10 native circuit regressions), plus mutant m5 for the inclusive row-order guard.

### s1 (2026-10-21)

- Start condition MET:  HEAD ae82f95 (the M15 kickoff, USER).  RULED 2026-10-21:  the USER accepts the recommendations (a) to (k) of docs/M15-PLAN.md section 9 (H1, the hint merge;  the lowering rule D14 changes;  no IR form).
- Built src/circuit.c:  the merge pass of D3 (conditions (1) to (5)) in lower_circuit, after the checks and before fold_affine, with the `at` remap;  D4 in fill_advice and witness_checks (WITNESS_FAIL when the hint value is not the merged cell);  the D14 comment.  No new gate, column, IR form or halo2/ change.
- Regenerated the circuit and witness goldens of 4 circuits.  Rows/k/copies match section 6:  poseidon 670/10/1,058;  transfer 1,339/11/2,116;  kyc 1,342/11/2,122;  kycMerkle 14,882/14/23,582.  The goldens of square, nonzero, poly and range match HEAD byte for byte.
- New example examples/sboxHint.glue and its WITNESS_FAIL row in test/circuit/refuse.txt (the hint t is 4, but its check gives 243).  Rows 24 and 25 of refuse.txt now name row 200 (was row 280).  test/circuit-regressions.c has the selfHint regression.
- SPEC.md 7.1 (item 1 and the merge item) and the D14 paragraph.
- Smoke (sandbox off):  keygen --dev-srs, prove and verify exit 0 for poseidon (k 10, proof 1,344 B) and kycMerkle (k 14, proof 1,824 B).
- make -B check exit 0:  native circuit regressions 9 to 10;  examples 69 to 70;  circuit refusals 26 to 27.  Circuits 9, same-ir 19 and all other M14 close counts do not change.
- Mutants (section 8):  m1 KILLED (8 FAIL lines);  m2 KILLED (8);  m3 KILLED (2);  m4 KILLED (4);  m5 KILLED (1).  The good source was restored and the final make -B check exit 0.
- Evidence:  ~/Documents/glue-lang-M15-s1-probe/ (REPORT.md, patch.py, build.sh, gates.sh, run.out, smoke.txt, counts.txt, mutants.tsv, circuit.good.c and its SHA-256).  No forge, e2e, verifier or fixture change (s1-b).
- Next:  ~/Documents/glue-lang-M15-s1-b-PROMPT.md (the gates).

### s1-b (2026-10-21)

- Start condition MET:  HEAD e23bc92 `M15 s1: merge pow5-checked hint cells` (USER, on ae82f95), status empty;  src/circuit.c = ~/Documents/glue-lang-M15-s1-probe/circuit.good.c (SHA-256 a75495b7..dd7757).
- Gates (~/Documents/glue-lang-M15-s1-b-probe/phase1.sh and phase2.sh, the sandbox off, 307 s:  check 135 s, e2e 85 s, ceremony 65 s):  `make -B check` exit 0 (native circuit 11, path 46, face 35, HIT 44;  examples 70, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 27, satcheck refusals 6;  node regressions 6, printer 2);  `make check-clang` exit 0;  `forge build --root contracts --sizes` exit 0;  `make e2e` exit 0 (9 rows);  `forge test --root contracts` 100 passed;  `make e2e-ceremony` exit 0 (9 rows, max k 14);  `make rehearsal` exit 0 (anvil 31337);  `make deployment-test` exit 0.
- Count note:  the native circuit regressions are 11, not 10 as in `### s1`.  e23bc92 adds 2 regressions to test/circuit-regressions.c (check_self_hint and check_reflexive_copy);  the s1 logs (08:40 and 08:57) came before the second regression.  All 11 pass.  The other counts are the `### s1` counts.
- IR check:  before e2e, the worktree against HEAD is empty.  After e2e:  the 4 verifiers of section 6 (poseidon.sol, transfer.sol and kyc.sol 20 lines in and 20 out;  kycMerkle.sol 33 in and 33 out) and the 8 fixtures.  The verifiers of square, nonzero, poly and range do not change.  halo2/, src/, test/ and the docs do not change:  the generator is the same (D6).
- The verifier changes:  only the VK `mstore` lines of vk_digest, fixed_comms and permutation_comms.  kycMerkle.sol also changes the 5 domain constants:  k (0x0f to 0x0e, 15 to 14), n_inv, omega, omega_inv and omega_inv_to_l.  No pointer constant, proof read offset, quotient evaluation or pairing term changes.
- Proof sizes (build/e2e and the fixtures;  the D5 MODEL is exact):  poseidon 1,344 B, transfer 1,344 B, kyc 1,824 B, kycMerkle 1,824 B (as M14);  square 1,536 B, nonzero 1,344 B, poly 1,536 B, range 1,568 B (no change).
- The fixtures of square, nonzero, poly and range went back with `git restore` (4 lines each);  then `forge test --root contracts` 100 passed.
- e2e-ceremony:  the SHA-256 of ~/Documents/glue-lang-M9-s5-probe/srs/kzg_bn254_15.srs = the pin halo2/ceremony.sha256 (4cdb1688..0ab67b);  k:  kycMerkle 14 (15 at M14), kyc 11, transfer 11, poseidon 10, range 9, square, nonzero and poly 5 (max 14).  The k 15 file serves the k 14 circuit (D5).
- rehearsal:  kycMerkle at chain id 31337 on anvil;  the gate admits the kycMerkle proof from the anvil account 0 (eth_call, nothing sent).  deployment-test:  all checks passed.  No broadcast, no key, no live RPC.
- RSS (`/usr/bin/time -l`, the sandbox off, child exit code 0 for each, halo2/target/debug/glue-halo2 on test/circuit/kycMerkle.circuit.json and kycMerkle.witness.json, k 14, one sample each):  `keygen --dev-srs` 127,418,368 B max RSS (127 MB), 4.9 s;  `prove` 167,034,880 B max RSS (167 MB), 6.6 s, proof 1,824 B.  M14 s1-b (k 15):  keygen 188 MB, 20.1 s;  prove 202 MB, 27.5 s.  The RSS goes down by 32 % (keygen) and 17 % (prove);  the time goes down by 75 % and 76 %.  The D8 MODEL (about half) is less than the measured time drop and more than the measured RSS drop.  One sample each, the machine load is not controlled;  both measurements are far below the 4 GB limit.
- contracts/README.md rows (one `forge test --root contracts --gas-report`, the log ~/Documents/glue-lang-M15-s1-b-probe/gas-report.log;  the runtime size = the deployment size minus 32;  `verifyProof` = the max column):  poseidon 5,643 | 1,271,822 | 283,534 (was 5,643 | 1,271,834 | 283,534);  transfer 5,644 | 1,272,038 | 284,256 (was 5,644 | 1,272,062 | 284,256);  kyc 6,609 | 1,480,548 | 330,104 (was 6,610 | 1,480,728 | 330,104);  kycMerkle 6,614 | 1,481,688 | 330,294 (was 6,612 | 1,481,220 | 330,359).  The same report gives the 4 other README rows exactly (8 of 8 rows = the report).  All runtime sizes are below 20,000 B.
- The `verifyProof` gas does not change for poseidon, transfer and kyc;  it goes down by 65 for kycMerkle.  The deployment cost changes by -180 to +468 gas and the runtime size by -1 to +2 B (the bytes of the new VK constants).  The verifier cost does not depend on the row count (M14 s1-b):  the hint merge helps the prover, not the verifier.
- Staged on e23bc92 (10 files):  the 4 verifiers, the 4 fixtures, contracts/README.md and docs/STATUS.md.  The s1 paths are in e23bc92 and have no diff.  Commit message:  ~/Documents/glue-lang-M15-s1-b-commit-msg.txt.
- Next:  the USER commits s1-b;  then s2 (the close, ~/Documents/glue-lang-M15-s2-PROMPT.md).

### s2: close (2026-10-21)

- Start condition MET:  HEAD 8192f9d `M15 s1-b: regenerate the 4 merged-hint verifiers and fixtures` (USER), its parent e23bc92 `M15 s1: merge pow5-checked hint cells`, tree 911b7b94, status empty;  src/circuit.c = ~/Documents/glue-lang-M15-s1-probe/circuit.good.c (SHA-256 a75495b7..dd7757).
- Commits:  the kickoff ae82f95 (the plan, the docs/PLAN.md M15 row, `### kickoff`);  s1 e23bc92 (src/circuit.c, the 8 goldens of the 4 circuits, refuse.txt, examples/sboxHint.glue, test/circuit-regressions.c, SPEC.md 7.1 and D14, docs/M15-PLAN.md);  s1-b 8192f9d (the 4 verifiers, the 4 fixtures, contracts/README.md).  s2 changes docs only:  no change in src/, domain/, halo2/, contracts/, the Makefile, test/ and examples/.
- The rows, k and copies at HEAD (jq on the 4 goldens, ~/Documents/glue-lang-M15-s2/facts.sh):  poseidon 670, 10, 1,058;  transfer 1,339, 11, 2,116;  kyc 1,342, 11, 2,122;  kycMerkle 14,882, 14, 23,582 (= docs/M15-PLAN.md section 6, exact).  The proof sizes of the 4 fixtures at HEAD:  1,344, 1,344, 1,824 and 1,824 B (= D5).  The `// k` line of the kycMerkle verifier is 0x0e (k 14).
- Gate (the control, ~/Documents/glue-lang-M15-s2/check.sh, the log check.log there):  `make -B check` exit 0, 114 s (s1-b:  135 s):  native circuit 11, path 46, face 35, HIT 44;  examples 70, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 27, satcheck refusals 6;  gate 0 failures;  node regressions 6, printer 2.  These are the s1-b counts.  The s2 gate delta is none, so the session did not run forge or e2e;  the last full gates are at s1-b (each exit 0).  After the control the status names only the 4 docs of s2.
- The M15 close counts against the M14 close:  native circuit regressions 9 to 11 (s1 adds check_self_hint and check_reflexive_copy;  `### s1` says 10, and the `### s1-b` note gives the correction);  examples 69 to 70;  circuit refusals 26 to 27 (examples/sboxHint.glue, WITNESS_FAIL).  The other counts do not change.  The native circuit count 11 is the M15 close count.
- The rules kept:  M15 changes the lowering rule D14 (ruling (b);  SPEC.md 7.1 item 7, the hint merge) and adds no IR form:  no gate, no column and no row kind in docs/IR.md.  The gate arith, the lookup range8, the gate pow5, the 3 advice columns and the instance column do not change.  No conv rule, no new check code, no change in halo2/ or in the verifier generator.  4 verifiers change (poseidon, transfer, kyc, kycMerkle);  square, nonzero, poly and range stay byte for byte.
- Docs:  SPEC.md 7.1 (the table `Measured, M15` after item 7, the gas facts and the D8 facts);  SPEC.md 9 O24 (one paragraph after the table `Measured, M14`:  M15 builds the hint-row form as H1, and 7.1 gives the values);  docs/PLAN.md (the M15 row DONE 2026-10-21);  docs/M15-PLAN.md (the Status line, DONE);  this file (`### s2: close`).  The table `Measured, M14` is in SPEC.md section 9 (O24), not in 7.1.  The M15 table is in 7.1, as docs/M15-PLAN.md section 10 says, and the pointer paragraph is next to the M14 table.
- The measured facts (s1-b):  the `verifyProof` gas does not change for poseidon, transfer and kyc, and goes down by 65 for kycMerkle;  the deployment gas changes by -12, -24, -180 and +468.  The kycMerkle time goes down by 75 % (keygen, 20.1 to 4.9 s) and 76 % (prove, 27.5 to 6.6 s), more than the D8 MODEL (about half);  the RSS goes down by 32 % (keygen, 188 to 127 MB) and 17 % (prove, 202 to 167 MB), less than the MODEL.  One sample each;  far below the 4 GB limit.
- Open after M15:  M15 adds no open item.  O23 stays open (the deployment target, the target fields with the literal word VALUE, the PPoT check of the ceremony file:  USER items).  H1g, H1-mds and H2 are not open items;  each needs a USER request (ruling (j)).  The max Merkle depth of kycMerkle at k 14 is 22 (MODEL, D5).  The other open items of section 9 do not change.
- Next:  the USER commits s2;  then the USER picks M16 (O23 USER fields and the PPoT check, a deeper Merkle tree at k 14 (max depth 22, MODEL), or another topic).

## M16

### kickoff (2026-10-21)

- Start condition MET:  HEAD 81af21c `M15 s2: close M15 (k 14 for kycMerkle)` (USER), its parent 8192f9d, tree e80247fc, git status empty.
- Topic (USER pick, 2026-10-21, the answer to a question of the kickoff session):  a deeper Merkle tree for kycMerkle at k 14 (the max depth 22, the MODEL of docs/M15-PLAN.md D5).
- Plan:  docs/M16-PLAN.md, DRAFT 2026-10-21.
- Probe (~/Documents/glue-lang-M16-kickoff-probe/, read only;  no src/ change, no build):  the depth sites, the generator, the contracts and the docs that name the depth or the root, and the model of the rows (REPORT.md, run.sh, run.out).
- The answer:  at HEAD kycMerkle has depth 20, 14,882 rows, k 14 and 23,582 copies.  The Merkle depth is in the circuit text only (examples/kycMerkle.glue, from the M13 generator outside the repository);  src/, halo2/ and the Makefile have no depth parameter.  A level costs 677 rows and 1,073 copies (MODEL).  Depth 22:  16,236 rows, k 14, 25,728 copies, 132 rows under the limit 16,368 (MODEL).  Depth 21:  15,559 rows (809 under).  Depth 23:  16,913 rows, k 15.
- The changes (MODEL):  the kycMerkle example, its 2 goldens, expect.txt row 9, refuse.txt rows 20 and 26, the kycMerkle verifier (the VK constants only) and fixture, contracts/test/Verifiers.t.sol, contracts/README.md (the gas row) and SPEC.md.  No IR form, no conv rule, no change in src/, halo2/, KycGate or the scripts.  The proof size stays 1,824 B.
- Questions:  docs/M16-PLAN.md section 9, (a) to (k);  (a) is the depth (recommendation 22).
- Gates:  none ran;  the M15 close counts stay (native circuit 11, examples 70, circuit refusals 27, satcheck 6, circuits 9, same-ir 19, forge test 100, e2e 9 rows).
- Next:  the USER commits the kickoff and rules section 9;  then s1 (~/Documents/glue-lang-M16-s1-PROMPT.md).

### s1 (2026-10-21)

- Start condition MET:  HEAD 0e7efcb `M16 kickoff: plan depth-22 kycMerkle at k 14` (USER), its parent 81af21c, git status empty.  RULED 2026-10-21:  the USER accepts the recommendations (a) to (k) of docs/M16-PLAN.md section 9 (depth 22;  the name stays kycMerkle;  b20 = 0, b21 = 1, s20 = 231, s21 = 242;  Verifiers.t.sol in s1-b).
- The generator (D4):  ~/Documents/glue-lang-M16-s1-probe/run.sh, a port of the M13 generator, outside the repository.  The level-3 templates end at `hint l4` and at `b4 * b4 == b4`;  the root reference goes from level 19 to level 21.  The control:  the output at depth 20 equals examples/kycMerkle.glue of HEAD byte for byte (no header difference);  the same commands give the 2 goldens and the rows expect 9, refuse 20 and refuse 26 of HEAD again.
- examples/kycMerkle.glue at depth 22:  228 lines, 962,418 B (was 881,808 B);  line 228 has 940,525 chars (was 859,915, +9.4 %).  The header (lines 1, 2, 4, 9, 10, 14 and 21) names depth 22, b0 to b21, s0 to s21, the hash of level 21 and the M16 generator.  The private binders go from 43 to 47.
- Measured:  16,236 rows, k 14, 25,728 copies (= the MODEL of D1;  132 rows under the limit 16,368).  The advice, fixed and instance columns, the gates and the lookup are the same as at HEAD (D2).  langc check 15.5 s, circuit 15.0 s, witness 15.4 s, satcheck 2.0 s.  The new root is 18921323889410538904106118131460191082618624758723655813111215981830139585700.
- Installed:  the 2 goldens (circuit 4,438,959 B, witness 3,122,072 B), expect.txt row 9, refuse.txt rows 20 (root + 1) and 26 (b7 = 2), SPEC.md section 4 (one M16 sentence).  By hand, row 20 gives WITNESS_FAIL at the root constraint (228:940465) and row 26 at the bit constraint of b7 (228:647350).
- Mutants (section 10):  n1 (the generator stops at depth 21) KILLED (4 FAIL lines);  n2 (the children of level 21 swap) KILLED (2);  n3 (the bit constraint of level 21 goes) KILLED (2).  After each mutant the good file came back (SHA-256 = good).
- make -B check exit 0 (117 s):  native circuit 11, path 46, face 35, HIT 44;  examples 70, parse refusals 18, check refusals 157, evals 290, json 42 builds, build refusals 8, circuits 9, same-ir 19, circuit refusals 27, satcheck refusals 6;  node regressions 6, printer 2.  make check-clang exit 0.  Gate delta:  none.
- Evidence:  ~/Documents/glue-lang-M16-s1-probe/ (REPORT.md, run.sh, run.m13.sh, gates.sh, patch.py, run.out, gates.out, mutants.tsv, counts.txt, good.glue and its SHA-256).  No forge, e2e, verifier, fixture or Verifiers.t.sol change (s1-b, ruling (g)).
- Next:  ~/Documents/glue-lang-M16-s1-b-PROMPT.md (the gates).
