//! Mock prover, real round trips, key files and IR refusals.

use glue_halo2::error::KeyProblem;
use glue_halo2::{
    mock, CircuitDoc, Digest, Error, GlueCircuit, Keys, Proof, Shape, Srs, Verifier, WitnessDoc,
};
use halo2_proofs::halo2curves::bn256::Fr;
use serde_json::{json, Value};

const SQUARE_C: &str = include_str!("fixtures/square.circuit.json");
const SQUARE_W: &str = include_str!("fixtures/square.witness.json");
const RANGE8_C: &str = include_str!("fixtures/range8.circuit.json");
const RANGE8_W: &str = include_str!("fixtures/range8.witness.json");
const GOLDEN_SQUARE_C: &str = include_str!("../../test/circuit/square.circuit.json");
const GOLDEN_SQUARE_W: &str = include_str!("../../test/circuit/square.witness.json");
const GOLDEN_RANGE_C: &str = include_str!("../../test/circuit/range.circuit.json");
const GOLDEN_RANGE_W: &str = include_str!("../../test/circuit/range.witness.json");
const GOLDEN_POLY_C: &str = include_str!("../../test/circuit/poly.circuit.json");
const GOLDEN_POLY_W: &str = include_str!("../../test/circuit/poly.witness.json");

const ALL: [(&str, &str, &str); 5] = [
    ("square", SQUARE_C, SQUARE_W),
    ("range8", RANGE8_C, RANGE8_W),
    ("golden square", GOLDEN_SQUARE_C, GOLDEN_SQUARE_W),
    ("golden range", GOLDEN_RANGE_C, GOLDEN_RANGE_W),
    ("golden poly", GOLDEN_POLY_C, GOLDEN_POLY_W),
];

const R: &str = "21888242871839275222246405745257275088548364400416034343698204186575808495617";

/// A malformed document and the test that its refusal is the right kind.
type Refusal = (String, fn(&Error) -> bool);

fn load(circuit: &str, witness: &str) -> Result<GlueCircuit, Error> {
    CircuitDoc::parse(circuit)
        .map(Shape::from)
        .and_then(|shape| {
            WitnessDoc::parse(witness, shape.doc()).map(|w| GlueCircuit::proving(shape, w))
        })
}

fn keys_for(circuit: &GlueCircuit) -> Result<Keys, Error> {
    Keys::generate(Srs::dev(circuit.shape().doc().k()), circuit.shape())
}

fn test_error(why: &str) -> Error {
    Error::Usage(format!("test: {why}"))
}

/// Replace the value at a JSON pointer.
fn edited(text: &str, pointer: &str, value: Value) -> Result<String, Error> {
    let mut doc = serde_json::from_str::<Value>(text).map_err(|e| test_error(&e.to_string()))?;
    doc.pointer_mut(pointer)
        .map(|slot| *slot = value)
        .ok_or_else(|| test_error(pointer))
        .map(|()| doc.to_string())
}

/// Add a key at the top level.
fn with_key(text: &str, key: &str) -> Result<String, Error> {
    let mut doc = serde_json::from_str::<Value>(text).map_err(|e| test_error(&e.to_string()))?;
    doc.as_object_mut()
        .map(|map| map.insert(key.to_string(), json!(1)))
        .ok_or_else(|| test_error("not an object"))
        .map(|_| doc.to_string())
}

#[test]
fn mock_is_satisfied_on_every_fixture() -> Result<(), Error> {
    ALL.iter()
        .try_for_each(|(_, c, w)| load(c, w).and_then(|circuit| mock(&circuit)))
}

#[test]
fn mock_refuses_tampered_witnesses() -> Result<(), Error> {
    let cases = [
        (SQUARE_C, edited(SQUARE_W, "/advice/2/0", json!("10"))?),
        (SQUARE_C, edited(SQUARE_W, "/instance/0/0", json!("10"))?),
        (SQUARE_C, edited(SQUARE_W, "/advice/1/0", json!("4"))?),
        (RANGE8_C, edited(RANGE8_W, "/advice/0/0", json!("256"))?),
    ];
    cases.iter().try_for_each(|(c, w)| {
        load(c, w).map(|circuit| {
            let result = mock(&circuit);
            assert!(matches!(result, Err(Error::Unsatisfied(_))), "{result:?}");
        })
    })
}

#[test]
fn dev_srs_round_trip_on_every_fixture() -> Result<(), Error> {
    ALL.iter().try_for_each(|(_, c, w)| {
        load(c, w).and_then(|circuit| {
            keys_for(&circuit)
                .and_then(|keys| keys.prove(&circuit).and_then(|proof| keys.verify(&proof)))
        })
    })
}

#[test]
fn mutant_witness_proof_does_not_verify() -> Result<(), Error> {
    let bad = edited(SQUARE_W, "/advice/2/0", json!("10"))
        .and_then(|w| edited(&w, "/instance/0/0", json!("10")))?;
    let circuit = load(SQUARE_C, &bad)?;
    let keys = keys_for(&circuit)?;
    let result = keys.prove(&circuit).and_then(|proof| keys.verify(&proof));
    assert!(result.is_err(), "a mutant witness verified");
    Ok(())
}

#[test]
fn tampered_proof_or_instance_does_not_verify() -> Result<(), Error> {
    let circuit = load(SQUARE_C, SQUARE_W)?;
    let keys = keys_for(&circuit)?;
    let proof = keys.prove(&circuit)?;
    let instance = Proof::new(proof.bytes().to_vec(), vec![vec![Fr::from(10u64)]]);
    let flipped = proof
        .bytes()
        .iter()
        .enumerate()
        .map(|(i, b)| if i == 40 { b ^ 1 } else { *b })
        .collect::<Vec<_>>();
    let bytes = Proof::new(flipped, proof.columns().to_vec());
    assert!(
        keys.verify(&instance).is_err(),
        "a tampered instance verified"
    );
    assert!(keys.verify(&bytes).is_err(), "a tampered proof verified");
    keys.verify(&proof)
}

#[test]
fn keys_and_proof_json_survive_the_disk() -> Result<(), Error> {
    let circuit = load(RANGE8_C, RANGE8_W)?;
    let dir = std::env::temp_dir().join("glue-halo2-test-keys");
    keys_for(&circuit)?.save(&dir)?;
    let proof = Keys::load(&dir, circuit.shape())?.prove(&circuit)?;
    let read = Proof::from_json(&proof.to_json())?;
    assert!(read == proof, "proof JSON round trip changed the proof");
    Verifier::load(&dir, circuit.shape())?.verify(&read)
}

#[test]
fn saved_keys_refuse_changed_fixed_values_and_copies() -> Result<(), Error> {
    let circuit = load(GOLDEN_SQUARE_C, GOLDEN_SQUARE_W)?;
    let dir = std::env::temp_dir().join(format!("glue-halo2-bound-keys-{}", std::process::id()));
    keys_for(&circuit)?.save(&dir)?;
    let cases = [
        edited(GOLDEN_SQUARE_C, "/fixed_values/3/2", json!("2"))?,
        edited(GOLDEN_SQUARE_C, "/copies/0/0/row", json!(1))?,
    ];
    cases.iter().try_for_each(|text| {
        let shape = Shape::from(CircuitDoc::parse(text)?);
        assert!(
            Verifier::load(&dir, &shape).is_err(),
            "stale verifying key accepted"
        );
        assert!(
            Keys::load(&dir, &shape).is_err(),
            "stale proving key accepted"
        );
        Ok(())
    })
}

#[test]
fn generated_keys_refuse_another_circuit_shape() -> Result<(), Error> {
    let circuit = load(GOLDEN_SQUARE_C, GOLDEN_SQUARE_W)?;
    let keys = keys_for(&circuit)?;
    let changed = edited(GOLDEN_SQUARE_C, "/fixed_values/3/2", json!("2"))?;
    let other = load(&changed, GOLDEN_SQUARE_W)?;
    assert!(
        keys.prove(&other).is_err(),
        "proof returned for a different circuit"
    );
    assert!(
        keys.solidity(other.shape()).is_err(),
        "verifier returned for a different circuit"
    );
    Ok(())
}

#[test]
fn verification_requires_the_declared_public_input_count() -> Result<(), Error> {
    let circuit = load(GOLDEN_SQUARE_C, GOLDEN_SQUARE_W)?;
    let keys = keys_for(&circuit)?;
    let proof = keys.prove(&circuit)?;
    let dir = std::env::temp_dir().join(format!("glue-halo2-public-keys-{}", std::process::id()));
    keys.save(&dir)?;
    let changed = edited(GOLDEN_SQUARE_C, "/public", json!(2))?;
    let shape = Shape::from(CircuitDoc::parse(&changed)?);
    // The VK's commitments are unchanged, but the document needs two inputs.
    let result = Verifier::load(&dir, &shape)?.verify(&proof);
    assert!(matches!(result, Err(Error::Shape { .. })), "{result:?}");
    Ok(())
}

#[test]
fn proof_json_with_a_stale_derived_key_is_refused() -> Result<(), Error> {
    let circuit = load(SQUARE_C, SQUARE_W)?;
    let json = keys_for(&circuit)?.prove(&circuit)?.to_json();
    let ten = json!(format!("0x{:064x}", 10));
    let cases = [
        ("instances", edited(&json, "/instances/0", ten)?),
        ("calldata", edited(&json, "/calldata", json!("0x00"))?),
    ];
    cases.iter().try_for_each(|(key, text)| {
        Proof::from_json(text).map_or_else(
            |err| {
                assert!(
                    matches!(&err, Error::Shape { at, .. } if at == key),
                    "{err}"
                );
                Ok(())
            },
            |_| Err(test_error(&format!("a stale `{key}` was read"))),
        )
    })
}

#[test]
fn solidity_verifier_renders_for_square() -> Result<(), Error> {
    let circuit = load(SQUARE_C, SQUARE_W)?;
    let code = keys_for(&circuit)?.solidity(circuit.shape())?;
    assert!(
        code.contains("function verifyProof"),
        "no verifyProof in the contract"
    );
    let proof = keys_for(&circuit)?.prove(&circuit)?;
    assert!(
        proof.calldata().len() > proof.bytes().len(),
        "calldata is too short"
    );
    Ok(())
}

#[test]
fn srs_smaller_than_k_is_refused() -> Result<(), Error> {
    let dir = std::env::temp_dir().join("glue-halo2-test-srs");
    std::fs::create_dir_all(&dir).map_err(|e| Error::io(&dir, &e))?;
    let path = dir.join("srs4.bin");
    Srs::dev(4).write(&path)?;
    assert!(matches!(Srs::read(&path, 5), Err(Error::Shape { .. })));
    Srs::read(&path, 3).map(|srs| assert!(srs.k() == 3))
}

/// The pin check comes first: a wrong pin or a changed file is `Error::Pin`,
/// and with the right pin the `k` check still applies.
#[test]
fn pinned_srs_refuses_a_wrong_pin_and_a_changed_file() -> Result<(), Error> {
    let dir = std::env::temp_dir().join("glue-halo2-test-pin");
    std::fs::create_dir_all(&dir).map_err(|e| Error::io(&dir, &e))?;
    let path = dir.join("srs4.bin");
    Srs::dev(4).write(&path)?;
    let bytes = std::fs::read(&path).map_err(|e| Error::io(&path, &e))?;
    let pin = Digest::of(&bytes);
    assert!(Srs::read_pinned(&path, 3, &pin)?.k() == 3);
    assert!(matches!(
        Srs::read_pinned(&path, 5, &pin),
        Err(Error::Shape { .. })
    ));
    let hex = pin.to_string();
    let flip = |c: char| if c == '0' { '1' } else { '0' };
    let flipped = hex
        .chars()
        .take(1)
        .map(flip)
        .chain(hex.chars().skip(1))
        .collect::<String>();
    let wrong = Digest::parse(&flipped)?;
    assert!(matches!(
        Srs::read_pinned(&path, 3, &wrong),
        Err(Error::Pin { expected, actual, .. }) if expected == wrong && actual == pin
    ));
    let plus = dir.join("srs4-plus.bin");
    std::fs::write(&plus, [bytes.as_slice(), &[0u8]].concat()).map_err(|e| Error::io(&plus, &e))?;
    assert!(matches!(
        Srs::read_pinned(&plus, 3, &pin),
        Err(Error::Pin { expected, .. }) if expected == pin
    ));
    assert!(matches!(Digest::parse("abc"), Err(Error::Usage(_))));
    assert!(matches!(
        Digest::parse(&"g".repeat(64)),
        Err(Error::Usage(_))
    ));
    assert!(Digest::parse(&hex)? == pin);
    Ok(())
}

fn circuit_refusal(text: &str) -> Result<Error, Error> {
    CircuitDoc::parse(text).map_or_else(Ok, |_| Err(test_error("the document was accepted")))
}

fn witness_refusal(text: &str) -> Result<Error, Error> {
    CircuitDoc::parse(SQUARE_C).and_then(|doc| {
        WitnessDoc::parse(text, &doc)
            .map_or_else(Ok, |_| Err(test_error("the witness was accepted")))
    })
}

#[test]
fn malformed_ir_is_refused() -> Result<(), Error> {
    let values = [json!("01"), json!("0x1"), json!(1), json!(R), json!("-1")];
    values.into_iter().try_for_each(|value| {
        circuit_refusal(&edited(SQUARE_C, "/fixed_values/2/0", value)?).map(|err| {
            assert!(matches!(err, Error::Value { .. }), "{err}");
        })
    })?;
    let shape = |err: &Error| matches!(err, Error::Shape { .. });
    let cases: [Refusal; 6] = [
        (edited(SQUARE_C, "/rows", json!(32))?, |e| {
            matches!(e, Error::TooManyRows { .. })
        }),
        (with_key(SQUARE_C, "extra")?, |e| {
            matches!(
                e,
                Error::Key {
                    problem: KeyProblem::Unknown,
                    ..
                }
            )
        }),
        (edited(SQUARE_C, "/glue-lang-plonkish", json!(2))?, |e| {
            matches!(e, Error::Version { .. })
        }),
        (edited(SQUARE_C, "/field", json!("bls12-381"))?, |e| {
            matches!(e, Error::Version { .. })
        }),
        (
            edited(SQUARE_C, "/gates/0/polys/0", json!({"a": [3, 0]}))?,
            |e| matches!(e, Error::Shape { .. }),
        ),
        (edited(SQUARE_C, "/gates/0/polys", json!([]))?, |e| {
            matches!(e, Error::Shape { .. })
        }),
    ];
    cases.iter().try_for_each(|(text, want)| {
        circuit_refusal(text).map(|err| assert!(want(&err), "{err}"))
    })?;
    let mismatch = witness_refusal(&edited(SQUARE_W, "/name", json!("other"))?)?;
    assert!(matches!(mismatch, Error::NameMismatch { .. }), "{mismatch}");
    let length = witness_refusal(&edited(SQUARE_W, "/advice/0", json!(["3", "4"]))?)?;
    assert!(shape(&length), "{length}");
    Ok(())
}
