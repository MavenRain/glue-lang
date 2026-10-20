#!/bin/sh
# The anvil rehearsal of the deployment (docs/M9-PLAN.md D6): a local anvil
# with the chain id of the target network, the ceremony verifier NAME, a
# KycGate in front of it, then the read-only checks of check.sh. The ONLY
# broadcast is to this anvil. The sender is anvil account 0, unlocked in
# the node (`--unlocked --sender`): no key and no keystore anywhere.
#   sh contracts/script/rehearsal.sh NAME CHAIN_ID SCOPE MIN PORT
# Run it from the repository root (`make rehearsal`). It needs the outputs
# of `make e2e-ceremony SRS=FILE`. The logs go to build/e2e-ceremony/.
set -e
NAME=${1:-kycMerkle}
CHAIN_ID=${2:-31337}
SCOPE=${3:-9}
MIN=${4:-18}
PORT=${5:-8577}
CEREMONY=build/e2e-ceremony
PROOF=$CEREMONY/$NAME-proof/proof.json
SENDER=0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
SCRIPT=contracts/script
export FOUNDRY_PROFILE=ceremony

if [ ! -f "$CEREMONY/verifiers/$NAME.sol" ] || [ ! -f "$PROOF" ]; then
  echo "rehearsal: no $CEREMONY/verifiers/$NAME.sol or no $PROOF: run make e2e-ceremony SRS=FILE first" >&2
  exit 2
fi

# A unique IPC socket belongs to this anvil even if PORT is occupied.
# All RPC requests use it, so another server on PORT receives no calls.
ANVIL_DIR=$(mktemp -d /tmp/glue-rehearsal.XXXXXX)
RPC=$ANVIL_DIR/anvil.ipc

# The first 0x address on the first line that holds WORD and an address.
address_after() {
  awk -v word="$1" 'index($0, word) { for (i = 1; i <= NF; i++) if ($i ~ /^0x[0-9a-fA-F]+$/ && length($i) == 42) { print $i; exit } }' "$2"
}

anvil --chain-id "$CHAIN_ID" --host 127.0.0.1 --port "$PORT" --ipc "$RPC" --silent &
ANVIL=$!
cleanup() {
  kill "$ANVIL" 2>/dev/null || :
  wait "$ANVIL" 2>/dev/null || :
  rm -f "$RPC"
  rmdir "$ANVIL_DIR"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
i=0
while ! cast chain-id --rpc-url "$RPC" >/dev/null 2>&1; do
  i=$((i + 1))
  kill -0 "$ANVIL" 2>/dev/null || { echo "rehearsal: anvil stopped (port $PORT in use?)" >&2; exit 1; }
  [ "$i" -le 100 ] || { echo "rehearsal: anvil did not answer on port $PORT" >&2; exit 1; }
  sleep 0.1
done
got=$(cast chain-id --rpc-url "$RPC")
[ "$got" = "$CHAIN_ID" ] || { echo "rehearsal: chain id is $got, expected $CHAIN_ID" >&2; exit 1; }
echo "rehearsal: anvil chain id $got on port $PORT, sender $SENDER (unlocked in the node)"

forge build --root contracts >/dev/null
echo "rehearsal: ceremony profile built"

VLOG=$CEREMONY/rehearsal-verifier.log
if ! forge script $SCRIPT/DeployVerifier.s.sol --root contracts --sig 'run(string)' "$NAME" \
    --rpc-url "$RPC" --broadcast --unlocked --sender "$SENDER" > "$VLOG" 2>&1; then
  tail -20 "$VLOG" >&2
  echo "rehearsal: the verifier deploy failed (see $VLOG)" >&2
  exit 1
fi
VERIFIER=$(address_after verifier "$VLOG")
[ -n "$VERIFIER" ] || { echo "rehearsal: no verifier address in $VLOG" >&2; exit 1; }
echo "rehearsal: $NAME verifier $VERIFIER"
awk '/Estimated total gas|Paid:/ { print "rehearsal:   " $0 }' "$VLOG"

ANCHOR=$(cast to-dec "$(jq -r '.instances[0]' "$PROOF")")
GLOG=$CEREMONY/rehearsal-gate.log
if ! forge script $SCRIPT/DeployKycGate.s.sol --root contracts \
    --sig 'run(address,address,uint256,uint256,uint256)' "$VERIFIER" "$SENDER" "$SCOPE" "$MIN" "$ANCHOR" \
    --rpc-url "$RPC" --broadcast --unlocked --sender "$SENDER" > "$GLOG" 2>&1; then
  tail -20 "$GLOG" >&2
  echo "rehearsal: the gate deploy failed (see $GLOG)" >&2
  exit 1
fi
GATE=$(address_after KycGate "$GLOG")
[ -n "$GATE" ] || { echo "rehearsal: no gate address in $GLOG" >&2; exit 1; }
echo "rehearsal: KycGate $GATE (issuer $SENDER, scope $SCOPE, min $MIN, anchor approved)"
awk '/Estimated total gas|Paid:/ { print "rehearsal:   " $0 }' "$GLOG"

RPC_URL=$RPC CHAIN_ID=$CHAIN_ID NAME=$NAME VERIFIER=$VERIFIER GATE=$GATE ISSUER=$SENDER SCOPE=$SCOPE MIN=$MIN sh $SCRIPT/check.sh
echo "rehearsal: ok ($NAME at chain id $CHAIN_ID, verifier $VERIFIER, gate $GATE)"
