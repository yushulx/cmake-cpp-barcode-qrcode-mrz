#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

// One native core, two GUIs: the FFI wrapper and the C++ bridge are the same files
// the Slint example uses, so both apps decode identically.
#[path = "../../../src/scanner.rs"]
mod scanner;
#[cfg(windows)]
#[path = "../../../src/camera.rs"]
mod litecam;

mod commands;
mod media;

use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};

use scanner::{BarcodeReader, DEFAULT_LICENSE};

/// `BarcodeReader` owns a raw C++ handle, which is neither Send nor Sync. Tauri
/// state must be, and the mutex around it keeps every access serialized.
pub struct Reader(BarcodeReader);
unsafe impl Send for Reader {}

pub struct AppState {
    pub reader: Mutex<Result<Reader, String>>,
    pub media: Mutex<media::MediaStore>,
    pub camera_stop: Mutex<Option<Arc<AtomicBool>>>,
}

fn main() {
    let reader = BarcodeReader::new(DEFAULT_LICENSE).map(Reader);

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState {
            reader: Mutex::new(reader),
            media: Mutex::new(media::MediaStore::default()),
            camera_stop: Mutex::new(None),
        })
        // Serves the preview PNG and the live camera JPEG to the <img> element.
        .register_uri_scheme_protocol("media", media::handle)
        .invoke_handler(tauri::generate_handler![
            commands::startup_status,
            commands::open_image,
            commands::decode_path,
            commands::set_page,
            commands::start_camera,
            commands::stop_camera,
        ])
        .run(tauri::generate_context!())
        .expect("error while running the Tauri application");
}