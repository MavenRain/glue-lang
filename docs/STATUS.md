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
