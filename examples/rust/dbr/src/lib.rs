//! Rust wrapper for the [Dynamsoft Barcode Reader SDK](https://www.dynamsoft.com/capture-vision/docs/core/introduction/)
//! (DBR 11.6.60, built on Dynamsoft Capture Vision 3.6.60).
//!
//! Reads 1D and 2D barcodes — QR Code, DataMatrix, PDF417, Code 128, EAN/UPC,
//! Aztec, MaxiCode and more — from image files (JPEG, PNG, TIFF, BMP, GIF,
//! WebP), PDF files and raw RGB888 pixel buffers.
//!
//! # License
//!
//! A license key is required; the crate ships no built-in key. Apply for a
//! free 30-day trial at
//! <https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform>.
//! New users are initially issued a 24-hour temporary license; submit the
//! trial application to get a 30-day key.
//!
//! # Native SDK
//!
//! The crate carries the C++ bridge; the SDK headers come from `DCV_DIR`, from
//! the `dcv/` folder of the sample repository, or from the copy staged with a
//! published release. The native libraries and decoding models (~40–70 MB) are
//! downloaded once from the official `dynamsoft-capture-vision-bundle` PyPI
//! wheel (SHA-256 verified, cached per platform) and placed next to the built
//! binaries. Set the `DCV_DIR` environment variable to a local Dynamsoft
//! Capture Vision 3.6.60 directory (with `lib/` and `resource/`) to build
//! offline.
//!
//! # Example
//!
//! ```no_run
//! let reader = dbr::BarcodeReader::new("YOUR-LICENSE-KEY").unwrap();
//! for barcode in reader.decode_file("label.png").unwrap() {
//!     println!("{}: {}", barcode.format, barcode.text);
//! }
//! ```

use std::ffi::{c_void, CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int};
use std::path::Path;

#[repr(C)]
struct RawBarcode {
    format: *const c_char,
    text: *const c_char,
    x1: c_int,
    y1: c_int,
    x2: c_int,
    y2: c_int,
    x3: c_int,
    y3: c_int,
    x4: c_int,
    y4: c_int,
}

#[repr(C)]
struct RawResults {
    barcodes: *mut RawBarcode,
    count: c_int,
    error_code: c_int,
    error_message: [c_char; 256],
}

extern "C" {
    fn dbr_init_license(
        license: *const c_char,
        error_message: *mut c_char,
        error_message_capacity: c_int,
    ) -> c_int;
    fn dbr_create_instance() -> *mut c_void;
    fn dbr_destroy_instance(instance: *mut c_void);
    fn dbr_decode_file(instance: *mut c_void, path: *const c_char) -> *mut RawResults;
    fn dbr_decode_buffer(
        instance: *mut c_void,
        bytes: *const u8,
        width: c_int,
        height: c_int,
        stride: c_int,
    ) -> *mut RawResults;
    fn dbr_free_results(results: *mut RawResults);
}

/// A decoded barcode.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BarcodeResult {
    /// Barcode format name reported by the SDK, e.g. `QR_CODE`, `DATA_MATRIX`,
    /// `PDF417`, `CODE_128`.
    pub format: String,
    /// Decoded text. Binary payloads are returned lossily as UTF-8.
    pub text: String,
    /// Corner coordinates in pixels, clockwise from the top-left corner.
    pub points: [(i32, i32); 4],
}

/// Errors returned by the barcode reader.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    /// The license key was rejected by the SDK.
    License { code: i32, message: String },
    /// The native reader instance could not be created.
    InstanceCreation,
    /// An argument was invalid (interior NUL byte, non-UTF-8 path,
    /// non-positive dimensions or a pixel buffer that is too small).
    InvalidInput(String),
    /// The SDK failed to capture or decode the input. `code` is the SDK error
    /// code; "no barcode found" is **not** an error and yields an empty list.
    Decode { code: i32, message: String },
    /// The native bridge ran out of memory.
    OutOfMemory,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::License { code, message } => {
                write!(f, "license initialization failed (code {code}): {message}")
            }
            Error::InstanceCreation => write!(f, "failed to create barcode reader instance"),
            Error::InvalidInput(message) => write!(f, "invalid input: {message}"),
            Error::Decode { code, message } => write!(f, "decode failed (code {code}): {message}"),
            Error::OutOfMemory => write!(f, "out of memory"),
        }
    }
}

impl std::error::Error for Error {}

/// A Dynamsoft barcode reader instance.
///
/// Create one with [`BarcodeReader::new`] and reuse it for any number of
/// decode calls; the native instance is released on drop.
#[derive(Debug)]
pub struct BarcodeReader {
    instance: *mut c_void,
}

impl BarcodeReader {
    /// Initializes the SDK license and creates a reader instance.
    ///
    /// Get a trial key at
    /// <https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform>.
    pub fn new(license: &str) -> Result<Self, Error> {
        let license = CString::new(license)
            .map_err(|_| Error::InvalidInput("license key contains a NUL byte".into()))?;
        let mut message = [0 as c_char; 256];
        let code = unsafe {
            dbr_init_license(license.as_ptr(), message.as_mut_ptr(), message.len() as c_int)
        };
        if code != 0 {
            let message = unsafe { CStr::from_ptr(message.as_ptr()) }
                .to_string_lossy()
                .into_owned();
            return Err(Error::License { code, message });
        }

        let instance = unsafe { dbr_create_instance() };
        if instance.is_null() {
            return Err(Error::InstanceCreation);
        }
        Ok(Self { instance })
    }

    /// Decodes all barcodes in an image or PDF file.
    ///
    /// Returns an empty list when no barcode is found. Supported inputs
    /// include JPEG, PNG, TIFF, BMP, GIF, WebP and PDF (all pages).
    pub fn decode_file(&self, path: impl AsRef<Path>) -> Result<Vec<BarcodeResult>, Error> {
        let path = path.as_ref();
        let path_str = path
            .to_str()
            .ok_or_else(|| Error::InvalidInput(format!("path is not valid UTF-8: {}", path.display())))?;
        let path_c = CString::new(path_str)
            .map_err(|_| Error::InvalidInput("path contains a NUL byte".into()))?;
        let raw = unsafe { dbr_decode_file(self.instance, path_c.as_ptr()) };
        unsafe { collect_results(raw) }
    }

    /// Decodes barcodes from a raw RGB888 frame (3 bytes per pixel, rows
    /// tightly packed), e.g. a camera frame.
    ///
    /// Returns an empty list when no barcode is found.
    pub fn decode_buffer(
        &self,
        rgb: &[u8],
        width: i32,
        height: i32,
    ) -> Result<Vec<BarcodeResult>, Error> {
        if width <= 0 || height <= 0 {
            return Err(Error::InvalidInput(format!(
                "dimensions must be positive, got {width}x{height}"
            )));
        }
        let stride = width
            .checked_mul(3)
            .ok_or_else(|| Error::InvalidInput("width is too large".into()))?;
        let required = stride as usize * height as usize;
        if rgb.len() < required {
            return Err(Error::InvalidInput(format!(
                "buffer too small: {width}x{height} RGB888 needs {required} bytes, got {}",
                rgb.len()
            )));
        }
        let raw = unsafe {
            dbr_decode_buffer(self.instance, rgb.as_ptr(), width, height, stride)
        };
        unsafe { collect_results(raw) }
    }
}

impl Drop for BarcodeReader {
    fn drop(&mut self) {
        unsafe { dbr_destroy_instance(self.instance) };
    }
}

unsafe fn collect_results(raw: *mut RawResults) -> Result<Vec<BarcodeResult>, Error> {
    if raw.is_null() {
        return Err(Error::OutOfMemory);
    }
    let results = &*raw;
    if results.error_code != 0 {
        let message = CStr::from_ptr(results.error_message.as_ptr())
            .to_string_lossy()
            .into_owned();
        let error = Error::Decode {
            code: results.error_code,
            message,
        };
        dbr_free_results(raw);
        return Err(error);
    }

    let mut barcodes = Vec::new();
    if results.count > 0 && !results.barcodes.is_null() {
        let items = std::slice::from_raw_parts(results.barcodes, results.count as usize);
        for item in items {
            let format = CStr::from_ptr(item.format).to_string_lossy().into_owned();
            let text = CStr::from_ptr(item.text).to_string_lossy().into_owned();
            barcodes.push(BarcodeResult {
                format,
                text,
                points: [
                    (item.x1, item.y1),
                    (item.x2, item.y2),
                    (item.x3, item.y3),
                    (item.x4, item.y4),
                ],
            });
        }
    }
    dbr_free_results(raw);
    Ok(barcodes)
}