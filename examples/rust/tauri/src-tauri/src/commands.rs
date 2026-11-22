//! The command layer: everything the webview can ask Rust to do.
//!
//! Sync commands run on the main thread, which keeps the native reader handle on
//! one thread — the same contract the Slint UI has. `open_image` is async because
//! a modal file dialog must not block the thread that pumps window messages.

use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

use image::ImageEncoder;
use serde::Serialize;
use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_dialog::DialogExt;

use crate::scanner::{self, BarcodeResult};
use crate::AppState;

const LICENSE_HINT: &str = "Set a valid license in examples/rust/src/scanner.rs (DEFAULT_LICENSE).";

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct BarcodeItem {
    pub format: String,
    pub text: String,
    pub location: String,
    pub points: [(i32, i32); 4],
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Preview {
    pub seq: u64,
    pub width: u32,
    pub height: u32,
}

#[derive(Serialize)]
#[serde(tag = "kind", rename_all = "camelCase")]
pub enum DecodeReply {
    /// Nothing was decoded; keep the current preview and results.
    Message { status: String },
    /// `preview: None` means the input has no raster preview (a PDF).
    Decoded {
        status: String,
        preview: Option<Preview>,
        items: Vec<BarcodeItem>,
    },
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CameraFrame {
    pub seq: u64,
    pub width: u32,
    pub height: u32,
    pub items: Vec<BarcodeItem>,
}

/// Reported once at startup so a bad license shows up before the first decode.
#[tauri::command]
pub fn startup_status(state: State<AppState>) -> Option<String> {
    state
        .reader
        .lock()
        .unwrap()
        .as_ref()
        .err()
        .map(|err| format!("{err}. {LICENSE_HINT}"))
}

/// Native file picker, seeded at the repository's `images/` folder.
#[tauri::command]
pub async fn open_image(app: AppHandle) -> Option<String> {
    let mut dialog = app
        .dialog()
        .file()
        .set_title("Open image or PDF")
        .add_filter(
            "Images & PDF",
            &["jpg", "jpeg", "png", "bmp", "tif", "tiff", "gif", "webp", "pdf"],
        );
    if let Some(dir) = samples_dir() {
        dialog = dialog.set_directory(dir);
    }

    let (tx, mut rx) = tauri::async_runtime::channel(1);
    dialog.pick_file(move |file| {
        let _ = tx.try_send(file);
    });

    rx.recv()
        .await
        .flatten()
        .and_then(|path| path.into_path().ok())
        .map(|path| path.to_string_lossy().into_owned())
}

#[tauri::command]
pub fn decode_path(state: State<AppState>, path: String) -> DecodeReply {
    stop_camera_inner(&state);

    let path = path.trim().to_string();
    if path.is_empty() {
        return DecodeReply::Message {
            status: "Enter an image path or click \"Open Image...\".".into(),
        };
    }
    if !std::path::Path::new(&path).exists() {
        return DecodeReply::Message {
            status: format!("File not found: {path}"),
        };
    }

    let guard = state.reader.lock().unwrap();
    let reader = match guard.as_ref() {
        Ok(reader) => &reader.0,
        Err(err) => {
            return DecodeReply::Message {
                status: format!("{err}. {LICENSE_HINT}"),
            }
        }
    };

    // Decode the same pixels we display. The SDK applies EXIF orientation when it
    // reads a file itself, but the preview is re-encoded from raw pixels, so both
    // stay in one coordinate space. PDFs fall back to letting the SDK read the file.
    let loaded = image::io::Reader::open(&path)
        .ok()
        .and_then(|reader| reader.with_guessed_format().ok())
        .and_then(|reader| reader.decode().ok());

    let (raster, decoded) = match &loaded {
        Some(image) => {
            let rgb = image.to_rgb8();
            let (width, height) = (rgb.width(), rgb.height());
            let decoded = reader.decode_buffer(rgb.as_raw(), width as i32, height as i32);
            let png = encode_png(rgb.as_raw(), width, height).map(|png| (png, width, height));
            (png, decoded)
        }
        None => (None, reader.decode(&path)),
    };

    let preview = raster.map(|(png, width, height)| {
        let mut store = state.media.lock().unwrap();
        store.preview_png = png;
        store.preview_seq += 1;
        Preview {
            seq: store.preview_seq,
            width,
            height,
        }
    });

    match decoded {
        Ok(barcodes) => {
            let count = barcodes.len();
            let status = if count == 0 {
                "No barcode found.".to_string()
            } else {
                format!("Decoded {count} barcode(s).")
            };
            DecodeReply::Decoded {
                status,
                preview,
                items: barcodes.into_iter().map(to_item).collect(),
            }
        }
        Err(err) => DecodeReply::Decoded {
            status: format!("Decode error: {err}"),
            preview,
            items: Vec::new(),
        },
    }
}

#[tauri::command]
pub fn start_camera(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    let mut slot = state.camera_stop.lock().unwrap();
    if slot.is_some() {
        return Ok(());
    }

    #[cfg(windows)]
    {
        let stop = Arc::new(AtomicBool::new(false));
        *slot = Some(stop.clone());
        spawn_camera(app, stop);
        Ok(())
    }
    #[cfg(not(windows))]
    {
        let _ = app;
        Err("Camera capture is only wired for Windows in this example.".into())
    }
}

#[tauri::command]
pub fn stop_camera(state: State<AppState>) {
    stop_camera_inner(&state);
}

fn stop_camera_inner(state: &AppState) {
    if let Some(stop) = state.camera_stop.lock().unwrap().take() {
        stop.store(true, Ordering::Relaxed);
    }
}

#[cfg(windows)]
fn spawn_camera(app: AppHandle, stop: Arc<AtomicBool>) {
    thread::spawn(move || {
        let mut camera = match crate::litecam::Camera::open(0) {
            Ok(camera) => camera,
            Err(err) => return camera_failed(&app, err),
        };
        // The camera owns its reader: the native handle stays on this thread.
        let reader = match scanner::BarcodeReader::new(scanner::DEFAULT_LICENSE) {
            Ok(reader) => reader,
            Err(err) => return camera_failed(&app, err),
        };

        let mut last_frame = Instant::now();
        while !stop.load(Ordering::Relaxed) {
            let Some(frame) = camera.grab() else {
                thread::sleep(Duration::from_millis(10));
                continue;
            };

            // Cap the stream near 30 fps so decoding and JPEG encoding do not
            // flood the event channel with frames the webview never shows.
            let pause = Duration::from_millis(33).saturating_sub(last_frame.elapsed());
            if !pause.is_zero() {
                thread::sleep(pause);
            }
            if stop.load(Ordering::Relaxed) {
                break;
            }

            let items = reader
                .decode_buffer(&frame.rgb, frame.width, frame.height)
                .unwrap_or_default()
                .into_iter()
                .map(to_item)
                .collect();
            let Some(jpeg) = encode_jpeg(&frame.rgb, frame.width as u32, frame.height as u32) else {
                continue;
            };

            let seq = {
                let state = app.state::<AppState>();
                let mut store = state.media.lock().unwrap();
                store.frame_jpeg = jpeg;
                store.frame_seq += 1;
                store.frame_seq
            };
            let _ = app.emit(
                "camera-frame",
                CameraFrame {
                    seq,
                    width: frame.width as u32,
                    height: frame.height as u32,
                    items,
                },
            );
            last_frame = Instant::now();
        }
    });
}

#[cfg(windows)]
fn camera_failed(app: &AppHandle, err: String) {
    app.state::<AppState>().camera_stop.lock().unwrap().take();
    let _ = app.emit("camera-error", err);
}

fn encode_png(rgb: &[u8], width: u32, height: u32) -> Option<Vec<u8>> {
    let mut png = Vec::new();
    image::codecs::png::PngEncoder::new_with_quality(
        &mut png,
        image::codecs::png::CompressionType::Fast,
        image::codecs::png::FilterType::Adaptive,
    )
    .write_image(rgb, width, height, image::ColorType::Rgb8)
    .ok()?;
    Some(png)
}

fn encode_jpeg(rgb: &[u8], width: u32, height: u32) -> Option<Vec<u8>> {
    let mut jpeg = Vec::new();
    image::codecs::jpeg::JpegEncoder::new_with_quality(&mut jpeg, 80)
        .write_image(rgb, width, height, image::ColorType::Rgb8)
        .ok()?;
    Some(jpeg)
}

fn to_item(barcode: BarcodeResult) -> BarcodeItem {
    BarcodeItem {
        format: barcode.format,
        text: barcode.text,
        location: format_location(&barcode.points),
        points: barcode.points,
    }
}

fn format_location(points: &[(i32, i32); 4]) -> String {
    points
        .iter()
        .map(|(x, y)| format!("({x}, {y})"))
        .collect::<Vec<_>>()
        .join("  ")
}

/// Best-effort locate the repository's `images/` folder to seed the file dialog.
fn samples_dir() -> Option<PathBuf> {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let from_manifest = manifest.join("../../../../images");
    if from_manifest.is_dir() {
        return std::path::absolute(from_manifest).ok();
    }
    if let Ok(exe) = std::env::current_exe() {
        for ancestor in exe.ancestors() {
            let candidate = ancestor.join("images");
            if candidate.is_dir() {
                return Some(candidate.to_path_buf());
            }
        }
    }
    None
}