#!/usr/bin/env python3
"""Read barcodes and QR codes from a WebP image.

Usage:
    main.py <image-file> [license-file]

The license key comes from <license-file> when given, otherwise from the
DYNAMSOFT_LICENSE_KEY environment variable, otherwise from the bundled one-day
public trial key.

Dynamsoft Capture Vision does not decode WebP itself (it reports error -10006
"The file type is not supported."), so the image is decoded with OpenCV and
handed to CaptureVisionRouter as a BGR buffer. Everything else - localization,
decoding, and the four-point result - comes from the Dynamsoft SDK
(dynamsoft-barcode-reader-bundle 11.6.5000).
"""

import os
import sys

import cv2

from dynamsoft_barcode_reader_bundle import (
    CaptureVisionRouter,
    EnumErrorCode,
    EnumImagePixelFormat,
    EnumPresetTemplate,
    ImageData,
    LicenseManager,
)

TRIAL_LICENSE_KEY = (
    "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6"
    "IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ=="
)
LICENSE_OK_CODES = (
    EnumErrorCode.EC_OK,
    EnumErrorCode.EC_LICENSE_WARNING,
    EnumErrorCode.EC_LICENSE_CACHE_USED,
)


def resolve_license(argv):
    """Prefer the explicit license file, then the environment, then the trial key."""
    if len(argv) > 2:
        with open(argv[2], encoding="utf-8") as stream:
            return stream.read().strip()
    return os.environ.get("DYNAMSOFT_LICENSE_KEY", TRIAL_LICENSE_KEY)


def load_image(path):
    """Decode the WebP file with OpenCV, which ships libwebp support."""
    image = cv2.imread(path, cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"Cannot read image: {path}")
    return image


def to_image_data(image):
    """Wrap a BGR (or grayscale) numpy array as Dynamsoft ImageData."""
    height, width = image.shape[:2]
    if image.ndim == 3:
        return ImageData(
            image.tobytes(), width, height, width * image.shape[2], EnumImagePixelFormat.IPF_BGR_888
        )
    return ImageData(image.tobytes(), width, height, width, EnumImagePixelFormat.IPF_GRAYSCALED)


def print_results(items):
    for index, item in enumerate(items):
        points = item.get_location().points
        print(f"Index: {index}")
        print(f"     Barcode format: {item.get_format_string()}")
        print(f"     Barcode value: {item.get_text()}")
        print("     Points: " + " ".join(f"({point.x}, {point.y})" for point in points))
        print("-----------------------------------")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    license_key = resolve_license(sys.argv)
    error_code, error_message = LicenseManager.init_license(license_key)
    print(f"License status: {error_code} {error_message}")
    if error_code not in LICENSE_OK_CODES:
        return 1

    try:
        image_data = to_image_data(load_image(sys.argv[1]))
    except ValueError as error:
        print(error)
        return 1

    router = CaptureVisionRouter()
    template = os.environ.get("DBR_TEMPLATE", EnumPresetTemplate.PT_READ_BARCODES.value)
    result = router.capture(image_data, template)
    if result.get_error_code() != EnumErrorCode.EC_OK:
        print(f"Capture failed: {result.get_error_code()} {result.get_error_string()}")
        return 1

    barcode_result = result.get_decoded_barcodes_result()
    items = barcode_result.get_items() if barcode_result is not None else []
    if not items:
        print("No barcode found.")
        return 0

    print_results(items)
    return 0


if __name__ == "__main__":
    sys.exit(main())
