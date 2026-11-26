use std::env;
use cc::Build;

use std::fs;
use walkdir::WalkDir;
use std::path::{Path, PathBuf};

/// Library names the sample links against. DBR cannot be linked on its own:
/// the symbols it exposes are spread over the Capture Vision modules, and the
/// linkers on Linux/macOS do not follow DT_NEEDED / LC_LOAD_DYLIB entries when
/// resolving undefined references.
const DCV_LIBS: &[&str] = &[
    "DynamsoftCaptureVisionRouter",
    "DynamsoftBarcodeReader",
    "DynamsoftCore",
    "DynamsoftLicense",
    "DynamsoftUtility",
];

/// Marks which SDK version has been deployed into the profile directory.
const STAMP_FILE: &str = ".dcv-sdk-version";

fn main() {
    // Determine the target operating system
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // `../../dcv` is where this repository ships the SDK; absolute so that the
    // paths survive rustc resolving -L relative to its own working directory.
    let dcv_dir = manifest_dir.join("../../dcv");

    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed={}", dcv_dir.join("include").display());
    println!("cargo:warning=DCV SDK: {}", dcv_dir.join("lib").display());

    let mut deployed = true;
    match target_os.as_str() {
        "windows" => {
            let lib_dir = dcv_dir.join("lib/win");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for name in DCV_LIBS {
                println!("cargo:rustc-link-lib=static={name}x64");
            }

            // Copy *.dll files to the output path for Windows
            deployed &= copy_shared_libs_from_dir_to_out_dir(&lib_dir, &get_out_dir(), "dll");

            // Link the bundled litecam (Media Foundation) camera library
            let litecam_dir = manifest_dir.join("../../litecam/dist");
            println!("cargo:rustc-link-search=native={}", litecam_dir.join("lib/windows/release").display());
            println!("cargo:rustc-link-lib=dylib=litecam");
            let litecam_dll = litecam_dir.join("lib/windows/release/litecam.dll");
            if litecam_dll.exists() {
                let _ = fs::copy(litecam_dll, get_out_dir().join("litecam.dll"));
            }

            // Copy resource directories (Templates and Models)
            copy_resource_directories(&dcv_dir, &get_out_dir());
        },
        "linux" => {
            // ARM64 boards (Raspberry Pi, Graviton) use the arm64 build.
            let arch_dir = if target_arch == "aarch64" { "arm64" } else { "x64" };
            let lib_dir = dcv_dir.join("lib/linux").join(arch_dir);
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for name in DCV_LIBS {
                println!("cargo:rustc-link-lib=dylib={name}");
            }

            // The SDK libraries are copied next to the binaries below, so the
            // executable only has to look in its own directory.
            println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN");

            // Copy *.so files to the output path for Linux
            deployed &= copy_shared_libs_from_dir_to_out_dir(&lib_dir, &get_out_dir(), "so");

            // Copy resource directories (Templates and Models)
            copy_resource_directories(&dcv_dir, &get_out_dir());
        },
        "macos" => {
            let lib_dir = dcv_dir.join("lib/mac");
            println!("cargo:rustc-link-search=native={}", lib_dir.display());
            for name in DCV_LIBS {
                println!("cargo:rustc-link-lib=dylib={name}");
            }

            // The dylibs are copied next to the binaries and reference each
            // other as @rpath/..., which @loader_path resolves in the same
            // directory as the running executable.
            println!("cargo:rustc-link-arg=-Wl,-rpath,@loader_path");

            // Copy *.dylib files to the output path for macOS
            deployed &= copy_shared_libs_from_dir_to_out_dir(&lib_dir, &get_out_dir(), "dylib");

            // Copy resource directories (Templates and Models)
            copy_resource_directories(&dcv_dir, &get_out_dir());
        },
        _ => {
            panic!("Unsupported target OS: {}", target_os);
        }
    }

    if deployed {
        let _ = fs::write(get_out_dir().join(STAMP_FILE), sdk_version());
    }

    // Compile the C++ code
    let mut cpp = Build::new();
    cpp.cpp(true)
        .include(dcv_dir.join("include"))
        .file(manifest_dir.join("lib/bridge.cpp"));
    if target_os == "windows" {
        cpp.include(manifest_dir.join("../../litecam/dist/include"))
            .file(manifest_dir.join("lib/camera_bridge.cpp"));
    }
    cpp.compile("bridge");

    // The C++ bridge is edited together with the Tauri example; make sure cargo
    // recompiles it even though build.rs emits other cargo directives.
    println!("cargo:rerun-if-changed=lib/bridge.cpp");
    println!("cargo:rerun-if-changed=lib/bridge.h");
    if target_os == "windows" {
        println!("cargo:rerun-if-changed=lib/camera_bridge.cpp");
    }

    // Tell cargo to tell rustc to link the compiled C library
    println!("cargo:rustc-link-lib=static=bridge");

    // Add the directory where the library was built to the search path
    println!("cargo:rustc-link-search=native={}", env::var("OUT_DIR").unwrap());

    // Compile the Slint UI for the barcode-reader-ui binary
    slint_build::compile("ui/app.slint").expect("Slint build failed");
}

fn get_out_dir() -> PathBuf {
    // OUT_DIR is <target>/<profile>/build/<crate>-<hash>/out; the binaries run
    // from <profile>, so that is where the SDK runtime has to land.
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    out_dir
        .ancestors()
        .nth(3)
        .unwrap_or(&out_dir)
        .to_path_buf()
}

fn copy_shared_libs_from_dir_to_out_dir(src_dir: &Path, out_dir: &Path, extension: &str) -> bool {
    if !src_dir.is_dir() {
        panic!(
            "Dynamsoft libraries not found in {}. The dcv/ folder must ship lib/ for every supported platform.",
            src_dir.display()
        );
    }
    let refresh = needs_refresh(out_dir);
    let mut ok = true;
    for entry in WalkDir::new(src_dir).into_iter().filter_map(|e| e.ok()) {
        if entry.path().extension().and_then(|ext| ext.to_str()) == Some(extension) {
            let lib_path = entry.path();
            let dest_path = out_dir.join(lib_path.file_name().unwrap());

            // Refresh everything when the bundled SDK changed: keeping an
            // existing file silently leaves the previous SDK version in
            // target/, which makes a version bump look like a no-op.
            if !refresh && dest_path.exists() {
                continue;
            }

            match fs::copy(lib_path, dest_path.clone()) {
                Ok(_) => println!("Copied {} to {}", lib_path.display(), dest_path.display()),
                Err(e) => {
                    // A running binary may hold the old library. That is not
                    // fatal: the stamp stays behind, so the next build retries.
                    ok = false;
                    println!(
                        "Warning: Failed to copy {} to {}: {}",
                        lib_path.display(),
                        dest_path.display(),
                        e
                    );
                }
            }
        }
    }
    ok
}

/// The stamp records which SDK version the binaries in `out_dir` were deployed
/// with, so an SDK bump re-copies the runtime while ordinary rebuilds stay fast.
fn needs_refresh(out_dir: &Path) -> bool {
    let stamp = out_dir.join(STAMP_FILE);
    let current = sdk_version();
    let previous = fs::read_to_string(&stamp).ok();
    if previous.as_deref().map(str::trim) == Some(current.as_str()) {
        return false;
    }
    println!("Deploying Dynamsoft SDK {current} runtime files");
    let _ = fs::create_dir_all(out_dir);
    // Drop the stale stamp first: `main` rewrites it only after every copy
    // succeeded, so an interrupted deploy is retried on the next build.
    let _ = fs::remove_file(&stamp);
    true
}

fn sdk_version() -> String {
    let header = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("../../dcv/include/DynamsoftBarcodeReader.h");
    let text = fs::read_to_string(&header).unwrap_or_default();
    text.lines()
        .find_map(|line| {
            let rest = line.trim().strip_prefix("#define DBR_VERSION")?;
            Some(rest.trim().trim_matches('"').to_string())
        })
        .unwrap_or_else(|| "unknown".to_string())
}

fn copy_resource_directories(dcv_dir: &Path, out_dir: &Path) {
    for name in ["Templates", "Models"] {
        let src = dcv_dir.join("resource").join(name);
        let dest = out_dir.join(name);

        if src.exists() {
            if dest.exists() {
                let _ = fs::remove_dir_all(&dest);
            }
            copy_dir_recursive(&src, &dest)
                .unwrap_or_else(|e| panic!("Failed to copy {name} directory: {e}"));
            println!("Copied {name} directory to {}", dest.display());
        }
    }
}

fn copy_dir_recursive(src: &Path, dest: &Path) -> std::io::Result<()> {
    fs::create_dir_all(dest)?;
    
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let src_path = entry.path();
        let dest_path = dest.join(entry.file_name());
        
        if src_path.is_dir() {
            copy_dir_recursive(&src_path, &dest_path)?;
        } else {
            fs::copy(&src_path, &dest_path)?;
        }
    }
    
    Ok(())
}