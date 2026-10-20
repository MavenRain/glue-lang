#!/bin/sh
# Integration regressions for the rehearsal and read-only deployment checks.
# Run from the repository root after make e2e-ceremony SRS=FILE.
set -eu
CEREMONY=build/e2e-ceremony
REHEARSAL=${REHEARSAL:-contracts/script/rehearsal.sh}
CHECK=${CHECK:-contracts/script/check.sh}
SENDER=0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
PROOF=$CEREMONY/kyc-proof/proof.json
test -f "$PROOF" || { echo "deployment-test: run make e2e-ceremony SRS=FILE first" >&2; exit 2; }
WORK=$(mktemp -d "$CEREMONY/deployment-test.XXXXXX")
echo "deployment-test: logs in $WORK"
anvil --chain-id 31338 --host 127.0.0.1 --port 0 >"$WORK/anvil.log" 2>&1 &
ANVIL=$!
cleanup() {
  kill "$ANVIL" 2>/dev/null || :
  wait "$ANVIL" 2>/dev/null || :
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
i=0
while :; do
  kill -0 "$ANVIL" 2>/dev/null || { cat "$WORK/anvil.log" >&2; exit 1; }
  PORT=$(awk '/^Listening on 127[.]0[.]0[.]1:/ { sub(/.*:/, "", $3); print $3; exit }' "$WORK/anvil.log")
  [ -z "$PORT" ] || break
  i=$((i + 1))
  [ "$i" -lt 100 ] || { echo "deployment-test: anvil did not start" >&2; exit 1; }
  sleep 0.1
done
RPC_URL=http://127.0.0.1:$PORT
export RPC_URL
failures=0
fail() { echo "deployment-test: FAIL $1" >&2; failures=$((failures + 1)); }

# Rehearsing on an occupied port must leave the existing node untouched,
# even when its chain id differs from the requested rehearsal chain.
before=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
if sh "$REHEARSAL" kyc 31337 9 18 "$PORT" >"$WORK/occupied-port.log" 2>&1; then
  fail "the occupied port rehearsal succeeded"
fi
after=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
if [ "$before" != "$after" ]; then
  fail "the occupied port received transactions (nonce $before -> $after)"
else
  echo "deployment-test: occupied port received no transactions"
fi

# Deploy a real ceremony verifier and gate to the node owned by this test.
export FOUNDRY_PROFILE=ceremony
forge build --root contracts >"$WORK/build.log" 2>&1
forge script contracts/script/DeployVerifier.s.sol --root contracts --sig 'run(string)' kyc \
  --rpc-url "$RPC_URL" --broadcast --unlocked --sender "$SENDER" >"$WORK/verifier.log" 2>&1
VERIFIER=$(awk '/kyc verifier / { print $NF; exit }' "$WORK/verifier.log")
test -n "$VERIFIER"
ANCHOR=$(cast to-dec "$(jq -r '.instances[0]' "$PROOF")")
forge script contracts/script/DeployKycGate.s.sol --root contracts \
  --sig 'run(address,address,uint256,uint256,uint256)' "$VERIFIER" "$SENDER" 9 18 "$ANCHOR" \
  --rpc-url "$RPC_URL" --broadcast --unlocked --sender "$SENDER" >"$WORK/gate.log" 2>&1
GATE=$(awk '/ KycGate 0x/ { print $NF; exit }' "$WORK/gate.log")
test -n "$GATE"
NAME=kyc CHAIN_ID=31338 SCOPE=9 MIN=18 ISSUER=$SENDER
export NAME CHAIN_ID SCOPE MIN VERIFIER GATE ISSUER

before=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
if sh "$CHECK" >"$WORK/valid-check.log" 2>&1; then
  echo "deployment-test: the configured issuer passes"
else
  cat "$WORK/valid-check.log" >&2
  fail "the configured issuer was refused"
fi
after=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
[ "$before" = "$after" ] || fail "the deployment check sent transactions"
NF=$(cast to-dec "$(jq -r '.instances[1]' "$PROOF")")
used=$(cast call "$GATE" 'used(uint256)(bool)' "$NF" --rpc-url "$RPC_URL")
[ "$used" = false ] || fail "the deployment check consumed the nullifier"

if ISSUER='' sh "$CHECK" >"$WORK/missing-issuer.log" 2>&1; then
  fail "the gate check accepted a missing expected issuer"
else
  if awk '/ISSUER is empty/ { found=1 } END { exit !found }' "$WORK/missing-issuer.log"; then
    echo "deployment-test: a missing expected issuer is refused"
  else
    fail "the missing issuer check failed for another reason"
  fi
fi

cast send "$GATE" 'setIssuer(address)' 0x000000000000000000000000000000000000bEEF \
  --unlocked --from "$SENDER" --rpc-url "$RPC_URL" >"$WORK/set-issuer.log"
if sh "$CHECK" >"$WORK/wrong-issuer.log" 2>&1; then
  fail "the gate check accepted an unexpected issuer"
else
  if awk '/the gate issuer is/ { found=1 } END { exit !found }' "$WORK/wrong-issuer.log"; then
    echo "deployment-test: an unexpected issuer is refused"
  else
    fail "the unexpected issuer check failed for another reason"
  fi
fi
# The proof's sender remains the credential holder after the issuer changes.
# The admission simulation must use the fifth instance, not ISSUER.
before=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
if ISSUER=0x000000000000000000000000000000000000bEEF sh "$CHECK" >"$WORK/changed-issuer.log" 2>&1; then
  echo "deployment-test: the proof sender passes with a different issuer"
else
  cat "$WORK/changed-issuer.log" >&2
  fail "the proof sender was confused with the issuer"
fi
after=$(cast nonce "$SENDER" --rpc-url "$RPC_URL")
[ "$before" = "$after" ] || fail "the changed-issuer check sent transactions"
used=$(cast call "$GATE" 'used(uint256)(bool)' "$NF" --rpc-url "$RPC_URL")
[ "$used" = false ] || fail "the changed-issuer check consumed the nullifier"
[ "$failures" -eq 0 ] || exit 1
echo "deployment-test: all checks passed"
