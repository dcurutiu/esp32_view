# esp32_view

A lightweight embedded application that captures photos on request (over MQTT) and uploads them to [Cloudinary](https://cloudinary.com/) storage. Runs on an ESP32-S3 and is designed for unattended, continuous operation.

> **⚠️ Disclaimer**
> Cloudinary is used here as a **free, reliable HTTP endpoint to store images during prototyping and testing**. The free tier provides 25 credits/month — sufficient for thousands of small image uploads. It is not an official integration and carries no guarantees around uptime, data retention, or compliance beyond Cloudinary's own terms.
>
> The storage backend is **not hardcoded into the architecture** — it can be swapped out entirely for any other HTTP-based storage service (AWS S3, Cloudflare R2, Firebase Storage, a self-hosted endpoint, etc.) by replacing the `ApiSecret` values and adjusting the upload logic in `cloud_storage/cloudinary_manager`.

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI PSRAM) |
| Camera | OV3660 |

## Project Layout

```
esp32_view/
├── esp32_view.ino      # sketch entry point (setup/loop)
├── sketch.yaml         # arduino-cli defaults: board (+ partition scheme), port
├── src/                # all modules, compiled recursively by arduino-cli
│   ├── camera/
│   ├── cloud_storage/
│   ├── led/
│   ├── mqtt/
│   └── wifi/
├── docs/
├── .clangd             # clangd config (strips Xtensa flags clang rejects)
└── .vscode/settings.json  # clangd arguments
```

## Build System

The project is built with **arduino-cli**. The sketch folder name matches `esp32_view.ino`, and `sketch.yaml` provides the board, partition scheme and port, so no extra flags are needed. `arduino-cli` compiles `src/` recursively; headers inside `src/` are included relative to the including file (e.g. `"../led/led_manager.h"`), and from the `.ino` as `"src/..."`, so no include-path flags are required either. Editor setup for VS Code + clangd is described at the end of this document.

---

## Configuration Files

Before building, you must provide two header files with your own credentials. These files are **not tracked by git** and must be created manually.

### `src/wifi/wifi_passwd.h`

Holds the Wi-Fi network credentials:

```cpp
#ifndef WIFI_PASSWD_H
#define WIFI_PASSWD_H

struct WifiPasswd
{
    static constexpr char *ssid = "YOUR_SSID";
    static constexpr char *password = "YOUR_PASSWORD";
};

#endif // WIFI_PASSWD_H
```

### `src/cloud_storage/api_secret.h`

Holds the Cloudinary credentials and upload configuration:

```cpp
#ifndef API_SECRET_H
#define API_SECRET_H

struct ApiSecret
{
    constexpr static const char *CLOUDINARY_HOST = "api.cloudinary.com";
    constexpr static const char *CLOUD_NAME      = "your_cloud_name";
    constexpr static const char *API_KEY         = "your_api_key";
    constexpr static const char *API_SECRET      = "your_api_secret";

    // Fixed public_id → the asset URL never changes between uploads
    // (signed uploads send overwrite=true and invalidate=true, no upload preset needed)
    constexpr static const char *PUBLIC_ID = "latest";

    // Reliable ICMP-responsive host for the connectivity ping check
    // (api.cloudinary.com blocks ICMP)
    constexpr static const char *PING_HOST = "8.8.8.8";

    // flespi MQTT broker. MQTT_TOKEN is a flespi token with MQTT access (see docs/GHID-FLESPI.md).
    // Topics must not start with "flespi/".
    constexpr static const char *MQTT_HOST         = "mqtt.flespi.io";
    constexpr static const uint16_t MQTT_PORT      = 8883;
    constexpr static const char *MQTT_TOKEN        = "your_flespi_token";
    constexpr static const char *MQTT_STATUS_TOPIC = "camera/status";
    constexpr static const char *MQTT_COMMAND_TOPIC = "camera/command";
};

#endif // API_SECRET_H
```

You can find `CLOUD_NAME`, `API_KEY` and `API_SECRET` in your Cloudinary dashboard under **Settings → API Keys**.

#### Signed uploads

Uploads are **signed** with the API secret, so no upload preset is needed (unsigned presets cannot enable overwrite). Each upload sends `public_id`, `overwrite=true` and `invalidate=true`, so every capture replaces the same asset and the CDN cache is cleared. The signature needs a valid clock, so the device syncs time over NTP (`time.google.com`) after connecting to Wi-Fi.

> The API secret is stored in the device's flash. Keep `api_secret.h` out of git (it is gitignored).

#### Online status and commands over MQTT (flespi)

After connecting to Wi-Fi the device connects to flespi's MQTT broker (`mqtt.flespi.io:8883`, TLS) using a flespi token as the username and an empty password. It publishes a retained `online` message on `camera/status`, and registers a last will so the broker publishes a retained `offline` there if the device disappears. Subscribe to `camera/status` (for example with flespi's MQTT Board) to see it.

Create the token in **flespi → Platform → Tokens** (template *MQTT ACL token*) with publish/subscribe access limited to `camera/#`, and put it in `MQTT_TOKEN`. See [`docs/GHID-FLESPI.md`](docs/GHID-FLESPI.md) for details.

#### Commands

The device no longer captures on a timer. It subscribes to `camera/command` and acts on the payload:

| Payload   | Action                                                        |
|-----------|---------------------------------------------------------------|
| `acquire` | Captures an image and uploads it to Cloudinary (signed upload) |

Publish `acquire` to `camera/command` from flespi's MQTT Board. Exposure calibration is currently disabled. For an Android client see [`docs/ANDROID.md`](docs/ANDROID.md).

---

## Building & Uploading with arduino-cli

### 1. Install toolchain

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### 2. Install dependencies

```bash
arduino-cli lib install "Adafruit NeoPixel@1.15.5"
arduino-cli lib install "ESPping"
arduino-cli lib install "PubSubClient"
```

### 3. Find the board's port

```bash
arduino-cli board list
```

Note the port (e.g. `COM3`). The "Board Name" shown (e.g. `ESP32 Family Device`) is only a guess from the generic Espressif USB ID — ignore it.

### 4. Check `sketch.yaml`

`sketch.yaml` holds the defaults, so no `--fqbn`, `--board-options` or `-p` flags are needed:

```yaml
default_fqbn: esp32:esp32:gen4-ESP32-S3R8n16:PartitionScheme=gen4esp32scheme4
default_port: COM3
default_programmer: ""
```

Set `default_port` to the port from step 3. The partition scheme is `Huge App (16MB APP)`; additional board options can be appended to the FQBN, comma-separated. No custom `partitions.csv` is needed.

### 5. Compile

```bash
arduino-cli compile
```

### 6. Upload

```bash
arduino-cli upload
```

To use a different port without editing `sketch.yaml`: `arduino-cli upload -p <port>`.

### 7. Monitor serial output

```bash
arduino-cli monitor -c baudrate=115200
```

### Editor support (clangd)

To (re)generate the compilation database used by clangd for completion and navigation:

```bash
arduino-cli compile --only-compilation-database --build-path build
```

Re-run it whenever you add files or libraries. See [Editor Setup](#editor-setup-vs-code--clangd) for the full VS Code configuration.

---

## Partition Scheme

The build uses the board's `Huge App (16MB APP)` partition scheme (`PartitionScheme=gen4esp32scheme4`), set in the FQBN in `sketch.yaml`. OTA updates are not supported by this scheme; use `gen4esp32scheme3` (Large App w/ OTA, 8MB APP/8MB OTA) if you need them.

---

## Editor Setup (VS Code + clangd)

Out of the box, VS Code offers no linting or "Go to Definition" for Arduino code; a language server fixes that.

### 1. Extensions

Install **clangd**, **C/C++** and **Arduino** (recommended in `.vscode/extensions.json`), and disable the C/C++ IntelliSense engine so it doesn't clash with clangd: `"C_Cpp.intelliSenseEngine": "disabled"`.

### 2. Generate `compile_commands.json`

From the repo root (the build needs the gitignored `api_secret.h` and `wifi_passwd.h`, see [Configuration Files](#configuration-files)):

```bash
arduino-cli compile --only-compilation-database --build-path build
```

Re-run it whenever you add files or libraries. `build/` is gitignored.

### 3. `.clangd`

```yaml
CompileFlags:
  Remove: [-m*, -f*, -mlongcalls, -mtext-section-literals, -fstrict-volatile-bitfields, -free, -fipa-pta]
Diagnostics:
  UnusedIncludes: None
---
If:
  PathMatch: .*\.ino
CompileFlags:
  Add: [-xc++]
```

The Xtensa/GCC flags must be removed because clang rejects them. The second fragment makes clangd parse the `.ino` sketch as C++.

### 4. `.vscode/settings.json`

```json
{
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build",
    "--query-driver=**/xtensa-esp32s3-elf-g++*",
    "--background-index"
  ],
  "C_Cpp.intelliSenseEngine": "disabled"
}
```

`--query-driver` lets clangd find the Xtensa toolchain headers. Run **clangd: Restart language server** afterwards.

Optionally wrap the compile, upload and monitor commands in `.vscode/tasks.json`.

---

## Alternative: Arduino IDE

Using the [Arduino IDE](https://www.arduino.cc/en/software) is also an option. It provides an integrated experience (compilation, upload, and serial monitor in one window) and handles board/library management through a GUI.

That said, I personally prefer arduino-cli: it works easily with VS Code and clangd, and everything (compile, upload, monitor) runs straight from VS Code's integrated terminal.
