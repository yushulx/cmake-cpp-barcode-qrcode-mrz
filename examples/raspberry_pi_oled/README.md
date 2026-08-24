# Headless C++ Barcode Reader for Raspberry Pi (with OLED Display)

This sample turns a Raspberry Pi into an **easy-to-run barcode and QR code reader**.
It captures frames from a USB webcam with **Video4Linux2 (V4L2)** — no OpenCV, no
GUI, no desktop environment needed — decodes barcodes with the
[Dynamsoft Capture Vision SDK](https://www.dynamsoft.com/capture-vision/overview/)
(`dcv` in this repository), and shows the decoded text on an
**SSD1306 OLED display** (`0.91"` 128x32) as well as on the console. The OLED
display is **optional**: without it the program keeps working and only prints
results to the console.

## Hardware

- [Raspberry Pi 4](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) (Raspberry Pi OS, 64-bit)
- Raspberry Pi RGB Cooling HAT with adjustable fan and OLED display (optional — any SSD1306 display works)
- USB Webcam (UVC) (optional — a virtual camera can be used for testing)

## Prerequisites

1. Install the build tools and, if you plan to use the OLED display, enable the
   **I2C** interface (`sudo raspi-config` → Interface Options → I2C):

   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake
   sudo usermod -aG i2c $USER
   ```

   The Dynamsoft libraries (Linux **aarch64**, x64), templates and models in the
   `dcv` directory are already checked into this repository — nothing else needs
   to be downloaded.

## Build

```bash
mkdir build
cd build
cmake ..
make
```

The `barcode_reader` executable is created together with the Dynamsoft `.so`
libraries and the `Models`/`Templates` runtime resources (copied next to the
executable in `CMakeLists.txt`).

## Run

```bash
./barcode_reader
```

By default the program reads from `/dev/video0` at 640x480 and stops
automatically after **120 seconds**. Usage:

```text
./barcode_reader [--device /dev/video0] [--width 640] [--height 480] [--duration 120] [--no-oled]
```

| Option          | Default      | Description                              |
|-----------------|--------------|------------------------------------------|
| `--device`      | `/dev/video0`| Camera capture device                    |
| `--width`, `--height` | `640 480` | Camera resolution (YUYV)     |
| `--duration`    | `120`        | Auto-stop after N seconds (0 = disabled) |
| `--no-oled`     | off          | Skip the OLED display entirely           |

The **OLED display is optional**. On startup the program probes `/dev/i2c-1` at
address `0x3C`; if no display responds, it prints a notice and keeps running
with results shown on the console only. `--no-oled` skips the probe entirely
for users who do not have the display attached:

```text
Dynamsoft Barcode Reader (headless)
Camera: /dev/video0 (640x480)
OLED: enabled (128x32, /dev/i2c-1)
Auto-stop: 120 seconds
Press 'q' or Ctrl+C to quit.
Frame 1: QR_CODE -> https://www.dynamsoft.com/
Processed 101 frames in 5.0s (20.2 frames per second), barcode found in 101 frames
```

The decoding loop never runs forever. The program stops when any of the
following happens:

- The **`q`** key is pressed in the terminal
- **Ctrl+C** is received (SIGINT, SIGTERM)
- The run duration is reached

## Test Without a Webcam (Virtual Camera)

No physical camera? Create a virtual V4L2 camera with **v4l2loopback** and feed
any barcode image into it:

```bash
sudo apt install -y v4l2loopback-dkms ffmpeg
sudo modprobe v4l2loopback exclusive_caps=1 video_nr=2

# Show the QR code image to "the camera" at 25 fps
ffmpeg -re -loop 1 -i qr.png -vf scale=640:480 -pix_fmt yuyv422 -f v4l2 /dev/video2
```

Then run the reader against the virtual device:

```bash
./barcode_reader --device /dev/video2 --duration 30
```

You should see the QR code decoded on the first frame. Use
`v4l2-ctl --list-devices` to find the loopback device number if it differs.

## License

The program uses a **free public trial license** by default
(`DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==`, based on the Dynamsoft
organization ID). A network connection is required for this license.

For a private 30-day trial license, register at
<https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform>
and replace `LICENSE_KEY` in `main.cxx`.

## How It Works

- **V4L2 capture** (`v4l2_capture.cpp`): streams YUYV frames through
  memory-mapped buffers and converts them to RGB888. No OpenCV dependency.
- **Barcode decoding** (`main.cxx`): creates a `CCaptureVisionRouter` from the
  Dynamsoft Capture Vision SDK, feeds each frame via `Capture()` with the
  `ReadBarcodes` preset template, and reads back `CDecodedBarcodesResult`.
- **OLED display** (`ssd1306_i2c.c`): a plain `i2c-dev` implementation
  (`/dev/i2c-1`, address `0x3C`) — no WiringPi required. The display is probed
  at startup and skipped automatically when it is not attached.

## Blog

[How to Build C++ Barcode Reader App on Raspberry Pi 4](https://www.dynamsoft.com/codepool/raspberry-pi-cpp-barcode-reader.html)
