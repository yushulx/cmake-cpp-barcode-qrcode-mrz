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
};

const view = { hasPreview: false, width: 0, height: 0, items: [], cameraOn: false };

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

function showPreview(url, width, height) {
  view.hasPreview = true;
  view.width = width;
  view.height = height;
  el.image.src = url;
  el.stage.hidden = false;
  el.placeholder.hidden = true;
  layout();
}

function clearPreview() {
  view.hasPreview = false;
  view.width = 0;
  view.height = 0;
  el.image.removeAttribute("src");
  el.stage.hidden = true;
  el.placeholder.hidden = false;
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
  if (!view.items.length) return;
  ctx.lineWidth = Math.max(1, 2 / scale);
  ctx.strokeStyle = "#00b0ff";
  ctx.fillStyle = "rgba(0, 176, 255, 0.125)";
  for (const item of view.items) {
    ctx.beginPath();
    item.points.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
    ctx.closePath();
    ctx.fill();
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

async function decode(path) {
  setCameraOn(false); // the backend stops the camera before decoding a file
  const reply = await invoke("decode_path", { path });
  setStatus(reply.status);
  if (reply.kind === "message") return;

  renderResults(reply.items);
  if (reply.preview) {
    showPreview(
      mediaUrl("preview.png", reply.preview.seq),
      reply.preview.width,
      reply.preview.height,
    );
  } else {
    clearPreview();
  }
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
  showPreview(mediaUrl("camera.jpg", frame.seq), frame.width, frame.height);
  renderResults(frame.items);
  setStatus(`Camera: ${frame.items.length} barcode(s)`);
});

await listen("camera-error", (event) => {
  setCameraOn(false);
  setStatus(event.payload);
});

new ResizeObserver(layout).observe(el.preview);

const startupError = await invoke("startup_status");
if (startupError) setStatus(startupError);