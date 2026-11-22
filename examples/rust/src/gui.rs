#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

slint::include_modules!();

mod scanner;
#[cfg(windows)]
mod camera;

use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use scanner::{BarcodeReader, DEFAULT_LICENSE};
use slint::{Image, ModelRc, Rgb8Pixel, SharedPixelBuffer, SharedString, Timer, TimerMode, VecModel};

struct CameraControl {
    stop: Arc<AtomicBool>,
}

struct CameraMsg {
    rgb: Vec<u8>,
    width: i32,
    height: i32,
    results: Vec<scanner::BarcodeResult>,
    error: Option<String>,
}

fn cam_error(msg: String) -> CameraMsg {
    CameraMsg { rgb: Vec::new(), width: 0, height: 0, results: Vec::new(), error: Some(msg) }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let ui = MainWindow::new()?;

    let reader = match BarcodeReader::new(DEFAULT_LICENSE) {
        Ok(reader) => Rc::new(reader),
        Err(err) => {
            ui.set_status(SharedString::from(format!(
                "{err}. Set a valid license in src/scanner.rs (DEFAULT_LICENSE)."
            )));
            ui.run()?;
            return Ok(());
        }
    };

    let camera_ctrl: Rc<RefCell<Option<CameraControl>>> = Rc::new(RefCell::new(None));
    let camera_rx: Rc<RefCell<Option<Receiver<CameraMsg>>>> = Rc::new(RefCell::new(None));

    // "Open Image..." button: native file picker, then decode the selection.
    {
        let weak = ui.as_weak();
        let reader = reader.clone();
        let camera_ctrl = camera_ctrl.clone();
        let camera_rx = camera_rx.clone();
        ui.on_open_file(move || {
            let Some(ui) = weak.upgrade() else { return };
            stop_camera(&ui, &camera_ctrl, &camera_rx);
            let mut dialog = rfd::FileDialog::new()
                .set_title("Open image or PDF")
                .add_filter(
                    "Images & PDF",
                    &["jpg", "jpeg", "png", "bmp", "tif", "tiff", "gif", "webp", "pdf"],
                );
            if let Some(dir) = samples_dir() {
                dialog = dialog.set_directory(dir);
            }
            if let Some(path) = dialog.pick_file() {
                let path_str = path.to_string_lossy().into_owned();
                ui.set_file_path(SharedString::from(path_str.clone()));
                decode_and_show(&ui, &reader, &path_str);
            }
        });
    }

    // "Decode" button / Enter in the path field: decode the typed path.
    {
        let weak = ui.as_weak();
        let reader = reader.clone();
        let camera_ctrl = camera_ctrl.clone();
        let camera_rx = camera_rx.clone();
        ui.on_decode_file(move || {
            let Some(ui) = weak.upgrade() else { return };
            stop_camera(&ui, &camera_ctrl, &camera_rx);
            let path = ui.get_file_path().trim().to_string();
            decode_and_show(&ui, &reader, &path);
        });
    }

    // Clear a stale preview when the path box is emptied.
    {
        let weak = ui.as_weak();
        ui.on_path_edited(move |text| {
            let Some(ui) = weak.upgrade() else { return };
            if text.trim().is_empty() {
                ui.set_has_preview(false);
                ui.set_preview(Image::default());
            }
        });
    }

    // "Start/Stop Camera" button.
    {
        let weak = ui.as_weak();
        let camera_ctrl = camera_ctrl.clone();
        let camera_rx = camera_rx.clone();
        ui.on_toggle_camera(move || {
            let Some(ui) = weak.upgrade() else { return };
            let mut ctrl = camera_ctrl.borrow_mut();
            if let Some(c) = ctrl.take() {
                c.stop.store(true, Ordering::Relaxed);
                *camera_rx.borrow_mut() = None;
                ui.set_camera_on(false);
                ui.set_status(SharedString::from("Camera stopped."));
            } else {
                #[cfg(windows)]
                {
                    let (tx, rx) = mpsc::channel();
                    *camera_rx.borrow_mut() = Some(rx);
                    let stop = Arc::new(AtomicBool::new(false));
                    *ctrl = Some(CameraControl { stop: stop.clone() });
                    ui.set_camera_on(true);
                    ui.set_status(SharedString::from("Starting camera..."));
                    spawn_camera(tx, stop);
                }
                #[cfg(not(windows))]
                {
                    ui.set_status(SharedString::from(
                        "Camera capture is only wired for Windows in this example.",
                    ));
                }
            }
        });
    }

    // Poll the camera thread and repaint preview + overlay + results live.
    let timer = Timer::default();
    {
        let weak = ui.as_weak();
        let camera_ctrl = camera_ctrl.clone();
        let camera_rx = camera_rx.clone();
        timer.start(TimerMode::Repeated, Duration::from_millis(33), move || {
            let Some(ui) = weak.upgrade() else { return };
            if !ui.get_camera_on() {
                return;
            }
            let mut latest = None;
            {
                let mut rx = camera_rx.borrow_mut();
                let Some(rx) = rx.as_mut() else { return };
                while let Ok(msg) = rx.try_recv() {
                    latest = Some(msg);
                }
            }
            if let Some(msg) = latest {
                if !apply_camera_msg(&ui, msg) {
                    camera_ctrl.borrow_mut().take();
                    *camera_rx.borrow_mut() = None;
                }
            }
        });
    }

    ui.run()?;
    Ok(())
}

fn stop_camera(
    ui: &MainWindow,
    ctrl: &Rc<RefCell<Option<CameraControl>>>,
    rx: &Rc<RefCell<Option<Receiver<CameraMsg>>>>,
) {
    if let Some(c) = ctrl.borrow_mut().take() {
        c.stop.store(true, Ordering::Relaxed);
    }
    *rx.borrow_mut() = None;
    ui.set_camera_on(false);
}

#[cfg(windows)]
fn spawn_camera(tx: Sender<CameraMsg>, stop: Arc<AtomicBool>) {
    thread::spawn(move || {
        let mut cam = match camera::Camera::open(0) {
            Ok(cam) => cam,
            Err(err) => {
                let _ = tx.send(cam_error(err));
                return;
            }
        };
        let reader = match BarcodeReader::new(DEFAULT_LICENSE) {
            Ok(reader) => reader,
            Err(err) => {
                let _ = tx.send(cam_error(err));
                return;
            }
        };
        while !stop.load(Ordering::Relaxed) {
            match cam.grab() {
                Some(frame) => {
                    let results = reader
                        .decode_buffer(&frame.rgb, frame.width, frame.height)
                        .unwrap_or_default();
                    let msg = CameraMsg {
                        rgb: frame.rgb,
                        width: frame.width,
                        height: frame.height,
                        results,
                        error: None,
                    };
                    if tx.send(msg).is_err() {
                        break;
                    }
                }
                None => thread::sleep(Duration::from_millis(10)),
            }
        }
    });
}

/// Returns false when the camera should be torn down (error reported).
fn apply_camera_msg(ui: &MainWindow, msg: CameraMsg) -> bool {
    if let Some(err) = msg.error {
        ui.set_camera_on(false);
        ui.set_status(SharedString::from(err));
        return false;
    }
    if msg.width <= 0 || msg.height <= 0 {
        return true;
    }
    let expected = (msg.width as usize) * (msg.height as usize) * 3;
    if msg.rgb.len() == expected {
        let mut buffer = SharedPixelBuffer::<Rgb8Pixel>::new(msg.width as u32, msg.height as u32);
        buffer.make_mut_bytes().copy_from_slice(&msg.rgb);
        ui.set_preview(Image::from_rgb8(buffer));
        ui.set_has_preview(true);
        ui.set_image_w(msg.width as f32);
        ui.set_image_h(msg.height as f32);
    }
    let count = msg.results.len();
    let items: Vec<BarcodeItem> = msg.results.into_iter().map(to_item).collect();
    ui.set_results(ModelRc::new(Rc::new(VecModel::from(items))));
    ui.set_status(SharedString::from(format!("Camera: {count} barcode(s)")));
    true
}

fn decode_and_show(ui: &MainWindow, reader: &Rc<BarcodeReader>, path: &str) {
    if path.is_empty() {
        ui.set_status(SharedString::from("Enter an image path or click \"Open Image...\"."));
        return;
    }
    if !Path::new(path).exists() {
        ui.set_status(SharedString::from(format!("File not found: {path}")));
        return;
    }

    // Decode the same pixels we display. The SDK applies EXIF orientation when
    // reading a file but Slint shows raw pixels, so decoding the file directly
    // would desync the overlay on EXIF-rotated photos. Feeding the displayed RGB
    // buffer keeps both in one coordinate space. Non-raster inputs (PDF) fall
    // back to letting the SDK read the file.
    let loaded = Image::load_from_path(Path::new(path));
    let decoded = match &loaded {
        Ok(image) => match image.to_rgb8() {
            Some(rgb) => {
                reader.decode_buffer(rgb.as_bytes(), rgb.width() as i32, rgb.height() as i32)
            }
            None => reader.decode(path),
        },
        Err(_) => reader.decode(path),
    };

    match loaded {
        Ok(image) => {
            let size = image.size();
            ui.set_image_w(size.width as f32);
            ui.set_image_h(size.height as f32);
            ui.set_preview(image);
            ui.set_has_preview(true);
        }
        Err(_) => {
            ui.set_preview(Image::default());
            ui.set_has_preview(false);
        }
    }

    match decoded {
        Ok(barcodes) => {
            let count = barcodes.len();
            let items: Vec<BarcodeItem> = barcodes.into_iter().map(to_item).collect();
            ui.set_results(ModelRc::new(Rc::new(VecModel::from(items))));
            let summary = if count == 0 {
                "No barcode found.".to_string()
            } else {
                format!("Decoded {count} barcode(s).")
            };
            ui.set_status(SharedString::from(summary));
        }
        Err(err) => {
            ui.set_results(ModelRc::default());
            ui.set_status(SharedString::from(format!("Decode error: {err}")));
        }
    }
}

fn to_item(b: scanner::BarcodeResult) -> BarcodeItem {
    BarcodeItem {
        format: SharedString::from(b.format),
        text: SharedString::from(b.text),
        location: SharedString::from(format_location(&b.points)),
        x1: b.points[0].0 as f32,
        y1: b.points[0].1 as f32,
        x2: b.points[1].0 as f32,
        y2: b.points[1].1 as f32,
        x3: b.points[2].0 as f32,
        y3: b.points[2].1 as f32,
        x4: b.points[3].0 as f32,
        y4: b.points[3].1 as f32,
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
    let from_manifest = manifest.join("../../images");
    if from_manifest.is_dir() {
        return from_manifest.canonicalize().ok();
    }
    if let Ok(exe) = std::env::current_exe() {
        for ancestor in exe.ancestors() {
            let candidate = ancestor.join("images");
            if candidate.is_dir() {
                return Some(candidate);
            }
        }
    }
    None
}