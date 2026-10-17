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
