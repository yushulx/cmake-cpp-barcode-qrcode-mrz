# C++ Barcode Reader Benchmark

This project measures barcode decoding accuracy and speed for ZXing-C++ and Dynamsoft Barcode Reader on the public [BarBeR dataset](https://ditto.ing.unimore.it/barber/). Both readers receive the same RGB888 pixels and are evaluated with the same ground truth matching rules.

https://github.com/user-attachments/assets/d38375b7-f57b-448e-a378-166df17ad96a

## Dependencies

- CMake 3.16 or later
- A C++20 compiler
- Dynamsoft Capture Vision SDK in `../../dcv`
- A valid Dynamsoft Barcode Reader license
- Python 3 for result validation and report generation

The `zxing-cpp` directory is a Git submodule.

## Get the Source

Clone the parent repository with submodules.

```powershell
git clone --recurse-submodules https://github.com/yushulx/cmake-cpp-barcode-qrcode.git
```

Initialize the submodule in an existing checkout.

```powershell
git submodule update --init --recursive
```

Update ZXing-C++ when a newer revision is needed.

```powershell
git -C zxing-cpp fetch origin
git -C zxing-cpp checkout origin/master
```

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target barcode_benchmark benchmark_tests
ctest --test-dir build -C Release --output-on-failure
```

CMake builds ZXing-C++ from the checked-out submodule and copies the required Dynamsoft libraries, templates, and models next to the executable.

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

The command is resumable. Each result key contains the sample ID, decoder, and repetition number. Existing keys are skipped safely. The raw stream is stored in `results.jsonl` so a long run can append one complete record at a time. A complete `results.json` package is also written for tools that prefer a single JSON document.

To compare a different DBR preset, pass `--dbr-template ReadBarcodes_SpeedFirst` or `--dbr-template ReadBarcodes_ReadRateFirst`.

Decode timing starts immediately before the SDK call and ends immediately after it returns. Image loading, matching, JSON serialization, console output, and report generation are excluded. Decoder order is deterministically shuffled for every image and repetition.

## Validate Results

```powershell
python tools/validate_results.py `
  --results results/full/results.jsonl `
  --summary results/full/summary.json `
  --expected-images 7894 `
  --expected-ground-truth 8615 `
  --expected-repetitions 1
```

A complete result file contains 15,788 unique decoder records.

## Benchmark Results

The current full run uses one repetition on 7,894 unique BarBeR images. Recall is calculated as correct ground truth matches divided by 8,615 eligible ground truth instances. Precision is calculated as correct predictions divided by evaluated predictions, where evaluated predictions are `correct + wrong_text + wrong_format + extra_result`.

| Decoder | Correct | Recall | Precision | Image all-read rate | Mean decode time | Median decode time | P95 decode time |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dynamsoft Barcode Reader 11.4.20.7177 | 7,444 / 8,615 | 86.41% | 91.44% | 86.57% | 70.08 ms | 44.99 ms | 208.51 ms |
| ZXing-C++ 3.1.0 | 5,855 / 8,615 | 67.96% | 93.17% | 67.79% | 74.09 ms | 44.34 ms | 250.22 ms |

DBR read 1,589 more ground truth barcodes in this run and improved recall by 18.44 percentage points. ZXing-C++ had 1.73 percentage points higher precision. DBR's mean decoder call was 70.08 ms versus 74.09 ms for ZXing-C++, about 5.4% lower in this run. The matching analysis found no remaining errors caused only by UPC-A or EAN-13 leading zero differences. DBR `CODE39EXTENDED` output is treated as `CODE_39` when the payload matches.

## Generate the Report

```powershell
python tools/generate_html_report.py `
  --inventory manifests/barber_source_files.json `
  --environment configs/benchmark_environment.json `
  --results results/full/results.jsonl `
  --results-json results/full/results.json `
  --matching-analysis results/full/matching_analysis.json `
  --summary results/full/summary.json `
  --output report/index.html
```

The HTML report contains the measured summary, method disclosure, dataset audit, matching analysis, and per-image raw records.

## Matching Rules

- Ground truth and predictions are matched one to one as multisets
- The key is canonical barcode format plus exact normalized payload
- UPC-A and the equivalent zero-prefixed EAN-13 value are treated as equal
- DBR `CODE39EXTENDED` output is treated as `CODE_39`
- Barcode location is not part of the score
- Unsupported formats remain visible in coverage-adjusted metrics
- Decoder errors and input pipeline errors are never counted as no-read results

## Reproducibility

Raw records include the decoder name, runtime version, config hash, manifest hash, repetition number, image load time, decode time, predictions, matches, and explicit errors. `results.jsonl` is the append-only raw stream. `results.json` contains the same records plus the summary in one JSON document. Generated benchmark manifests, results, reports, and license files are excluded from Git.

The measured machine and build settings are recorded in `configs/benchmark_environment.json`.
