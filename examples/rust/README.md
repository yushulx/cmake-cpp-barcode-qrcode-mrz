# Rust Barcode Scanner

A barcode scanner implemented in Rust, using FFI bindings to the Dynamsoft Barcode Reader SDK. This is a Rust port of the C++ command-line example with enhanced functionality. It ships two binaries that share the same native decoding core:

- **`barcode-scanner`** — the original command-line tool (interactive + batch modes).
- **`barcode-reader-ui`** — a desktop GUI built with [Slint](https://slint.dev) that previews the image and lists every decoded barcode.
- **[`tauri/`](tauri/)** — a second desktop GUI with identical features, built with [Tauri](https://tauri.app) (Rust backend + HTML/CSS/JS frontend). It reuses the same native decoding core and is meant as a teaching example; see [`tauri/README.md`](tauri/README.md) for a walkthrough of how Tauri wraps the native library.

## Features

- **Desktop UI (Slint)**: Open an image, see a preview, and browse all decoded results with format, text, and corner coordinates
- **Interactive Mode**: Run without arguments to enter interactive mode
- **Batch Processing**: Pass one or more file paths as arguments to process them directly
- **Multiple Barcode Types**: Supports all barcode types supported by Dynamsoft SDK
- **Coordinate Information**: Shows precise barcode location coordinates
- **File Validation**: Checks if files exist before processing
- **Cross-Platform**: Works on Windows, macOS, and Linux

## Prerequisites

1. **Rust Toolchain**: Install Rust from [rustup.rs](https://rustup.rs/)
2. **Dynamsoft SDK**: the one bundled in [`dcv/`](../../dcv) — `build.rs` links against it and copies the runtime libraries plus `Templates/`/`Models/` next to the binaries, so nothing has to be installed system-wide
3. **C++ toolchain**: Visual Studio Build Tools (MSVC) on Windows, `g++` on Linux, clang + Xcode Command Line Tools on macOS
4. **Dynamsoft Trial License**: Get a trial license from [Dynamsoft](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform)

## Building

```bash
cd examples/rust
cargo clean
cargo build --release
```

This produces both `barcode-scanner` (CLI) and `barcode-reader-ui` (Slint GUI) in `target/release`. The first build downloads and compiles Slint, so it takes longer than subsequent builds.

## Usage

### Desktop UI (Slint)

```bash
cargo run --release --bin barcode-reader-ui
```

Or on Windows, double-click `run-ui.bat`. In the window:

1. Click **Open Image...** and pick a file (the dialog starts in the repo's `images/` folder), or type/paste a full path and press **Decode** (Enter works too).
2. The left pane shows the image; the right pane lists each barcode's **format**, **text**, and four corner **coordinates**.
3. The status bar reports the decoded count or any error.

> The Slint preview supports JPEG, PNG, TIFF, BMP, GIF, and WebP; PDFs are decoded but not previewed. The Tauri UI additionally previews every page of PDFs and multi-page TIFFs with a pager.

### Desktop UI (Tauri)

The [`tauri/`](tauri/) subfolder is a separate crate that reproduces the Slint GUI with a web frontend:

```bash
cd examples/rust/tauri
cargo run --release
```

Or on Windows, double-click `tauri/run.bat`. It offers the same Open Image / path / Decode / Camera workflow and the same results list. [`tauri/README.md`](tauri/README.md) explains the Tauri-specific pieces (commands, events, custom `media:` protocol, state, capabilities, CSP) and maps each one back to its Slint equivalent.

### Interactive Mode

Run without arguments to enter interactive mode:

```bash
cargo run --bin barcode-scanner
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
cargo run --bin barcode-scanner -- "../../images/QRCode.tif"

# Multiple files
cargo run --bin barcode-scanner -- "../../images/QRCode.tif" "../../images/Code128.png" "../../images/multi-barcode.jpg"
```

### With Built Executable

After building, you can use the executable directly:

```bash
# Interactive mode
./target/release/barcode-scanner

# Batch mode
./target/release/barcode-scanner "path/to/image.jpg"

# Desktop UI
./target/release/barcode-reader-ui
```

## Supported Image Formats

- JPEG (.jpg, .jpeg)
- PNG (.png)
- TIFF (.tif, .tiff)
- BMP (.bmp)
- PDF (.pdf) - Multi-page support
- And other formats supported by Dynamsoft SDK

## Configuration

Set your license key in `src/scanner.rs` (shared by both binaries):

```rust
pub const DEFAULT_LICENSE: &str = "YOUR-LICENSE-KEY-HERE";
```

## Architecture

The Rust implementation uses FFI (Foreign Function Interface) to call the native Dynamsoft C library. `build.rs` compiles `lib/bridge.cpp`, links the SDK, copies the runtime DLLs/resources next to the binaries, and compiles the Slint UI (`ui/app.slint`).

- `src/scanner.rs`: FFI declarations plus a safe `BarcodeReader` wrapper (license init, decode, cleanup) shared by both binaries
- `src/main.rs`: Command-line logic with interactive and batch modes (`barcode-scanner`)
- `src/gui.rs`: Slint GUI logic — file dialog, image preview, and results model (`barcode-reader-ui`)
- `ui/app.slint`: Declarative Slint UI (toolbar, preview pane, results list, status bar)
- `lib/bridge.cpp` / `lib/bridge.h`: C ABI bridge over the Dynamsoft Capture Vision Router
- `Cargo.toml`: Project configuration and dependencies

![Rust Barcode Reader](https://www.dynamsoft.com/codepool/img/2024/06/rust-command-line-barcode-reader.jpg)

## Blog
[How to Build a Command-Line Barcode Reader with Rust and C++ Barcode SDK](https://www.dynamsoft.com/codepool/rust-barcode-reader-command-line.html)
