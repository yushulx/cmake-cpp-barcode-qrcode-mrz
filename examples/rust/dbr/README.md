# dbr

[![crates.io](https://img.shields.io/crates/v/dbr.svg)](https://crates.io/crates/dbr)
[![docs.rs](https://docs.rs/dbr/badge.svg)](https://docs.rs/dbr)

Rust wrapper for the **Dynamsoft Barcode Reader SDK** — read QR Code, DataMatrix,
PDF417, Code 128, EAN/UPC, Aztec, MaxiCode and other 1D/2D barcodes from image
files, PDFs and raw pixel buffers.

This is the modern successor of the `dbr` 0.1.x crate (2018). It wraps
**Dynamsoft Barcode Reader 11.6.50** through the **Dynamsoft Capture Vision
3.6.50** C++ API, following the
[Rust example](https://github.com/yushulx/cmake-cpp-barcode-qrcode-mrz/tree/main/examples/rust)
in the official sample repository.

## Supported platforms

| OS | Architecture |
| --- | --- |
| Windows | x64 (MSVC toolchain) |
| Linux | x64, arm64 |
| macOS | x64, arm64 (universal SDK binaries) |

## Install

```bash
cargo add dbr
```

Building requires a C++ compiler: Visual Studio Build Tools (MSVC) on Windows,
g++ on Linux, clang + Xcode Command Line Tools on macOS (the build uses
`install_name_tool` to make the SDK dylibs relocatable).

## License key

A license key is required to run the SDK; the crate ships no built-in key.
Apply for a free **30-day trial** at:
<https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform>

> New users are initially issued a 24-hour temporary license. To use the SDK
> beyond one day, submit the trial application on that page for a 30-day key.

## Usage

```rust
let reader = dbr::BarcodeReader::new("YOUR-LICENSE-KEY")?;

// Image or PDF file (all pages)
for barcode in reader.decode_file("label.png")? {
    println!("{}: {}", barcode.format, barcode.text);
    println!("corners: {:?}", barcode.points);
}

// Raw RGB888 frame, e.g. from a camera
let results = reader.decode_buffer(&rgb_bytes, width, height)?;
```

`decode_file` / `decode_buffer` return an empty list when no barcode is found;
failures (bad license, unreadable file, SDK errors) are reported as
`dbr::Error`. Supported file inputs include JPEG, PNG, TIFF, BMP, GIF, WebP and
PDF.

Run the bundled example:

```bash
DYNAMSOFT_LICENSE_KEY=your-key cargo run --example decode -- path/to/image.png
```

Run the tests (skipped gracefully when no key is set):

```bash
DYNAMSOFT_LICENSE_KEY=your-key cargo test
```

## How the native SDK is obtained

The crate itself stays small (~1.5 MB): it bundles the C++ bridge source, the
SDK headers and the Windows import libraries. The native runtime (DLL / so /
dylib, preset templates and decoding models, ~40–70 MB per platform) is too
large for crates.io, so `build.rs` fetches it automatically:

1. **Default — official PyPI wheel.** On first build the crate downloads the
   platform wheel of
   [`dynamsoft-capture-vision-bundle` 3.6.5000](https://pypi.org/project/dynamsoft-capture-vision-bundle/),
   verifies its SHA-256, and caches it under your user cache directory
   (`%LOCALAPPDATA%\dbr-rs` on Windows, `~/.cache/dbr-rs` on Linux/macOS).
   The native libraries in the wheel are byte-identical to the C++ SDK
   distribution. Subsequent builds are offline.
2. **Local override — `DCV_DIR`.** Set `DCV_DIR` to a local Dynamsoft Capture
   Vision 3.6.50 directory laid out like the
   [sample repository's `dcv/`](https://github.com/yushulx/cmake-cpp-barcode-qrcode-mrz/tree/main/dcv)
   folder (`lib/win`, `lib/linux/x64`, `lib/linux/arm64`, `lib/mac`,
   `resource/Templates`, `resource/Models`) to skip the download entirely.

Either way the runtime libraries and resources (`Templates/`, `Models/`,
`*.data`) are placed next to the built binaries, tests and examples, so
`cargo run` / `cargo test` work out of the box on every supported platform.

### Deploying a standalone binary

Copy the executable together with **all** SDK runtime files from the same
`target/<profile>` directory (libraries, `Templates/`, `Models/`, `*.data`)
into one folder:

- **Windows** — works as-is; the loader searches the executable's directory.
- **macOS** — works as-is; the build rewrites the dylib install names to
  `@loader_path`, so libraries resolve relative to the executable.
- **Linux** — the SDK libraries use plain SONAMEs, so the executable also
  needs an rpath pointing at its own directory. Add this to your project's
  `.cargo/config.toml`:

  ```toml
  [target.x86_64-unknown-linux-gnu]
  rustflags = ["-C", "link-arg=-Wl,-rpath,$ORIGIN"]

  [target.aarch64-unknown-linux-gnu]
  rustflags = ["-C", "link-arg=-Wl,-rpath,$ORIGIN"]
  ```

  Alternatively, set `LD_LIBRARY_PATH` to the deployment folder at runtime.

## Version mapping

| Crate version | Barcode Reader | Capture Vision |
| --- | --- | --- |
| 11.6.50 | 11.6.50 | 3.6.50 |

## License

The Rust wrapper and bridge code are MIT licensed (see [LICENSE](LICENSE)).
The Dynamsoft SDK components (headers, import libraries and the downloaded
runtime) are proprietary and subject to Dynamsoft's license terms; a valid
license key is required at runtime.