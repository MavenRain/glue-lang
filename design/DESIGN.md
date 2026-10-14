# glue-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
glue-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---
I want to write a new programming language with the following features:

As far as type constructors and higher types, include products, coproducts, dependent types, monads (map/bind), algebras and coalgebras (fold/unfold), and paths/loops/quotients/HITs/QITs (as from Cubical TT).  Proof irrelevance is prohibited.

For allowed base types and operations, take the minimal span of the following (mainly from 'direction 2' and 'direction 4'):



<pasted_content id="99c5">
**All five can be implemented in Cubical Type Theory (Cubical Agda), but with very different degrees of natural fit.** Cubical TT supplies computational univalence, path types, higher inductive types (HITs, including set-quotients), and a native higher-groupoid structure on every type. That makes some of the directions essentially native, while others are possible but not specially helped by the cubical features.

### Strongest native fits (2 and 4)
- **Direction 2 (quotient/path types for circuit relations and rewrites)**  
  This is a direct use of the primitives. Circuit functional equivalence, or the equivalence relation “same solution set / same relation \(R\)”, can be turned into a set-quotient HIT  
  ```agda
  data Circuit / ∼ : Type where
    [_] : Circuit → Circuit / ∼
    eq/ : c₁ ∼ c₂ → [ c₁ ] ≡ [ c₂ ]
    squash/ : isSet (Circuit / ∼)
  ```  
  Univalence then lets you transport any proof or construction along the resulting path. Optimization rewrites become paths that can be composed and used for computational transport. This is exactly what Cubical Agda was designed for.

- **Direction 4 (HITs + univalence for protocol/circuit laws)**  
  This is the textbook application of Cubical Agda (and of the original Vivekanandan HoTT-Crypt examples). Point constructors for messages/ciphertexts/proofs, path constructors for correctness laws (decrypt ∘ encrypt ≡ id, homomorphic properties, etc.), and higher path constructors for algebraic coherences. Mapping the HIT front-end to a concrete R1CS/Plonkish implementation is then an equivalence, and univalence turns that equivalence into a path along which properties transport. Cubical Agda gives definitional computation rules for the path constructors, so the extracted functions actually compute.

### Very natural (3 and parts of 5)
- **Direction 3 (categorical composable security)**  
  The existing formalization (`input-output-hk/categorical-crypto`) is ordinary Agda with `--without-K`. Porting or redoing it in Cubical Agda is straightforward; there are already cubical category libraries. The monoidal structure, simulators, and composition theorems do not *require* cubical features, but path types and univalence make the treatment of “equality of protocols” and of attack models cleaner (no need for setoid-style setoids). Higher categorical extensions (e.g., protocols up to coherent equivalence) become available for free.

- **Direction 5 (sheaf/torsor view and higher stacks)**  
  The 1-categorical sheaf/torsor model of \(\Sigma\)-protocols can be formalized in any dependent type theory, including Cubical Agda. The interesting part—higher stacks, \(\infty\)-topoi, or geometric morphisms that capture composition or malicious-verifier security—is precisely the domain of synthetic homotopy theory and higher category theory that Cubical TT is built for. One can develop the necessary higher sheaf models internally (there is ongoing work on constructive higher sheaf models of cubical type theory). This is research-level rather than “just code it,” but the foundation is present.

### Possible but not specially helped (1)
- **Direction 1 (Maurer-style abstractions / group-homomorphism preimage protocols)**  
  Perfectly implementable: groups, homomorphisms, the generic \(\Sigma\)-protocol, and the special-soundness / honest-verifier zero-knowledge proofs are ordinary algebraic constructions. Cubical Agda can host them (and the cubical library already has groups, rings, etc.), but you do not need univalence or HITs for the core result. The same development works equally well in Lean 4 or plain Agda. Cubical features become useful only if you later want to quotient the space of protocols or transport proofs across equivalent group presentations.

### Practical recommendation
If the goal is to exploit Cubical TT rather than merely use a dependently typed language, start with **2 and 4**. They turn the higher-order equivalences you are looking for into actual paths that compute, which is the distinctive payoff of the cubical setting. Direction 3 is a good second step once you have the algebraic and HIT infrastructure; direction 5 is the natural long-term extension if you want geometric or higher-stack models of security. Direction 1 can be done anywhere and does not particularly benefit from Cubical TT.
</pasted_content id="99c5">

 

<pasted_content id="99c5">
**Minimal span for Direction 2 (circuit equivalence via set-quotients / path types) and Direction 4 (HIT front-end + univalence transport) in Cubical Agda.**

Both assume `{-# OPTIONS --cubical #-}` and the usual imports from the cubical library (`Cubical.Foundations.Prelude`, `Cubical.Foundations.Univalence`, `Cubical.HITs.SetQuotients`, etc.). The fragments below are self-contained sketches of the core types and operations you would need; they are deliberately small.

### Direction 2 — Circuit relations as set-quotients
Goal: turn “these two circuits define the same relation” into a path so that proofs and constructions transport.

```agda
-- Core data
postulate
  Field : Type
  Witness : Type
  Public : Type

-- A circuit is a predicate on (public, witness)
Circuit : Type
Circuit = Public → Witness → Type

-- Satisfaction / solution set
Satisfies : Circuit → Public → Witness → Type
Satisfies C x w = C x w

-- Equivalence of circuits: same solution set (for every public input)
_∼_ : Circuit → Circuit → Type
C₁ ∼ C₂ = ∀ x w → Satisfies C₁ x w ≃ Satisfies C₂ x w

-- Set-quotient of circuits by functional equivalence
data Circuit/∼ : Type where
  [_]   : Circuit → Circuit/∼
  eq/   : {C₁ C₂ : Circuit} → C₁ ∼ C₂ → [ C₁ ] ≡ [ C₂ ]
  squash/ : isSet Circuit/∼

-- Key operations
-- 1. Recursor / universal property (maps out of the quotient)
rec : {B : Type} → isSet B
    → (f : Circuit → B)
    → (∀ {C₁ C₂} → C₁ ∼ C₂ → f C₁ ≡ f C₂)
    → Circuit/∼ → B
rec Bset f resp [ C ]     = f C
rec Bset f resp (eq/ e i) = resp e i
rec Bset f resp (squash/ _ _ _ _ i j) = isSet→SquareP (λ _ _ → Bset) _ _ _ _ i j

-- 2. Transport of a property along an equivalence
transportProp : {P : Circuit → Type}
              → {C₁ C₂ : Circuit} → C₁ ∼ C₂
              → P C₁ → P C₂
transportProp {P} e p = transport (cong P (eq/ e)) p

-- 3. Concrete example: “optimized circuit” path
postulate
  original opt : Circuit
  optEq : original ∼ opt

optPath : [ original ] ≡ [ opt ]
optPath = eq/ optEq

-- Any property proved for the original now holds for the optimized version
-- (and the transport computes).
```

Minimal operations you actually use: `[_]`, `eq/`, the recursor `rec`, and `transport` (or `subst`) along the resulting paths. The set-truncation constructor `squash/` is only needed so that the quotient is a set (required for the universal property to be well-behaved).

### Direction 4 — HIT front-end for a protocol/circuit law + univalence
Goal: write the abstract specification as a HIT whose path constructors are the laws, then map it by an equivalence to a concrete implementation; univalence turns the equivalence into a path that transports correctness.

```agda
-- Abstract HIT specification of a simple one-time-pad-style law
-- (point constructors + a path constructor for the correctness law)
data OTPSpec : Type where
  message  : OTPSpec
  cipher   : OTPSpec
  encrypt  : message ≡ cipher          -- the law “encryption produces a cipher”
  -- (higher paths can be added for algebraic properties if needed)

-- Concrete implementation (ordinary type, e.g. bits or field elements)
postulate
  Impl : Type
  m c  : Impl
  enc  : m ≡ c                         -- the real encryption function, packaged as a path
                                       -- (or an equivalence m ≃ c)

-- Interpreter / projection that realises the HIT in the concrete type
interp : OTPSpec → Impl
interp message = m
interp cipher  = c
interp (encrypt i) = enc i

-- Inverse direction (requires a bit more structure in a real example;
-- here we postulate the other half of the equivalence)
postulate
  interp⁻¹ : Impl → OTPSpec
  sec      : ∀ x → interp (interp⁻¹ x) ≡ x
  ret      : ∀ s → interp⁻¹ (interp s) ≡ s

-- The two types are equivalent
spec≃impl : OTPSpec ≃ Impl
spec≃impl = isoToEquiv (iso interp interp⁻¹ sec ret)

-- Univalence turns the equivalence into a path
spec≡impl : OTPSpec ≡ Impl
spec≡impl = ua spec≃impl

-- Transport a property proved on the specification to the implementation
-- (computes definitionally via uaβ)
transportLaw : {P : Type → Type} → P OTPSpec → P Impl
transportLaw p = transport (cong P spec≡impl) p

-- Example: a correctness certificate that lived on the HIT
-- is now a certificate on the concrete type.
correctness : message ≡ cipher
correctness = encrypt

correctnessImpl : m ≡ c
correctnessImpl = transport (λ i → spec≡impl i → spec≡impl i) correctness
-- or more directly: transport (cong (λ T → T → T) spec≡impl) correctness
```

Minimal types/operations:
- The HIT itself (`OTPSpec` with point constructors `message`/`cipher` and path constructor `encrypt`).
- An equivalence `spec≃impl` (built from an isomorphism or by hand).
- `ua : A ≃ B → A ≡ B` and the computation rule `uaβ` (so transport along `ua e` is the underlying function of `e`).
- `transport` / `subst` to move properties.

### Shared infrastructure you will also need
```agda
-- Equivalences (already in the cubical library)
_≃_ : Type → Type → Type
A ≃ B = Σ[ f ∈ (A → B) ] isEquiv f

-- Univalence
ua : {A B : Type} → A ≃ B → A ≡ B

-- Path algebra (composition, inversion, …)
_∙_ : {x y z : A} → x ≡ y → y ≡ z → x ≡ z
sym : {x y : A} → x ≡ y → y ≡ x

-- Function extensionality (often useful when relating circuits)
funExt : {f g : A → B} → (∀ x → f x ≡ g x) → f ≡ g
```

These two fragments give the complete minimal span: for Direction 2 the set-quotient of circuits together with its recursor and transport; for Direction 4 a HIT specification, an equivalence to a concrete implementation, `ua`, and transport of the laws. Everything else (more complex circuits, higher paths for homomorphic properties, R1CS encoding of the concrete side, etc.) is built by extending these constructors and using the same transport/recursor pattern.
</pasted_content id="99c5">

 

<pasted_content id="99c5">
**A practically useful output circuit type for Telcoin Network is a Plonkish (or R1CS) representation of a note-based private transfer / shielded balance circuit** (with optional compliance hooks).

Telcoin Network is a standard EVM L1 (Narwhal/Bullshark consensus + full EVM execution, TEL as native gas token). It has no native ZK layer today, but the roadmap and engineering notes explicitly mention “zero-knowledge privacy and on-chain agentic identity” at prototype stage. The highest-leverage use cases are therefore:

- Private TEL or eUSD/stablecoin transfers (shielded pool style)
- Private balance proofs or selective disclosure for compliance (AML range + membership in a clean set)
- Proofs that can be verified cheaply on-chain via a Groth16/Plonk verifier contract or a future precompile

These circuits are exactly the kind of objects that benefit from the higher-order structure of Directions 2 and 4.

### Minimal concrete type that is useful on Telcoin

```agda
-- Concrete Plonkish circuit for a private transfer (Halo2-style)
-- Public inputs: nullifier, new commitment, merkle root (or ASP root), amount commitment (optional)
-- Private witness: note, spend key, merkle path, randomness, etc.

record PrivateTransferCircuit : Type where
  field
    -- Domain size (power of two)
    n : ℕ
    -- Columns
    advice   : Fin n → Field          -- witness columns
    fixed    : Fin n → Field          -- selectors / constants
    instance : Fin n → Field          -- public inputs
    -- Constraints (multivariate polynomials that must vanish on the domain)
    gateConstraints : List (Polynomial Field)
    -- Lookup arguments (for range checks, membership, etc.)
    lookups : List LookupArgument
    -- Permutation argument (copy constraints / wiring)
    permutation : Permutation n

-- The relation it defines (what Direction 2 quotients by)
TransferRelation : PublicInputs → Witness → Type
TransferRelation pub w =
  -- balance conservation + nullifier uniqueness + merkle membership
  -- + (optional) compliance membership
  balanceOk pub w × nullifierOk pub w × merkleOk pub w × complianceOk pub w

-- Functional equivalence of two circuits (the relation used for the quotient)
_∼_ : PrivateTransferCircuit → PrivateTransferCircuit → Type
C₁ ∼ C₂ = ∀ pub w → Satisfies C₁ pub w ≃ Satisfies C₂ pub w
```

### How Directions 2 and 4 use this type

**Direction 2 (quotient / path types)**  
Quotient the space of `PrivateTransferCircuit` by `∼`. Different arithmetizations, gate optimizations, or selector arrangements that implement the same transfer relation become equal. Any correctness, soundness, or gas-cost property proved for one representative transports to all equivalent circuits. This is useful when iterating on circuit optimizations for mobile provers or when swapping between Halo2 and Plonky3 backends while keeping the same on-chain verifier interface.

**Direction 4 (HIT front-end + univalence)**  
Define an abstract HIT that only mentions the transfer laws:

```agda
data TransferSpec : Type where
  note       : TransferSpec
  nullifier  : TransferSpec
  commitment : TransferSpec
  -- laws as path constructors
  noDoubleSpend : nullifier ≡ deriveNullifier note
  balanceCons   : oldValue ≡ newValue          -- or the appropriate conservation path
  ownership     : spendKey · note ≡ nullifier  -- simplified
```

Then construct an equivalence `TransferSpec ≃ PrivateTransferCircuit` (or more precisely an equivalence between the HIT and the set of circuits that satisfy the relation). Univalence turns it into a path, so any abstract law proved on the HIT becomes a concrete constraint satisfaction proof on the Plonkish circuit that can be handed to a Halo2/Plonky prover and verified on Telcoin’s EVM.

### Why this is useful on Telcoin specifically

- **Privacy for remittances / stablecoins**: Users can shield TEL or eUSD, transfer privately, and unshield. The circuit is small enough for mobile proving.
- **Compliance hooks**: Add a lookup or membership constraint against an Association Set Provider (ASP) root or a zk-KYC credential. This matches the regulated-stablecoin direction (eUSD, eEUR) without full surveillance.
- **EVM integration**: The output is a standard Plonkish circuit whose verifier is a Solidity contract (or can be aggregated). Public inputs are ordinary field elements that an EVM transaction can supply.
- **Formal verification payoff**: The Cubical formalization lets you prove once that every circuit in the quotient preserves balance and prevents double-spends, then transport the proof to the optimized production circuit that actually runs.

Other circuits (BLS signature verification for epoch shuffling, staking-proof circuits, cross-chain attestation) are possible, but the private-transfer / shielded-note circuit is the one that most directly leverages the higher-order equivalences while delivering an immediately usable feature for Telcoin’s user base.
</pasted_content id="99c5">

 

<pasted_content id="99c5">
**Yes — a ZK-KYC credential circuit is one of the most practically useful output types for Directions 2 and 4 on a chain like Telcoin Network.**

Telcoin operates under a Nebraska Digital Asset Depository Institution charter, issues bank-backed eUSD, and requires full KYC/AML for Type A accounts. A selective-disclosure ZK-KYC circuit lets a user prove predicates (“age ≥ 18”, “jurisdiction ∈ allowed set”, “not on sanctions list”, “credential not expired/revoked”) without revealing the underlying identity document or full attribute set. The proof can gate eUSD mint/redeem, private transfers, or agentic-identity actions while remaining EVM-verifiable.

### Minimal useful circuit type

```agda
-- Plonkish (or R1CS) circuit for a selective-disclosure KYC credential
record ZKKycCircuit : Type where
  field
    n : ℕ
    advice   : Fin n → Field
    fixed    : Fin n → Field
    instance : Fin n → Field          -- public inputs
    gates    : List (Polynomial Field)
    lookups  : List LookupArgument    -- range checks, country codes, etc.
    permutation : Permutation n

-- Public inputs (what the verifier / EVM contract sees)
record KycPublic : Type where
  field
    issuerRoot     : Field            -- Merkle root or issuer public key hash
    claimedPreds   : List Field       -- e.g. age≥18, jurisdiction code, risk score
    nullifier      : Field            -- replay / revocation protection
    userAddress    : Field            -- binding to the on-chain identity
    timestamp      : Field

-- Private witness
record KycWitness : Type where
  field
    credential     : CredentialData   -- attributes, expiry, issuer signature or path
    merklePath     : MerklePath       -- or signature verification data
    secrets        : List Field       -- blinding, nullifier seed, etc.

-- The relation the circuit enforces
KycRelation : KycPublic → KycWitness → Type
KycRelation pub w =
  -- issuer membership / signature valid
  -- attributes satisfy the claimed predicates
  -- credential not expired (timestamp check)
  -- nullifier correctly derived
  -- bound to userAddress
  issuerOk pub w × predicatesOk pub w × expiryOk pub w × nullifierOk pub w × bindingOk pub w
```

### How Directions 2 and 4 use it

**Direction 2 (quotient by functional equivalence)**  
Quotient `ZKKycCircuit` by the relation  
`C₁ ∼ C₂ ⇔ ∀ pub w → Satisfies C₁ pub w ≃ Satisfies C₂ pub w`.  
Different implementations (Circom vs Halo2 vs Noir/ACIR, different hash functions, optimized range checks, BBS+ vs ECDSA signature verification, etc.) that realize the same KYC relation become equal. Any soundness, completeness, or “no extra leakage” property transports across the quotient. This is useful when you optimize the circuit for mobile provers or switch backends while keeping a stable on-chain verifier ABI.

**Direction 4 (HIT front-end + univalence)**  
```agda
data KycSpec : Type where
  credential : KycSpec
  predicate  : KycSpec
  nullifier  : KycSpec
  -- laws as path constructors
  selectiveDisclosure : predicate ≡ derivePredicate credential
  noReplay            : nullifier ≡ deriveNullifier credential
  issuerTrust         : credential ∈ IssuerSet
  binding             : credential · userSecret ≡ userAddress
```
Construct an equivalence between the HIT (or the set of circuits satisfying `KycRelation`) and a concrete `ZKKycCircuit`. Univalence yields a path along which the abstract laws become concrete constraint satisfaction. The resulting circuit can be compiled to a Groth16/Plonk verifier that a Telcoin EVM contract calls.

### Why this is high-value for Telcoin

- Directly supports regulated eUSD and on-chain bank accounts: users prove compliance predicates without uploading full KYC data on-chain.
- Works with existing patterns (Merkle issuer registries, nullifiers for revocation, selective attribute commitments).
- Small enough for practical proving; public inputs are ordinary field elements an EVM transaction can supply.
- Composes cleanly with the private-transfer circuit from the previous message (prove KYC + private transfer in one proof or via recursion).
- The Cubical formalization gives a machine-checked guarantee that every circuit in the quotient only reveals the claimed predicates and nothing else.

This is a stronger match for Telcoin’s current regulatory posture than a pure privacy-transfer circuit, while still fitting the higher-order equivalence/quotient framework.
</pasted_content id="99c5">


The host language shall be TinyCC.  Give me 4 or 5 reasonable options from which to choose about the compilation output type.


Let's go with option 1, but the output should proceed through the downstream, producing the halo2 circuit and associated verifier contract, as well as deployment script(s) for deploying the contract.  Proceed with development.

