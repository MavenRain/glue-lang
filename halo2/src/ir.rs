//! Plonkish IR v1 reader (docs/IR.md). A document is parsed, then checked
//! against the contract. Every refusal is an `Error`, never a panic.

use std::collections::BTreeSet;

use halo2_proofs::halo2curves::bn256::Fr;
use serde_json::{Map, Value};

use crate::error::{Doc, Error, KeyProblem};
use crate::field::parse_decimal;

/// The largest `k`: the two-adicity of BN254 Fr.
const MAX_K: u64 = 28;

/// The kind of a column.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum ColumnKind {
    Advice,
    Fixed,
    Instance,
}

/// A column index inside one kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct ColumnIndex(usize);

impl ColumnIndex {
    pub fn get(self) -> usize {
        self.0
    }
}

/// A row rotation of a query.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Rot(i32);

impl Rot {
    pub fn get(self) -> i32 {
        self.0
    }
}

/// The column counts of a circuit.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Columns {
    advice: usize,
    fixed: usize,
    instance: usize,
}

impl Columns {
    pub fn count(self, kind: ColumnKind) -> usize {
        match kind {
            ColumnKind::Advice => self.advice,
            ColumnKind::Fixed => self.fixed,
            ColumnKind::Instance => self.instance,
        }
    }
}

/// An IR expression.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Expr {
    Const(Fr),
    Query(ColumnKind, ColumnIndex, Rot),
    Sum(Box<Expr>, Box<Expr>),
    Product(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>),
}

impl Expr {
    /// Whether the expression queries only fixed columns, and which ones.
    fn fixed_only(&self) -> (bool, BTreeSet<usize>) {
        match self {
            Self::Const(_) => (true, BTreeSet::new()),
            Self::Query(kind, index, _) => match kind {
                ColumnKind::Fixed => (true, BTreeSet::from([index.get()])),
                ColumnKind::Advice | ColumnKind::Instance => (false, BTreeSet::new()),
            },
            Self::Sum(left, right) | Self::Product(left, right) => {
                let (lo, ls) = left.fixed_only();
                let (ro, rs) = right.fixed_only();
                (lo && ro, ls.union(&rs).copied().collect())
            }
            Self::Neg(inner) => inner.fixed_only(),
        }
    }
}

/// A gate: polys that are zero on every row.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Gate {
    name: String,
    polys: Vec<Expr>,
}

impl Gate {
    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn polys(&self) -> &[Expr] {
        &self.polys
    }
}

/// A lookup: (input, table) pairs.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Lookup {
    name: String,
    pairs: Vec<(Expr, Expr)>,
}

impl Lookup {
    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn pairs(&self) -> &[(Expr, Expr)] {
        &self.pairs
    }
}

/// One cell of a copy.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CellRef {
    kind: ColumnKind,
    index: ColumnIndex,
    row: usize,
}

impl CellRef {
    pub fn kind(self) -> ColumnKind {
        self.kind
    }
    pub fn index(self) -> ColumnIndex {
        self.index
    }
    pub fn row(self) -> usize {
        self.row
    }
}

/// A checked circuit document.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct CircuitDoc {
    name: String,
    k: u32,
    rows: usize,
    usable: usize,
    columns: Columns,
    gates: Vec<Gate>,
    lookups: Vec<Lookup>,
    copies: Vec<(CellRef, CellRef)>,
    fixed_values: Vec<Vec<Fr>>,
    public: usize,
}

const CIRCUIT_KEYS: [&str; 13] = [
    "glue-lang-plonkish",
    "name",
    "field",
    "k",
    "rows",
    "advice",
    "fixed",
    "instance",
    "gates",
    "lookups",
    "copies",
    "fixed_values",
    "public",
];

impl CircuitDoc {
    /// Parse and check a circuit document.
    pub fn parse(text: &str) -> Result<Self, Error> {
        let value = serde_json::from_str::<Value>(text).map_err(|e| Error::Json {
            doc: Doc::Circuit,
            reason: e.to_string(),
        })?;
        let map = object(&value, "circuit")?;
        exact_keys(map, &CIRCUIT_KEYS, Doc::Circuit)?;
        version(map, "glue-lang-plonkish", Doc::Circuit)?;
        get(map, "field", Doc::Circuit).and_then(|v| {
            v.as_str()
                .filter(|s| *s == "bn254-fr")
                .map(|_| ())
                .ok_or(Error::Version {
                    doc: Doc::Circuit,
                    key: "field".to_string(),
                })
        })?;
        let k = get(map, "k", Doc::Circuit)
            .and_then(|v| {
                v.as_u64()
                    .filter(|k| (1..=MAX_K).contains(k))
                    .ok_or_else(|| Error::shape("k", "not an integer in 1..=28"))
            })
            .and_then(|k| u32::try_from(k).map_err(|_| Error::shape("k", "too large")))?;
        let columns = Columns {
            advice: count(map, "advice")?,
            fixed: count(map, "fixed")?,
            instance: count(map, "instance")?,
        };
        let parsed = Self {
            name: get(map, "name", Doc::Circuit)?
                .as_str()
                .filter(|s| !s.is_empty())
                .ok_or_else(|| Error::shape("name", "not a non-empty string"))?
                .to_string(),
            k,
            rows: count(map, "rows")?,
            usable: 0,
            columns,
            gates: listed(map, "gates", |v, at| gate(v, at, columns))?,
            lookups: listed(map, "lookups", |v, at| lookup(v, at, columns))?,
            copies: listed(map, "copies", |v, at| copy(v, at, columns))?,
            fixed_values: listed(map, "fixed_values", values)?,
            public: count(map, "public")?,
        };
        parsed.with_usable_rows().and_then(Self::checked)
    }

    /// Set `usable` from the blinding factors of the configured circuit.
    fn with_usable_rows(self) -> Result<Self, Error> {
        let blinding = crate::circuit::blinding_factors(&self);
        1usize
            .checked_shl(self.k)
            .and_then(|n| n.checked_sub(blinding + 1))
            .map(|usable| Self { usable, ..self })
            .ok_or_else(|| Error::shape("k", "no usable rows"))
    }

    /// The checks that need the usable rows.
    fn checked(self) -> Result<Self, Error> {
        let table_columns = self
            .lookups
            .iter()
            .flat_map(|l| l.pairs.iter().map(|(_, table)| table.fixed_only().1))
            .fold(BTreeSet::new(), |acc, set| {
                acc.union(&set).copied().collect()
            });
        let rows_ok = if self.rows > self.usable {
            Err(Error::TooManyRows {
                rows: self.rows,
                usable: self.usable,
            })
        } else {
            Ok(())
        };
        rows_ok
            .and_then(|()| self.check_public())
            .and_then(|()| self.check_fixed(&table_columns))
            .and_then(|()| self.check_copies())
            .map(|()| self)
    }

    fn check_public(&self) -> Result<(), Error> {
        match () {
            () if self.public > 0 && self.columns.instance == 0 => Err(Error::shape(
                "public",
                "public inputs need an instance column",
            )),
            () if self.public > self.usable => {
                Err(Error::shape("public", "more than the usable rows"))
            }
            () => Ok(()),
        }
    }

    fn check_fixed(&self, table_columns: &BTreeSet<usize>) -> Result<(), Error> {
        let count_ok = if self.fixed_values.len() == self.columns.fixed {
            Ok(())
        } else {
            Err(Error::shape(
                "fixed_values",
                "one array for each fixed column",
            ))
        };
        count_ok.and_then(|()| {
            self.fixed_values
                .iter()
                .enumerate()
                .try_for_each(|(i, column)| {
                    let len = column.len();
                    let table = table_columns.contains(&i);
                    match () {
                        () if len == self.rows => Ok(()),
                        () if table && len > self.rows && len <= self.usable => Ok(()),
                        () => Err(Error::shape(
                            format!("fixed_values[{i}]"),
                            format!(
                                "length {len}: want rows {} (a table column up to {})",
                                self.rows, self.usable
                            ),
                        )),
                    }
                })
        })
    }

    fn check_copies(&self) -> Result<(), Error> {
        self.copies
            .iter()
            .enumerate()
            .try_for_each(|(i, (left, right))| {
                [left, right].iter().try_for_each(|cell| {
                    let limit = match cell.kind {
                        ColumnKind::Advice | ColumnKind::Fixed => self.rows,
                        ColumnKind::Instance => self.usable,
                    };
                    if cell.row < limit {
                        Ok(())
                    } else {
                        Err(Error::shape(
                            format!("copies[{i}]"),
                            format!("row {} is not below {limit}", cell.row),
                        ))
                    }
                })
            })
    }

    pub fn name(&self) -> &str {
        &self.name
    }
    pub fn k(&self) -> u32 {
        self.k
    }
    pub fn rows(&self) -> usize {
        self.rows
    }
    pub fn usable_rows(&self) -> usize {
        self.usable
    }
    pub fn columns(&self) -> Columns {
        self.columns
    }
    pub fn gates(&self) -> &[Gate] {
        &self.gates
    }
    pub fn lookups(&self) -> &[Lookup] {
        &self.lookups
    }
    pub fn copies(&self) -> &[(CellRef, CellRef)] {
        &self.copies
    }
    pub fn fixed_values(&self) -> &[Vec<Fr>] {
        &self.fixed_values
    }
    pub fn public(&self) -> usize {
        self.public
    }
}

/// A checked witness document.
#[derive(Clone, Debug, Default)]
pub struct WitnessDoc {
    advice: Vec<Vec<Fr>>,
    instance: Vec<Vec<Fr>>,
}

impl WitnessDoc {
    /// Parse a witness document and check it against its circuit.
    pub fn parse(text: &str, circuit: &CircuitDoc) -> Result<Self, Error> {
        let value = serde_json::from_str::<Value>(text).map_err(|e| Error::Json {
            doc: Doc::Witness,
            reason: e.to_string(),
        })?;
        let map = object(&value, "witness")?;
        exact_keys(
            map,
            &["glue-lang-witness", "name", "advice", "instance"],
            Doc::Witness,
        )?;
        version(map, "glue-lang-witness", Doc::Witness)?;
        let name = get(map, "name", Doc::Witness)?
            .as_str()
            .ok_or_else(|| Error::shape("name", "not a string"))?;
        let named = if name == circuit.name() {
            Ok(())
        } else {
            Err(Error::NameMismatch {
                circuit: circuit.name().to_string(),
                witness: name.to_string(),
            })
        };
        named?;
        let advice = columns_of(
            get(map, "advice", Doc::Witness)?,
            "advice",
            circuit.columns().advice,
        )?;
        let instance = columns_of(
            get(map, "instance", Doc::Witness)?,
            "instance",
            circuit.columns().instance,
        )?;
        advice.iter().enumerate().try_for_each(|(i, column)| {
            if column.len() == circuit.rows() {
                Ok(())
            } else {
                Err(Error::shape(
                    format!("advice[{i}]"),
                    format!("length {}, want rows {}", column.len(), circuit.rows()),
                ))
            }
        })?;
        instance
            .iter()
            .enumerate()
            .try_for_each(|(i, column)| {
                let ok = if i == 0 {
                    column.len() == circuit.public()
                } else {
                    column.len() <= circuit.usable_rows()
                };
                if ok {
                    Ok(())
                } else {
                    Err(Error::shape(
                        format!("instance[{i}]"),
                        format!("length {}", column.len()),
                    ))
                }
            })
            .map(|()| Self { advice, instance })
    }

    pub fn advice(&self) -> &[Vec<Fr>] {
        &self.advice
    }
    pub fn instance(&self) -> &[Vec<Fr>] {
        &self.instance
    }
    /// Instance column 0: the public inputs, in row order.
    pub fn public(&self) -> &[Fr] {
        self.instance.first().map_or(&[], Vec::as_slice)
    }
}

fn object<'a>(value: &'a Value, at: &str) -> Result<&'a Map<String, Value>, Error> {
    value
        .as_object()
        .ok_or_else(|| Error::shape(at, "not an object"))
}

fn array<'a>(value: &'a Value, at: &str) -> Result<&'a Vec<Value>, Error> {
    value
        .as_array()
        .ok_or_else(|| Error::shape(at, "not an array"))
}

/// Parse each item of the array under `key`.
fn listed<T>(
    map: &Map<String, Value>,
    key: &str,
    item: impl Fn(&Value, &str) -> Result<T, Error>,
) -> Result<Vec<T>, Error> {
    get(map, key, Doc::Circuit)
        .and_then(|v| array(v, key))
        .and_then(|items| {
            items
                .iter()
                .enumerate()
                .map(|(i, v)| item(v, &format!("{key}[{i}]")))
                .collect()
        })
}

fn exact_keys(map: &Map<String, Value>, keys: &[&str], doc: Doc) -> Result<(), Error> {
    let unknown = map
        .keys()
        .find(|k| !keys.contains(&k.as_str()))
        .map(|k| Error::Key {
            doc,
            key: k.clone(),
            problem: KeyProblem::Unknown,
        });
    let missing = keys
        .iter()
        .find(|k| !map.contains_key(**k))
        .map(|k| Error::Key {
            doc,
            key: (*k).to_string(),
            problem: KeyProblem::Missing,
        });
    unknown.or(missing).map_or(Ok(()), Err)
}

fn get<'a>(map: &'a Map<String, Value>, key: &str, doc: Doc) -> Result<&'a Value, Error> {
    map.get(key).ok_or_else(|| Error::Key {
        doc,
        key: key.to_string(),
        problem: KeyProblem::Missing,
    })
}

fn version(map: &Map<String, Value>, key: &str, doc: Doc) -> Result<(), Error> {
    get(map, key, doc).and_then(|v| {
        v.as_u64()
            .filter(|n| *n == 1)
            .map(|_| ())
            .ok_or(Error::Version {
                doc,
                key: key.to_string(),
            })
    })
}

fn count(map: &Map<String, Value>, key: &str) -> Result<usize, Error> {
    get(map, key, Doc::Circuit).and_then(|v| {
        v.as_u64()
            .and_then(|n| usize::try_from(n).ok())
            .ok_or_else(|| Error::shape(key, "not a non-negative integer"))
    })
}

fn field_value(value: &Value, at: &str) -> Result<Fr, Error> {
    value
        .as_str()
        .ok_or(Error::Value {
            at: at.to_string(),
            reason: "not a JSON string",
        })
        .and_then(|text| {
            parse_decimal(text).map_err(|reason| Error::Value {
                at: at.to_string(),
                reason,
            })
        })
}

fn values(value: &Value, at: &str) -> Result<Vec<Fr>, Error> {
    array(value, at).and_then(|items| {
        items
            .iter()
            .enumerate()
            .map(|(i, v)| field_value(v, &format!("{at}[{i}]")))
            .collect()
    })
}

fn columns_of(value: &Value, at: &str, want: usize) -> Result<Vec<Vec<Fr>>, Error> {
    array(value, at).and_then(|found| {
        if found.len() == want {
            found
                .iter()
                .enumerate()
                .map(|(i, v)| values(v, &format!("{at}[{i}]")))
                .collect()
        } else {
            Err(Error::shape(
                at,
                format!("{} columns, want {want}", found.len()),
            ))
        }
    })
}

fn two(items: &[Value]) -> Option<(&Value, &Value)> {
    items.first().zip(items.get(1)).filter(|_| items.len() == 2)
}

fn name_of(map: &Map<String, Value>, at: &str) -> Result<String, Error> {
    get(map, "name", Doc::Circuit).and_then(|v| {
        v.as_str()
            .map(str::to_string)
            .ok_or_else(|| Error::shape(at, "name is not a string"))
    })
}

fn gate(value: &Value, at: &str, columns: Columns) -> Result<Gate, Error> {
    let map = object(value, at)?;
    exact_keys(map, &["name", "polys"], Doc::Circuit)?;
    let polys = get(map, "polys", Doc::Circuit)
        .and_then(|v| array(v, at))
        .and_then(|items| {
            items
                .iter()
                .enumerate()
                .map(|(i, v)| expr(v, &format!("{at}.polys[{i}]"), columns))
                .collect::<Result<Vec<_>, _>>()
        })?;
    if polys.is_empty() {
        Err(Error::shape(at, "a gate needs at least one poly"))
    } else {
        name_of(map, at).map(|name| Gate { name, polys })
    }
}

fn lookup(value: &Value, at: &str, columns: Columns) -> Result<Lookup, Error> {
    let map = object(value, at)?;
    exact_keys(map, &["name", "inputs", "tables"], Doc::Circuit)?;
    let exprs = |key: &str| {
        get(map, key, Doc::Circuit)
            .and_then(|v| array(v, at))
            .and_then(|items| {
                items
                    .iter()
                    .enumerate()
                    .map(|(i, v)| expr(v, &format!("{at}.{key}[{i}]"), columns))
                    .collect::<Result<Vec<_>, _>>()
            })
    };
    let inputs = exprs("inputs")?;
    let tables = exprs("tables")?;
    match () {
        () if inputs.is_empty() || inputs.len() != tables.len() => Err(Error::shape(
            at,
            "inputs and tables need the same non-zero length",
        )),
        () if !tables.iter().all(|t| t.fixed_only().0) => {
            Err(Error::shape(at, "tables use fixed columns only"))
        }
        () => name_of(map, at).map(|name| Lookup {
            name,
            pairs: inputs.into_iter().zip(tables).collect(),
        }),
    }
}

fn copy(value: &Value, at: &str, columns: Columns) -> Result<(CellRef, CellRef), Error> {
    array(value, at)
        .and_then(|items| two(items).ok_or_else(|| Error::shape(at, "a copy is a pair of cells")))
        .and_then(|(left, right)| {
            cell(left, at, columns).and_then(|l| cell(right, at, columns).map(|r| (l, r)))
        })
}

fn cell(value: &Value, at: &str, columns: Columns) -> Result<CellRef, Error> {
    let map = object(value, at)?;
    exact_keys(map, &["col", "index", "row"], Doc::Circuit)?;
    let kind = get(map, "col", Doc::Circuit)?
        .as_str()
        .and_then(kind_of)
        .ok_or_else(|| Error::shape(at, "col is not advice, fixed or instance"))?;
    let index = get(map, "index", Doc::Circuit)?
        .as_u64()
        .and_then(|n| usize::try_from(n).ok())
        .filter(|n| *n < columns.count(kind))
        .ok_or_else(|| Error::shape(at, "index is not a column of that kind"))?;
    get(map, "row", Doc::Circuit).and_then(|v| {
        v.as_u64()
            .and_then(|n| usize::try_from(n).ok())
            .map(|row| CellRef {
                kind,
                index: ColumnIndex(index),
                row,
            })
            .ok_or_else(|| Error::shape(at, "row is not a non-negative integer"))
    })
}

fn kind_of(text: &str) -> Option<ColumnKind> {
    [
        ("advice", ColumnKind::Advice),
        ("fixed", ColumnKind::Fixed),
        ("instance", ColumnKind::Instance),
    ]
    .into_iter()
    .find(|(name, _)| *name == text)
    .map(|(_, kind)| kind)
}

fn expr(value: &Value, at: &str, columns: Columns) -> Result<Expr, Error> {
    let map = object(value, at)?;
    let (key, arg) = map
        .iter()
        .next()
        .filter(|_| map.len() == 1)
        .ok_or_else(|| Error::shape(at, "an expression has exactly one key"))?;
    let both = |op: &str| {
        array(arg, at)
            .and_then(|items| two(items).ok_or_else(|| Error::shape(at, "takes two expressions")))
            .and_then(|(l, r)| {
                expr(l, &format!("{at}.{op}[0]"), columns).and_then(|left| {
                    expr(r, &format!("{at}.{op}[1]"), columns)
                        .map(|right| (Box::new(left), Box::new(right)))
                })
            })
    };
    match key.as_str() {
        "c" => field_value(arg, at).map(Expr::Const),
        "a" => query(arg, at, ColumnKind::Advice, columns),
        "f" => query(arg, at, ColumnKind::Fixed, columns),
        "i" => query(arg, at, ColumnKind::Instance, columns),
        "+" => both("+").map(|(l, r)| Expr::Sum(l, r)),
        "*" => both("*").map(|(l, r)| Expr::Product(l, r)),
        "-" => expr(arg, &format!("{at}.-"), columns).map(|e| Expr::Neg(Box::new(e))),
        other => Err(Error::Key {
            doc: Doc::Circuit,
            key: other.to_string(),
            problem: KeyProblem::Unknown,
        }),
    }
}

fn query(arg: &Value, at: &str, kind: ColumnKind, columns: Columns) -> Result<Expr, Error> {
    let (col, rot) = array(arg, at)
        .and_then(|items| two(items).ok_or_else(|| Error::shape(at, "a query is [COL, ROT]")))?;
    let index = col
        .as_u64()
        .and_then(|n| usize::try_from(n).ok())
        .filter(|n| *n < columns.count(kind))
        .ok_or_else(|| Error::shape(at, "column is not a column of that kind"))?;
    rot.as_i64()
        .and_then(|n| i32::try_from(n).ok())
        .map(|rotation| Expr::Query(kind, ColumnIndex(index), Rot(rotation)))
        .ok_or_else(|| Error::shape(at, "rotation is not a 32-bit integer"))
}
