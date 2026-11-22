// Tauri injects its API as window.__TAURI__ (app.withGlobalTauri), so this demo
// needs no bundler and no npm packages at runtime.
const { invoke, convertFileSrc } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

const el = {
  open: document.getElementById("open"),
  path: document.getElementById("path"),
  decode: document.getElementById("decode"),
  camera: document.getElementById("camera"),
  preview: document.getElementById("preview"),
  stage: document.getElementById("stage"),
  image: document.getElementById("image"),
  overlay: document.getElementById("overlay"),
  placeholder: document.getElementById("placeholder"),
  results: document.getElementById("results"),
  count: document.getElementById("count"),
  status: document.getElementById("status"),
  pager: document.getElementById("pager"),
  prev: document.getElementById("prev"),
  next: document.getElementById("next"),
  pageLabel: document.getElementById("pageLabel"),
};

const view = {
  hasPreview: false,
  imageReady: false,
  width: 0,
  height: 0,
  items: [],
  cameraOn: false,
  page: 0,
  pages: 0,
};

// The Rust side serves pixels over a custom `media:` scheme; convertFileSrc
// builds the right URL for each platform (http://media.localhost on Windows).
function mediaUrl(name, seq) {
  return `${convertFileSrc(name, "media")}?seq=${seq}`;
}

function setStatus(text) {
  el.status.textContent = text;
}

function setCameraOn(on) {
  view.cameraOn = on;
  el.camera.textContent = on ? "Stop Camera" : "Start Camera";
}

function showPreview(url, width, height, waitForLoad = false) {
  view.hasPreview = true;
  view.width = width;
  view.height = height;
  // File pages wait for the <img> load event before the overlay is drawn; a
  // camera frame already has the previous frame on screen and must not flicker.
  view.imageReady = !waitForLoad;
  el.image.src = url;
  el.stage.hidden = false;
  el.placeholder.hidden = true;
  layout();
}

function clearPreview() {
  view.hasPreview = false;
  view.imageReady = false;
  view.width = 0;
  view.height = 0;
  view.page = 0;
  view.pages = 0;
  el.image.removeAttribute("src");
  el.stage.hidden = true;
  el.placeholder.hidden = false;
  updatePager();
}

// Multi-page PDFs/TIFFs get a pager under the preview; single images hide it.
function updatePager() {
  const show = view.hasPreview && view.pages > 1;
  el.pager.hidden = !show;
  if (!show) return;
  el.pageLabel.textContent = `Page ${view.page + 1} / ${view.pages}`;
  el.prev.disabled = view.page <= 0;
  el.next.disabled = view.page >= view.pages - 1;
}

// Letterbox the image inside the preview pane, then give the overlay canvas the
// image's own pixel size so barcode coordinates need no conversion.
function layout() {
  if (!view.hasPreview || !view.width || !view.height) return;
  const box = el.preview.getBoundingClientRect();
  const scale = Math.min(box.width / view.width, box.height / view.height);
  const width = view.width * scale;
  const height = view.height * scale;
  el.stage.style.width = `${width}px`;
  el.stage.style.height = `${height}px`;
  el.stage.style.left = `${(box.width - width) / 2}px`;
  el.stage.style.top = `${(box.height - height) / 2}px`;

  if (el.overlay.width !== view.width) el.overlay.width = view.width;
  if (el.overlay.height !== view.height) el.overlay.height = view.height;
  drawOverlay(scale);
}

function drawOverlay(scale) {
  const ctx = el.overlay.getContext("2d");
  ctx.clearRect(0, 0, el.overlay.width, el.overlay.height);
  // Wait for the new pixels to be on screen; drawing boxes over the previous
  // image (or a blank stage) while the new page decodes looks broken.
  if (!view.imageReady || !view.items.length) return;
  ctx.lineWidth = Math.max(1, 2 / scale);
  ctx.strokeStyle = "#00b0ff";
  // Stroke only: a translucent fill would tint the barcode and make a small
  // QR look colour-shifted once the preview is scaled down.
  for (const item of view.items) {
    ctx.beginPath();
    item.points.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
    ctx.closePath();
    ctx.stroke();
  }
}

// Barcode text is untrusted input, so every field goes through textContent.
function renderResults(items) {
  view.items = items;
  el.count.textContent = items.length;
  el.results.replaceChildren();

  if (!items.length) {
    const empty = document.createElement("div");
    empty.className = "empty";
    empty.textContent = "No results yet.";
    el.results.append(empty);
  }

  items.forEach((item, index) => {
    const row = document.createElement("div");
    row.className = index % 2 ? "row alt" : "row";
    for (const [className, text] of [
      ["row-format", `${index + 1}. ${item.format}`],
      ["row-text", item.text],
      ["row-location", item.location],
    ]) {
      const line = document.createElement("div");
      line.className = className;
      line.textContent = text;
      row.append(line);
    }
    el.results.append(row);
  });

  layout();
}

// Shared by a fresh decode and by page switches: both replies carry the preview
// and the results of whichever page is current.
function applyDecoded(reply) {
  setStatus(reply.status);
  view.page = reply.page ?? 0;
  view.pages = reply.pages ?? 0;
  if (reply.preview) {
    showPreview(
      mediaUrl("preview.jpg", reply.preview.seq),
      reply.preview.width,
      reply.preview.height,
      true,
    );
  } else {
    clearPreview();
  }
  renderResults(reply.items);
  updatePager();
}

async function decode(path) {
  setCameraOn(false); // the backend stops the camera before decoding a file
  const reply = await invoke("decode_path", { path });
  if (reply.kind === "message") {
    setStatus(reply.status);
    return;
  }
  applyDecoded(reply);
}

async function gotoPage(delta) {
  if (view.pages <= 1) return;
  const target = view.page + delta;
  if (target < 0 || target >= view.pages) return;
  const reply = await invoke("set_page", { page: target });
  if (reply.kind === "message") {
    setStatus(reply.status);
    return;
  }
  applyDecoded(reply);
}

el.open.addEventListener("click", async () => {
  if (view.cameraOn) {
    await invoke("stop_camera");
    setCameraOn(false);
  }
  const path = await invoke("open_image");
  if (!path) return;
  el.path.value = path;
  el.decode.disabled = false;
  await decode(path);
});

el.decode.addEventListener("click", () => decode(el.path.value));

el.prev.addEventListener("click", () => gotoPage(-1));
el.next.addEventListener("click", () => gotoPage(1));

document.addEventListener("keydown", (event) => {
  if (view.pages <= 1 || event.target === el.path) return;
  if (event.key === "ArrowLeft") {
    gotoPage(-1);
    event.preventDefault();
  } else if (event.key === "ArrowRight") {
    gotoPage(1);
    event.preventDefault();
  }
});

el.path.addEventListener("input", () => {
  el.decode.disabled = el.path.value.trim() === "";
  if (!el.path.value.trim()) clearPreview();
});

el.path.addEventListener("keydown", (event) => {
  if (event.key === "Enter") decode(el.path.value);
});

el.camera.addEventListener("click", async () => {
  if (view.cameraOn) {
    await invoke("stop_camera");
    setCameraOn(false);
    setStatus("Camera stopped.");
    return;
  }
  setCameraOn(true);
  setStatus("Starting camera...");
  try {
    await invoke("start_camera");
  } catch (err) {
    setCameraOn(false);
    setStatus(String(err));
  }
});

await listen("camera-frame", (event) => {
  const frame = event.payload;
  view.page = 0;
  view.pages = 0;
  showPreview(mediaUrl("camera.jpg", frame.seq), frame.width, frame.height);
  renderResults(frame.items);
  updatePager();
  setStatus(`Camera: ${frame.items.length} barcode(s)`);
});

await listen("camera-error", (event) => {
  setCameraOn(false);
  setStatus(event.payload);
});

new ResizeObserver(layout).observe(el.preview);

// Draw the barcode boxes only once the new page/frame is actually visible.
el.image.addEventListener("load", () => {
  view.imageReady = true;
  layout();
});

const startupError = await invoke("startup_status");
if (startupError) setStatus(startupError);