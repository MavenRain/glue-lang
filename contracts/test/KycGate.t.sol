// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

import {IVerifier, KycGate} from "../src/KycGate.sol";
import {Halo2Verifier as KycVerifier} from "../src/verifiers/kyc.sol";
import {Halo2Verifier as KycMerkleVerifier} from "../src/verifiers/kycMerkle.sol";
import {Vm, VM_ADDRESS} from "./Vm.sol";

/// The tests of KycGate (docs/M9-PLAN.md D7) against the kyc and kycMerkle
/// verifiers and the proofs in test/fixtures/. The fixtures are at scope 9
/// and min 18. The public inputs are `[anchor, nf, scope, min]` with
/// anchor = cm (kyc) or the tree root (kycMerkle).
contract KycGateTest {
    Vm private constant VM = Vm(VM_ADDRESS);

    /// The BN254 scalar field modulus r.
    uint256 private constant R = 21888242871839275222246405745257275088548364400416034343698204186575808495617;

    uint256 private constant SCOPE = 9;
    uint256 private constant MIN = 18;

    IVerifier private kyc;
    IVerifier private kycMerkle;
    bytes private kycProof;
    uint256[] private kycInstances;
    bytes private merkleProof;
    uint256[] private merkleInstances;

    function setUp() public {
        kyc = IVerifier(address(new KycVerifier()));
        kycMerkle = IVerifier(address(new KycMerkleVerifier()));
        (kycProof, kycInstances) = fixture("kyc");
        (merkleProof, merkleInstances) = fixture("kycMerkle");
    }

    /// The proof and the public inputs of test/fixtures/NAME.proof.json.
    function fixture(string memory name) private view returns (bytes memory proof, uint256[] memory instances) {
        string memory json = VM.readFile(string.concat(VM.projectRoot(), "/test/fixtures/", name, ".proof.json"));
        proof = VM.parseJsonBytes(json, ".proof");
        instances = VM.parseJsonUintArray(json, ".instances");
    }

    /// A gate in front of `v` with this contract as the issuer.
    function gate(IVerifier v, uint256 scope_, uint256 min_) private returns (KycGate) {
        return new KycGate(v, address(this), scope_, min_);
    }

    /// The same deploy as an external call, so that expectRevert sees the
    /// constructor revert.
    function deploy(IVerifier v, uint256 scope_, uint256 min_) external returns (KycGate) {
        return new KycGate(v, address(this), scope_, min_);
    }

    /// A gate at the fixture scope and min with the kycMerkle root approved.
    function approvedMerkleGate() private returns (KycGate g) {
        g = gate(kycMerkle, SCOPE, MIN);
        g.approve(merkleInstances[0], true);
    }

    function testAdmitsTheKycMerkleFixture() public {
        KycGate g = approvedMerkleGate();
        uint256 nf = g.admit(merkleProof, merkleInstances);
        require(nf == merkleInstances[1], "admit did not return the nullifier");
        require(g.used(nf), "the nullifier is not recorded");
    }

    function testRefusesTheSameNullifierTwice() public {
        KycGate g = approvedMerkleGate();
        uint256 nf = g.admit(merkleProof, merkleInstances);
        VM.expectRevert(abi.encodeWithSelector(KycGate.NullifierUsed.selector, nf));
        g.admit(merkleProof, merkleInstances);
    }

    function testRefusesAnUnapprovedAnchor() public {
        KycGate g = gate(kycMerkle, SCOPE, MIN);
        VM.expectRevert(abi.encodeWithSelector(KycGate.AnchorNotApproved.selector, merkleInstances[0]));
        g.admit(merkleProof, merkleInstances);
    }

    function testRefusesARevokedAnchor() public {
        KycGate g = approvedMerkleGate();
        g.approve(merkleInstances[0], false);
        VM.expectRevert(abi.encodeWithSelector(KycGate.AnchorNotApproved.selector, merkleInstances[0]));
        g.admit(merkleProof, merkleInstances);
    }

    function testRefusesAnotherScope() public {
        KycGate g = gate(kycMerkle, SCOPE + 1, MIN);
        g.approve(merkleInstances[0], true);
        VM.expectRevert(abi.encodeWithSelector(KycGate.WrongScope.selector, SCOPE));
        g.admit(merkleProof, merkleInstances);
    }

    function testRefusesAnotherMin() public {
        KycGate g = gate(kycMerkle, SCOPE, MIN - 1);
        g.approve(merkleInstances[0], true);
        VM.expectRevert(abi.encodeWithSelector(KycGate.WrongMin.selector, MIN));
        g.admit(merkleProof, merkleInstances);
    }

    function testRefusesAWrongInstanceCount() public {
        KycGate g = approvedMerkleGate();
        uint256[] memory three = new uint256[](3);
        three[0] = merkleInstances[0];
        three[1] = merkleInstances[1];
        three[2] = merkleInstances[2];
        VM.expectRevert(abi.encodeWithSelector(KycGate.WrongInstanceCount.selector, 3));
        g.admit(merkleProof, three);
    }

    function testRefusesATamperedProofAndRecordsNothing() public {
        KycGate g = approvedMerkleGate();
        bytes memory tampered = merkleProof;
        tampered[0] = bytes1(uint8(tampered[0]) ^ 0x01);
        VM.expectRevert(abi.encodeWithSelector(KycGate.ProofRejected.selector));
        g.admit(tampered, merkleInstances);
        require(!g.used(merkleInstances[1]), "a rejected proof recorded the nullifier");
    }

    function testRefusesMinAboveTheBound() public {
        VM.expectRevert(abi.encodeWithSelector(KycGate.MinAboveBound.selector, R - 255));
        this.deploy(kycMerkle, SCOPE, R - 255);
        KycGate g = this.deploy(kycMerkle, SCOPE, R - 256);
        require(g.min() == R - 256, "min r - 256 did not deploy");
        VM.expectRevert(abi.encodeWithSelector(KycGate.ScopeOutOfField.selector, R));
        this.deploy(kycMerkle, R, MIN);
    }

    function testOnlyTheIssuerApproves() public {
        KycGate g = gate(kycMerkle, SCOPE, MIN);
        VM.prank(address(0xBEEF));
        VM.expectRevert(abi.encodeWithSelector(KycGate.NotIssuer.selector));
        g.approve(merkleInstances[0], true);
        VM.prank(address(0xBEEF));
        VM.expectRevert(abi.encodeWithSelector(KycGate.NotIssuer.selector));
        g.setIssuer(address(0xBEEF));
        g.setIssuer(address(0xBEEF));
        require(g.issuer() == address(0xBEEF), "setIssuer by the issuer did not change the issuer");
        VM.expectRevert(abi.encodeWithSelector(KycGate.NotIssuer.selector));
        g.approve(merkleInstances[0], true);
    }

    function testAdmitsTheKycFixtureWithAnApprovedCommitment() public {
        KycGate g = gate(kyc, SCOPE, MIN);
        g.approve(kycInstances[0], true);
        uint256 nf = g.admit(kycProof, kycInstances);
        require(nf == kycInstances[1] && g.used(nf), "the kyc admission did not record the nullifier");
    }
}
