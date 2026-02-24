# Command-Line Barcode & QR Code Reader (C++)

This example shows how to build and run a C++ command-line app that reads barcodes/QR codes from:

- A single image/PDF/TIFF file
- A directory (processed recursively)
- Multiple paths passed in one command

The app is implemented in [`main.cpp`](./main.cpp) and uses **Dynamsoft Capture Vision Router** with the `PT_READ_BARCODES` preset template.

## Supported input formats

The sample accepts these file extensions:

- `.jpg`, `.jpeg`, `.png`, `.bmp`, `.gif`
- `.tif`, `.tiff`
- `.pdf`

## Build

From this folder (`examples/command-line`):

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

After building, the executable is generated in `build/`:

- Linux/macOS: `./main`
- Windows: `./Release/main.exe` (or `./main.exe` depending on generator)

> During post-build steps, required Dynamsoft libraries, `Templates/`, and `Models/` are copied next to the executable automatically.

## Run

### 1) Process a single file

```bash
./main ../single_page.tif
```

### 2) Process a directory recursively

```bash
./main ../../images
```

### 3) Process multiple paths in one command

```bash
./main ../single_page.tif ../multi_page.tif ../../images
```

### 4) Interactive mode

If no arguments are provided, the app enters interactive mode:

```bash
./main
```

Then type a full file or directory path. Enter `q` to quit.

## Output behavior

- **Single file**: prints detailed results (format, text, and quadrilateral points per barcode).
- **Directory**: prints per-file summary and directory statistics.
- **Multiple inputs**: prints per-input results and an overall summary.

## License key

The demo initializes a trial license in source code. For production use, replace it with your own key in `main.cpp`:

```cpp
CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
```

Get a trial license here:

- https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform

## Sample files in this folder

- `single_page.tif`
- `multi_page.tif`
- `multi_page.pdf`
- `test.pdf`

You can use these files to quickly validate decoding.
