# Rust Barcode Scanner — Tauri GUI

The same barcode reader as the [Slint example](../README.md), rebuilt with
[Tauri](https://tauri.app) to show how a desktop GUI is put together when the UI
is a webview and the logic stays in Rust.

Both apps share **one native core**: `../src/scanner.rs`, `../src/camera.rs` and
`../lib/bridge.cpp` are compiled into this app unchanged, so decoding, licensing
and camera capture behave identically. Only the presentation layer differs.

## Features (identical to the Slint UI)

- Open an image or PDF through a native file dialog, or type/paste a path and press **Decode** (Enter works too)
- Image preview with a cyan quadrilateral drawn over every decoded barcode
- Results list showing each barcode's **format**, **text** and four corner **coordinates**
- Live camera scanning with the same preview, overlay and results
- Status bar reporting the decoded count or the error

## Quick start

Prerequisites: Rust (MSVC toolchain on Windows), the WebView2 runtime (preinstalled
on Windows 11), and the Dynamsoft SDK + `litecam` already vendored in this repo
(`dcv/`, `litecam/dist/`).

```bash
cd examples/rust/tauri/src-tauri
cargo run --release
```

Or double-click `run.bat`. No Node.js, no bundler and no `npm install` are needed
to build or run: the frontend is plain HTML/CSS/JS and Tauri embeds it into the
binary.

The Tauri CLI is optional and only adds dev-server orchestration and installers:

```bash
cd examples/rust/tauri
npm install
npm run dev      # tauri dev
npm run build    # tauri build → src-tauri/target/release/bundle/
```

## Project layout

```
tauri/
├── ui/                     # the frontend, loaded into the system webview
│   ├── index.html          # toolbar, preview pane, results list, status bar
│   ├── style.css           # the Slint look, in CSS
│   └── app.js              # invoke() calls, event listeners, canvas overlay
└── src-tauri/
    ├── Cargo.toml
    ├── build.rs            # tauri_build + cc: links Dynamsoft/litecam, copies DLLs
    ├── tauri.conf.json     # window, frontendDist, CSP, bundle icons
    ├── capabilities/       # what the webview is allowed to call
    ├── icons/
    └── src/
        ├── main.rs         # Builder: state, plugin, protocol, invoke_handler
        ├── commands.rs     # #[tauri::command] handlers + camera worker thread
        └── media.rs        # custom `media:` URI scheme serving PNG/JPEG bytes
```

## How the two halves talk

```
webview (ui/app.js)                         Rust (src-tauri/src)
───────────────────                         ────────────────────
invoke("decode_path", { path })   ───────►  #[tauri::command] fn decode_path
        ◄─────── DecodeReply (JSON) ────────┘
listen("camera-frame")            ◄───────  app.emit("camera-frame", CameraFrame)
<img src="media:…/preview.png">   ───────►  register_uri_scheme_protocol("media")
        ◄─────── image/png bytes ──────────┘
```

Three channels, three jobs: **commands** for request/response, **events** for
push notifications, and a **custom URI scheme** for bulk pixels.

## Slint → Tauri, piece by piece

| Concern | Slint (`../`) | Tauri (here) |
| --- | --- | --- |
| UI declaration | `ui/app.slint` | `ui/index.html` + `style.css` |
| Button click | `callback open-file()` / `ui.on_open_file(..)` | `invoke("open_image")` / `#[tauri::command]` |
| Push data into the UI | `ui.set_results(..)`, `ui.set_status(..)` | command return value, or `app.emit(..)` + `listen(..)` |
| Live camera repaint | `Timer` polls an `mpsc` channel every 33 ms | worker thread emits `camera-frame`; no polling |
| Getting pixels on screen | `Image::from_rgb8(SharedPixelBuffer)` | `<img>` fed by the `media:` protocol |
| Barcode quadrilaterals | Slint `Path` + `MoveTo`/`LineTo` | `<canvas>` sized to the image, drawn in image coordinates |
| Native file picker | `rfd::FileDialog` | `tauri-plugin-dialog` |
| Shared mutable state | `Rc<RefCell<Option<..>>>` | `.manage(AppState)` + `Mutex<..>` |
| UI compilation | `slint_build::compile("ui/app.slint")` | `tauri_build::build()` + `tauri::generate_context!()` |

### 1. Commands replace callbacks

A Slint callback is a closure registered on the UI object. A Tauri command is a
Rust function registered in `invoke_handler` and called from JavaScript:

```rust
#[tauri::command]
pub fn decode_path(state: State<AppState>, path: String) -> DecodeReply { .. }
```

```js
const reply = await invoke("decode_path", { path });
```

Arguments and return values are serialized with serde — `#[serde(rename_all =
"camelCase")]` keeps the JSON idiomatic on the JS side. `State<T>` is injected by
Tauri from whatever was passed to `.manage(..)`.

### 2. Async commands for blocking dialogs

`open_image` is `async` and uses the plugin's callback API plus a channel, so the
modal picker never blocks the thread that pumps window messages:

```rust
let (tx, mut rx) = tauri::async_runtime::channel(1);
dialog.pick_file(move |file| { let _ = tx.try_send(file); });
rx.recv().await.flatten()..
```

The other commands stay synchronous: Tauri runs them on the main thread, which
keeps the native Capture Vision handle on a single thread — the same contract the
Slint UI has. The camera worker owns a *second* reader instance for its thread.

### 3. Events replace the polling timer

Slint polls a channel on a 33 ms timer and repaints. Here the camera thread pushes:

```rust
app.emit("camera-frame", CameraFrame { seq, width, height, items })?;
```

```js
await listen("camera-frame", (event) => { .. });
```

The worker caps itself at ~30 fps so decoding and JPEG encoding never flood the
event channel with frames the webview would drop anyway.

### 4. A custom URI scheme for pixels

Sending a 1280×720 frame as base64 through IPC on every tick would waste megabytes
a second. Instead Rust keeps the latest preview PNG and camera JPEG in `MediaStore`
and serves them over a registered scheme:

```rust
.register_uri_scheme_protocol("media", media::handle)
```

```js
img.src = `${convertFileSrc("camera.jpg", "media")}?seq=${seq}`;
```

`convertFileSrc` produces `http://media.localhost/camera.jpg` on Windows and
`media://localhost/camera.jpg` elsewhere. The `seq` counter busts the image cache;
the payload travels as a normal HTTP response, not as JSON.

### 5. Overlay geometry

The canvas is given the **image's own pixel size** and stretched over the
letterboxed `<img>`, so barcode coordinates are drawn without any conversion.
`layout()` recomputes the contain-fit rectangle on resize, and the stroke width is
divided by the scale so the outline stays ~2 screen pixels like the Slint version.

### 6. Security: capabilities and CSP

`capabilities/default.json` grants the window only `core:default` (invoke + events).
The file dialog is opened from Rust, so the webview gets no dialog permission at
all. `tauri.conf.json` sets a CSP that allows the `media:` scheme for images and
`ipc:` for Tauri's own channel. Barcode text is untrusted input, so the results
list is built with `textContent`, never `innerHTML`.

### 7. No bundler

`"withGlobalTauri": true` injects `window.__TAURI__`, and `"frontendDist": "../ui"`
points at static files. That is the whole frontend toolchain: three files you can
edit and reload. Add Vite/React later by setting `devUrl` and `beforeDevCommand`
if you want a component framework.

### 8. Native build

`build.rs` does two jobs: `tauri_build::build()` generates the context, permissions
and Windows resource, and `cc` compiles the shared `../../lib/bridge.cpp` (+
`camera_bridge.cpp`) against the vendored SDK. It also copies the Dynamsoft DLLs,
`litecam.dll`, `Templates/` and `Models/` next to the executable, because the SDK
resolves them at runtime from the module directory.

## Notes

- **License**: shared with the other examples via `../src/scanner.rs`
  (`DEFAULT_LICENSE`). A bad key is reported in the status bar at startup.
- **Coordinate space**: the preview is re-encoded from raw RGB and the SDK decodes
  that same buffer, so EXIF-rotated photos cannot desync the overlay.
- **PDF**: decoded by the SDK, but not previewed (same as Slint).
- **Camera**: Windows only, via the bundled `litecam` (Media Foundation) library.
  Other platforms get the same status message as the Slint example.
- **Image formats**: JPEG, PNG, TIFF, BMP, GIF and WebP are previewed; anything
  else the `image` crate cannot read falls back to SDK-side file decoding.