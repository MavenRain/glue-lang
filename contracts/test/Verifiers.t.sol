// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

import {Halo2Verifier as NonzeroVerifier} from "../src/verifiers/nonzero.sol";
import {Halo2Verifier as PolyVerifier} from "../src/verifiers/poly.sol";
import {Halo2Verifier as RangeVerifier} from "../src/verifiers/range.sol";
import {Halo2Verifier as SquareVerifier} from "../src/verifiers/square.sol";
import {Vm, VM_ADDRESS} from "./Vm.sol";

/// The interface of docs/IR.md section 5. Each generated verifier has it.
interface Verifier {
    function verifyProof(bytes calldata proof, uint256[] calldata instances) external view returns (bool);
}

/// The tests of one example circuit: its verifier (src/verifiers/NAME.sol)
/// against the proof in test/fixtures/NAME.proof.json. `make e2e` writes
/// both with the same keys. Each example is one contract below.
abstract contract VerifierCase {
    Vm internal constant VM = Vm(VM_ADDRESS);

    /// The BN254 scalar field modulus r.
    uint256 private constant R = 21888242871839275222246405745257275088548364400416034343698204186575808495617;

    Verifier private verifier;
    bytes private proof;
    uint256[] internal instances;
    bytes private calldata_;

    /// The example name: the file names of the verifier and the fixture.
    function name() internal pure virtual returns (string memory);

    /// Deploy the verifier of the example.
    function deploy() internal virtual returns (address);

    function setUp() public {
        verifier = Verifier(deploy());
        string memory json = VM.readFile(string.concat(VM.projectRoot(), "/test/fixtures/", name(), ".proof.json"));
        proof = VM.parseJsonBytes(json, ".proof");
        instances = VM.parseJsonUintArray(json, ".instances");
        calldata_ = VM.parseJsonBytes(json, ".calldata");
    }

    function accepts(bytes memory proof_, uint256[] memory instances_) private view returns (bool) {
        (bool ok, bytes memory result) =
            address(verifier).staticcall(abi.encodeWithSelector(Verifier.verifyProof.selector, proof_, instances_));
        return ok && result.length == 32 && abi.decode(result, (bool));
    }

    function copyInstances() private view returns (uint256[] memory copy) {
        copy = new uint256[](instances.length);
        for (uint256 i = 0; i < instances.length; i++) {
            copy[i] = instances[i];
        }
    }

    function testCliCalldataIsTheAbiEncoding() public view {
        bytes memory encoded = abi.encodeWithSelector(Verifier.verifyProof.selector, proof, instances);
        require(keccak256(encoded) == keccak256(calldata_), "the CLI calldata is not the ABI encoding");
    }

    function testValidProofVerifies() public view {
        require(verifier.verifyProof(proof, instances), "the valid proof is rejected");
        // The rejection tests below go through `accepts`: it must say yes here.
        require(accepts(proof, instances), "accepts() rejects the valid proof");
    }

    function testCliCalldataVerifies() public view {
        (bool ok, bytes memory result) = address(verifier).staticcall(calldata_);
        require(ok && abi.decode(result, (bool)), "the CLI calldata is rejected");
    }

    function testRedirectedInstancePointerIsRejected() public view {
        uint256[] memory bad = copyInstances();
        if (bad.length == 0) {
            // No public input (nonzero): the appended array has one value, so its count is wrong.
            bad = new uint256[](1);
        } else {
            bad[0] = addmod(bad[0], 1, R);
        }
        bytes memory redirected = bytes.concat(calldata_, abi.encode(bad));
        // Point the ABI argument at the appended array. The original valid
        // array remains at the generator's hardcoded offset.
        uint256 offset = calldata_.length - 4 + 32;
        assembly {
            mstore(add(redirected, 0x44), offset)
        }
        (bool ok, bytes memory result) = address(verifier).staticcall(redirected);
        require(!ok || !abi.decode(result, (bool)), "redirected invalid instances are accepted");
    }

    function testRedirectedProofPointerIsRejected() public view {
        bytes memory redirected = bytes.concat(calldata_, abi.encode(uint256(32)), new bytes(32));
        uint256 offset = calldata_.length - 4;
        assembly {
            mstore(add(redirected, 0x24), offset)
        }
        (bool ok, bytes memory result) = address(verifier).staticcall(redirected);
        require(!ok || !abi.decode(result, (bool)), "redirected invalid proof is accepted");
    }

    function testTamperedInstanceIsRejected() public view {
        for (uint256 i = 0; i < instances.length; i++) {
            uint256[] memory bad = copyInstances();
            bad[i] = addmod(bad[i], 1, R);
            require(!accepts(proof, bad), "a tampered instance is accepted");
        }
    }

    function testInstancePlusModulusIsRejected() public view {
        for (uint256 i = 0; i < instances.length; i++) {
            uint256[] memory bad = copyInstances();
            bad[i] = bad[i] + R;
            require(!accepts(proof, bad), "an instance not below r is accepted");
        }
    }

    function testWrongInstanceCountIsRejected() public view {
        // No public input (nonzero): there is no shorter array, only a longer one.
        uint256[] memory fewer = new uint256[](instances.length == 0 ? 0 : instances.length - 1);
        uint256[] memory more = new uint256[](instances.length + 1);
        for (uint256 i = 0; i < instances.length; i++) {
            more[i] = instances[i];
            if (i < fewer.length) {
                fewer[i] = instances[i];
            }
        }
        require(instances.length == 0 || !accepts(proof, fewer), "too few instances are accepted");
        require(!accepts(proof, more), "too many instances are accepted");
    }

    function testTamperedProofIsRejected() public view {
        uint256[4] memory offsets = [uint256(0), 40, proof.length / 2, proof.length - 1];
        for (uint256 i = 0; i < offsets.length; i++) {
            bytes memory bad = bytes.concat(proof);
            bad[offsets[i]] = bad[offsets[i]] ^ 0x01;
            require(!accepts(bad, instances), "a tampered proof is accepted");
        }
    }

    function testTruncatedProofIsRejected() public view {
        bytes memory short = new bytes(proof.length - 32);
        for (uint256 i = 0; i < short.length; i++) {
            short[i] = proof[i];
        }
        require(!accepts(short, instances), "a truncated proof is accepted");
    }
}

/// examples/square.glue with y=9 x=3 (test/circuit/expect.txt).
contract SquareVerifierTest is VerifierCase {
    function name() internal pure override returns (string memory) {
        return "square";
    }

    function deploy() internal override returns (address) {
        return address(new SquareVerifier());
    }

    function testThePublicInputIsY() public view {
        require(instances.length == 1 && instances[0] == 9, "the public input of square is not y=9");
    }
}

/// examples/range.glue with next=256 b=255.
contract RangeVerifierTest is VerifierCase {
    function name() internal pure override returns (string memory) {
        return "range";
    }

    function deploy() internal override returns (address) {
        return address(new RangeVerifier());
    }

    function testThePublicInputIsNext() public view {
        require(instances.length == 1 && instances[0] == 256, "the public input of range is not next=256");
    }
}

/// examples/poly.glue with a=3 y=19 x=2.
contract PolyVerifierTest is VerifierCase {
    function name() internal pure override returns (string memory) {
        return "poly";
    }

    function deploy() internal override returns (address) {
        return address(new PolyVerifier());
    }

    function testThePublicInputsAreAAndY() public view {
        require(
            instances.length == 2 && instances[0] == 3 && instances[1] == 19, "the public inputs of poly are not a=3 y=19"
        );
    }
}

/// examples/nonzero.glue with x=3. The hint v is private, so there is no
/// public input.
contract NonzeroVerifierTest is VerifierCase {
    function name() internal pure override returns (string memory) {
        return "nonzero";
    }

    function deploy() internal override returns (address) {
        return address(new NonzeroVerifier());
    }

    function testThereIsNoPublicInput() public view {
        require(instances.length == 0, "nonzero has a public input");
    }
}
