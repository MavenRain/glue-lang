// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

import {IVerifier, KycGate} from "../src/KycGate.sol";
import {Console, Vm, VM_ADDRESS} from "../test/Vm.sol";

/// Deploy a KycGate (docs/M9-PLAN.md D6, D7) in front of a deployed
/// verifier: `--sig 'run(address,address,uint256,uint256,uint256)'
/// VERIFIER ISSUER SCOPE MIN ANCHOR`. VERIFIER is the address that the
/// DeployVerifier run printed. When ANCHOR is not 0, the script also
/// approves it: that call comes from the broadcaster, so it reverts with
/// NotIssuer unless the broadcaster is ISSUER. The key and the chain come
/// from the forge flags (`--rpc-url`, `--account NAME`, `--broadcast`);
/// nothing in the script names them. contracts/README.md has the runbook.
contract DeployKycGate {
    Vm private constant VM = Vm(VM_ADDRESS);

    function run(address verifier, address issuer, uint256 scope, uint256 min, uint256 anchor)
        external
        returns (address gate)
    {
        VM.startBroadcast();
        KycGate deployed = new KycGate(IVerifier(verifier), issuer, scope, min);
        if (anchor != 0) {
            deployed.approve(anchor, true);
        }
        VM.stopBroadcast();
        gate = address(deployed);
        Console.log("KycGate", gate);
    }
}
