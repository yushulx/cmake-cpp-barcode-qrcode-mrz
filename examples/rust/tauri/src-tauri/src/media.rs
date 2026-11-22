//! A custom `media:` URI scheme. The webview pulls pixels from here instead of
//! receiving megabytes of base64 through the IPC channel on every camera frame.

use image::ImageEncoder;
use tauri::http::{header, Request, Response, StatusCode};
use tauri::{Manager, UriSchemeContext, Wry};

use crate::commands::BarcodeItem;
use crate::AppState;

/// One previewable page of the loaded file. Pixels stay raw until the webview
/// asks for the page, then the JPEG is encoded once and cached.
pub struct PageImage {
    pub rgb: Vec<u8>,
    pub width: u32,
    pub height: u32,
    pub items: Vec<BarcodeItem>,
    pub jpeg: Option<Vec<u8>>,
}

#[derive(Default)]
pub struct MediaStore {
    /// `/preview.jpg` serves `pages[current]`; a single image is just one page.
    pub pages: Vec<PageImage>,
    pub current: usize,
    pub preview_seq: u64,
    pub frame_jpeg: Vec<u8>,
    pub frame_seq: u64,
}

pub fn handle(ctx: UriSchemeContext<'_, Wry>, request: Request<Vec<u8>>) -> Response<Vec<u8>> {
    let state = ctx.app_handle().state::<AppState>();
    let mut store = state.media.lock().unwrap();
    let current = store.current;

    // The frontend appends ?seq=N so each new frame bypasses the image cache.
    let (body, content_type) = match request.uri().path() {
        "/preview.jpg" => match store.pages.get_mut(current) {
            Some(page) => {
                if page.jpeg.is_none() {
                    if let Some(jpeg) = encode_jpeg(&page.rgb, page.width, page.height) {
                        page.jpeg = Some(jpeg);
                        // The encoded page replaces the raw buffer; re-encoding is
                        // never needed because the JPEG itself is cached.
                        page.rgb = Vec::new();
                    }
                }
                (page.jpeg.clone().unwrap_or_default(), "image/jpeg")
            }
            None => (Vec::new(), "image/jpeg"),
        },
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

pub fn encode_jpeg(rgb: &[u8], width: u32, height: u32) -> Option<Vec<u8>> {
    let mut jpeg = Vec::new();
    image::codecs::jpeg::JpegEncoder::new_with_quality(&mut jpeg, 80)
        .write_image(rgb, width, height, image::ColorType::Rgb8)
        .ok()?;
    Some(jpeg)
}