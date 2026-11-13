# C++ Barcode Reader Benchmark

This project measures barcode decoding accuracy and speed for **ZXing-C++**, **Dynamsoft Barcode Reader** and the **Scandit Barcode Scanner SDK for Linux** on the public [BarBeR dataset](https://ditto.ing.unimore.it/barber/). All three readers receive the same RGB888 pixels and are evaluated with the same ground truth matching rules.

https://github.com/user-attachments/assets/c2996e2c-e05e-4d2a-b1b4-f41b0d71c6c6

## Dependencies

- CMake 3.16 or later
- A C++20 compiler
- Dynamsoft Capture Vision SDK in `../../dcv`
- A valid Dynamsoft Barcode Reader license
- Python 3 for result validation and report generation
- **Scandit Linux SDK 8.7.1** (Linux/amd64 only) and a **Linux** trial key
- Docker (Linux containers) to run the three-way benchmark

The `zxing-cpp` directory is a Git submodule pinned at v3.1.1. The bundled Dynamsoft Capture Vision SDK is 3.6.50 / DBR 11.6.50, matching the 11.6.5000 release used by the Python and JavaScript companions.

### Scandit SDK and license

Scandit's Barcode Scanner SDK for Linux is a **C API** shipped as a Debian package from the [Scandit dashboard](https://ssl.scandit.com/dashboard/downloads) (`SDK Installation`). It is **not redistributable**, so it is not committed. Download `scandit-barcodescanner-linux_8.7.1.zip`, then place `libscandit-barcodescanner-8.7.1-amd64.deb` in `third_party/scandit-barcodescanner-linux_8.7.1/`. `Dockerfile` installs it into the container.

Put the Linux trial key in `scandit-license-key.txt` in this directory (excluded from Git), or set `SCANDIT_LICENSE_KEY`. The key is Linux-only, must be created for the Linux platform, and requires network access for activation. Multi-scan needs the "tracking" license feature; when it is missing the adapter falls back to single-code scanning and prints the reason.

## Get the Source

Clone the parent repository with submodules.

```powershell
git clone --recurse-submodules https://github.com/yushulx/cmake-cpp-barcode-qrcode.git
```

Initialize the submodule in an existing checkout.

```powershell
git submodule update --init --recursive
```

## Build (native, two decoders)

The Scandit adapter is compiled only when the Linux SDK headers and library are found. On Windows and macOS the project builds ZXing-C++ and DBR only.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target barcode_benchmark benchmark_tests
ctest --test-dir build -C Release --output-on-failure
```

CMake builds ZXing-C++ from the checked-out submodule and copies the required Dynamsoft libraries, templates, and models next to the executable.

## Build and run with Docker (three decoders)

```bash
tools/run_benchmark_docker.sh smoke   # 10 images, sanity check
tools/run_benchmark_docker.sh full    # all 7,894 images
tools/run_benchmark_docker.sh shell   # interactive container
```

Set `BARBER_IMAGES` if the dataset lives elsewhere. The default is `D:/images/BarBeR_Dataset/BarBeR - Dataset/dataset/images`. The script builds `barber-benchmark:scandit-8.7.1`, compiles into `/tmp/build` inside the container, and writes results to `results/full_linux/`.

Two Linux-specific details are handled by the script:

1. The Linux DCV build resolves AI models relative to the directory its libraries were **linked from**, not next to the executable. The script stages a private copy of `dcv/` at `/tmp/dcv` with `Models/` beside the libraries.
2. The stock `DBR-PresetTemplates.json` does not declare `CaptureVisionModelOptions` on Linux, which produces `Model file is not found`. `configs/dbr_preset_templates_linux.json` is the same official preset template with all 21 model declarations added, and is passed via `--dbr-config`.

For a faster full run, several processes can share one `--output` directory while working on disjoint manifest slices with `--start-index` / `--end-index`. `appendResult` uses a cross-process lock and skips existing result keys, so the run is resumable and safe to parallelize. The published run used four slices of the same 7,894-image manifest, all with identical config and manifest hashes.

## Prepare the BarBeR Dataset

The expected dataset layout is:

```text
BarBeR - Dataset/
  Annotations/VIA/
  dataset/images/
```

Run the audit before every published benchmark.

```powershell
build/Release/barcode_benchmark.exe audit `
  --images "D:/images/public-barcode-dataset/BarBeR - Dataset/dataset/images" `
  --annotations "D:/images/public-barcode-dataset/BarBeR - Dataset/Annotations/VIA" `
  --output manifests
```

The audit recursively reads every VGG JSON file, validates image availability, validates payload structure, resolves overlapping annotations, and removes exact duplicate images. The generated benchmark manifest contains only unique images with at least one reliable ground truth value.

For the current local dataset, the audit starts from 8,748 image records and 9,818 annotations. It excludes 853 images without reliable ground truth and one exact duplicate image. The final manifest contains 7,894 unique images, 7,894 unique SHA-256 image hashes, and 8,615 ground truth barcode instances. The inventory in `manifests/barber_source_files.json` records every exclusion and the SHA-256 hash of each annotation source.

## Run the Full Benchmark

Store the Dynamsoft license in a local text file that is not committed. This checkout uses `../../license-key.txt` from the repository root.

```powershell
build/Release/barcode_benchmark.exe run `
  --images "D:/images/public-barcode-dataset/BarBeR - Dataset/dataset/images" `
  --manifest manifests/benchmark_manifest.jsonl `
  --output results/full `
  --dbr-template ReadBarcodes_Default `
  --zxing-config configs/zxing_all_supported.json `
  --license-key-file "../../license-key.txt" `
  --repetitions 1
```

Use `--decoders` to select a subset, for example `--decoders zxing-cpp,dynamsoft-dbr,scandit-linux`. Add `--scandit-license-key-file`, `--scandit-config` and `--scandit-data-dir` when the Scandit adapter is enabled. `DYNAMSOFT_LICENSE_KEY` and `SCANDIT_LICENSE_KEY` override the key files.

The command is resumable. Each result key contains the sample ID, decoder, and repetition number. Existing keys are skipped safely. The raw stream is stored in `results.jsonl` so a long run can append one complete record at a time. A complete `results.json` package is also written for tools that prefer a single JSON document.

Decode timing starts immediately before the SDK decode call and ends immediately after it returns. Image loading, matching, JSON serialization, console output, and report generation are excluded. For Scandit, only `sc_recognition_context_process_frame` is timed; the surrounding per-image frame-sequence calls and result retrieval are outside the timed region. Decoder order is deterministically shuffled for every image and repetition.

## Validate Results

```powershell
python tools/validate_results.py `
  --results results/full_linux/results.jsonl `
  --summary results/full_linux/summary.json `
  --expected-images 7894 `
  --expected-ground-truth 8615 `
  --expected-repetitions 1
```

`--expected-ground-truth 8615` is the audited annotation count stored in each record. Scoring excludes 204 `^` placeholders, so recall uses 8,411. A complete three-decoder result file contains 23,682 unique decoder records.

## Benchmark Results

The full run uses one repetition on 7,894 unique BarBeR images in a single Ubuntu 24.04 linux/amd64 container. Recall is correct ground truth matches divided by 8,411 scored ground truth instances. Every annotation ends in exactly one of correct, wrong read or no read, and the three sum to 8,411 for all three decoders.

| Decoder | Correct | Recall | Wrong reads | No read | Mean decode time | Median decode time | P95 decode time |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dynamsoft Barcode Reader 11.6.50.29329 | **8,050 / 8,411** | **95.71%** | 85 (1.01%) | **276 (3.28%)** | 444.05 ms | 261.60 ms | 1422.82 ms |
| Scandit Barcode Scanner SDK 8.7.1 (Linux) | 7,776 / 8,411 | 92.45% | **73 (0.87%)** | 562 (6.68%) | 105.73 ms | 89.78 ms | **229.06 ms** |
| ZXing-C++ 3.1.1 | 6,035 / 8,411 | 71.75% | 96 (1.14%) | 2,280 (27.11%) | **70.90 ms** | **38.51 ms** | 265.90 ms |

> **No precision column.** Precision would be correct predictions over all predictions, and its
> denominator is the predictions rather than the annotations. On BarBeR that ratio is dominated by
> the fact that the annotations are curated rather than exhaustive: a second barcode on the same
> photograph, or a symbology the dataset does not cover — DBR's Linux preset template detects
> Pharmacode One-Track on images BarBeR has no annotation for — is charged as a false positive. It
> measures annotation coverage as much as decoder error, so it is not reported.

Scandit read 1,741 more ground truth barcodes than ZXing-C++ and 274 fewer than DBR, a gap of 3.26 percentage points in recall behind DBR and 20.70 points ahead of ZXing-C++. It also produced the fewest wrong reads of the three (73, against DBR's 85 and ZXing-C++'s 96); what separates it from DBR is the no-read column, 562 against 276.

Unsupported-format counts are DBR 0, Scandit 23, ZXing-C++ 227. Scandit's 23 are the JAPAN_POST (17) and POSTNET (6) annotations it has no symbology for; ZXing-C++ adds KIX, IATA_2_OF_5 and USPS Intelligent Mail to that. DBR supports every BarBeR symbology, so its whole no-read column is barcodes it did not return. Six DBR frames returned 'Operation timeout' and are counted as no-reads (`decoder_error`), never as wrong reads.

**Timing caveat:** four worker processes handled disjoint manifest slices on 16 logical processors, so absolute decode times include CPU contention. Compare readers only within this run; do not compare these times with the earlier single-process Windows measurement. The Linux DCV build runs the official preset template with deep-learning models and is a different binary from that Windows build.

Failures are split into **wrong reads** (`wrong_text + wrong_format`) and **no read** (`not_found + unsupported_format + decoder_error`), so correct + wrong reads + no read equals the 8,411 scored ground-truth instances. A prediction that no annotation claims is not a fourth outcome — the harness cannot tell an unannotated barcode from a hallucination on BarBeR — so it is set aside rather than charged to an instance. `image_all_read_rate` is retained in `summary.json` only as a diagnostic.

## Generate the Report

```powershell
python tools/generate_html_report.py `
  --inventory manifests/barber_source_files.json `
  --environment configs/benchmark_environment.json `
  --results results/full_linux/results.jsonl `
  --matching-analysis results/full_linux/matching_analysis.json `
  --summary results/full_linux/summary.json `
  --output report/index.html
```

The HTML report contains the measured summary, a decoder comparison table, method disclosure, full environment details, dataset audit, and per-image raw records.

Regenerate the cover image, poster, and slide video:

```powershell
python tools/generate_benchmark_media.py `
  --inventory manifests/barber_source_files.json `
  --summary results/full_linux/summary.json `
  --output report/media
```

## Matching Rules

Protocol v2. v1 differed in three rules; every record in `results/full_linux/` was re-scored with the rules below and carries `"protocol": "protocol-v2"`.

- Ground truth and predictions are matched one to one as multisets
- The key is canonical barcode format plus exact normalized payload
- UPC-A and the equivalent zero-prefixed EAN-13 value are treated as equal
- CODE_39 start/stop asterisks are stripped before scoring
- CODE_128 / GS1-128 leading `{GS}`, `{FNC1}`, and ASCII GS markers are stripped before scoring
- **v2:** GS1 application identifiers printed in parentheses are unwrapped before comparison, so a reader that returns `(90)85320` matches the annotation `9085320`. Scoped to CODE_128 / GS1_128
- **v2:** a `ROYAL_MAIL` payload that is the annotation's prefix, one character shorter, counts as the same reading. The Linux DBR build returns 10 of the 11 characters; the trailing character is not reconstructible from the payload with the tables available here, so the rule is limited to this one symbology and to a single missing character
- **v2:** `CODE_128` and `GS1_128` are one format family. GS1-128 is Code 128 carrying GS1 data — same bars, same code sets, same check character, with an FNC1 in the first data position as the only difference (AIM separates them as ]C0 / ]C1). The readers report that through three different channels: DBR prefixes the payload with `{GS}`, ZXing exposes the AIM identifier, Scandit sets a GS1 carrier flag. Scoring the name a vendor happens to use charged Scandit with 172 wrong formats and DBR / ZXing with 4 each; the family rule removes that asymmetry and payloads still have to match exactly
- **v2:** the canonical format table maps the underscored BarBeR labels `ROYAL_MAIL`, `IATA_2_OF_5` and `USPS_INTELLIGENT_MAIL` to the spellings the decoders use. v1 stripped the underscores before lookup, so those names matched no decoders' supported set and 192 readable annotations were scored as unsupported by all three readers
- HTML entities, trailing newlines, and a leading `\000001` escape are normalized before scoring
- Ground truth payload `^` is treated as an unreliable placeholder and excluded from scoring
- DBR `CODE39EXTENDED` output is treated as `CODE_39`
- Scandit `ean13upca` covers both EAN-13 and UPC-A; a GS1 carrier flag on `code128` is reported as `GS1_128`
- Scandit 8.7.1 exposes no `JAPAN_POST` or `POSTNET` symbology, so those ground truth instances are scored as `unsupported_format`
- Scandit payloads are surfaced as UTF-8 text (invalid bytes are mapped from Latin-1) and the untouched raw bytes are retained as hex
- Barcode location is not part of the score
- Unsupported formats remain visible in coverage-adjusted metrics
- Decoder errors and input pipeline errors are never counted as no-read results

To re-score an existing `results.jsonl` after a matching-rule change, without re-running the decoders:

```powershell
cmake --build build --config Release --target rematch_results
build/Release/rematch_results.exe --results results/full_linux/results.jsonl --output results/full_linux
```

## Reproducibility

Raw records include the decoder name, runtime version, config hash, manifest hash, repetition number, image load time, decode time, predictions, matches, and explicit errors. `results.jsonl` is the append-only raw stream. `results.json` contains the same records plus the summary in one JSON document. Generated benchmark manifests, results, reports, license files and the Scandit SDK are excluded from Git.

The measured machine and build settings are recorded in `configs/benchmark_environment.json`.

## Blog
[Benchmark Barcode Reading in C++ with ZXing-C++ and Dynamsoft Barcode Reader](https://www.dynamsoft.com/codepool/benchmark-barcode-reading-cpp-zxing-dynamsoft-barcode-reader.html)
