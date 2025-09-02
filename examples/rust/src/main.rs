mod bindings;

use std::io::{self, Write};
use std::ffi::CString;
use std::path::Path;
use std::env;
use bindings::*;

fn main() {
    // Check for command line arguments
    let args: Vec<String> = env::args().collect();
    
    if args.len() > 1 {
        // Non-interactive mode - process file(s) directly
        for file_path in &args[1..] {
            process_file_non_interactive(file_path);
        }
    } else {
        // Interactive mode
        run_interactive_mode();
    }
}

fn run_interactive_mode() {
    println!("*************************************************");
    println!("Welcome to Dynamsoft Barcode Demo (Rust Version)");
    println!("*************************************************");
    println!("Hints: Please input 'Q' or 'q' to quit the application.");

    // Initialize license
    let license = "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==";
    
    let ret = unsafe {
        let license_cstr = CString::new(license).expect("CString::new failed");
        init_license(license_cstr.as_ptr())
    };

    if ret != 0 {
        println!("License initialization failed with code: {}", ret);
        return;
    }

    // Create barcode reader instance
    let reader_ptr = unsafe { CVR_CreateInstance() };
    if reader_ptr.is_null() {
        println!("Failed to create barcode reader instance");
        return;
    }

    // Main processing loop
    loop {
        print!("\n>> Step 1: Input your image file's full path:\n");
        io::stdout().flush().unwrap();

        let mut input = String::new();
        io::stdin().read_line(&mut input).expect("Failed to read line");
        
        // Trim whitespace and remove surrounding quotes if present
        let input = input.trim()
            .trim_start_matches([' ', '\t', '\n', '\r', '"', '\''])
            .trim_end_matches([' ', '\t', '\n', '\r', '"', '\'']);

        // Check if the user wants to exit
        if input.eq_ignore_ascii_case("q") {
            break;
        }

        if input.is_empty() {
            continue;
        }

        // Validate file exists
        let path = Path::new(input);
        if !path.exists() {
            println!("Please input a valid path.");
            continue;
        }

        // Process the file
        process_image(reader_ptr, input);
    }

    // Cleanup
    unsafe {
        CVR_DestroyInstance(reader_ptr);
    }
}

fn process_file_non_interactive(file_path: &str) {
    // Initialize license
    let license = "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==";
    
    let ret = unsafe {
        let license_cstr = CString::new(license).expect("CString::new failed");
        init_license(license_cstr.as_ptr())
    };

    if ret != 0 {
        eprintln!("License initialization failed with code: {}", ret);
        return;
    }

    // Create barcode reader instance
    let reader_ptr = unsafe { CVR_CreateInstance() };
    if reader_ptr.is_null() {
        eprintln!("Failed to create barcode reader instance");
        return;
    }

    // Validate file exists
    let path = Path::new(file_path);
    if !path.exists() {
        eprintln!("File not found: {}", file_path);
        unsafe {
            CVR_DestroyInstance(reader_ptr);
        }
        return;
    }

    // Process the file
    process_image(reader_ptr, file_path);

    // Cleanup
    unsafe {
        CVR_DestroyInstance(reader_ptr);
    }
}

fn process_image(reader_ptr: *mut std::ffi::c_void, file_path: &str) {
    println!("Processing file: {}", file_path);

    let path_cstr = match CString::new(file_path) {
        Ok(cstr) => cstr,
        Err(e) => {
            println!("Invalid file path: {}", e);
            return;
        }
    };

    unsafe {
        let results_ptr = decode_barcode_file(reader_ptr, path_cstr.as_ptr());

        if results_ptr.is_null() {
            println!("No barcode found.");
        } else {
            let results = &*results_ptr;
            
            if results.count == 0 {
                println!("No barcode found.");
            } else {
                let barcodes = std::slice::from_raw_parts(results.barcodes, results.count as usize);
                
                println!("Decoded {} barcodes", results.count);
                
                for (i, barcode) in barcodes.iter().enumerate() {
                    let barcode_format = std::ffi::CStr::from_ptr(barcode.barcode_type)
                        .to_string_lossy();
                    let barcode_text = std::ffi::CStr::from_ptr(barcode.barcode_value)
                        .to_string_lossy();

                    println!("Result {}", i + 1);
                    println!("Barcode Format: {}", barcode_format);
                    println!("Barcode Text: {}", barcode_text);
                    println!("Point 1: ({}, {})", barcode.x1, barcode.y1);
                    println!("Point 2: ({}, {})", barcode.x2, barcode.y2);
                    println!("Point 3: ({}, {})", barcode.x3, barcode.y3);
                    println!("Point 4: ({}, {})", barcode.x4, barcode.y4);
                }
            }

            free_barcode(results_ptr);
        }
    }

    println!();
}
