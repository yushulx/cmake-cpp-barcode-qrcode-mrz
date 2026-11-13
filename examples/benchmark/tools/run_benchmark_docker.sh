#!/usr/bin/env bash
# Reproduce the three-way BarBeR benchmark (ZXing-C++, DBR, Scandit Linux) in a
# linux/amd64 container. Scandit's SDK is Linux-only and its .deb is not
# redistributable, so it must be present under third_party/ before building.
#
# Usage (Git Bash on Windows):
#   tools/run_benchmark_docker.sh smoke
#   tools/run_benchmark_docker.sh full
#   tools/run_benchmark_docker.sh shell
set -euo pipefail

BENCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_DIR="$(cd "$BENCH_DIR/../.." && pwd)"
IMAGE_NAME="${BENCH_IMAGE:-barber-benchmark:scandit-8.7.1}"
DATASET_IMAGES="${BARBER_IMAGES:-D:/images/BarBeR_Dataset/BarBeR - Dataset/dataset/images}"
ACTION="${1:-smoke}"

winpath() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi
}

if [[ ! -f "$BENCH_DIR/scandit-license-key.txt" ]]; then
    echo "Missing $BENCH_DIR/scandit-license-key.txt (excluded from Git)" >&2
    exit 1
fi

docker build --platform linux/amd64 -f "$BENCH_DIR/Dockerfile" -t "$IMAGE_NAME" "$BENCH_DIR"

MOUNTS=(
    --platform linux/amd64
    -v "$(winpath "$REPO_DIR")":/workspace/repo
    -v "$(winpath "$DATASET_IMAGES")":/images:ro
    -w /workspace/repo/examples/benchmark
    -e "DYNAMSOFT_LICENSE_KEY=$(tr -d '\r\n' < "$REPO_DIR/license-key.txt")"
    -e "SCANDIT_LICENSE_KEY=$(tr -d '\r\n' < "$BENCH_DIR/scandit-license-key.txt")"
)

# The Linux DCV build resolves model files relative to the directory the .so
# files were linked from, not next to the executable. Stage a private copy of
# the SDK inside the container with Models/ beside the libraries. Its preset
# template also needs explicit CaptureVisionModelOptions (see
# configs/dbr_preset_templates_linux.json), which fixes "Model file is not found".
BUILD='set -e
B=/workspace/repo/examples/benchmark
rm -rf /tmp/dcv
cp -a /workspace/repo/dcv /tmp/dcv
cp -a /tmp/dcv/resource/Models /tmp/dcv/lib/linux/x64/Models
cmake -S "$B" -B /tmp/build -DCMAKE_BUILD_TYPE=Release -DDCV_ROOT=/tmp/dcv
cmake --build /tmp/build --target barcode_benchmark -j "$(nproc)"'

common_args="--images /images
             --license-key-file /workspace/repo/license-key.txt
             --scandit-license-key-file \$B/scandit-license-key.txt
             --decoders zxing-cpp,dynamsoft-dbr,scandit-linux
             --zxing-config \$B/configs/zxing_all_supported.json
             --scandit-config \$B/configs/scandit_all_supported.json
             --dbr-config \$B/configs/dbr_preset_templates_linux.json
             --dbr-template ReadBarcodes_Default
             --scandit-data-dir /var/tmp/scandit
             --repetitions 1"

case "$ACTION" in
    shell)
        exec docker run --rm -it "${MOUNTS[@]}" "$IMAGE_NAME" bash -c "$BUILD; exec bash"
        ;;
    smoke|full)
        if [[ "$ACTION" == "smoke" ]]; then
            MANIFEST=manifests/smoke_manifest.jsonl
            OUTPUT=results/smoke_linux
            SUBCOMMAND=smoke
        else
            MANIFEST=manifests/benchmark_manifest.jsonl
            OUTPUT=results/full_linux
            SUBCOMMAND=run
        fi
        RUN="cd /tmp/build
./barcode_benchmark $SUBCOMMAND \
  $common_args \
  --manifest \$B/$MANIFEST \
  --output \$B/$OUTPUT"
        docker run --rm "${MOUNTS[@]}" "$IMAGE_NAME" bash -c "$BUILD
$RUN"
        ;;
    *)
        echo "unknown action: $ACTION (use smoke, full or shell)" >&2
        exit 2
        ;;
esac
