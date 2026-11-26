use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use cc::Build;
use walkdir::WalkDir;

/// Marks which SDK version has been deployed into the profile directory.
const STAMP_FILE: &str = ".dcv-sdk-version";

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // src-tauri -> tauri -> rust -> examples -> repository root
    let repo = absolute(manifest.join("../../../.."));
    let rust_example = absolute(manifest.join("../.."));
    let dcv = repo.join("dcv");
    let litecam = repo.join("litecam").join("dist");
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let bin_dir = profile_dir();
    // Re-copy every SDK file when the bundled version changed; otherwise keep
    // the existing copies, which may still be memory-mapped by a running app.
    let refresh = needs_refresh(&bin_dir, &dcv);
    let mut deployed = true;

    // DBR alone is not enough: the linkers on Linux/macOS do not resolve
    // undefined references through the SDK's own DT_NEEDED / LC_LOAD_DYLIB
    // entries, so every module the bridge calls into has to be named.
    let modules = [
        "DynamsoftCaptureVisionRouter",
        "DynamsoftBarcodeReader",
        "DynamsoftCore",
        "DynamsoftLicense",
        "DynamsoftUtility",
    ];

    match target_os.as_str() {
        "windows" => {
            let lib_dir = dcv.join("lib").join("win");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for lib in modules {
                println!("cargo:rustc-link-lib=static={lib}x64");
            }
            // Windows keeps its import libraries next to the DLLs, and the
            // loader searches the executable directory.
            deployed &= copy_extension(&lib_dir, &bin_dir, "dll", refresh);

            let cam_dir = litecam.join("lib").join("windows").join("release");
            println!("cargo:rustc-link-search=native={}", cam_dir.display());
            println!("cargo:rustc-link-lib=dylib=litecam");
            deployed &= copy_file(&cam_dir.join("litecam.dll"), &bin_dir.join("litecam.dll"), refresh);

            deployed &= copy_resources(&dcv, &bin_dir, refresh);
        }
        "linux" => {
            // ARM64 boards (Raspberry Pi, Graviton) use the arm64 build.
            let arch = if target_arch == "aarch64" { "arm64" } else { "x64" };
            let lib_dir = dcv.join("lib").join("linux").join(arch);
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for lib in modules {
                println!("cargo:rustc-link-lib=dylib={lib}");
            }
            // The libraries are copied next to the executable, so resolve them
            // from there instead of hard-coding the repository path.
            println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN");
            deployed &= copy_extension(&lib_dir, &bin_dir, "so", refresh);
            deployed &= copy_resources(&dcv, &bin_dir, refresh);
        }
        "macos" => {
            let lib_dir = dcv.join("lib").join("mac");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for lib in modules {
                println!("cargo:rustc-link-lib=dylib={lib}");
            }
            // The dylibs reference each other as @rpath/..., which @loader_path
            // resolves in the directory of the running executable.
            println!("cargo:rustc-link-arg=-Wl,-rpath,@loader_path");
            deployed &= copy_extension(&lib_dir, &bin_dir, "dylib", refresh);
            deployed &= copy_resources(&dcv, &bin_dir, refresh);
        }
        other => panic!("Unsupported target OS: {other}"),
    }

    if deployed {
        let _ = fs::write(bin_dir.join(STAMP_FILE), sdk_version(&dcv));
    }

    // The C ABI bridge is shared with the Slint example: one native core, two GUIs.
    let bridge = rust_example.join("lib").join("bridge.cpp");
    let camera_bridge = rust_example.join("lib").join("camera_bridge.cpp");
    let mut cpp = Build::new();
    cpp.cpp(true)
        .include(dcv.join("include"))
        .file(&bridge);
    if target_os == "windows" {
        cpp.include(litecam.join("include")).file(&camera_bridge);
    }
    cpp.compile("bridge");
    println!("cargo:rustc-link-lib=static=bridge");
    println!("cargo:rustc-link-search=native={}", env::var("OUT_DIR").unwrap());

    for path in [
        bridge,
        rust_example.join("lib").join("bridge.h"),
        rust_example.join("src").join("scanner.rs"),
        absolute(manifest.join("../ui")),
    ] {
        println!("cargo:rerun-if-changed={}", path.display());
    }
    if target_os == "windows" {
        println!("cargo:rerun-if-changed={}", camera_bridge.display());
        println!(
            "cargo:rerun-if-changed={}",
            rust_example.join("src").join("camera.rs").display()
        );
    }

    tauri_build::build();
}

/// `target/debug` or `target/release`, where the executable and its DLLs live.
/// OUT_DIR is `<target>/<profile>/build/<pkg>-<hash>/out`.
fn profile_dir() -> PathBuf {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    out_dir
        .ancestors()
        .nth(3)
        .expect("unexpected OUT_DIR layout")
        .to_path_buf()
}

/// Canonicalize without the `\\?\` prefix, which MSVC's linker does not accept.
fn absolute(path: PathBuf) -> PathBuf {
    let resolved = path.canonicalize().unwrap_or(path);
    let text = resolved.to_string_lossy();
    PathBuf::from(text.strip_prefix(r"\\?\").unwrap_or(&text).to_string())
}

/// The stamp records which SDK version the binaries in `profile` were deployed
/// with, so an SDK bump re-copies the runtime while ordinary rebuilds stay fast.
fn needs_refresh(profile: &Path, dcv: &Path) -> bool {
    let stamp = profile.join(STAMP_FILE);
    let current = sdk_version(dcv);
    let previous = fs::read_to_string(&stamp).ok();
    if previous.as_deref().map(str::trim) == Some(current.as_str()) {
        return false;
    }
    println!("cargo:warning=deploying Dynamsoft SDK {current} runtime files");
    let _ = fs::create_dir_all(profile);
    // Drop the stale stamp first: `main` rewrites it only after every copy
    // succeeded, so an interrupted deploy is retried on the next build.
    let _ = fs::remove_file(&stamp);
    true
}

fn sdk_version(dcv: &Path) -> String {
    let header = dcv.join("include").join("DynamsoftBarcodeReader.h");
    fs::read_to_string(header)
        .unwrap_or_default()
        .lines()
        .find_map(|line| {
            let rest = line.trim().strip_prefix("#define DBR_VERSION")?;
            Some(rest.trim().trim_matches('"').to_string())
        })
        .unwrap_or_else(|| "unknown".to_string())
}

fn copy_file(src: &Path, dest: &Path, refresh: bool) -> bool {
    if !src.exists() {
        println!("cargo:warning=missing native asset: {}", src.display());
        return true;
    }
    // A running instance keeps its DLLs locked, so only touch the file when the
    // deployed SDK version changed.
    if dest.exists() && !refresh {
        return true;
    }
    match fs::copy(src, dest) {
        Ok(_) => true,
        Err(err) => {
            println!("cargo:warning=failed to copy {}: {err}", src.display());
            false
        }
    }
}

fn copy_extension(src_dir: &Path, dest_dir: &Path, extension: &str, refresh: bool) -> bool {
    if !src_dir.is_dir() {
        panic!(
            "Dynamsoft libraries not found in {}. The dcv/ folder must ship lib/ for every supported platform.",
            src_dir.display()
        );
    }
    let mut ok = true;
    for entry in WalkDir::new(src_dir).into_iter().filter_map(|e| e.ok()) {
        if entry.path().extension().and_then(|e| e.to_str()) == Some(extension) {
            ok &= copy_file(entry.path(), &dest_dir.join(entry.file_name()), refresh);
        }
    }
    ok
}

fn copy_resources(dcv: &Path, bin_dir: &Path, refresh: bool) -> bool {
    let mut ok = true;
    for name in ["Templates", "Models"] {
        let src = dcv.join("resource").join(name);
        if src.is_dir() {
            ok &= copy_dir(&src, &bin_dir.join(name), refresh);
        }
    }
    ok
}

fn copy_dir(src: &Path, dest: &Path, refresh: bool) -> bool {
    if let Err(err) = fs::create_dir_all(dest) {
        println!("cargo:warning=failed to create {}: {err}", dest.display());
        return false;
    }
    let mut ok = true;
    for entry in WalkDir::new(src).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        let relative = path.strip_prefix(src).unwrap();
        let target = dest.join(relative);
        if path.is_dir() {
            let _ = fs::create_dir_all(&target);
        } else {
            ok &= copy_file(path, &target, refresh);
        }
    }
    ok
}