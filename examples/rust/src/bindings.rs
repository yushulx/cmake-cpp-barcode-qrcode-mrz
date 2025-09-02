use std::ffi::c_void;
use std::os::raw::c_char;
use std::os::raw::c_int;

#[repr(C)]
pub struct Barcode {
    pub barcode_type: *const c_char,
    pub barcode_value: *const c_char,
    pub x1: c_int,
    pub y1: c_int,
    pub x2: c_int,
    pub y2: c_int,
    pub x3: c_int,
    pub y3: c_int,
    pub x4: c_int,
    pub y4: c_int,
}

#[repr(C)]
pub struct BarcodeResults {
    pub barcodes: *mut Barcode,
    pub count: c_int,
}

extern "C" {
    pub fn free_barcode(barcode: *mut BarcodeResults);
    pub fn init_license(license: *const c_char) -> c_int;
    pub fn decode_barcode_file(instance: *mut c_void, filename: *const c_char) -> *mut BarcodeResults;
    pub fn CVR_CreateInstance() -> *mut c_void;
    pub fn CVR_DestroyInstance(instance: *mut c_void); 
}
