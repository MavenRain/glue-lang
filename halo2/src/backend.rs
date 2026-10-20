//! The SRS, keys, proofs, native verification, the Solidity verifier and the
//! EVM calldata.

use std::fmt;
use std::fs::{self, File};
use std::io::{self, BufReader, BufWriter, Write};
use std::path::Path;

use halo2_proofs::dev::MockProver;
use halo2_proofs::halo2curves::bn256::{Bn256, Fr, G1Affine};
use halo2_proofs::plonk::{
    create_proof, keygen_pk, keygen_vk, verify_proof, ProvingKey, VerifyingKey,
};
use halo2_proofs::poly::commitment::Params;
use halo2_proofs::poly::kzg::commitment::{KZGCommitmentScheme, ParamsKZG};
use halo2_proofs::poly::kzg::multiopen::{ProverSHPLONK, VerifierSHPLONK};
use halo2_proofs::poly::kzg::strategy::SingleStrategy;
use halo2_proofs::poly::Rotation;
use halo2_proofs::transcript::TranscriptWriterBuffer;
use halo2_proofs::SerdeFormat;
use halo2_solidity_verifier::{
    encode_calldata, BatchOpenScheme, Keccak256Transcript, SolidityGenerator,
};
use rand_chacha::ChaCha20Rng;
use rand_core::{OsRng, SeedableRng};
use serde_json::Value;
use sha2::{Digest as _, Sha256};

use crate::circuit::{GlueCircuit, Shape};
use crate::error::{Doc, Error, KeyProblem};
use crate::field::{from_hex, to_hex};

/// The seed of the development SRS. Anyone can rebuild that SRS, so it is
/// INSECURE: use it for local tests only.
const DEV_SEED: u64 = 0x676c_7565_6c61_6e67;

const SRS_FILE: &str = "srs.bin";
const VK_FILE: &str = "vk.bin";
const PK_FILE: &str = "pk.bin";

fn halo2(step: &'static str) -> impl Fn(halo2_proofs::plonk::Error) -> Error {
    move |err| Error::Halo2(format!("{step}: {err}"))
}

fn write_file(
    path: &Path,
    write: impl FnOnce(&mut BufWriter<File>) -> io::Result<()>,
) -> Result<(), Error> {
    File::create(path)
        .and_then(|file| {
            let mut out = BufWriter::new(file);
            write(&mut out).and_then(|()| out.flush())
        })
        .map_err(|err| Error::io(path, &err))
}

fn read_file<T>(
    path: &Path,
    read: impl FnOnce(&mut BufReader<File>) -> io::Result<T>,
) -> Result<T, Error> {
    File::open(path)
        .and_then(|file| read(&mut BufReader::new(file)))
        .map_err(|err| Error::io(path, &err))
}

/// A SHA-256 digest: the pin of an SRS file.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Digest([u8; 32]);

impl Digest {
    /// Parse 64 hex digits (either case).
    pub fn parse(hex: &str) -> Result<Self, Error> {
        hex::decode(hex)
            .ok()
            .and_then(|bytes| <[u8; 32]>::try_from(bytes).ok())
            .map(Self)
            .ok_or_else(|| Error::Usage("--sha256 HEX needs 64 hex digits".to_string()))
    }

    /// The SHA-256 of `bytes`.
    pub fn of(bytes: &[u8]) -> Self {
        Self(Sha256::digest(bytes).into())
    }
}

impl fmt::Display for Digest {
    /// Lowercase hex, 64 digits.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&hex::encode(self.0))
    }
}

/// The KZG structured reference string, sized to the circuit `k`.
#[derive(Clone, Debug)]
pub struct Srs(ParamsKZG<Bn256>);

impl Srs {
    /// An INSECURE SRS from a fixed seed. Use it for local tests only.
    pub fn dev(k: u32) -> Self {
        Self(ParamsKZG::<Bn256>::setup(
            k,
            ChaCha20Rng::seed_from_u64(DEV_SEED),
        ))
    }

    /// Read an SRS file (halo2 KZG format) and downsize it to `k`.
    pub fn read(path: &Path, k: u32) -> Result<Self, Error> {
        read_file(path, ParamsKZG::<Bn256>::read).and_then(|params| Self::sized(params, k))
    }

    /// Read an SRS file whose SHA-256 is `pin`, and downsize it to `k`.
    /// The pin check comes before the parse and before the `k` check.
    pub fn read_pinned(path: &Path, k: u32, pin: &Digest) -> Result<Self, Error> {
        fs::read(path)
            .map_err(|err| Error::io(path, &err))
            .and_then(|bytes| {
                let actual = Digest::of(&bytes);
                if actual == *pin {
                    ParamsKZG::<Bn256>::read(&mut io::Cursor::new(&bytes))
                        .map_err(|err| Error::io(path, &err))
                } else {
                    Err(Error::Pin {
                        path: path.to_path_buf(),
                        expected: *pin,
                        actual,
                    })
                }
            })
            .and_then(|params| Self::sized(params, k))
    }

    /// Refuse a file with a `k` below the circuit `k`, then downsize.
    fn sized(params: ParamsKZG<Bn256>, k: u32) -> Result<Self, Error> {
        if params.k() < k {
            Err(Error::shape(
                "srs",
                format!("k {} is less than the circuit k {k}", params.k()),
            ))
        } else {
            Ok(Self::downsized(params, k))
        }
    }

    fn downsized(params: ParamsKZG<Bn256>, k: u32) -> Self {
        // `downsize` works in place.
        let mut params = params;
        if params.k() > k {
            params.downsize(k);
        }
        Self(params)
    }

    pub fn write(&self, path: &Path) -> Result<(), Error> {
        write_file(path, |out| self.0.write(out))
    }

    pub fn k(&self) -> u32 {
        self.0.k()
    }
}

/// A proof with its instance columns.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Proof {
    bytes: Vec<u8>,
    columns: Vec<Vec<Fr>>,
}

impl Proof {
    pub fn new(bytes: Vec<u8>, columns: Vec<Vec<Fr>>) -> Self {
        Self { bytes, columns }
    }

    pub fn bytes(&self) -> &[u8] {
        &self.bytes
    }

    /// Every instance column.
    pub fn columns(&self) -> &[Vec<Fr>] {
        &self.columns
    }

    /// Instance column 0: the public inputs of `verifyProof`.
    pub fn public(&self) -> &[Fr] {
        self.columns.first().map_or(&[], Vec::as_slice)
    }

    /// The EVM calldata for `verifyProof(bytes,uint256[])`.
    pub fn calldata(&self) -> Vec<u8> {
        encode_calldata(None, &self.bytes, self.public())
    }

    /// The proof JSON: `proof`, `instances` (column 0), `columns`, `calldata`.
    pub fn to_json(&self) -> String {
        let hex_column = |column: &[Fr]| column.iter().map(to_hex).collect::<Vec<_>>();
        let value = serde_json::json!({
            "proof": format!("0x{}", hex::encode(&self.bytes)),
            "instances": hex_column(self.public()),
            "columns": self.columns.iter().map(Vec::as_slice).map(hex_column).collect::<Vec<_>>(),
            "calldata": format!("0x{}", hex::encode(self.calldata())),
        });
        format!("{value:#}\n")
    }

    /// Read a proof JSON. The `columns` key gives the instances. The
    /// `instances` and `calldata` keys are what the EVM sees, so they must
    /// agree with `proof` and `columns`.
    pub fn from_json(text: &str) -> Result<Self, Error> {
        let value = serde_json::from_str::<Value>(text).map_err(|err| Error::Json {
            doc: Doc::Proof,
            reason: err.to_string(),
        })?;
        let key = |name: &str| {
            value.get(name).ok_or_else(|| Error::Key {
                doc: Doc::Proof,
                key: name.to_string(),
                problem: KeyProblem::Missing,
            })
        };
        let bytes = key("proof").and_then(|proof| hex_bytes(proof, "proof"))?;
        key("columns")
            .and_then(|columns| {
                columns
                    .as_array()
                    .ok_or_else(|| Error::shape("columns", "not an array"))
            })
            .and_then(|columns| {
                columns
                    .iter()
                    .enumerate()
                    .map(|(i, column)| hex_values(column, &format!("columns[{i}]")))
                    .collect::<Result<Vec<_>, _>>()
            })
            .map(|columns| Self { bytes, columns })
            .and_then(|proof| {
                key("instances")
                    .and_then(|instances| hex_values(instances, "instances"))
                    .and_then(|instances| {
                        agree("instances", instances.as_slice() == proof.public())
                    })
                    .and_then(|()| key("calldata"))
                    .and_then(|calldata| hex_bytes(calldata, "calldata"))
                    .and_then(|calldata| agree("calldata", calldata == proof.calldata()))
                    .map(|()| proof)
            })
    }
}

/// A derived key of the proof JSON must agree with `proof` and `columns`.
fn agree(at: &str, same: bool) -> Result<(), Error> {
    if same {
        Ok(())
    } else {
        Err(Error::shape(
            at,
            "does not agree with `proof` and `columns`",
        ))
    }
}

fn hex_bytes(value: &Value, at: &str) -> Result<Vec<u8>, Error> {
    value
        .as_str()
        .and_then(|text| text.strip_prefix("0x"))
        .and_then(|digits| hex::decode(digits).ok())
        .ok_or_else(|| Error::shape(at, "not a 0x hex string"))
}

fn hex_values(value: &Value, at: &str) -> Result<Vec<Fr>, Error> {
    value
        .as_array()
        .ok_or_else(|| Error::shape(at, "not an array"))
        .and_then(|items| {
            items
                .iter()
                .map(|item| {
                    item.as_str().and_then(from_hex).ok_or(Error::Value {
                        at: at.to_string(),
                        reason: "not a 0x hex value below r",
                    })
                })
                .collect()
        })
}

fn check_srs(srs: &Srs, shape: &Shape) -> Result<(), Error> {
    if srs.k() == shape.doc().k() {
        Ok(())
    } else {
        Err(Error::shape(
            "srs",
            format!("k {} is not the circuit k {}", srs.k(), shape.doc().k()),
        ))
    }
}

fn check_shape(expected: &Shape, actual: &Shape) -> Result<(), Error> {
    if expected.doc() == actual.doc() {
        Ok(())
    } else {
        Err(Error::shape("circuit", "does not match the keys"))
    }
}

fn check_instances(shape: &Shape, proof: &Proof) -> Result<(), Error> {
    let doc = shape.doc();
    if proof.columns().len() == doc.columns().count(crate::ir::ColumnKind::Instance)
        && proof.columns().iter().enumerate().all(|(index, values)| {
            if index == 0 {
                values.len() == doc.public()
            } else {
                values.len() <= doc.usable_rows()
            }
        })
    {
        Ok(())
    } else {
        Err(Error::shape(
            "instances",
            "do not match the circuit document",
        ))
    }
}

/// Key readers configure the supplied shape but retain the saved fixed and
/// permutation commitments. Rebuild the VK to bind those to this document.
fn check_vk(srs: &Srs, vk: &VerifyingKey<G1Affine>, shape: &Shape) -> Result<(), Error> {
    let expected =
        keygen_vk(&srs.0, &GlueCircuit::keygen(shape.clone())).map_err(halo2("check key"))?;
    let encoded = |key: &VerifyingKey<G1Affine>| {
        let mut bytes = Vec::new();
        key.write(&mut bytes, SerdeFormat::RawBytes)
            .map(|()| bytes)
            .map_err(|err| Error::Halo2(format!("encode key: {err}")))
    };
    encoded(vk).and_then(|actual| {
        encoded(&expected).and_then(|wanted| {
            if actual == wanted {
                Ok(())
            } else {
                Err(Error::shape("keys", "do not match the circuit document"))
            }
        })
    })
}

/// The assembly reads canonical calldata positions. Reject arguments whose
/// ABI offsets point elsewhere, so it checks the values Solidity decoded.
fn guard_calldata(text: String) -> Result<String, Error> {
    let anchor = "        assembly {\n";
    if text.contains(anchor) {
        let guarded = text.replacen(
            anchor,
            concat!(
                "        assembly {\n",
                "            if iszero(eq(proof.offset, PROOF_CPTR)) { revert(0, 0) }\n",
                "            if iszero(eq(instances.offset, INSTANCE_CPTR)) { revert(0, 0) }\n",
            ),
            1,
        );
        if guarded.ends_with('\n') {
            Ok(guarded)
        } else {
            Ok(guarded + "\n")
        }
    } else {
        Err(Error::Unsupported("missing verifier assembly".to_string()))
    }
}

fn verify_with(srs: &Srs, vk: &VerifyingKey<G1Affine>, proof: &Proof) -> Result<(), Error> {
    let columns = proof.columns.iter().map(Vec::as_slice).collect::<Vec<_>>();
    let mut transcript = Keccak256Transcript::<G1Affine, &[u8]>::new(proof.bytes.as_slice());
    verify_proof::<_, VerifierSHPLONK<'_, Bn256>, _, _, SingleStrategy<'_, Bn256>>(
        &srs.0,
        vk,
        SingleStrategy::new(&srs.0),
        &[columns.as_slice()],
        &mut transcript,
    )
    .map_err(halo2("verify"))
}

/// Check the asserts of `SolidityGenerator::new` first, so that a circuit
/// it cannot take is an error and not a panic.
fn solidity_with(srs: &Srs, vk: &VerifyingKey<G1Affine>, public: usize) -> Result<String, Error> {
    let cs = vk.cs();
    match () {
        () if cs.num_advice_columns() == 0 => Err(Error::Unsupported(
            "a circuit with no advice column".to_string(),
        )),
        () if cs.num_instance_columns() > 1 => Err(Error::Unsupported(
            "more than one instance column".to_string(),
        )),
        () if cs
            .instance_queries()
            .iter()
            .any(|(_, rotation)| *rotation != Rotation::cur()) =>
        {
            Err(Error::Unsupported("a rotated instance query".to_string()))
        }
        () => SolidityGenerator::new(&srs.0, vk, BatchOpenScheme::Bdfg21, public)
            .render()
            .map_err(|err| Error::Unsupported(format!("render: {err}")))
            .and_then(guard_calldata),
    }
}

/// The SRS and the verifying key: what `verify` and `solidity` need.
#[derive(Clone, Debug)]
pub struct Verifier {
    srs: Srs,
    vk: VerifyingKey<G1Affine>,
    shape: Shape,
}

impl Verifier {
    /// Read `srs.bin` and `vk.bin` from a keys directory.
    pub fn load(dir: &Path, shape: &Shape) -> Result<Self, Error> {
        let srs = Srs::read(&dir.join(SRS_FILE), shape.doc().k())?;
        read_file(&dir.join(VK_FILE), |input| {
            VerifyingKey::<G1Affine>::read::<_, GlueCircuit>(
                input,
                SerdeFormat::RawBytes,
                shape.clone(),
            )
        })
        .and_then(|vk| {
            check_vk(&srs, &vk, shape).map(|()| Self {
                srs,
                vk,
                shape: shape.clone(),
            })
        })
    }

    pub fn verify(&self, proof: &Proof) -> Result<(), Error> {
        check_instances(&self.shape, proof).and_then(|()| verify_with(&self.srs, &self.vk, proof))
    }

    /// The Solidity verifier contract, with the verifying key in it.
    pub fn solidity(&self, shape: &Shape) -> Result<String, Error> {
        check_shape(&self.shape, shape)
            .and_then(|()| solidity_with(&self.srs, &self.vk, shape.doc().public()))
    }
}

/// The SRS and the proving key.
#[derive(Clone, Debug)]
pub struct Keys {
    srs: Srs,
    pk: ProvingKey<G1Affine>,
    shape: Shape,
}

impl Keys {
    /// Make the verifying and proving keys of a circuit.
    pub fn generate(srs: Srs, shape: &Shape) -> Result<Self, Error> {
        let circuit = GlueCircuit::keygen(shape.clone());
        check_srs(&srs, shape).and_then(|()| {
            keygen_vk(&srs.0, &circuit)
                .and_then(|vk| keygen_pk(&srs.0, vk, &circuit))
                .map(|pk| Self {
                    srs,
                    pk,
                    shape: shape.clone(),
                })
                .map_err(halo2("keygen"))
        })
    }

    /// Write `srs.bin`, `vk.bin` and `pk.bin` to a keys directory.
    pub fn save(&self, dir: &Path) -> Result<(), Error> {
        fs::create_dir_all(dir)
            .map_err(|err| Error::io(dir, &err))
            .and_then(|()| self.srs.write(&dir.join(SRS_FILE)))
            .and_then(|()| {
                write_file(&dir.join(VK_FILE), |out| {
                    self.pk.get_vk().write(out, SerdeFormat::RawBytes)
                })
            })
            .and_then(|()| {
                write_file(&dir.join(PK_FILE), |out| {
                    self.pk.write(out, SerdeFormat::RawBytes)
                })
            })
    }

    /// Read `srs.bin` and `pk.bin` from a keys directory.
    pub fn load(dir: &Path, shape: &Shape) -> Result<Self, Error> {
        let srs = Srs::read(&dir.join(SRS_FILE), shape.doc().k())?;
        read_file(&dir.join(PK_FILE), |input| {
            ProvingKey::<G1Affine>::read::<_, GlueCircuit>(
                input,
                SerdeFormat::RawBytes,
                shape.clone(),
            )
        })
        .and_then(|pk| {
            check_vk(&srs, pk.get_vk(), shape).map(|()| Self {
                srs,
                pk,
                shape: shape.clone(),
            })
        })
    }

    /// Prove a circuit with its witness. Run `mock` first: halo2 makes a
    /// proof that does not verify when the witness does not satisfy.
    pub fn prove(&self, circuit: &GlueCircuit) -> Result<Proof, Error> {
        check_shape(&self.shape, circuit.shape())?;
        let columns = circuit.instances();
        let slices = columns.iter().map(Vec::as_slice).collect::<Vec<_>>();
        let mut transcript = Keccak256Transcript::<G1Affine, Vec<u8>>::new(Vec::new());
        create_proof::<KZGCommitmentScheme<Bn256>, ProverSHPLONK<'_, Bn256>, _, _, _, _>(
            &self.srs.0,
            &self.pk,
            std::slice::from_ref(circuit),
            &[slices.as_slice()],
            OsRng,
            &mut transcript,
        )
        .map_err(halo2("prove"))
        .map(|()| Proof::new(transcript.finalize(), columns.clone()))
    }

    pub fn verify(&self, proof: &Proof) -> Result<(), Error> {
        check_instances(&self.shape, proof)
            .and_then(|()| verify_with(&self.srs, self.pk.get_vk(), proof))
    }

    pub fn solidity(&self, shape: &Shape) -> Result<String, Error> {
        check_shape(&self.shape, shape)
            .and_then(|()| solidity_with(&self.srs, self.pk.get_vk(), shape.doc().public()))
    }
}

/// Run the mock prover: `Error::Unsatisfied` when the witness fails.
pub fn mock(circuit: &GlueCircuit) -> Result<(), Error> {
    MockProver::run(circuit.shape().doc().k(), circuit, circuit.instances())
        .map_err(halo2("mock"))
        .and_then(|prover| {
            prover.verify().map_err(|failures| {
                Error::Unsatisfied(
                    failures
                        .iter()
                        .map(ToString::to_string)
                        .collect::<Vec<_>>()
                        .join("; "),
                )
            })
        })
}
