//! The halo2 circuit that a checked circuit document describes.

use std::collections::BTreeSet;
use std::sync::Arc;

use halo2_proofs::arithmetic::Field;
use halo2_proofs::circuit::{Cell, Layouter, Region, RegionIndex, SimpleFloorPlanner, Value};
use halo2_proofs::halo2curves::bn256::Fr;
use halo2_proofs::plonk::{
    Advice, Any, Circuit, Column, ConstraintSystem, Error as PlonkError, Expression, Fixed,
    Instance, VirtualCells,
};
use halo2_proofs::poly::Rotation;

use crate::ir::{CellRef, CircuitDoc, ColumnKind, Expr, WitnessDoc};

/// The circuit shape. It is `Circuit::Params`, so keygen, prove and the key
/// readers configure the same columns, gates and lookups.
#[derive(Clone, Debug, Default)]
pub struct Shape(Arc<CircuitDoc>);

impl From<CircuitDoc> for Shape {
    fn from(doc: CircuitDoc) -> Self {
        Self(Arc::new(doc))
    }
}

impl Shape {
    pub fn doc(&self) -> &CircuitDoc {
        &self.0
    }
}

/// The columns of a configured circuit.
#[derive(Clone, Debug)]
pub struct Config {
    advice: Vec<Column<Advice>>,
    fixed: Vec<Column<Fixed>>,
    instance: Vec<Column<Instance>>,
}

impl Config {
    fn column(&self, kind: ColumnKind, index: usize) -> Option<Column<Any>> {
        match kind {
            ColumnKind::Advice => self.advice.get(index).map(|c| Column::<Any>::from(*c)),
            ColumnKind::Fixed => self.fixed.get(index).map(|c| Column::<Any>::from(*c)),
            ColumnKind::Instance => self.instance.get(index).map(|c| Column::<Any>::from(*c)),
        }
    }

    /// The cell of a copy. The circuit has one region, at row 0.
    fn cell(&self, cell: CellRef) -> Option<Cell> {
        self.column(cell.kind(), cell.index().get())
            .map(|column| Cell {
                region_index: RegionIndex::from(0),
                row_offset: cell.row(),
                column,
            })
    }
}

/// A circuit document, with a witness when it proves.
#[derive(Clone, Debug, Default)]
pub struct GlueCircuit {
    shape: Shape,
    witness: Option<Arc<WitnessDoc>>,
}

impl GlueCircuit {
    /// The circuit without a witness, for keygen.
    pub fn keygen(shape: Shape) -> Self {
        Self {
            shape,
            witness: None,
        }
    }

    /// The circuit with a witness, for the mock prover and the prover.
    pub fn proving(shape: Shape, witness: WitnessDoc) -> Self {
        Self {
            shape,
            witness: Some(Arc::new(witness)),
        }
    }

    pub fn shape(&self) -> &Shape {
        &self.shape
    }

    /// The instance columns of the witness (empty columns without one).
    pub fn instances(&self) -> Vec<Vec<Fr>> {
        self.witness.as_ref().map_or_else(
            || vec![Vec::new(); self.shape.doc().columns().count(ColumnKind::Instance)],
            |witness| witness.instance().to_vec(),
        )
    }

    fn advice_value(&self, column: usize, row: usize) -> Value<Fr> {
        self.witness
            .as_ref()
            .and_then(|w| w.advice().get(column))
            .and_then(|values| values.get(row))
            .map_or_else(Value::unknown, |value| Value::known(*value))
    }
}

/// Lower an IR expression. The reader checked every column, so the
/// fallback for a missing column (the constant one) never applies; it
/// fails closed.
fn lower(cells: &mut VirtualCells<'_, Fr>, config: &Config, expr: &Expr) -> Expression<Fr> {
    match expr {
        Expr::Const(value) => Expression::Constant(*value),
        Expr::Query(kind, index, rot) => config
            .column(*kind, index.get())
            .map_or(Expression::Constant(Fr::ONE), |column| {
                cells.query_any(column, Rotation(rot.get()))
            }),
        Expr::Sum(left, right) => lower(cells, config, left) + lower(cells, config, right),
        Expr::Product(left, right) => lower(cells, config, left) * lower(cells, config, right),
        Expr::Neg(inner) => -lower(cells, config, inner),
    }
}

fn assign_fixed(
    region: &mut Region<'_, Fr>,
    config: &Config,
    doc: &CircuitDoc,
) -> Result<(), PlonkError> {
    doc.fixed_values()
        .iter()
        .zip(config.fixed.iter())
        .try_for_each(|(values, column)| {
            values.iter().enumerate().try_for_each(|(row, value)| {
                region
                    .assign_fixed(|| "fixed", *column, row, || Value::known(*value))
                    .map(|_| ())
            })
        })
}

fn assign_advice(
    region: &mut Region<'_, Fr>,
    config: &Config,
    circuit: &GlueCircuit,
) -> Result<(), PlonkError> {
    config
        .advice
        .iter()
        .enumerate()
        .try_for_each(|(index, column)| {
            (0..circuit.shape.doc().rows()).try_for_each(|row| {
                let value = circuit.advice_value(index, row);
                region
                    .assign_advice(|| "advice", *column, row, move || value)
                    .map(|_| ())
            })
        })
}

fn constrain_copies(
    region: &mut Region<'_, Fr>,
    config: &Config,
    doc: &CircuitDoc,
) -> Result<(), PlonkError> {
    doc.copies().iter().try_for_each(|(left, right)| {
        config
            .cell(*left)
            .zip(config.cell(*right))
            .map_or(Err(PlonkError::Synthesis), |(l, r)| {
                region.constrain_equal(l, r)
            })
    })
}

impl Circuit<Fr> for GlueCircuit {
    type Config = Config;
    type FloorPlanner = SimpleFloorPlanner;
    type Params = Shape;

    fn without_witnesses(&self) -> Self {
        Self::keygen(self.shape.clone())
    }

    fn params(&self) -> Shape {
        self.shape.clone()
    }

    fn configure(meta: &mut ConstraintSystem<Fr>) -> Config {
        Self::configure_with_params(meta, Shape::default())
    }

    fn configure_with_params(meta: &mut ConstraintSystem<Fr>, shape: Shape) -> Config {
        let doc = shape.doc();
        let columns = doc.columns();
        let config = Config {
            advice: (0..columns.count(ColumnKind::Advice))
                .map(|_| meta.advice_column())
                .collect(),
            fixed: (0..columns.count(ColumnKind::Fixed))
                .map(|_| meta.fixed_column())
                .collect(),
            instance: (0..columns.count(ColumnKind::Instance))
                .map(|_| meta.instance_column())
                .collect(),
        };
        doc.copies()
            .iter()
            .flat_map(|(left, right)| [*left, *right])
            .map(|cell| (cell.kind(), cell.index().get()))
            .collect::<BTreeSet<_>>()
            .into_iter()
            .filter_map(|(kind, index)| config.column(kind, index))
            .for_each(|column| meta.enable_equality(column));
        doc.gates().iter().for_each(|gate| {
            meta.create_gate(gate.name(), |cells| {
                gate.polys()
                    .iter()
                    .map(|poly| lower(cells, &config, poly))
                    .collect::<Vec<_>>()
            })
        });
        doc.lookups().iter().for_each(|lookup| {
            meta.lookup_any(lookup.name(), |cells| {
                lookup
                    .pairs()
                    .iter()
                    .map(|(input, table)| {
                        (lower(cells, &config, input), lower(cells, &config, table))
                    })
                    .collect()
            });
        });
        config
    }

    fn synthesize(
        &self,
        config: Config,
        mut layouter: impl Layouter<Fr>,
    ) -> Result<(), PlonkError> {
        let doc = self.shape.doc();
        layouter.assign_region(
            || "glue",
            |mut region| {
                assign_fixed(&mut region, &config, doc)
                    .and_then(|()| assign_advice(&mut region, &config, self))
                    .and_then(|()| constrain_copies(&mut region, &config, doc))
            },
        )
    }
}

/// The blinding rows of the configured circuit. The usable rows of `2^k`
/// are `2^k - (blinding_factors + 1)`.
pub(crate) fn blinding_factors(doc: &CircuitDoc) -> usize {
    let mut meta = ConstraintSystem::<Fr>::default();
    let _config = GlueCircuit::configure_with_params(&mut meta, Shape::from(doc.clone()));
    meta.blinding_factors()
}
