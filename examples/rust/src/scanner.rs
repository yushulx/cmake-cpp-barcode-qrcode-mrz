use std::ffi::{c_void, CStr, CString};
use std::os::raw::{c_char, c_int};

#[repr(C)]
struct Barcode {
    barcode_type: *const c_char,
    barcode_value: *const c_char,
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
struct BarcodeResults {
    barcodes: *mut Barcode,
    count: c_int,
}

#[repr(C)]
struct PageResult {
    width: c_int,
    height: c_int,
    rgb: *mut u8,
    rgb_length: c_int,
    barcodes: *mut Barcode,
    count: c_int,
}

#[repr(C)]
struct PageResults {
    pages: *mut PageResult,
    count: c_int,
}

extern "C" {
    fn free_barcode(barcode: *mut BarcodeResults);
    fn free_page_results(results: *mut PageResults);
    fn init_license(license: *const c_char) -> c_int;
    fn decode_barcode_file(instance: *mut c_void, filename: *const c_char) -> *mut BarcodeResults;
    fn decode_barcode_file_pages(instance: *mut c_void, filename: *const c_char) -> *mut PageResults;
    fn decode_barcode_buffer(
        instance: *mut c_void,
        bytes: *const u8,
        width: c_int,
        height: c_int,
        stride: c_int,
    ) -> *mut BarcodeResults;
    fn CVR_CreateInstance() -> *mut c_void;
    fn CVR_DestroyInstance(instance: *mut c_void);
}

/// Default Dynamsoft trial license shared by the command-line and UI examples.
/// Request your own key at https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
pub const DEFAULT_LICENSE: &str = "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==";

#[derive(Clone, Debug)]
pub struct BarcodeResult {
    pub format: String,
    pub text: String,
    pub points: [(i32, i32); 4],
}

/// One rasterized page from a file plus the barcodes found on it.
#[derive(Clone, Debug)]
pub struct PageDecode {
    pub rgb: Vec<u8>,
    pub width: i32,
    pub height: i32,
    pub barcodes: Vec<BarcodeResult>,
}

pub struct BarcodeReader {
    instance: *mut c_void,
}

impl BarcodeReader {
    /// Initialize the license and create a Capture Vision Router instance.
    pub fn new(license: &str) -> Result<Self, String> {
        let license_cstr = CString::new(license).map_err(|e| e.to_string())?;
        let ret = unsafe { init_license(license_cstr.as_ptr()) };
        if ret != 0 {
            return Err(format!("License initialization failed with code: {ret}"));
        }

        let instance = unsafe { CVR_CreateInstance() };
        if instance.is_null() {
            return Err("Failed to create barcode reader instance".to_string());
        }

        Ok(Self { instance })
    }

    /// Decode all barcodes in an image/PDF file. An empty vector means none were found.
    #[allow(dead_code)] // used by the Slint example; Tauri decodes page by page
    pub fn decode(&self, file_path: &str) -> Result<Vec<BarcodeResult>, String> {
        let path_cstr = CString::new(file_path).map_err(|e| format!("Invalid file path: {e}"))?;

        unsafe {
            let results_ptr = decode_barcode_file(self.instance, path_cstr.as_ptr());
            Ok(collect_results(results_ptr))
        }
    }

    /// Decode barcodes from a raw RGB888 frame (e.g. a camera frame).
    pub fn decode_buffer(&self, rgb: &[u8], width: i32, height: i32) -> Result<Vec<BarcodeResult>, String> {
        let stride = width * 3;
        let results_ptr =
            unsafe { decode_barcode_buffer(self.instance, rgb.as_ptr(), width, height, stride) };
        Ok(unsafe { collect_results(results_ptr) })
    }

    /// Decode a file page by page. Each page carries the rasterized RGB888 pixels
    /// the SDK decoded, so a PDF or multi-page TIFF can be previewed and navigated.
    /// An empty vector means the file could not be rasterized.
    pub fn decode_pages(&self, file_path: &str) -> Result<Vec<PageDecode>, String> {
        let path_cstr = CString::new(file_path).map_err(|e| format!("Invalid file path: {e}"))?;
        let results_ptr =
            unsafe { decode_barcode_file_pages(self.instance, path_cstr.as_ptr()) };
        Ok(unsafe { collect_pages(results_ptr) })
    }
}

unsafe fn collect_results(results_ptr: *mut BarcodeResults) -> Vec<BarcodeResult> {
    if results_ptr.is_null() {
        return Vec::new();
    }
    let results = &*results_ptr;
    let mut barcodes = Vec::new();
    if results.count > 0 && !results.barcodes.is_null() {
        let raw = std::slice::from_raw_parts(results.barcodes, results.count as usize);
        for barcode in raw {
            barcodes.push(to_barcode_result(barcode));
        }
    }
    free_barcode(results_ptr);
    barcodes
}

unsafe fn collect_pages(results_ptr: *mut PageResults) -> Vec<PageDecode> {
    if results_ptr.is_null() {
        return Vec::new();
    }
    let results = &*results_ptr;
    let mut pages = Vec::new();
    if results.count > 0 && !results.pages.is_null() {
        let raw = std::slice::from_raw_parts(results.pages, results.count as usize);
        for page in raw {
            let rgb = if page.rgb_length > 0 && !page.rgb.is_null() {
                std::slice::from_raw_parts(page.rgb, page.rgb_length as usize).to_vec()
            } else {
                Vec::new()
            };
            let mut barcodes = Vec::new();
            if page.count > 0 && !page.barcodes.is_null() {
                let items = std::slice::from_raw_parts(page.barcodes, page.count as usize);
                for barcode in items {
                    barcodes.push(to_barcode_result(barcode));
                }
            }
            pages.push(PageDecode {
                rgb,
                width: page.width,
                height: page.height,
                barcodes,
            });
        }
    }
    free_page_results(results_ptr);
    pages
}

unsafe fn to_barcode_result(barcode: &Barcode) -> BarcodeResult {
    BarcodeResult {
        format: CStr::from_ptr(barcode.barcode_type).to_string_lossy().into_owned(),
        text: CStr::from_ptr(barcode.barcode_value).to_string_lossy().into_owned(),
        points: [
            (barcode.x1, barcode.y1),
            (barcode.x2, barcode.y2),
            (barcode.x3, barcode.y3),
            (barcode.x4, barcode.y4),
        ],
    }
}

impl Drop for BarcodeReader {
    fn drop(&mut self) {
        unsafe { CVR_DestroyInstance(self.instance) };
    }
}