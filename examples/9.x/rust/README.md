# Command-Line Barcode Reader in Rust
This project showcases a simple command-line barcode reader built using Rust and the Dynamsoft Barcode Reader SDK.

## Prequisites
- [Rust](https://www.rust-lang.org/tools/install)
- [Dynamsoft Barcode Reader Trial License](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform)

## Usage
1. Set the license key in `src/main.rs`:

    ```rust
    let license = "LICENSE-KEY";
    ```

2. Clean and build the app:

    ```bash
    cargo clean
    cargo build 
    ```

3. Run the app:

    ```bash
    cargo run
    ```

    ![Rust Barcode Reader](https://www.dynamsoft.com/codepool/img/2024/06/rust-command-line-barcode-reader.jpg)

## Blog
[How to Build a Command-Line Barcode Reader with Rust and C++ Barcode SDK](https://www.dynamsoft.com/codepool/rust-barcode-reader-command-line.html)
