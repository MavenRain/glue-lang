# Builds the tcc-json kit with TinyCC. See README.md.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c src/front/fr.c src/front/interval.c src/front/face.c src/front/kan.c
TARGET = src/json.c src/circuit.c
SRC = src/main.c $(FRONT) $(TARGET)
HEADERS = $(wildcard src/*.h src/front/*.h)

# The halo2 back end (halo2/README.md).
HALO2 = halo2/target/debug/glue-halo2
HALO2_SRC = $(wildcard halo2/src/*.rs) halo2/Cargo.toml halo2/Cargo.lock

# Each line of test/circuit/expect.txt is `PROG NAME BINDER=VALUE...`: one
# example circuit (examples/PROG.glue) and its inputs. The gate uses the
# same table for the goldens.
E2E_TABLE = test/circuit/expect.txt
E2E = build/e2e
VERIFIERS = contracts/src/verifiers
FIXTURES = contracts/test/fixtures
CEREMONY = build/e2e-ceremony
CEREMONY_PIN = halo2/ceremony.sha256

.PHONY: build check check-clang clean e2e e2e-ceremony rehearsal deploy-check deployment-test

build: build/langc

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c

build/circuit-regressions: test/circuit-regressions.c $(FRONT) src/circuit.c $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o $@ test/circuit-regressions.c $(FRONT) src/circuit.c build/domain.c

build/path-regressions: test/path-regressions.c $(FRONT) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o $@ test/path-regressions.c $(FRONT) build/domain.c

build/face-regressions: test/face-regressions.c $(FRONT) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o $@ test/face-regressions.c $(FRONT) build/domain.c

build/hit-regressions: test/hit-regressions.c $(FRONT) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o $@ test/hit-regressions.c $(FRONT) build/domain.c

check: build/langc build/circuit-regressions build/path-regressions build/face-regressions build/hit-regressions check-clang
	build/circuit-regressions
	build/path-regressions
	build/face-regressions
	build/hit-regressions
	sh test/gate.sh
	TCC="$(TCC)" node test/regressions.js
	node test/printer-regressions.js

$(HALO2): $(HALO2_SRC)
	cargo build --manifest-path halo2/Cargo.toml -j 2

# The whole chain for each example circuit: circuit, witness, satcheck,
# keygen with the INSECURE development SRS, prove, verify and the Solidity
# verifier. The verifiers and the proofs go to contracts/. Then Forge tests
# them, and the deploy script runs as a simulation (no RPC, no broadcast).
# `forge clean` drops the cache, which can name a verifier that is gone.
e2e: build/langc $(HALO2)
	rm -rf $(E2E) $(VERIFIERS)
	rm -f $(FIXTURES)/*.proof.json
	mkdir -p $(E2E) $(VERIFIERS) $(FIXTURES)
	while read -r prog name args; do \
	  out=$(E2E)/$$name; \
	  echo "e2e: $$name"; \
	  build/langc circuit examples/$$prog.glue $$name -o $$out.circuit.json \
	  && build/langc witness examples/$$prog.glue $$name $$args -o $$out.witness.json \
	  && build/langc satcheck $$out.circuit.json $$out.witness.json \
	  && $(HALO2) keygen --circuit $$out.circuit.json --dev-srs --keys $$out-keys \
	  && $(HALO2) prove --circuit $$out.circuit.json --witness $$out.witness.json --keys $$out-keys --out $$out-proof \
	  && $(HALO2) verify --circuit $$out.circuit.json --keys $$out-keys --proof $$out-proof/proof.json \
	  && $(HALO2) solidity --circuit $$out.circuit.json --keys $$out-keys --out $(VERIFIERS)/$$name.sol \
	  && cp $$out-proof/proof.json $(FIXTURES)/$$name.proof.json \
	  || exit 1; \
	done <$(E2E_TABLE)
	forge clean --root contracts
	forge test --root contracts
	for name in $$(awk '{ print $$2 }' $(E2E_TABLE)); do \
	  forge script contracts/script/DeployVerifier.s.sol --root contracts --sig 'run(string)' $$name || exit 1; \
	done

# The same chain with the ceremony SRS (docs/M9-PLAN.md D5): `make
# e2e-ceremony SRS=FILE`. The back end checks the SHA-256 of FILE against
# the pin in $(CEREMONY_PIN) before it reads the file. The keys, proofs and
# verifiers go to $(CEREMONY)/, never to contracts/: the gates stay on the
# development SRS. No Forge step: the deploy (M9 s6) takes a verifier from
# $(CEREMONY)/verifiers/.
e2e-ceremony: build/langc $(HALO2)
	@test -n "$(SRS)" || { echo "e2e-ceremony: give SRS=FILE (the ceremony KZG params file)" >&2; exit 2; }
	rm -rf $(CEREMONY)
	mkdir -p $(CEREMONY)/verifiers
	pin=$$(awk '{ print $$1 }' "$(CEREMONY_PIN)"); \
	while read -r prog name args; do \
	  out=$(CEREMONY)/$$name; \
	  echo "e2e-ceremony: $$name"; \
	  build/langc circuit examples/$$prog.glue $$name -o $$out.circuit.json \
	  && build/langc witness examples/$$prog.glue $$name $$args -o $$out.witness.json \
	  && build/langc satcheck $$out.circuit.json $$out.witness.json \
	  && $(HALO2) keygen --circuit $$out.circuit.json --srs "$(SRS)" --sha256 "$$pin" --keys $$out-keys \
	  && $(HALO2) prove --circuit $$out.circuit.json --witness $$out.witness.json --keys $$out-keys --out $$out-proof \
	  && $(HALO2) verify --circuit $$out.circuit.json --keys $$out-keys --proof $$out-proof/proof.json \
	  && $(HALO2) solidity --circuit $$out.circuit.json --keys $$out-keys --out $(CEREMONY)/verifiers/$$name.sol \
	  || exit 1; \
	done <$(E2E_TABLE)

# The deployment of docs/M9-PLAN.md D6 and D7. `make rehearsal` deploys the
# ceremony verifier NAME and a KycGate on a local anvil with CHAIN_ID and
# runs the checks: the only broadcast is to that anvil, from an anvil
# account that is unlocked in the node. `make deploy-check` runs the same
# read-only checks against a deployment (cast call and cast code only). The
# USER broadcast is the two forge script commands of contracts/README.md
# `## The deployment runbook`; nothing here sends to a live chain.
NAME ?= kycMerkle
CHAIN_ID ?= 31337
SCOPE ?= 9
MIN ?= 18
PORT ?= 8577

rehearsal:
	sh contracts/script/rehearsal.sh $(NAME) $(CHAIN_ID) $(SCOPE) $(MIN) $(PORT)

deploy-check:
	@test -n "$(RPC_URL)" || { echo "deploy-check: give RPC_URL=URL VERIFIER=0x.. [GATE=0x.. ISSUER=0x.. SCOPE=N MIN=N] [CHAIN_ID=N] [NAME=kycMerkle]" >&2; exit 2; }
	@test -n "$(VERIFIER)" || { echo "deploy-check: give VERIFIER=0x.. (the address of the DeployVerifier run)" >&2; exit 2; }
	RPC_URL="$(RPC_URL)" CHAIN_ID="$(CHAIN_ID)" NAME="$(NAME)" VERIFIER="$(VERIFIER)" GATE="$(GATE)" ISSUER="$(ISSUER)" SCOPE="$(SCOPE)" MIN="$(MIN)" sh contracts/script/check.sh

deployment-test:
	sh contracts/test/deployment.sh

clean:
	rm -rf build
