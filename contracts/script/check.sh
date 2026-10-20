#!/bin/sh
# The read-only checks of a deployment (docs/M9-PLAN.md D6): the chain id,
# the verifier code against the ceremony build artifact, the verifier on
# the ceremony proof, then the KycGate parameters and its admission as an
# eth_call. `cast call` and `cast code` are the only RPC verbs: nothing is
# sent.
#   RPC_URL=.. NAME=.. VERIFIER=0x.. [GATE=0x.. SCOPE=.. MIN=.. ISSUER=0x..] [CHAIN_ID=..] sh contracts/script/check.sh
# or the same values as arguments in that order. PROOF and ARTIFACT are the
# `make e2e-ceremony` proof and the ceremony profile artifact of NAME. Each
# check prints `check: <what> ok`; the first failure exits 1.
RPC_URL=${RPC_URL:-$1}
NAME=${NAME:-$2}
VERIFIER=${VERIFIER:-$3}
GATE=${GATE:-$4}
SCOPE=${SCOPE:-$5}
MIN=${MIN:-$6}
CHAIN_ID=${CHAIN_ID:-$7}
ISSUER=${ISSUER:-$8}
PROOF=${PROOF:-build/e2e-ceremony/$NAME-proof/proof.json}
ARTIFACT=${ARTIFACT:-build/e2e-ceremony/out/$NAME.sol/Halo2Verifier.json}

fail() { echo "check: $1: FAILED" >&2; exit 1; }
lower() { printf '%s' "$1" | tr 'A-F' 'a-f'; }
first() { awk '{ print $1; exit }'; }

[ -n "$RPC_URL" ] || fail "RPC_URL is empty"
[ -n "$NAME" ] || fail "NAME is empty"
[ -n "$VERIFIER" ] || fail "VERIFIER is empty"
[ -z "$GATE" ] || [ -n "$ISSUER" ] || fail "ISSUER is empty (required with GATE)"
[ -f "$PROOF" ] || fail "no proof $PROOF (make e2e-ceremony SRS=FILE)"
[ -f "$ARTIFACT" ] || fail "no artifact $ARTIFACT (FOUNDRY_PROFILE=ceremony forge build --root contracts)"

if [ -n "$CHAIN_ID" ]; then
  got=$(cast chain-id --rpc-url "$RPC_URL") || fail "chain id"
  [ "$got" = "$CHAIN_ID" ] || fail "chain id is $got, not $CHAIN_ID"
  echo "check: chain id $CHAIN_ID ok"
fi

code=$(cast code "$VERIFIER" --rpc-url "$RPC_URL") || fail "cast code $VERIFIER"
[ "$code" != "0x" ] || fail "no code at $VERIFIER"
want=$(cast keccak "$(jq -r .deployedBytecode.object "$ARTIFACT")")
have=$(cast keccak "$code")
[ "$have" = "$want" ] || fail "the code at $VERIFIER is not the $NAME ceremony artifact"
echo "check: verifier code = the $NAME ceremony artifact ok"

out=$(cast call "$VERIFIER" --data "$(jq -r .calldata "$PROOF")" --rpc-url "$RPC_URL") || fail "verifyProof call"
case "$out" in
  *01) echo "check: verifier accepts the $NAME proof ok" ;;
  *) fail "verifyProof returned $out" ;;
esac

if [ -n "$GATE" ]; then
  v=$(cast call "$GATE" 'verifier()(address)' --rpc-url "$RPC_URL" | first) || fail "gate verifier()"
  [ "$(lower "$v")" = "$(lower "$VERIFIER")" ] || fail "the gate verifier is $v, not $VERIFIER"
  echo "check: gate verifier = $VERIFIER ok"
  issuer=$(cast call "$GATE" 'issuer()(address)' --rpc-url "$RPC_URL") || fail "gate issuer()"
  issuer=$(printf '%s\n' "$issuer" | first)
  [ "$(lower "$issuer")" = "$(lower "$ISSUER")" ] || fail "the gate issuer is $issuer, not $ISSUER"
  echo "check: gate issuer = $ISSUER ok"
  s=$(cast call "$GATE" 'scope()(uint256)' --rpc-url "$RPC_URL" | first) || fail "gate scope()"
  [ "$s" = "$SCOPE" ] || fail "the gate scope is $s, not $SCOPE"
  echo "check: gate scope $SCOPE ok"
  m=$(cast call "$GATE" 'min()(uint256)' --rpc-url "$RPC_URL" | first) || fail "gate min()"
  [ "$m" = "$MIN" ] || fail "the gate min is $m, not $MIN"
  echo "check: gate min $MIN ok"
  nf=$(cast to-dec "$(jq -r '.instances[1]' "$PROOF")" | first)
  u=$(cast call "$GATE" 'used(uint256)(bool)' "$nf" --rpc-url "$RPC_URL" | first) || fail "gate used()"
  [ "$u" = "false" ] || fail "the nullifier of the proof is used"
  echo "check: gate nullifier unused ok"
  instances=$(for h in $(jq -r '.instances[]' "$PROOF"); do cast to-dec "$h" | first; done | paste -s -d , -)
  got=$(cast call "$GATE" 'admit(bytes,uint256[])(uint256)' "$(jq -r .proof "$PROOF")" "[$instances]" --rpc-url "$RPC_URL" | first) \
    || fail "gate admit (eth_call)"
  [ "$got" = "$nf" ] || fail "admit returned $got, not the nullifier $nf"
  echo "check: gate admits the $NAME proof (eth_call, nothing sent) ok"
fi
echo "check: all ok"
