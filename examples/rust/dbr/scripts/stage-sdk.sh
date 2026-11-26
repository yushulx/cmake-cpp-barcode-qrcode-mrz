#!/usr/bin/env bash
# Stages the SDK files a published `dbr` crate needs, so that the repository
# does not have to carry a second copy of them.
#
# `cargo publish` packages the crate directory only, so the headers and the
# Windows import libraries have to sit inside `examples/rust/dbr` at that
# moment. They are gitignored: run this script right before `cargo publish`,
# and `git clean -fd examples/rust/dbr/include examples/rust/dbr/lib`
# afterwards.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dcv="$(cd "$here/../../../dcv" && pwd)"
version="$(sed -n 's/^#define DBR_VERSION "\(.*\)"/\1/p' "$dcv/include/DynamsoftBarcodeReader.h")"

if [ -z "$version" ]; then
  echo "error: cannot read DBR_VERSION from $dcv/include/DynamsoftBarcodeReader.h" >&2
  exit 1
fi

mkdir -p "$here/include" "$here/lib/win"

cp "$dcv"/include/*.h "$here/include/"

# Only the modules the bridge links against; the rest of the SDK is fetched at
# build time from the PyPI wheel.
for lib in DynamsoftCaptureVisionRouter DynamsoftBarcodeReader DynamsoftCore \
           DynamsoftLicense DynamsoftUtility; do
  cp "$dcv/lib/win/${lib}x64.lib" "$here/lib/win/"
done

echo "Staged Dynamsoft Barcode Reader $version into $(basename "$here")/"
echo "  include/  $(ls "$here/include" | wc -l | tr -d ' ') headers"
echo "  lib/win   $(ls "$here/lib/win" | wc -l | tr -d ' ') import libraries"
