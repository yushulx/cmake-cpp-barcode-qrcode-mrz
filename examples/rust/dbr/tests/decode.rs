use std::env;

use dbr::{BarcodeReader, Error};

fn licensed_reader() -> Option<BarcodeReader> {
    let key = env::var("DYNAMSOFT_LICENSE_KEY").ok().filter(|k| !k.is_empty())?;
    Some(BarcodeReader::new(&key).expect("license initialization failed"))
}

macro_rules! require_reader {
    () => {
        match licensed_reader() {
            Some(reader) => reader,
            None => {
                eprintln!("skipped: DYNAMSOFT_LICENSE_KEY is not set");
                return;
            }
        }
    };
}

#[test]
fn invalid_license_is_reported() {
    let err = BarcodeReader::new("not-a-valid-license").unwrap_err();
    assert!(matches!(err, Error::License { .. }), "got: {err:?}");
}

#[test]
fn decode_qr_file() {
    let reader = require_reader!();
    let results = reader
        .decode_file(concat!(env!("CARGO_MANIFEST_DIR"), "/tests/assets/QRCode.tif"))
        .unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].format, "QR_CODE");
    assert_eq!(results[0].text, "www.dynamsoft.com");
    assert_eq!(results[0].points[0], (1005, 42));
}

#[test]
fn decode_multiple_formats() {
    let reader = require_reader!();
    let results = reader
        .decode_file(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/tests/assets/AllSupportedBarcodeTypes.png"
        ))
        .unwrap();
    assert!(results.len() >= 5, "got {} results", results.len());
    let formats: Vec<&str> = results.iter().map(|r| r.format.as_str()).collect();
    assert!(formats.contains(&"QR_CODE"), "formats: {formats:?}");
    assert!(formats.contains(&"DATAMATRIX"), "formats: {formats:?}");
    assert!(formats.contains(&"PDF417"), "formats: {formats:?}");
}

#[test]
fn missing_file_is_decode_error() {
    let reader = require_reader!();
    let err = reader.decode_file("no-such-file.png").unwrap_err();
    assert!(matches!(err, Error::Decode { .. }), "got: {err:?}");
}

#[test]
fn decode_buffer_matches_file() {
    let reader = require_reader!();
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/assets/AllSupportedBarcodeTypes.png"
    );
    let mut from_file = reader.decode_file(path).unwrap();
    assert!(!from_file.is_empty());

    let img = image::open(path).unwrap().to_rgb8();
    let (width, height) = img.dimensions();
    let mut from_buffer = reader
        .decode_buffer(img.as_raw(), width as i32, height as i32)
        .unwrap();
    from_file.sort_by(|a, b| (&a.format, &a.text).cmp(&(&b.format, &b.text)));
    from_buffer.sort_by(|a, b| (&a.format, &a.text).cmp(&(&b.format, &b.text)));
    assert_eq!(from_file.len(), from_buffer.len());
    for (a, b) in from_file.iter().zip(from_buffer.iter()) {
        assert_eq!(a.format, b.format);
        assert_eq!(a.text, b.text);
        // The file and raw-buffer paths preprocess slightly differently, so
        // corner coordinates may drift by a few pixels.
        for ((ax, ay), (bx, by)) in a.points.iter().zip(b.points.iter()) {
            assert!(
                (ax - bx).abs() <= 4 && (ay - by).abs() <= 4,
                "points diverge: {a:?} vs {b:?}"
            );
        }
    }
}

#[test]
fn buffer_too_small_is_invalid_input() {
    let reader = require_reader!();
    let err = reader.decode_buffer(&[0u8; 10], 100, 100).unwrap_err();
    assert!(matches!(err, Error::InvalidInput(_)), "got: {err:?}");
}

#[test]
fn empty_image_yields_no_barcodes() {
    let reader = require_reader!();
    let white = vec![255u8; 64 * 64 * 3];
    let results = reader.decode_buffer(&white, 64, 64).unwrap();
    assert!(results.is_empty());
}