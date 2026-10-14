# glue-lang status

The build implements milestone M1 of docs/PLAN.md: the vertical slice from
a `circuit` declaration to a Solidity verifier. It has the lang-template
core of M0 (the sample domain, the former checker, the JSON target). The
cubical part is M2. M2 is in progress: see `## M2`.

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

1. M2: the cubical part (docs/M2-PLAN.md). Slices s1 and s2 are done.
   Slices s3 to s5 remain (docs/M2-PLAN.md section 10).

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
