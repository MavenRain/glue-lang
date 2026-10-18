// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

import {Console, Vm, VM_ADDRESS} from "../test/Vm.sol";

/// Deploy the verifier of one example circuit: `--sig 'run(string)' NAME`,
/// with NAME the file name of src/verifiers/NAME.sol (square, range, poly,
/// nonzero, poseidon). The key comes from the forge flags (`--account NAME`
/// or `--private-key KEY`) and the chain from `--rpc-url`. The verifying key
/// is in the contract, so there is one contract for each circuit.
/// The script gets the creation code from the build artifact (`getCode`)
/// and does not import the verifiers: each verifier has approximately 5.8 KB
/// of creation code, and a script that imports 5 of them is above the
/// EIP-170 limit of `forge build --sizes`. A NAME with no artifact stops
/// the script in `getCode`.
contract DeployVerifier {
    Vm private constant VM = Vm(VM_ADDRESS);

    function run(string calldata name) external returns (address verifier) {
        bytes memory code = VM.getCode(string.concat(name, ".sol:Halo2Verifier"));
        VM.startBroadcast();
        verifier = createFrom(code);
        VM.stopBroadcast();
        Console.log(string.concat(name, " verifier"), verifier);
    }

    /// A CREATE of the creation code, as `new` does: the broadcast sends it
    /// as a deployment.
    function createFrom(bytes memory code) private returns (address verifier) {
        assembly ("memory-safe") {
            verifier := create(0, add(code, 0x20), mload(code))
        }
        require(verifier != address(0), "DeployVerifier: the CREATE of the verifier failed");
    }
}
