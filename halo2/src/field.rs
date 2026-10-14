//! BN254 Fr values: the IR decimal form and the EVM hex form.

use halo2_proofs::arithmetic::Field;
use halo2_proofs::halo2curves::bn256::Fr;
use halo2_proofs::halo2curves::ff::PrimeField;

/// The modulus r of Fr, in decimal.
const MODULUS: &str =
    "21888242871839275222246405745257275088548364400416034343698204186575808495617";

/// Parse an IR value: a decimal integer in `[0, r)` with no sign and no
/// leading zeros.
pub fn parse_decimal(text: &str) -> Result<Fr, &'static str> {
    let digits_ok = !text.is_empty() && text.bytes().all(|b| b.is_ascii_digit());
    let below_r = text.len() < MODULUS.len() || (text.len() == MODULUS.len() && text < MODULUS);
    match () {
        () if !digits_ok => Err("not a decimal integer string"),
        () if text.len() > 1 && text.starts_with('0') => Err("leading zero"),
        () if !below_r => Err("not below the field modulus r"),
        () => Ok(text.bytes().fold(Fr::ZERO, |acc, b| {
            acc * Fr::from(10u64) + Fr::from(u64::from(b.saturating_sub(b'0')))
        })),
    }
}

/// The value as a 0x-prefixed big-endian uint256 hex string.
pub fn to_hex(value: &Fr) -> String {
    let big_endian = value.to_repr().iter().rev().copied().collect::<Vec<u8>>();
    format!("0x{}", hex::encode(big_endian))
}

/// Parse a 0x-prefixed big-endian uint256 hex string below r.
pub fn from_hex(text: &str) -> Option<Fr> {
    text.strip_prefix("0x")
        .and_then(|digits| hex::decode(digits).ok())
        .and_then(|bytes| <[u8; 32]>::try_from(bytes.into_iter().rev().collect::<Vec<u8>>()).ok())
        .and_then(|little_endian| Option::<Fr>::from(Fr::from_repr(little_endian)))
}
