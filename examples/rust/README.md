# Rust Barcode Scanner

A command-line barcode scanner implemented in Rust, using FFI bindings to the Dynamsoft Barcode Reader SDK. This is a Rust port of the C++ command-line example with enhanced functionality.

## Features

- **Interactive Mode**: Run without arguments to enter interactive mode
- **Batch Processing**: Pass one or more file paths as arguments to process them directly
- **Multiple Barcode Types**: Supports all barcode types supported by Dynamsoft SDK
- **Coordinate Information**: Shows precise barcode location coordinates
- **File Validation**: Checks if files exist before processing
- **Cross-Platform**: Works on Windows, macOS, and Linux

## Prerequisites

1. **Rust Toolchain**: Install Rust from [rustup.rs](https://rustup.rs/)
2. **Dynamsoft SDK**: The Dynamsoft Capture Vision SDK must be installed and accessible
3. **Visual Studio Build Tools** (Windows): Required for FFI compilation
4. **Dynamsoft Trial License**: Get a trial license from [Dynamsoft](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform)

## Building

```bash
cd examples/rust
cargo clean
cargo build --release
```

## Usage

### Interactive Mode

Run without arguments to enter interactive mode:

```bash
cargo run
```

Example session:
```
*************************************************
Welcome to Dynamsoft Barcode Demo (Rust Version)
*************************************************
Hints: Please input 'Q' or 'q' to quit the application.

>> Step 1: Input your image file's full path:
../../images/QRCode.tif
Processing file: ../../images/QRCode.tif
Decoded 1 barcodes
Result 1
Barcode Format: QR_CODE
Barcode Text: https://www.dynamsoft.com
Point 1: (154, 154)
Point 2: (346, 154)
Point 3: (346, 346)
Point 4: (154, 346)

>> Step 1: Input your image file's full path:
q
```

### Batch Mode

Process one or more files directly:

```bash
# Single file
cargo run -- "../../images/QRCode.tif"

# Multiple files
cargo run -- "../../images/QRCode.tif" "../../images/Code128.png" "../../images/multi-barcode.jpg"
```

### With Built Executable

After building, you can use the executable directly:

```bash
# Interactive mode
./target/release/dynamsoft-barcode-cli

# Batch mode
./target/release/dynamsoft-barcode-cli "path/to/image.jpg"
```

## Supported Image Formats

- JPEG (.jpg, .jpeg)
- PNG (.png)
- TIFF (.tif, .tiff)
- BMP (.bmp)
- PDF (.pdf) - Multi-page support
- And other formats supported by Dynamsoft SDK

## Configuration

Set your license key in `src/main.rs`:

```rust
let license = "YOUR-LICENSE-KEY-HERE";
```

## Architecture

The Rust implementation uses FFI (Foreign Function Interface) to call the native Dynamsoft C library:

- `bindings.rs`: Contains FFI declarations and C structure definitions
- `main.rs`: Main application logic with both interactive and batch modes
- `Cargo.toml`: Project configuration and dependencies

![Rust Barcode Reader](https://www.dynamsoft.com/codepool/img/2024/06/rust-command-line-barcode-reader.jpg)

## Blog
[How to Build a Command-Line Barcode Reader with Rust and C++ Barcode SDK](https://www.dynamsoft.com/codepool/rust-barcode-reader-command-line.html)
