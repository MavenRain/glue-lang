//! The glue-halo2 command line. See halo2/README.md.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use comp_cat_rs::effect::io::Io;
use glue_halo2::{
    mock, CircuitDoc, Digest, Error, GlueCircuit, Keys, Proof, Shape, Srs, Verifier, WitnessDoc,
};

const USAGE: &str = "glue-halo2 check --circuit C --witness W
glue-halo2 keygen --circuit C (--srs FILE [--sha256 HEX] | --dev-srs) --keys DIR
glue-halo2 prove --circuit C --witness W --keys DIR --out DIR
glue-halo2 verify --circuit C --keys DIR --proof FILE.json
glue-halo2 solidity --circuit C --keys DIR --out FILE.sol";

/// Where keygen gets the SRS.
enum SrsSource {
    File(PathBuf),
    /// A file whose SHA-256 must be the pin.
    Pinned(PathBuf, Digest),
    /// An INSECURE SRS from a fixed seed. The user must ask for it.
    Dev,
}

enum Command {
    Check {
        circuit: PathBuf,
        witness: PathBuf,
    },
    Keygen {
        circuit: PathBuf,
        srs: SrsSource,
        keys: PathBuf,
    },
    Prove {
        circuit: PathBuf,
        witness: PathBuf,
        keys: PathBuf,
        out: PathBuf,
    },
    Verify {
        circuit: PathBuf,
        keys: PathBuf,
        proof: PathBuf,
    },
    Solidity {
        circuit: PathBuf,
        keys: PathBuf,
        out: PathBuf,
    },
}

/// The `--name value` pairs of a command. A bare `--dev-srs` has no value.
struct Flags(Vec<(String, Option<String>)>);

impl Flags {
    fn parse(rest: &[&str], allowed: &[&str]) -> Result<Self, Error> {
        pairs(rest).and_then(|pairs| {
            pairs
                .iter()
                .find(|(name, _)| !allowed.contains(&name.as_str()))
                .map_or(Ok(Self(pairs.clone())), |(name, _)| {
                    Err(Error::Usage(format!("unknown flag `{name}`")))
                })
        })
    }

    fn has(&self, name: &str) -> bool {
        self.0.iter().any(|(flag, _)| flag == name)
    }

    fn value(&self, name: &str) -> Option<&str> {
        self.0
            .iter()
            .find(|(flag, _)| flag == name)
            .and_then(|(_, value)| value.as_deref())
    }

    fn path(&self, name: &str) -> Option<PathBuf> {
        self.value(name).map(PathBuf::from)
    }

    fn need(&self, name: &str) -> Result<PathBuf, Error> {
        self.path(name)
            .ok_or_else(|| Error::Usage(format!("missing `{name} PATH`")))
    }

    fn need_hex(&self, name: &str) -> Result<Digest, Error> {
        self.value(name)
            .ok_or_else(|| Error::Usage(format!("missing `{name} HEX`")))
            .and_then(Digest::parse)
    }
}

fn pairs(rest: &[&str]) -> Result<Vec<(String, Option<String>)>, Error> {
    let prepend = |head: (String, Option<String>), tail: Vec<(String, Option<String>)>| {
        std::iter::once(head).chain(tail).collect::<Vec<_>>()
    };
    match rest {
        [] => Ok(Vec::new()),
        ["--dev-srs", tail @ ..] => {
            pairs(tail).map(|tail| prepend(("--dev-srs".to_string(), None), tail))
        }
        [name, value, tail @ ..] if name.starts_with("--") && !value.starts_with("--") => {
            pairs(tail).map(|tail| prepend((name.to_string(), Some(value.to_string())), tail))
        }
        [other, ..] => Err(Error::Usage(format!("unexpected `{other}`"))),
    }
}

fn command(args: &[&str]) -> Result<Command, Error> {
    match args {
        ["check", rest @ ..] => Flags::parse(rest, &["--circuit", "--witness"]).and_then(|f| {
            f.need("--circuit").and_then(|circuit| {
                f.need("--witness")
                    .map(|witness| Command::Check { circuit, witness })
            })
        }),
        ["keygen", rest @ ..] => Flags::parse(
            rest,
            &["--circuit", "--srs", "--dev-srs", "--keys", "--sha256"],
        )
        .and_then(|f| {
            let srs = match () {
                () if f.has("--sha256") && !f.has("--srs") => {
                    Err(Error::Usage("--sha256 HEX needs --srs FILE".to_string()))
                }
                () if f.has("--srs") == f.has("--dev-srs") => Err(Error::Usage(
                    "give one of `--srs FILE` and `--dev-srs`".to_string(),
                )),
                () if f.has("--dev-srs") => Ok(SrsSource::Dev),
                () if f.has("--sha256") => f.need("--srs").and_then(|path| {
                    f.need_hex("--sha256")
                        .map(|digest| SrsSource::Pinned(path, digest))
                }),
                () => f.need("--srs").map(SrsSource::File),
            };
            srs.and_then(|srs| {
                f.need("--circuit").and_then(|circuit| {
                    f.need("--keys")
                        .map(|keys| Command::Keygen { circuit, srs, keys })
                })
            })
        }),
        ["prove", rest @ ..] => Flags::parse(rest, &["--circuit", "--witness", "--keys", "--out"])
            .and_then(|f| {
                f.need("--circuit").and_then(|circuit| {
                    f.need("--witness").and_then(|witness| {
                        f.need("--keys").and_then(|keys| {
                            f.need("--out").map(|out| Command::Prove {
                                circuit,
                                witness,
                                keys,
                                out,
                            })
                        })
                    })
                })
            }),
        ["verify", rest @ ..] => {
            Flags::parse(rest, &["--circuit", "--keys", "--proof"]).and_then(|f| {
                f.need("--circuit").and_then(|circuit| {
                    f.need("--keys").and_then(|keys| {
                        f.need("--proof").map(|proof| Command::Verify {
                            circuit,
                            keys,
                            proof,
                        })
                    })
                })
            })
        }
        ["solidity", rest @ ..] => {
            Flags::parse(rest, &["--circuit", "--keys", "--out"]).and_then(|f| {
                f.need("--circuit").and_then(|circuit| {
                    f.need("--keys").and_then(|keys| {
                        f.need("--out")
                            .map(|out| Command::Solidity { circuit, keys, out })
                    })
                })
            })
        }
        [other, ..] => Err(Error::Usage(format!("unknown command `{other}`"))),
        [] => Err(Error::Usage("no command".to_string())),
    }
}

fn read_text(path: &Path) -> Result<String, Error> {
    fs::read_to_string(path).map_err(|err| Error::io(path, &err))
}

fn write_text(path: &Path, text: &str) -> Result<(), Error> {
    fs::write(path, text).map_err(|err| Error::io(path, &err))
}

fn shape_of(path: &Path) -> Result<Shape, Error> {
    read_text(path)
        .and_then(|text| CircuitDoc::parse(&text))
        .map(Shape::from)
}

/// The circuit with its witness, checked by the mock prover.
fn satisfied(circuit: &Path, witness: &Path) -> Result<GlueCircuit, Error> {
    shape_of(circuit).and_then(|shape| {
        read_text(witness)
            .and_then(|text| WitnessDoc::parse(&text, shape.doc()))
            .map(|witness| GlueCircuit::proving(shape, witness))
            .and_then(|circuit| mock(&circuit).map(|()| circuit))
    })
}

fn execute(command: Command) -> Result<String, Error> {
    match command {
        Command::Check { circuit, witness } => satisfied(&circuit, &witness).map(|c| {
            let doc = c.shape().doc();
            format!(
                "ok: `{}` is satisfied (k {}, rows {}, usable {})",
                doc.name(),
                doc.k(),
                doc.rows(),
                doc.usable_rows()
            )
        }),
        Command::Keygen { circuit, srs, keys } => shape_of(&circuit).and_then(|shape| {
            let k = shape.doc().k();
            let source = match srs {
                SrsSource::File(path) => Srs::read(&path, k),
                SrsSource::Pinned(path, digest) => Srs::read_pinned(&path, k, &digest),
                SrsSource::Dev => Ok(Srs::dev(k)),
            };
            source
                .and_then(|srs| Keys::generate(srs, &shape))
                .and_then(|made| made.save(&keys))
                .map(|()| format!("keys for `{}` in {}", shape.doc().name(), keys.display()))
        }),
        Command::Prove {
            circuit,
            witness,
            keys,
            out,
        } => satisfied(&circuit, &witness).and_then(|c| {
            Keys::load(&keys, c.shape())
                .and_then(|loaded| loaded.prove(&c))
                .and_then(|proof| {
                    fs::create_dir_all(&out)
                        .map_err(|err| Error::io(&out, &err))
                        .and_then(|()| {
                            fs::write(out.join("proof.bin"), proof.bytes())
                                .map_err(|err| Error::io(&out.join("proof.bin"), &err))
                        })
                        .and_then(|()| write_text(&out.join("proof.json"), &proof.to_json()))
                })
                .map(|()| format!("proof in {}", out.display()))
        }),
        Command::Verify {
            circuit,
            keys,
            proof,
        } => shape_of(&circuit).and_then(|shape| {
            Verifier::load(&keys, &shape).and_then(|verifier| {
                read_text(&proof)
                    .and_then(|text| Proof::from_json(&text))
                    .and_then(|proof| verifier.verify(&proof))
                    .map(|()| format!("ok: the proof of `{}` verifies", shape.doc().name()))
            })
        }),
        Command::Solidity { circuit, keys, out } => shape_of(&circuit).and_then(|shape| {
            Verifier::load(&keys, &shape)
                .and_then(|verifier| verifier.solidity(&shape))
                .and_then(|code| write_text(&out, &code))
                .map(|()| format!("verifier in {}", out.display()))
        }),
    }
}

fn main() -> ExitCode {
    let args = std::env::args().skip(1).collect::<Vec<_>>();
    let program = Io::suspend(move || {
        let words = args.iter().map(String::as_str).collect::<Vec<_>>();
        command(&words).and_then(execute)
    });
    program.run().map_or_else(
        |err| {
            eprintln!("glue-halo2: {err}");
            let usage = matches!(err, Error::Usage(_));
            if usage {
                eprintln!("{USAGE}");
            }
            ExitCode::FAILURE
        },
        |message| {
            println!("{message}");
            ExitCode::SUCCESS
        },
    )
}
