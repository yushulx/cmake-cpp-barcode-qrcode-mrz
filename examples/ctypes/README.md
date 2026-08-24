# Calling Dynamsoft Capture Vision from Python ctypes — no C++ build

Decode barcodes with the Dynamsoft Capture Vision C++ shared libraries directly
from Python. There is **no CMake project, no bridge DLL, and no compiler** in
this example — `dcv.py` resolves the exported C++ symbols by their ABI-specific
mangled names and dispatches the virtual methods of the returned objects through
their vtables.

## Prerequisites

- Python 3.8+
- The Dynamsoft Capture Vision SDK at `../../dcv/` (already in this repo).
  On Linux/macOS, set the library search path before launching Python so the
  dependency libraries resolve:
  - Linux: `export LD_LIBRARY_PATH=$PWD/../../dcv/lib/linux/x64:$LD_LIBRARY_PATH`
  - macOS: `export DYLD_LIBRARY_PATH=$PWD/../../dcv/lib/mac:$DYLD_LIBRARY_PATH`
- A license key in `../../license-key.txt` — get a
  [30-day free trial license](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform)

## Usage

```bash
python success.py           # decodes test.png, prints 18 barcodes
python success.py <image>   # decode your own image
python multithreading.py    # decodes on a background thread, shows GIL release
```

## How it works

1. **Exported C++ symbols** — the shared libraries export `CLicenseManager`
   methods, the `CCaptureVisionRouter` constructor/destructor, `Capture` and
   `GetVersion` under ABI-specific mangled names:
   - Windows (MSVC): `?Capture@CCaptureVisionRouter@cvr@dynamsoft@@QEAAPEAVCCapturedResult@23@PEBD0@Z`
   - Linux/macOS (Itanium): `_ZN9dynamsoft3cvr20CCaptureVisionRouter7CaptureEPKcS3_`
2. **Placement new** — the router is constructed into a Python-allocated
   buffer; no C++ `new` is needed.
3. **vtable dispatch** — result objects (`CCapturedResult`,
   `CDecodedBarcodesResult`, `CBarcodeResultItem`) are pure-virtual classes
   with no exported method names, so methods are invoked by reading the
   function pointer at a fixed vtable slot. Slot numbers differ between the
   MSVC ABI (one destructor slot) and the Itanium ABI (two destructor slots),
   and are defined at the top of `dcv.py`.
4. **Resource layout** — the SDK finds `Templates/` and `Models/` relative to
   the directory that hosts the router library, so `dcv.py` copies them there
   from `dcv/resource/` on first use.
5. **GIL release** — ctypes releases the GIL around each native call, so a
   decode running on a background thread does not block the rest of your
   Python code.

## Files

| File                | Purpose                                                          |
|---------------------|------------------------------------------------------------------|
| `dcv.py`            | ctypes wrapper: per-ABI symbol names, vtable slots, result classes |
| `success.py`        | Working demo: decode `test.png` (18 barcodes of 18 formats)       |
| `multithreading.py` | Demo: run decode on a background thread while the GIL stays free  |
| `test.png`          | Test image containing 18 barcodes                                 |

