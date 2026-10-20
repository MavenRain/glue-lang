// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

/// The interface of docs/IR.md section 5. Each generated verifier has it.
interface IVerifier {
    function verifyProof(bytes calldata proof, uint256[] calldata instances) external view returns (bool);
}

/// The admission policy of docs/M9-PLAN.md D7 in front of one kyc or
/// kycMerkle verifier.
///
/// The verifier, the scope and the minimum are fixed at the deploy. The
/// issuer approves an anchor: the commitment cm of a credential (the kyc
/// circuit) or the root of a credential tree (the kycMerkle circuit). A
/// caller admits with a proof and the four public inputs of the circuit in
/// the declared order: anchor, nf, scope, min. The gate checks that the
/// anchor is approved, that scope and min are its own and that the
/// nullifier nf is new. Then it asks the verifier. Only an accepted proof
/// records nf, so a second admission with the same credential at this
/// scope is refused (the replay). The caller chooses no anchor, no scope
/// and no min: each is a public input that the proof binds or a constant
/// of the gate.
///
/// The D1 bound: the circuit checks `attr >= min` with a range check on
/// `attr - min`, so min is at most r - 256 (r is the BN254 scalar field
/// modulus) and scope is below r. The constructor refuses other values.
///
/// Open item (docs/STATUS.md M9 s6): msg.sender is not bound by the proof.
/// A relayer can send a proof it has seen before the owner does, and the
/// nf is then spent by that admission. A binding needs a public input of
/// the circuit, which is not M9 work.
contract KycGate {
    /// The BN254 scalar field modulus r.
    uint256 private constant R = 21888242871839275222246405745257275088548364400416034343698204186575808495617;

    IVerifier public immutable verifier;
    uint256 public immutable scope;
    uint256 public immutable min;
    address public issuer;
    /// The approved anchors: cm (kyc) or the tree root (kycMerkle).
    mapping(uint256 => bool) public approved;
    /// The nullifiers of the admissions so far.
    mapping(uint256 => bool) public used;

    event IssuerChanged(address issuer);
    event Approved(uint256 indexed anchor, bool ok);
    event Admitted(uint256 indexed nf, uint256 indexed anchor, address indexed sender);

    error NotIssuer();
    error ScopeOutOfField(uint256 scope);
    error MinAboveBound(uint256 min);
    error WrongInstanceCount(uint256 count);
    error AnchorNotApproved(uint256 anchor);
    error WrongScope(uint256 scope);
    error WrongMin(uint256 min);
    error NullifierUsed(uint256 nf);
    error ProofRejected();

    constructor(IVerifier verifier_, address issuer_, uint256 scope_, uint256 min_) {
        if (scope_ >= R) revert ScopeOutOfField(scope_);
        if (min_ > R - 256) revert MinAboveBound(min_);
        verifier = verifier_;
        issuer = issuer_;
        scope = scope_;
        min = min_;
        emit IssuerChanged(issuer_);
    }

    modifier onlyIssuer() {
        if (msg.sender != issuer) revert NotIssuer();
        _;
    }

    /// Approve (ok) or revoke (not ok) an anchor. Only the issuer.
    function approve(uint256 anchor, bool ok) external onlyIssuer {
        approved[anchor] = ok;
        emit Approved(anchor, ok);
    }

    /// Give the issuer role to another address. Only the issuer.
    function setIssuer(address issuer_) external onlyIssuer {
        issuer = issuer_;
        emit IssuerChanged(issuer_);
    }

    /// Admit the holder of a credential: the proof and the public inputs
    /// `[anchor, nf, scope, min]` of the circuit. Returns nf and records it.
    function admit(bytes calldata proof, uint256[] calldata instances) external returns (uint256 nf) {
        if (instances.length != 4) revert WrongInstanceCount(instances.length);
        if (!approved[instances[0]]) revert AnchorNotApproved(instances[0]);
        if (instances[2] != scope) revert WrongScope(instances[2]);
        if (instances[3] != min) revert WrongMin(instances[3]);
        nf = instances[1];
        if (used[nf]) revert NullifierUsed(nf);
        (bool ok, bytes memory ret) =
            address(verifier).staticcall(abi.encodeWithSelector(IVerifier.verifyProof.selector, proof, instances));
        if (!(ok && ret.length == 32 && abi.decode(ret, (bool)))) revert ProofRejected();
        used[nf] = true;
        emit Admitted(nf, instances[0], msg.sender);
    }
}
