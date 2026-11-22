//! A custom `media:` URI scheme. The webview pulls pixels from here instead of
//! receiving megabytes of base64 through the IPC channel on every camera frame.

use tauri::http::{header, Request, Response, StatusCode};
use tauri::{Manager, UriSchemeContext, Wry};

use crate::AppState;

#[derive(Default)]
pub struct MediaStore {
    pub preview_png: Vec<u8>,
    pub preview_seq: u64,
    pub frame_jpeg: Vec<u8>,
    pub frame_seq: u64,
}

pub fn handle(ctx: UriSchemeContext<'_, Wry>, request: Request<Vec<u8>>) -> Response<Vec<u8>> {
    let state = ctx.app_handle().state::<AppState>();
    let store = state.media.lock().unwrap();

    // The frontend appends ?seq=N so each new frame bypasses the image cache.
    let (body, content_type) = match request.uri().path() {
        "/preview.png" => (store.preview_png.clone(), "image/png"),
        "/camera.jpg" => (store.frame_jpeg.clone(), "image/jpeg"),
        _ => {
            return Response::builder()
                .status(StatusCode::NOT_FOUND)
                .body(Vec::new())
                .unwrap()
        }
    };

    Response::builder()
        .status(StatusCode::OK)
        .header(header::CONTENT_TYPE, content_type)
        .header(header::CACHE_CONTROL, "no-store")
        .body(body)
        .unwrap()
}