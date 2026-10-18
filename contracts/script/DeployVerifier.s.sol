// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

import {Halo2Verifier as NonzeroVerifier} from "../src/verifiers/nonzero.sol";
import {Halo2Verifier as PolyVerifier} from "../src/verifiers/poly.sol";
import {Halo2Verifier as RangeVerifier} from "../src/verifiers/range.sol";
import {Halo2Verifier as SquareVerifier} from "../src/verifiers/square.sol";
import {Console, Vm, VM_ADDRESS} from "../test/Vm.sol";

/// Deploy the verifier of one example circuit: `--sig 'run(string)' NAME`,
/// with NAME one of square, range, poly, nonzero (src/verifiers/NAME.sol). The key
/// comes from the forge flags (`--account NAME` or `--private-key KEY`) and
/// the chain from `--rpc-url`. The verifying key is in the contract, so
/// there is one contract for each circuit.
contract DeployVerifier {
    Vm private constant VM = Vm(VM_ADDRESS);

    function run(string calldata name) external returns (address verifier) {
        bytes32 key = keccak256(bytes(name));
        VM.startBroadcast();
        verifier = deploy(key);
        VM.stopBroadcast();
        Console.log(string.concat(name, " verifier"), verifier);
    }

    function deploy(bytes32 key) private returns (address) {
        if (key == keccak256("square")) {
            return address(new SquareVerifier());
        }
        if (key == keccak256("range")) {
            return address(new RangeVerifier());
        }
        if (key == keccak256("poly")) {
            return address(new PolyVerifier());
        }
        if (key == keccak256("nonzero")) {
            return address(new NonzeroVerifier());
        }
        revert("DeployVerifier: the circuit is not square, range, poly or nonzero");
    }
}
