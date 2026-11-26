use std::env;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::Duration;

use sha2::{Digest, Sha256};

const DCV_VERSION: &str = "3.6.6000";

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
        url: "https://files.pythonhosted.org/packages/0d/2c/18c14cfd222a266e490d056b4e27570698e646ed585abffbb3b156d951cb/dynamsoft_capture_vision_bundle-3.6.6000-cp312-cp312-win_amd64.whl",
        sha256: "b6ae9e44cb99f8c797c3367f144118dcbb7cc15d7eec1fa3dd27496599362c19",
    },
    Wheel {
        tag: "linux-x64",
        url: "https://files.pythonhosted.org/packages/ad/00/744a9b70f53100f44854d1e14bdf765d9c2bb77c7d93646eebb65b6f697e/dynamsoft_capture_vision_bundle-3.6.6000-cp312-cp312-manylinux2014_x86_64.manylinux_2_17_x86_64.whl",
        sha256: "ba2c83aa8ad8ed6ff3d51d1eb382b445773c7f10b8df952f8d7dedc18a2d29dc",
    },
    Wheel {
        tag: "linux-arm64",
        url: "https://files.pythonhosted.org/packages/0b/29/fc852db71aaa8e948ef22081afcd9e61c4e37a04f7d645c2b978360ce432/dynamsoft_capture_vision_bundle-3.6.6000-cp312-cp312-manylinux_2_27_aarch64.manylinux_2_28_aarch64.whl",
        sha256: "a2b1cb0a952327b8ed90e77c797ff880525441b70bfae4fb46cb756868b13d11",
    },
    Wheel {
        tag: "macos-universal",
        url: "https://files.pythonhosted.org/packages/17/e6/4ed5cc749eeb318537fafc71ae601de2c23da3f81e1d40f7ffd548056e70/dynamsoft_capture_vision_bundle-3.6.6000-cp312-cp312-macosx_12_0_universal2.whl",
        sha256: "9c139f63c580409c2aac1974d6edb4ec72368463c9e4b53c29e46934bd90d7d6",
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

    // The SDK is used in place, never copied into this directory:
    //   1. DCV_DIR, if the user points at a Dynamsoft Capture Vision tree;
    //   2. ../../../dcv, the SDK that ships with this repository;
    //   3. the copies staged by scripts/stage-sdk.sh for a published crate,
    //      whose runtime then comes from the PyPI wheel.
    let repo_sdk = manifest_dir.join("../../../dcv");
    let staged = manifest_dir.join("include").is_dir();
    let sdk_root = match env::var_os("DCV_DIR") {
        Some(dir) => {
            let dir = PathBuf::from(dir);
            println!("cargo:rerun-if-changed={}", dir.join("include").display());
            Some(dir)
        }
        None if repo_has_sdk(&repo_sdk) => {
            println!("cargo:rerun-if-changed={}", repo_sdk.join("include").display());
            Some(repo_sdk.clone())
        }
        None => None,
    };

    let include_dir = match (&sdk_root, staged) {
        (Some(root), _) => root.join("include"),
        (None, true) => manifest_dir.join("include"),
        (None, false) => panic!(
            "dbr: no Dynamsoft Capture Vision SDK found.\n\
             Point DCV_DIR at a DCV 3.6.60 directory, or run \
             examples/rust/dbr/scripts/stage-sdk.sh before packaging."
        ),
    };

    let mut sdk = match &sdk_root {
        Some(root) => local_sdk(root.clone(), &target_os, &target_arch),
        None => downloaded_sdk(&target_os, &target_arch),
    };

    cc::Build::new()
        .cpp(true)
        .include(&include_dir)
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
            // Import libraries: from the SDK tree when one is available,
            // otherwise the copies staged with the crate for publishing.
            let import_dir = match &sdk_root {
                Some(root) => root.join("lib").join("win"),
                None => manifest_dir.join("lib").join("win"),
            };
            println!("cargo:rustc-link-search=native={}", import_dir.display());
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

/// True when `dir` looks like a Dynamsoft Capture Vision tree (`include/` and
/// `resource/`), which is how the SDK is laid out in this repository.
fn repo_has_sdk(dir: &Path) -> bool {
    dir.join("include").join("DynamsoftBarcodeReader.h").is_file()
        && dir.join("resource").join("Templates").is_dir()
}

fn local_sdk(dir: PathBuf, os: &str, arch: &str) -> Sdk {
    let dir = fs::canonicalize(&dir)
        .unwrap_or_else(|_| panic!("dbr: {} does not exist", dir.display()));
    // MSVC's linker rejects the \\?\ prefix that canonicalize() adds.
    let text = dir.to_string_lossy();
    let dir = PathBuf::from(text.strip_prefix(r"\\?\").unwrap_or(&text).to_string());
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
                "dbr: SDK directory {} is missing {}. Expected a Dynamsoft Capture Vision {DCV_VERSION} directory with lib/ and resource/ subdirectories.",
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