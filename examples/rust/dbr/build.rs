use std::env;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::Duration;

use sha2::{Digest, Sha256};

const DCV_VERSION: &str = "3.6.5000";

struct Wheel {
    tag: &'static str,
    url: &'static str,
    sha256: &'static str,
}

// Official Dynamsoft Capture Vision wheels on PyPI. The native libraries inside
// are byte-identical to the C++ SDK distribution of the same version.
const WHEELS: &[Wheel] = &[
    Wheel {
        tag: "win-x64",
        url: "https://files.pythonhosted.org/packages/3d/18/a186037850638914fdd9947e1ee64218cfc2f120d7b054a64100520a8f37/dynamsoft_capture_vision_bundle-3.6.5000-cp312-cp312-win_amd64.whl",
        sha256: "ec644979e0b9af506af91df0c6fe2fc352f2b16b4bd07e807ce8bea59ab89848",
    },
    Wheel {
        tag: "linux-x64",
        url: "https://files.pythonhosted.org/packages/4d/b4/73729fe206d1ff6263cb3b75df085895a79efc6e7429f88ccf837fb18743/dynamsoft_capture_vision_bundle-3.6.5000-cp312-cp312-manylinux2014_x86_64.manylinux_2_17_x86_64.whl",
        sha256: "5c23cdc0b3bd04fff7fb5df7a571bfa1f309f5e0053cacea40bb1734de7910d2",
    },
    Wheel {
        tag: "linux-arm64",
        url: "https://files.pythonhosted.org/packages/6b/50/b588c0c13afad1df6dc984b4ce04b870f478abc3a835d9725c4cbc5135ea/dynamsoft_capture_vision_bundle-3.6.5000-cp312-cp312-manylinux_2_27_aarch64.manylinux_2_28_aarch64.whl",
        sha256: "e832e50e52b6c2126bfadffda28353cacb9fbb77669dc3ac4d11d9d10380b68d",
    },
    Wheel {
        tag: "macos-universal",
        url: "https://files.pythonhosted.org/packages/71/15/746ee8f0a8cc12139c6264cd914a3f44478828b0d595545ccf11aaa6be19/dynamsoft_capture_vision_bundle-3.6.5000-cp312-cp312-macosx_12_0_universal2.whl",
        sha256: "b1a0552c05f86e80df45080e0688f6d968a69127d118e4fe7514ee14d39cc13c",
    },
];

struct Sdk {
    lib_dir: PathBuf,
    data_dir: PathBuf,
    templates_dir: PathBuf,
    models_dir: PathBuf,
}

fn main() {
    println!("cargo:rerun-if-env-changed=DCV_DIR");
    println!("cargo:rerun-if-changed=native/bridge.cpp");
    println!("cargo:rerun-if-changed=native/bridge.h");
    println!("cargo:rerun-if-changed=include");

    // docs.rs builds have no network and no native SDK; the extern declarations
    // alone are enough to render the documentation.
    if env::var_os("DOCS_RS").is_some() {
        return;
    }

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let runtime_dir = out_dir.join("runtime");

    let mut sdk = match env::var_os("DCV_DIR") {
        Some(dir) => local_sdk(PathBuf::from(dir), &target_os, &target_arch),
        None => downloaded_sdk(&target_os, &target_arch),
    };

    cc::Build::new()
        .cpp(true)
        .include(manifest_dir.join("include"))
        .file(manifest_dir.join("native/bridge.cpp"))
        .compile("dbr_bridge");

    // The SDK dylibs ship with @rpath install names, which downstream binaries
    // cannot resolve without their own LC_RPATH. Rewrite copies to @loader_path
    // so every artifact finds the libraries relative to its own location.
    if target_os == "macos" {
        sdk.lib_dir = patch_macos_dylibs(&sdk.lib_dir, &runtime_dir);
    }

    place_runtime_files(&sdk, &runtime_dir, &target_os);

    match target_os.as_str() {
        "windows" => {
            // Import libraries for the SDK DLLs are bundled with the crate.
            println!(
                "cargo:rustc-link-search=native={}",
                manifest_dir.join("lib").join("win").display()
            );
            for name in [
                "DynamsoftCaptureVisionRouterx64",
                "DynamsoftBarcodeReaderx64",
                "DynamsoftCorex64",
                "DynamsoftLicensex64",
                "DynamsoftUtilityx64",
            ] {
                println!("cargo:rustc-link-lib=dylib={name}");
            }
        }
        "linux" | "macos" => {
            // Search paths inside OUT_DIR are added to LD_LIBRARY_PATH /
            // DYLD_FALLBACK_LIBRARY_PATH by `cargo run` and `cargo test`.
            println!("cargo:rustc-link-search=native={}", runtime_dir.display());
            println!("cargo:rustc-link-search=native={}", sdk.lib_dir.display());
            for name in [
                "DynamsoftCaptureVisionRouter",
                "DynamsoftBarcodeReader",
                "DynamsoftCore",
                "DynamsoftLicense",
                "DynamsoftUtility",
            ] {
                println!("cargo:rustc-link-lib=dylib={name}");
            }
            if target_os == "linux" {
                println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN");
            } else {
                println!("cargo:rustc-link-arg=-Wl,-rpath,@loader_path");
            }
        }
        other => panic!("dbr: unsupported target OS: {other}"),
    }
}

fn platform_tag(os: &str, arch: &str) -> &'static str {
    match (os, arch) {
        ("windows", "x86_64") => "win-x64",
        ("linux", "x86_64") => "linux-x64",
        ("linux", "aarch64") => "linux-arm64",
        ("macos", "x86_64") | ("macos", "aarch64") => "macos-universal",
        _ => panic!("dbr: unsupported target {os}/{arch}; supported: windows-x64, linux-x64, linux-arm64, macos (x64/arm64)"),
    }
}

fn local_sdk(dir: PathBuf, os: &str, arch: &str) -> Sdk {
    let dir = fs::canonicalize(&dir)
        .unwrap_or_else(|_| panic!("dbr: DCV_DIR={} does not exist", dir.display()));
    let lib_dir = match platform_tag(os, arch) {
        "win-x64" => dir.join("lib").join("win"),
        "linux-x64" => dir.join("lib").join("linux").join("x64"),
        "linux-arm64" => dir.join("lib").join("linux").join("arm64"),
        _ => dir.join("lib").join("mac"),
    };
    let sdk = Sdk {
        lib_dir,
        data_dir: dir.join("resource"),
        templates_dir: dir.join("resource").join("Templates"),
        models_dir: dir.join("resource").join("Models"),
    };
    for required in [&sdk.lib_dir, &sdk.templates_dir, &sdk.models_dir] {
        if !required.is_dir() {
            panic!(
                "dbr: DCV_DIR={} is missing {}. Expected a Dynamsoft Capture Vision {DCV_VERSION} directory with lib/ and resource/ subdirectories.",
                dir.display(),
                required.display()
            );
        }
    }
    sdk
}

fn downloaded_sdk(os: &str, arch: &str) -> Sdk {
    let tag = platform_tag(os, arch);
    let wheel = WHEELS.iter().find(|w| w.tag == tag).unwrap();
    let cache = cache_root().join(format!("dbr-{DCV_VERSION}-{tag}"));
    let sdk_root = cache.join("sdk");
    let marker = cache.join(".verified");

    let cached = fs::read_to_string(&marker).ok().as_deref() == Some(wheel.sha256);
    if cached && sdk_root.is_dir() {
        return flat_sdk(sdk_root);
    }

    fs::create_dir_all(&cache).expect("dbr: failed to create SDK cache directory");
    let whl = cache.join("package.whl");
    if !file_has_sha256(&whl, wheel.sha256) {
        println!(
            "cargo:warning=dbr: downloading Dynamsoft Capture Vision {DCV_VERSION} ({tag}) from PyPI; cached at {}",
            cache.display()
        );
        let agent = ureq::AgentBuilder::new()
            .timeout(Duration::from_secs(600))
            .build();
        let response = agent.get(wheel.url).call().unwrap_or_else(|e| {
            panic!(
                "dbr: failed to download the native SDK from {}: {e}\n\
                 Set DCV_DIR to a local Dynamsoft Capture Vision {DCV_VERSION} directory to build offline.",
                wheel.url
            )
        });
        let mut bytes = Vec::new();
        response
            .into_reader()
            .read_to_end(&mut bytes)
            .expect("dbr: failed to read SDK download");
        let actual = hex(&Sha256::digest(&bytes));
        if actual != wheel.sha256 {
            panic!(
                "dbr: SDK download checksum mismatch (expected {}, got {actual})",
                wheel.sha256
            );
        }
        fs::write(&whl, &bytes).expect("dbr: failed to write SDK cache file");
    }

    let extract_dir = cache.join("extracted");
    if extract_dir.exists() {
        fs::remove_dir_all(&extract_dir).expect("dbr: failed to clean SDK extraction directory");
    }
    let file = fs::File::open(&whl).expect("dbr: failed to open cached SDK wheel");
    let mut archive = zip::ZipArchive::new(file).expect("dbr: cached SDK wheel is not a valid zip");
    archive
        .extract(&extract_dir)
        .expect("dbr: failed to extract SDK wheel");
    let root = extract_dir.join("dynamsoft_capture_vision_bundle");
    if sdk_root.exists() {
        fs::remove_dir_all(&sdk_root).expect("dbr: failed to replace cached SDK");
    }
    fs::rename(&root, &sdk_root).expect("dbr: failed to finalize cached SDK");
    fs::write(&marker, wheel.sha256).expect("dbr: failed to write SDK cache marker");

    flat_sdk(sdk_root)
}

fn flat_sdk(root: PathBuf) -> Sdk {
    Sdk {
        lib_dir: root.clone(),
        data_dir: root.clone(),
        templates_dir: root.join("Templates"),
        models_dir: root.join("Models"),
    }
}

fn cache_root() -> PathBuf {
    if let Some(local) = env::var_os("LOCALAPPDATA") {
        return PathBuf::from(local).join("dbr-rs");
    }
    if let Some(xdg) = env::var_os("XDG_CACHE_HOME") {
        return PathBuf::from(xdg).join("dbr-rs");
    }
    if let Some(home) = env::var_os("HOME") {
        return PathBuf::from(home).join(".cache").join("dbr-rs");
    }
    PathBuf::from(env::var("OUT_DIR").unwrap()).join("dbr-cache")
}

fn file_has_sha256(path: &Path, expected: &str) -> bool {
    let Ok(mut file) = fs::File::open(path) else {
        return false;
    };
    let mut hasher = Sha256::new();
    if std::io::copy(&mut file, &mut hasher).is_err() {
        return false;
    }
    hex(&hasher.finalize()) == expected
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn patch_macos_dylibs(lib_dir: &Path, runtime_dir: &Path) -> PathBuf {
    fs::create_dir_all(runtime_dir).expect("dbr: failed to create runtime directory");
    let dylibs: Vec<PathBuf> = fs::read_dir(lib_dir)
        .expect("dbr: failed to list SDK libraries")
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().and_then(|e| e.to_str()) == Some("dylib"))
        .collect();

    for src in &dylibs {
        let name = src.file_name().unwrap().to_str().unwrap();
        let dest = runtime_dir.join(name);
        if !dest.exists() {
            fs::copy(src, &dest).unwrap_or_else(|e| panic!("dbr: failed to copy {name}: {e}"));
        }
        install_name_tool(&["-id", &format!("@loader_path/{name}"), dest.to_str().unwrap()]);
        for other in &dylibs {
            let other_name = other.file_name().unwrap().to_str().unwrap();
            // -change fails when the reference is absent; that is fine.
            let _ = Command::new("install_name_tool")
                .args([
                    "-change",
                    &format!("@rpath/{other_name}"),
                    &format!("@loader_path/{other_name}"),
                    dest.to_str().unwrap(),
                ])
                .output();
        }
        // Editing a dylib invalidates its code signature; arm64 refuses to
        // load unsigned binaries, so apply an ad-hoc signature.
        let output = Command::new("codesign")
            .args(["--force", "--sign", "-", dest.to_str().unwrap()])
            .output()
            .expect("dbr: codesign not found; install the Xcode Command Line Tools");
        if !output.status.success() {
            panic!(
                "dbr: codesign failed for {name}: {}",
                String::from_utf8_lossy(&output.stderr)
            );
        }
    }
    runtime_dir.to_path_buf()
}

fn install_name_tool(args: &[&str]) {
    let output = Command::new("install_name_tool")
        .args(args)
        .output()
        .expect("dbr: install_name_tool not found; install the Xcode Command Line Tools");
    if !output.status.success() {
        panic!(
            "dbr: install_name_tool {} failed: {}",
            args.join(" "),
            String::from_utf8_lossy(&output.stderr)
        );
    }
}

// OUT_DIR is <target>/<profile>/build/<crate>-<hash>/out; binaries, tests and
// examples run from <profile>, <profile>/deps and <profile>/examples. The SDK
// resolves its libraries and Templates/Models relative to the running module,
// so every location that can load the libraries gets the full runtime set.
fn place_runtime_files(sdk: &Sdk, runtime_dir: &Path, target_os: &str) {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let profile_dir = out_dir
        .ancestors()
        .nth(3)
        .expect("dbr: unexpected OUT_DIR layout")
        .to_path_buf();
    let lib_ext = match target_os {
        "windows" => "dll",
        "linux" => "so",
        "macos" => "dylib",
        _ => unreachable!(),
    };

    for dest in [
        runtime_dir.to_path_buf(),
        profile_dir.clone(),
        profile_dir.join("deps"),
        profile_dir.join("examples"),
    ] {
        fs::create_dir_all(&dest).expect("dbr: failed to create runtime directory");
        for entry in fs::read_dir(&sdk.lib_dir).expect("dbr: failed to list SDK libraries") {
            let path = entry.expect("dbr: failed to read SDK library entry").path();
            let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
            if ext == lib_ext || ext == "data" {
                place_file(&path, &dest.join(path.file_name().unwrap()));
            }
        }
        for entry in fs::read_dir(&sdk.data_dir).expect("dbr: failed to list SDK data files") {
            let path = entry.expect("dbr: failed to read SDK data entry").path();
            if path.extension().and_then(|e| e.to_str()) == Some("data") {
                place_file(&path, &dest.join(path.file_name().unwrap()));
            }
        }
        place_dir(&sdk.templates_dir, &dest.join("Templates"));
        place_dir(&sdk.models_dir, &dest.join("Models"));
    }
}

fn place_file(src: &Path, dest: &Path) {
    // Skip files already in place: they may be locked by a running process.
    if dest.exists() {
        return;
    }
    if fs::hard_link(src, dest).is_err() {
        fs::copy(src, dest).unwrap_or_else(|e| {
            panic!("dbr: failed to place {}: {e}", dest.display())
        });
    }
}

fn place_dir(src: &Path, dest: &Path) {
    if !src.is_dir() {
        panic!("dbr: required SDK resource directory {} is missing", src.display());
    }
    fs::create_dir_all(dest).expect("dbr: failed to create resource directory");
    for entry in fs::read_dir(src).expect("dbr: failed to list SDK resource directory") {
        let path = entry.expect("dbr: failed to read SDK resource entry").path();
        let target = dest.join(path.file_name().unwrap());
        if path.is_dir() {
            place_dir(&path, &target);
        } else {
            place_file(&path, &target);
        }
    }
}