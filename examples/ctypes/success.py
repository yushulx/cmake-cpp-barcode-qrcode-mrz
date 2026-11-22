"""
Decode every barcode in test.png with Dynamsoft Capture Vision, using nothing
but Python ctypes — no C++ compiler, no CMake, no bridge library.

Works on Windows, Linux and macOS (see dcv.py for the per-ABI symbol handling).

Usage:
    python success.py [image_path]

The license key is read from ../../license-key.txt; edit LICENSE_KEY below or
pass your own key if the file is missing.
"""

import os
import sys

from dcv import CaptureVisionRouter

HERE = os.path.dirname(os.path.abspath(__file__))
SDK_ROOT = os.path.join(HERE, os.pardir, os.pardir, "dcv")
LICENSE_FILE = os.path.join(HERE, os.pardir, os.pardir, "license-key.txt")
LICENSE_KEY = "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3ODI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ=="

DEFAULT_IMAGE = os.path.join(HERE, "test.png")


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IMAGE

    license_key = LICENSE_KEY
    if os.path.isfile(LICENSE_FILE):
        license_key = open(LICENSE_FILE, "r", encoding="utf-8").read().strip()

    with CaptureVisionRouter(SDK_ROOT, license_key) as router:
        print(f"Dynamsoft Capture Vision v{router.version}\n")

        result = router.capture(image, "ReadBarcodes_Default")
        try:
            if result.error_code != 0:
                print(f"Capture failed ({result.error_code}): {result.error_string}")
                return 1

            decoded = result.get_decoded_barcodes_result()
            if not decoded:
                print("No barcode result.")
                return 0
            try:
                count = decoded.items_count
                print(f"Found {count} barcode(s) in {os.path.basename(image)}:\n")
                for i in range(count):
                    item = decoded.get_item(i)
                    print(f"[{i + 1:2d}] {item.format_string}")
                    print(f"     text: {item.text}")
                    print(f"     confidence: {item.confidence}")
                    print(f"     location: {item.location}")
            finally:
                decoded.release()
        finally:
            result.release()
    return 0


if __name__ == "__main__":
    sys.exit(main())
