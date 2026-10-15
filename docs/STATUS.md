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
- No refusal test for CIRCUIT_LOWER (SPEC.md O4).
- The Solidity generator takes one instance column, no rotated instance
  query and at least one advice column (halo2/README.md).

## Internal boundaries

The C compiler (`src/`, TinyCC) holds the front end, the lowering and the
witness. `test/gate.sh` checks the examples, the refusal tables, the
circuit goldens (`test/circuit/`) and the satcheck refusals
(`test/satcheck/`). docs/IR.md is the only interface between the compiler
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
