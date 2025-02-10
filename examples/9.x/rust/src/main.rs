mod bindings;
use std::io::{self, Write};
use std::ffi::CString;
use bindings::*;

fn main() {
    let license = "LICENSE-KEY";
    
    let ret = unsafe {
        let license = CString::new(license).expect("CString::new failed");
        init_license(license.as_ptr())
    };

    println!("InitLicense: {}", ret);

    let reader_ptr = unsafe { DBR_CreateInstance() };
    if reader_ptr.is_null() {
        panic!("Failed to create barcode reader instance");
    }

    loop {
        // Prompt the user to enter the file name
        print!("Please enter the file name (or type 'exit' to quit): ");
        io::stdout().flush().unwrap(); // Ensure the prompt is printed before reading input

        let mut file_name = String::new();
        io::stdin().read_line(&mut file_name).expect("Failed to read line");
        
        // Remove the newline character from the input
        let file_name = file_name.trim();

        // Check if the user wants to exit
        if file_name.to_lowercase() == "exit" {
            break;
        }

        // Use file_name for further processing
        println!("Processing file: {}", file_name);

        let path = CString::new(file_name).expect("CString::new failed");

        unsafe {
            let results_ptr = decode_barcode_file(reader_ptr, path.as_ptr());

            if results_ptr.is_null() {
                println!("No barcodes found.");
            } else {
                let results = &*results_ptr;
                let barcodes = std::slice::from_raw_parts(results.barcodes, results.count as usize);
    
                for (i, barcode) in barcodes.iter().enumerate() {
                    let barcode_type = std::ffi::CStr::from_ptr(barcode.barcode_type).to_string_lossy();
                    let barcode_value = std::ffi::CStr::from_ptr(barcode.barcode_value).to_string_lossy();
    
                    println!("Barcode {}: type = {}, value = {}", i + 1, barcode_type, barcode_value);
                    println!(
                        "Coordinates: ({}, {}), ({}, {}), ({}, {}), ({}, {})",
                        barcode.x1, barcode.y1, barcode.x2, barcode.y2,
                        barcode.x3, barcode.y3, barcode.x4, barcode.y4
                    );
                }
    
                free_barcode(results_ptr);
            }
        }
    }

    // Free the reader instance after usage
    unsafe {
        DBR_DestroyInstance(reader_ptr);
    }
}
