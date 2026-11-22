use std::env;
use std::process::ExitCode;

fn main() -> ExitCode {
    let license = match env::var("DYNAMSOFT_LICENSE_KEY") {
        Ok(key) if !key.is_empty() => key,
        _ => {
            eprintln!(
                "Set DYNAMSOFT_LICENSE_KEY to your Dynamsoft license key.\n\
                 Trial: https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform"
            );
            return ExitCode::FAILURE;
        }
    };

    let files: Vec<String> = env::args().skip(1).collect();
    if files.is_empty() {
        eprintln!("usage: cargo run --example decode -- <image-or-pdf>...");
        return ExitCode::FAILURE;
    }

    let reader = match dbr::BarcodeReader::new(&license) {
        Ok(reader) => reader,
        Err(err) => {
            eprintln!("{err}");
            return ExitCode::FAILURE;
        }
    };

    let mut failed = false;
    for file in &files {
        println!("File: {file}");
        match reader.decode_file(file) {
            Ok(barcodes) if barcodes.is_empty() => println!("  no barcode found"),
            Ok(barcodes) => {
                for barcode in &barcodes {
                    println!("  [{}] {}", barcode.format, barcode.text);
                    println!("    points: {:?}", barcode.points);
                }
            }
            Err(err) => {
                eprintln!("  {err}");
                failed = true;
            }
        }
    }
    if failed {
        ExitCode::FAILURE
    } else {
        ExitCode::SUCCESS
    }
}