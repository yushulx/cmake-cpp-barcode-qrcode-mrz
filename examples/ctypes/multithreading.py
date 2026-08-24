"""
Decode a barcode image on a background thread and keep the GIL free.

Python's Global Interpreter Lock (GIL) normally stops more than one thread from
running Python bytecode at a time. ctypes lifts that limit *for the duration of
a native call*: while a Dynamsoft Capture Vision ``Capture`` is running inside
the shared library, the GIL is released, so the rest of your Python code keeps
executing on another thread.

This runs the decode on a worker thread and, in parallel, measures how much
Python work the main thread can still do — proving the GIL is released while
the C++ decoder runs.

Usage:
    python multithreading.py [image_path]
"""

import os
import sys
import threading
import time

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

    # Create the router here; it is shared with the worker thread.
    router = CaptureVisionRouter(SDK_ROOT, license_key)
    print(f"Dynamsoft Capture Vision v{router.version}\n")

    result = {}

    def decode_worker():
        # This call runs in native C++ and releases the GIL while it runs.
        captured = router.capture(image, "ReadBarcodes_Default")
        try:
            decoded = captured.get_decoded_barcodes_result()
            result["count"] = decoded.items_count if captured.error_code == 0 else -1
            result["error"] = captured.error_string
        finally:
            captured.release()

    print(f"Decoding {os.path.basename(image)} on a background thread ...")
    thread = threading.Thread(target=decode_worker)
    thread.start()

    # While the decoder is busy, prove the main thread can still run Python.
    counter = 0
    start = time.perf_counter()
    while thread.is_alive():
        counter += 1
    elapsed = time.perf_counter() - start
    thread.join()

    print(f"  decoded {result.get('count')} barcode(s)")
    if result.get("error"):
        print(f"  error: {result['error']}")
    print(f"  main thread executed {counter:,} Python operations in {elapsed:.3f}s "
          f"while the decode ran")
    print("  -> the GIL is released during the native Capture call")
    router.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
