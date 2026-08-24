"""
Python ctypes bindings for Dynamsoft Capture Vision (DCV) — no C++ build.

Dynamsoft Capture Vision exposes its barcode engine through a C++ class API.
The shared libraries export those C++ methods (under ABI-specific mangled
names), but the pure-virtual result classes have no exported names at all, so
their methods are reached by walking the C++ vtable.

This module:

* resolves the exported C++ symbols for the host ABI,
* constructs ``CCaptureVisionRouter`` with placement new into a Python buffer,
* dispatches the virtual methods of the returned objects through their vtables.

Supported platforms
-------------------
=============  =======================  ===============================
OS             Library directory         Calling convention / ABI
=============  =======================  ===============================
Windows x64    ``dcv/lib/win``           MSVC x64 — 1 destructor vtable slot
Linux x64/arm  ``dcv/lib/linux/<arch>``  Itanium ABI — 2 destructor slots
macOS (arm64)  ``dcv/lib/mac``           Itanium ABI — 2 destructor slots
=============  =======================  ===============================

Resource layout
---------------
The SDK resolves its preset templates and neural models relative to the
*directory that hosts the router library*, not your working directory, so this
module copies ``Templates/`` and ``Models/`` from ``dcv/resource/`` to the
library directory on first use.

Linux/macOS note
----------------
On Linux/macOS the code uses the Itanium C++ ABI: the mangled names differ,
a virtual destructor occupies **two** vtable slots, and a by-value struct
return (``CQuadrilateral``) passes the hidden return buffer before ``this``.
Set ``LD_LIBRARY_PATH`` (Linux) or ``DYLD_LIBRARY_PATH`` (macOS) to the library
directory before launching Python so the dependency ``.so``/``.dylib`` files
resolve.
"""

import ctypes
import os
import platform
import shutil

_IS_WINDOWS = platform.system() == "Windows"
_IS_MAC = platform.system() == "Darwin"
_IS_LINUX = platform.system() == "Linux"


# ---------------------------------------------------------------------------
# Platform resolution
# ---------------------------------------------------------------------------
def _lib_layout(sdk_root):
    """Return (lib_dir, router_lib_name, license_lib_name) for this host."""
    if _IS_WINDOWS:
        return (os.path.join(sdk_root, "lib", "win"),
                "DynamsoftCaptureVisionRouterx64.dll",
                "DynamsoftLicensex64.dll")
    if _IS_MAC:
        return (os.path.join(sdk_root, "lib", "mac"),
                "libDynamsoftCaptureVisionRouter.dylib",
                "libDynamsoftLicense.dylib")
    if _IS_LINUX:
        machine = platform.machine().lower()
        arch = "arm64" if machine in ("aarch64", "arm64") else "x64"
        return (os.path.join(sdk_root, "lib", "linux", arch),
                "libDynamsoftCaptureVisionRouter.so",
                "libDynamsoftLicense.so")
    raise OSError("Unsupported platform: %s" % platform.system())


def _load(path):
    if _IS_WINDOWS:
        return ctypes.WinDLL(path)
    return ctypes.CDLL(path)


def _sym(lib, name):
    """Resolve an exported symbol, raising a clear error if it is missing."""
    try:
        return lib[name]
    except AttributeError:
        raise DCVError(
            f"The symbol '{name}' was not found in {getattr(lib, '_name', lib)!s}. "
            "The C++ exports differ between SDK builds: on macOS the Capture "
            "Vision router methods may not be exported for dynamic lookup. Use "
            "the official Python bundle instead, or verify this SDK build exposes "
            "the CCaptureVisionRouter symbols."
        ) from None


# ---------------------------------------------------------------------------
# Exported symbol names (per ABI)
# ---------------------------------------------------------------------------
if _IS_WINDOWS:
    # MSVC x64 mangled names
    S_CVR_CTOR = r"??0CCaptureVisionRouter@cvr@dynamsoft@@QEAA@XZ"
    S_CVR_DTOR = r"??1CCaptureVisionRouter@cvr@dynamsoft@@QEAA@XZ"
    S_CVR_INIT_SETTINGS_FILE = r"?InitSettingsFromFile@CCaptureVisionRouter@cvr@dynamsoft@@QEAAHPEBDQEADH@Z"
    S_CVR_CAPTURE_FILE = r"?Capture@CCaptureVisionRouter@cvr@dynamsoft@@QEAAPEAVCCapturedResult@23@PEBD0@Z"
    S_LIC_INIT = r"?InitLicense@CLicenseManager@license@dynamsoft@@SAHPEBDQEADH@Z"
    S_CVR_GET_VERSION = r"?GetVersion@CCaptureVisionRouterModule@cvr@dynamsoft@@SAPEBDXZ"
else:
    # Itanium ABI mangled names (Linux and macOS share the mangling; macOS
    # symbols are stored in the Mach-O table with an extra leading '_').
    _ITANIUM = {
        "ctor": "_ZN9dynamsoft3cvr20CCaptureVisionRouterC1Ev",
        "dtor": "_ZN9dynamsoft3cvr20CCaptureVisionRouterD1Ev",
        "init_settings_file":
            "_ZN9dynamsoft3cvr20CCaptureVisionRouter20InitSettingsFromFileEPKcPci",
        "capture_file": "_ZN9dynamsoft3cvr20CCaptureVisionRouter7CaptureEPKcS3_",
        "license_init": "_ZN9dynamsoft7license15CLicenseManager11InitLicenseEPKcPci",
        "get_version": "_ZN9dynamsoft3cvr26CCaptureVisionRouterModule10GetVersionEv",
    }
    if _IS_MAC:
        _ITANIUM = {k: "_" + v for k, v in _ITANIUM.items()}
    S_CVR_CTOR = _ITANIUM["ctor"]
    S_CVR_DTOR = _ITANIUM["dtor"]
    S_CVR_INIT_SETTINGS_FILE = _ITANIUM["init_settings_file"]
    S_CVR_CAPTURE_FILE = _ITANIUM["capture_file"]
    S_LIC_INIT = _ITANIUM["license_init"]
    S_CVR_GET_VERSION = _ITANIUM["get_version"]


# ---------------------------------------------------------------------------
# vtable slot numbers
# ---------------------------------------------------------------------------
# Counted from the class declarations in the DCV headers. MSVC counts one
# destructor slot; the Itanium ABI reserves two (D1 + D0), so every virtual
# method after the destructor is one slot further on non-Windows.
if _IS_WINDOWS:
    SLOT_CAPTURED = {
        "GetErrorCode": 4,
        "GetErrorString": 5,
        "GetItemsCount": 6,
        "Release": 12,
        "GetDecodedBarcodesResult": 13,
    }
    SLOT_DECODED = {
        "GetErrorCode": 4,
        "GetErrorString": 5,
        "GetItemsCount": 6,
        "GetItem": 7,
        "Release": 12,
    }
    SLOT_ITEM = {
        "GetFormatString": 9,
        "GetText": 10,
        "GetLocation": 13,
        "GetConfidence": 14,
    }
else:
    SLOT_CAPTURED = {
        "GetErrorCode": 5,
        "GetErrorString": 6,
        "GetItemsCount": 7,
        "Release": 13,
        "GetDecodedBarcodesResult": 14,
    }
    SLOT_DECODED = {
        "GetErrorCode": 5,
        "GetErrorString": 6,
        "GetItemsCount": 7,
        "GetItem": 8,
        "Release": 13,
    }
    SLOT_ITEM = {
        "GetFormatString": 10,
        "GetText": 11,
        "GetLocation": 14,
        "GetConfidence": 15,
    }


class DCVError(Exception):
    pass


class Quadrilateral(ctypes.Structure):
    """CQuadrilateral: CPoint points[4] (each point is int[2]) plus an int id.

    The trailing ``id`` member makes the struct 36 bytes, which changes how it
    is returned by value (hidden return-buffer pointer).
    """
    _fields_ = [("points", (ctypes.c_int * 2) * 4), ("id", ctypes.c_int)]


# ---------------------------------------------------------------------------
# vtable helpers
# ---------------------------------------------------------------------------
def _vtable_slot(obj, slot):
    vtable = ctypes.cast(ctypes.c_void_p(obj), ctypes.POINTER(ctypes.c_void_p)).contents
    return ctypes.cast(vtable, ctypes.POINTER(ctypes.c_void_p))[slot]


def _call(obj, slot, restype, argtypes=()):
    fn = ctypes.cast(ctypes.c_void_p(_vtable_slot(obj, slot)),
                     ctypes.CFUNCTYPE(restype, ctypes.c_void_p, *argtypes))
    return fn(obj)


def _ensure_runtime_resources(lib_dir, sdk_root):
    """Copy Templates/ and Models/ from dcv/resource next to the libraries."""
    resource_dir = os.path.join(sdk_root, "resource")
    for sub in ("Templates", "Models"):
        dst = os.path.join(lib_dir, sub)
        src = os.path.join(resource_dir, sub)
        if not os.path.isdir(dst) and os.path.isdir(src):
            shutil.copytree(src, dst)


# ---------------------------------------------------------------------------
# CaptureVisionRouter
# ---------------------------------------------------------------------------
class CaptureVisionRouter:
    """Owns a CCaptureVisionRouter instance created via placement new."""

    def __init__(self, sdk_root, license_key):
        self._sdk_root = os.path.abspath(sdk_root)
        self._lib_dir, self._cvr_name, self._lic_name = _lib_layout(self._sdk_root)
        _ensure_runtime_resources(self._lib_dir, self._sdk_root)

        # Windows resolves dependent DLLs from PATH.
        if _IS_WINDOWS:
            os.environ["PATH"] = self._lib_dir + os.pathsep + os.environ.get("PATH", "")

        self._license_dll = _load(os.path.join(self._lib_dir, self._lic_name))
        self._cvr_dll = _load(os.path.join(self._lib_dir, self._cvr_name))

        # 1. CLicenseManager::InitLicense(key, errorMsgBuffer, bufferLen)
        init_license = _sym(self._license_dll, S_LIC_INIT)
        init_license.restype = ctypes.c_int
        init_license.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
        err = ctypes.create_string_buffer(256)
        ret = init_license(license_key.encode("utf-8"), err, len(err))
        if ret != 0:
            raise DCVError(f"InitLicense failed ({ret}): {err.value.decode(errors='replace')}")

        # 2. new CCaptureVisionRouter()  ->  placement new on our buffer
        ctor = _sym(self._cvr_dll, S_CVR_CTOR)
        ctor.restype = None
        ctor.argtypes = [ctypes.c_void_p]
        self._router_mem = ctypes.create_string_buffer(4096)
        self._router = ctypes.addressof(self._router_mem)
        ctor(self._router)

        # 3. InitSettingsFromFile / Capture / GetVersion / destructor
        self._init_settings_file = _sym(self._cvr_dll, S_CVR_INIT_SETTINGS_FILE)
        self._init_settings_file.restype = ctypes.c_int
        self._init_settings_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                             ctypes.c_char_p, ctypes.c_int]

        self._capture_file = _sym(self._cvr_dll, S_CVR_CAPTURE_FILE)
        self._capture_file.restype = ctypes.c_void_p
        self._capture_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

        get_version = _sym(self._cvr_dll, S_CVR_GET_VERSION)
        get_version.restype = ctypes.c_char_p
        get_version.argtypes = []
        self.version = get_version().decode("utf-8")

        self._dtor = _sym(self._cvr_dll, S_CVR_DTOR)
        self._dtor.restype = None
        self._dtor.argtypes = [ctypes.c_void_p]

    def init_settings_from_file(self, template_path):
        err = ctypes.create_string_buffer(1024)
        ret = self._init_settings_file(self._router, template_path.encode("utf-8"),
                                       err, len(err))
        if ret != 0:
            raise DCVError(f"InitSettingsFromFile({template_path}) failed ({ret}): "
                           f"{err.value.decode(errors='replace')}")

    def capture(self, file_path, template_name="ReadBarcodes_Default"):
        ptr = self._capture_file(self._router, file_path.encode("utf-8"),
                                 template_name.encode("utf-8"))
        if not ptr:
            raise DCVError("Capture returned NULL")
        return CapturedResult(ptr)

    def close(self):
        if self._router:
            self._dtor(self._router)
            self._router = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


# ---------------------------------------------------------------------------
# Result wrappers
# ---------------------------------------------------------------------------
class CapturedResult:
    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def error_code(self):
        return _call(self._ptr, SLOT_CAPTURED["GetErrorCode"], ctypes.c_int)

    @property
    def error_string(self):
        s = _call(self._ptr, SLOT_CAPTURED["GetErrorString"], ctypes.c_char_p)
        return s.decode("utf-8") if s else ""

    def get_decoded_barcodes_result(self):
        ptr = _call(self._ptr, SLOT_CAPTURED["GetDecodedBarcodesResult"], ctypes.c_void_p)
        return DecodedBarcodesResult(ptr) if ptr else None

    def release(self):
        if self._ptr:
            _call(self._ptr, SLOT_CAPTURED["Release"], None)
            self._ptr = None


class DecodedBarcodesResult:
    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def error_code(self):
        return _call(self._ptr, SLOT_DECODED["GetErrorCode"], ctypes.c_int)

    @property
    def error_string(self):
        s = _call(self._ptr, SLOT_DECODED["GetErrorString"], ctypes.c_char_p)
        return s.decode("utf-8") if s else ""

    @property
    def items_count(self):
        return _call(self._ptr, SLOT_DECODED["GetItemsCount"], ctypes.c_int)

    def get_item(self, index):
        fn = ctypes.cast(ctypes.c_void_p(_vtable_slot(self._ptr, SLOT_DECODED["GetItem"])),
                         ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int))
        ptr = fn(self._ptr, index)
        return BarcodeResultItem(ptr) if ptr else None

    def release(self):
        if self._ptr:
            _call(self._ptr, SLOT_DECODED["Release"], None)
            self._ptr = None


class BarcodeResultItem:
    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def format_string(self):
        s = _call(self._ptr, SLOT_ITEM["GetFormatString"], ctypes.c_char_p)
        return s.decode("utf-8") if s else ""

    @property
    def text(self):
        s = _call(self._ptr, SLOT_ITEM["GetText"], ctypes.c_char_p)
        return s.decode("utf-8") if s else ""

    @property
    def confidence(self):
        return _call(self._ptr, SLOT_ITEM["GetConfidence"], ctypes.c_int)

    @property
    def location(self):
        """Return ((x1,y1),(x2,y2),(x3,y3),(x4,y4)).

        GetLocation() returns a CQuadrilateral (36 bytes) by value, so it uses
        a hidden return buffer. The argument order differs by ABI: MSVC passes
        (this, &buffer), the Itanium ABI passes (&buffer, this).
        """
        quad = Quadrilateral()
        fn = ctypes.cast(ctypes.c_void_p(_vtable_slot(self._ptr, SLOT_ITEM["GetLocation"])),
                         ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p))
        if _IS_WINDOWS:
            fn(self._ptr, ctypes.addressof(quad))
        else:
            fn(ctypes.addressof(quad), self._ptr)
        return tuple(tuple(p) for p in quad.points)
