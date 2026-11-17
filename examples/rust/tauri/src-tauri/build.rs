use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use cc::Build;
use walkdir::WalkDir;

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // src-tauri -> tauri -> rust -> examples -> repository root
    let repo = absolute(manifest.join("../../../.."));
    let rust_example = absolute(manifest.join("../.."));
    let dcv = repo.join("dcv");
    let litecam = repo.join("litecam").join("dist");
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let bin_dir = profile_dir();

    match target_os.as_str() {
        "windows" => {
            let lib_dir = dcv.join("lib").join("win");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for lib in [
                "DynamsoftCaptureVisionRouterx64",
                "DynamsoftBarcodeReaderx64",
                "DynamsoftCorex64",
                "DynamsoftLicensex64",
                "DynamsoftUtilityx64",
            ] {
                println!("cargo:rustc-link-lib=static={lib}");
            }
            copy_extension(&lib_dir, &bin_dir, "dll");

            let cam_dir = litecam.join("lib").join("windows").join("release");
            println!("cargo:rustc-link-search=native={}", cam_dir.display());
            println!("cargo:rustc-link-lib=dylib=litecam");
            copy_file(&cam_dir.join("litecam.dll"), &bin_dir.join("litecam.dll"));

            copy_resources(&dcv, &bin_dir);
        }
        "linux" => {
            let lib_dir = dcv.join("lib").join("linux").join("x64");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            println!("cargo:rustc-link-lib=dylib=DynamsoftBarcodeReader");
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
            copy_extension(&lib_dir, &bin_dir, "so");
            copy_resources(&dcv, &bin_dir);
        }
        "macos" => {
            let lib_dir = dcv.join("lib").join("mac");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            println!("cargo:rustc-link-lib=dylib=DynamsoftBarcodeReader");
            println!("cargo:rustc-link-arg=-Wl,-rpath,@loader_path/../../dcv/lib/mac");
            copy_extension(&lib_dir, &bin_dir, "dylib");
            copy_resources(&dcv, &bin_dir);
        }
        other => panic!("Unsupported target OS: {other}"),
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

fn copy_file(src: &Path, dest: &Path) {
    if !src.exists() {
        println!("cargo:warning=missing native asset: {}", src.display());
        return;
    }
    // A running instance keeps its DLLs locked; skipping is fine, they are identical.
    if dest.exists() {
        return;
    }
    if let Err(err) = fs::copy(src, dest) {
        println!("cargo:warning=failed to copy {}: {err}", src.display());
    }
}

fn copy_extension(src_dir: &Path, dest_dir: &Path, extension: &str) {
    for entry in WalkDir::new(src_dir).into_iter().filter_map(|e| e.ok()) {
        if entry.path().extension().and_then(|e| e.to_str()) == Some(extension) {
            copy_file(entry.path(), &dest_dir.join(entry.file_name()));
        }
    }
}

fn copy_resources(dcv: &Path, bin_dir: &Path) {
    for name in ["Templates", "Models"] {
        let src = dcv.join("resource").join(name);
        if src.is_dir() {
            copy_dir(&src, &bin_dir.join(name));
        }
    }
}

fn copy_dir(src: &Path, dest: &Path) {
    if let Err(err) = fs::create_dir_all(dest) {
        println!("cargo:warning=failed to create {}: {err}", dest.display());
        return;
    }
    for entry in WalkDir::new(src).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        let relative = path.strip_prefix(src).unwrap();
        let target = dest.join(relative);
        if path.is_dir() {
            let _ = fs::create_dir_all(&target);
        } else {
            copy_file(path, &target);
        }
    }
}