//! The crate error type.

use std::fmt;
use std::path::PathBuf;

use crate::backend::Digest;

/// The document that an error is about.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Doc {
    /// `NAME.circuit.json`
    Circuit,
    /// `NAME.witness.json`
    Witness,
    /// The proof JSON that `prove` writes.
    Proof,
}

/// Why a key is refused.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum KeyProblem {
    /// A required key is not there.
    Missing,
    /// The key is not in the contract.
    Unknown,
}

/// Every failure of the back end.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    /// The text is not JSON.
    Json { doc: Doc, reason: String },
    /// A key is missing or unknown.
    Key {
        doc: Doc,
        key: String,
        problem: KeyProblem,
    },
    /// The version key does not hold 1, or `field` is not `bn254-fr`.
    Version { doc: Doc, key: String },
    /// A field value is not a decimal string in `[0, r)`.
    Value { at: String, reason: &'static str },
    /// A count, length, index or kind is wrong.
    Shape { at: String, reason: String },
    /// `rows` is more than the usable rows of `2^k`.
    TooManyRows { rows: usize, usable: usize },
    /// The circuit and witness names are not equal.
    NameMismatch { circuit: String, witness: String },
    /// The witness does not satisfy the circuit (mock prover).
    Unsatisfied(String),
    /// halo2 refused a step (keygen, prove, verify).
    Halo2(String),
    /// A file could not be read or written.
    Io { path: String, reason: String },
    /// The SHA-256 of the SRS file is not the pin.
    Pin {
        path: PathBuf,
        expected: Digest,
        actual: Digest,
    },
    /// The circuit is valid IR, but the Solidity generator cannot take it.
    Unsupported(String),
    /// The command line is wrong.
    Usage(String),
}

impl Error {
    /// A `Shape` error at a location.
    pub fn shape(at: impl Into<String>, reason: impl Into<String>) -> Self {
        Self::Shape {
            at: at.into(),
            reason: reason.into(),
        }
    }

    /// An `Io` error for a path.
    pub fn io(path: &std::path::Path, err: &std::io::Error) -> Self {
        Self::Io {
            path: path.display().to_string(),
            reason: err.to_string(),
        }
    }
}

impl fmt::Display for Doc {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Circuit => write!(f, "circuit document"),
            Self::Witness => write!(f, "witness document"),
            Self::Proof => write!(f, "proof document"),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Json { doc, reason } => write!(f, "{doc}: not JSON: {reason}"),
            Self::Key { doc, key, problem } => match problem {
                KeyProblem::Missing => write!(f, "{doc}: missing key `{key}`"),
                KeyProblem::Unknown => write!(f, "{doc}: unknown key `{key}`"),
            },
            Self::Version { doc, key } => write!(f, "{doc}: unsupported `{key}`"),
            Self::Value { at, reason } => write!(f, "{at}: {reason}"),
            Self::Shape { at, reason } => write!(f, "{at}: {reason}"),
            Self::TooManyRows { rows, usable } => {
                write!(f, "rows {rows} is more than the usable rows {usable}")
            }
            Self::NameMismatch { circuit, witness } => {
                write!(
                    f,
                    "circuit name `{circuit}` is not witness name `{witness}`"
                )
            }
            Self::Unsatisfied(why) => write!(f, "witness does not satisfy the circuit: {why}"),
            Self::Halo2(why) => write!(f, "halo2: {why}"),
            Self::Io { path, reason } => write!(f, "{path}: {reason}"),
            Self::Pin {
                path,
                expected,
                actual,
            } => write!(
                f,
                "the SRS file {} has SHA-256 {actual}, the pin is {expected}",
                path.display()
            ),
            Self::Unsupported(why) => write!(f, "not supported: {why}"),
            Self::Usage(why) => write!(f, "usage: {why}"),
        }
    }
}

impl std::error::Error for Error {}
