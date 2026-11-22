mod scanner;

use std::env;
use std::io::{self, Write};
use std::path::Path;

use scanner::{BarcodeReader, DEFAULT_LICENSE};

fn main() {
    let args: Vec<String> = env::args().collect();

    let reader = match BarcodeReader::new(DEFAULT_LICENSE) {
        Ok(reader) => reader,
        Err(err) => {
            eprintln!("{err}");
            return;
        }
    };

    if args.len() > 1 {
        // Non-interactive mode - process file(s) directly
        for file_path in &args[1..] {
            if !Path::new(file_path).exists() {
                eprintln!("File not found: {file_path}");
                continue;
            }
            process_image(&reader, file_path);
        }
    } else {
        run_interactive_mode(&reader);
    }
}

fn run_interactive_mode(reader: &BarcodeReader) {
    println!("*************************************************");
    println!("Welcome to Dynamsoft Barcode Demo (Rust Version)");
    println!("*************************************************");
    println!("Hints: Please input 'Q' or 'q' to quit the application.");

    loop {
        print!("\n>> Step 1: Input your image file's full path:\n");
        io::stdout().flush().unwrap();

        let mut input = String::new();
        io::stdin().read_line(&mut input).expect("Failed to read line");

        // Trim whitespace and remove surrounding quotes if present
        let input = input
            .trim()
            .trim_start_matches([' ', '\t', '\n', '\r', '"', '\''])
            .trim_end_matches([' ', '\t', '\n', '\r', '"', '\'']);

        if input.eq_ignore_ascii_case("q") {
            break;
        }
        if input.is_empty() {
            continue;
        }
        if !Path::new(input).exists() {
            println!("Please input a valid path.");
            continue;
        }

        process_image(reader, input);
    }
}

fn process_image(reader: &BarcodeReader, file_path: &str) {
    println!("Processing file: {file_path}");

    match reader.decode(file_path) {
        Ok(barcodes) if barcodes.is_empty() => {
            println!("No barcode found.");
        }
        Ok(barcodes) => {
            println!("Decoded {} barcodes", barcodes.len());
            for (i, barcode) in barcodes.iter().enumerate() {
                println!("Result {}", i + 1);
                println!("Barcode Format: {}", barcode.format);
                println!("Barcode Text: {}", barcode.text);
                for (p, (x, y)) in barcode.points.iter().enumerate() {
                    println!("Point {}: ({}, {})", p + 1, x, y);
                }
            }
        }
        Err(err) => {
            println!("{err}");
        }
    }

    println!();
}