use std::env;
use cc::Build;

use std::fs;
use walkdir::WalkDir;
use std::path::{Path, PathBuf};

fn main() {
    // Determine the target operating system
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    println!("OUT_DIR: {}..............................................", env::var("OUT_DIR").unwrap());
    println!("cargo:warning=OS: {}..............................................", target_os);

    match target_os.as_str() {
        "windows" => {
            // Link Dynamsoft Capture Vision Router for Windows
            println!("cargo:rustc-link-search=../../dcv/lib/win");
            println!("cargo:rustc-link-lib=static=DynamsoftCaptureVisionRouterx64");
            println!("cargo:rustc-link-lib=static=DynamsoftBarcodeReaderx64");
            println!("cargo:rustc-link-lib=static=DynamsoftCorex64");
            println!("cargo:rustc-link-lib=static=DynamsoftLicensex64");
            println!("cargo:rustc-link-lib=static=DynamsoftUtilityx64");

            // Copy *.dll files to the output path for Windows
            let src_dir = Path::new("../../dcv/lib/win");
            copy_shared_libs_from_dir_to_out_dir(src_dir, &get_out_dir(), "dll");
            
            // Copy resource directories (Templates and Models)
            copy_resource_directories(&get_out_dir());
        },
        "linux" => {
            // Link Dynamsoft Barcode Reader for Linux
            println!("cargo:rustc-link-search=../../dcv/lib/linux/x64");
            println!("cargo:rustc-link-lib=dylib=DynamsoftBarcodeReader");

            // Set rpath for Linux
            println!("cargo:rustc-link-arg=-Wl,-rpath,../../dcv/lib/linux/x64");

            // Copy *.so files to the output path for Linux
            let src_dir = Path::new("../../dcv/lib/linux/x64");
            copy_shared_libs_from_dir_to_out_dir(src_dir, &get_out_dir(), "so");
            
            // Copy resource directories (Templates and Models)
            copy_resource_directories(&get_out_dir());
        },
        "macos" => {
            // Link Dynamsoft Barcode Reader for macOS
            println!("cargo:rustc-link-search=../../dcv/lib/mac");
            println!("cargo:rustc-link-lib=dylib=DynamsoftBarcodeReader");
            
            // Set rpath for macOS
            println!("cargo:rustc-link-arg=-Wl,-rpath,@loader_path/../../dcv/lib/mac");

            // Copy *.dylib files to the output path for macOS
            let src_dir = Path::new("../../dcv/lib/mac");
            copy_shared_libs_from_dir_to_out_dir(src_dir, &get_out_dir(), "dylib");
            
            // Copy resource directories (Templates and Models)
            copy_resource_directories(&get_out_dir());
        },
        _ => {
            panic!("Unsupported target OS: {}", target_os);
        }
    }

    // Compile the C++ code
    Build::new()
        .cpp(true)
        .include("../../dcv/include")
        .file("lib/bridge.cpp")
        .compile("bridge");

    // Tell cargo to tell rustc to link the compiled C library
    println!("cargo:rustc-link-lib=static=bridge");

    // Add the directory where the library was built to the search path
    println!("cargo:rustc-link-search=native={}", env::var("OUT_DIR").unwrap());
}

fn get_out_dir() -> PathBuf {
    let out_dir = env::var("OUT_DIR").unwrap();
    let debug_offset = out_dir.find("debug").unwrap_or(0);
    let release_offset = out_dir.find("release").unwrap_or(0);
    let mut path = String::from("");

    if debug_offset > 0 {
        println!(">>> where is debug {}", debug_offset);
        path.push_str(&format!("{}", &out_dir[..debug_offset]));
        path.push_str("debug");
        println!("{}", path);
    }

    if release_offset > 0 {
        println!(">>> where is release {}", release_offset);
        path.push_str(&format!("{}", &out_dir[..release_offset]));
        path.push_str("release");
        println!("{}", path);
    }

    PathBuf::from(path)
}

fn copy_shared_libs_from_dir_to_out_dir(src_dir: &Path, out_dir: &Path, extension: &str) {
    for entry in WalkDir::new(src_dir).into_iter().filter_map(|e| e.ok()) {
        if entry.path().extension().and_then(|ext| ext.to_str()) == Some(extension) {
            let lib_path = entry.path();
            let file_name = lib_path.file_name().unwrap();
            let dest_path = out_dir.join(file_name);

            // Skip if file already exists and is in use
            if dest_path.exists() {
                match fs::metadata(&dest_path) {
                    Ok(_) => {
                        println!("Skipping {} (already exists)", dest_path.display());
                        continue;
                    }
                    Err(_) => {} // File might be in use, try to copy anyway
                }
            }

            match fs::copy(lib_path, dest_path.clone()) {
                Ok(_) => println!("Copied {} to {}", lib_path.display(), dest_path.display()),
                Err(e) => {
                    println!("Warning: Failed to copy {} to {}: {}", 
                        lib_path.display(), dest_path.display(), e);
                    // Don't fail the build, just warn
                }
            }
        }
    }
}

fn copy_resource_directories(out_dir: &Path) {
    // Copy Templates directory
    let templates_src = Path::new("../../dcv/resource/Templates");
    let templates_dest = out_dir.join("Templates");
    
    if templates_src.exists() {
        if templates_dest.exists() {
            let _ = fs::remove_dir_all(&templates_dest);
        }
        copy_dir_recursive(templates_src, &templates_dest).expect("Failed to copy Templates directory");
        println!("Copied Templates directory to {}", templates_dest.display());
    }
    
    // Copy Models directory
    let models_src = Path::new("../../dcv/resource/Models");
    let models_dest = out_dir.join("Models");
    
    if models_src.exists() {
        if models_dest.exists() {
            let _ = fs::remove_dir_all(&models_dest);
        }
        copy_dir_recursive(models_src, &models_dest).expect("Failed to copy Models directory");
        println!("Copied Models directory to {}", models_dest.display());
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