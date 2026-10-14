//! glue-halo2: the halo2 back end of glue-lang. It reads the Plonkish IR v1
//! documents (docs/IR.md) and makes keys, proofs and a Solidity verifier.

pub mod backend;
pub mod circuit;
pub mod error;
pub mod field;
pub mod ir;

pub use backend::{mock, Keys, Proof, Srs, Verifier};
pub use circuit::{GlueCircuit, Shape};
pub use error::Error;
pub use ir::{CircuitDoc, WitnessDoc};
