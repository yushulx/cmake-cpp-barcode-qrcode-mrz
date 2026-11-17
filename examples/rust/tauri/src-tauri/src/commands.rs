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

use serde::Serialize;
use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_dialog::DialogExt;

use crate::media::{encode_jpeg, PageImage};
use crate::scanner::{self, BarcodeReader, BarcodeResult};
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
    /// `page`/`pages` describe the currently previewed page of the loaded file.
    /// `preview: None` means the input could not be rasterized at all.
    Decoded {
        status: String,
        preview: Option<Preview>,
        items: Vec<BarcodeItem>,
        page: u32,
        pages: u32,
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

    // CaptureMultiPages rasterizes every page of a PDF or multi-page TIFF, and the
    // SDK returns the exact pixels it decoded. Previewing those keeps the barcode
    // coordinates and the displayed image in one coordinate space.
    let mut pages: Vec<PageImage> = reader
        .decode_pages(&path)
        .unwrap_or_default()
        .into_iter()
        .filter(|page| !page.rgb.is_empty() && page.width > 0 && page.height > 0)
        .map(|page| PageImage {
            rgb: page.rgb,
            width: page.width as u32,
            height: page.height as u32,
            items: page.barcodes.into_iter().map(to_item).collect(),
            jpeg: None,
        })
        .collect();

    // The SDK cannot rasterize every format the dialog accepts (WebP, for one).
    // Fall back to the image crate for those so the preview still appears.
    if pages.is_empty() {
        if let Some(page) = decode_with_image_crate(&path, reader) {
            pages.push(page);
        }
    }

    let mut store = state.media.lock().unwrap();
    store.pages = pages;
    store.current = 0;
    store.preview_seq += 1;

    let page_count = store.pages.len() as u32;
    let total: usize = store.pages.iter().map(|page| page.items.len()).sum();
    let preview = store.pages.first().map(|page| Preview {
        seq: store.preview_seq,
        width: page.width,
        height: page.height,
    });
    let items = store
        .pages
        .first()
        .map(|page| page.items.clone())
        .unwrap_or_default();
    drop(store);

    let status = if total == 0 {
        "No barcode found.".to_string()
    } else if page_count > 1 {
        format!("Decoded {total} barcode(s) across {page_count} pages.")
    } else {
        format!("Decoded {total} barcode(s).")
    };

    DecodeReply::Decoded {
        status,
        preview,
        items,
        page: 0,
        pages: page_count,
    }
}

/// Switches the previewed page of the loaded file and returns that page's results.
#[tauri::command]
pub fn set_page(state: State<AppState>, page: u32) -> DecodeReply {
    let mut store = state.media.lock().unwrap();
    if store.pages.is_empty() {
        return DecodeReply::Message {
            status: "No document loaded.".into(),
        };
    }

    let index = (page as usize).min(store.pages.len() - 1);
    store.current = index;
    store.preview_seq += 1;

    let pages = store.pages.len() as u32;
    let current = &store.pages[index];
    let preview = Preview {
        seq: store.preview_seq,
        width: current.width,
        height: current.height,
    };
    let items = current.items.clone();

    DecodeReply::Decoded {
        status: format!("Page {} of {pages}.", index + 1),
        preview: Some(preview),
        items,
        page: index as u32,
        pages,
    }
}

/// Raster formats the SDK does not decode: preview the decoded pixels and run the
/// SDK's buffer path on that same buffer.
fn decode_with_image_crate(path: &str, reader: &BarcodeReader) -> Option<PageImage> {
    let image = image::io::Reader::open(path)
        .ok()?
        .with_guessed_format()
        .ok()?
        .decode()
        .ok()?;
    let rgb = image.to_rgb8();
    let (width, height) = (rgb.width(), rgb.height());
    let items = reader
        .decode_buffer(rgb.as_raw(), width as i32, height as i32)
        .unwrap_or_default()
        .into_iter()
        .map(to_item)
        .collect();
    Some(PageImage {
        rgb: rgb.into_raw(),
        width,
        height,
        items,
        jpeg: None,
    })
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
        let mut first_frame = true;
        while !stop.load(Ordering::Relaxed) {
            let Some(frame) = camera.grab() else {
                // The device can open while another app (a browser tab, Teams, ...)
                // keeps it busy; ReadSample then fails forever. Fail loudly instead
                // of leaving the UI on "Starting camera...".
                if first_frame && last_frame.elapsed() > Duration::from_secs(3) {
                    return camera_failed(
                        &app,
                        "Camera opened but delivered no frames. Close other apps using the \
                         camera (browser tabs, video calls) and try again."
                            .into(),
                    );
                }
                thread::sleep(Duration::from_millis(10));
                continue;
            };
            first_frame = false;

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