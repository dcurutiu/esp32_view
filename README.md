# esp32_view

A lightweight embedded application that periodically captures photos and uploads them to [Cloudinary](https://cloudinary.com/) storage. Runs on an ESP32-S3 and is designed for unattended, continuous operation.

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

    // Unsigned upload preset — create it in the Cloudinary dashboard:
    //   Settings → Upload → Upload presets → Add upload preset
    //   Signing Mode : Unsigned
    //   Overwrite    : ✓  (so every upload replaces the same asset)
    constexpr static const char *UPLOAD_PRESET = "esp32_preset";

    // Fixed public_id → the asset URL never changes between uploads
    constexpr static const char *PUBLIC_ID = "latest";

    // Reliable ICMP-responsive host for the connectivity ping check
    // (api.cloudinary.com blocks ICMP)
    constexpr static const char *PING_HOST = "8.8.8.8";
};

#endif // API_SECRET_H
```

You can find `CLOUD_NAME` and `API_KEY` in your Cloudinary dashboard under **Settings → API Keys**.

#### Upload preset setup

The upload preset must be created once before the first flash:

1. Go to **Cloudinary Dashboard → Settings → Upload → Upload presets**
2. Click **Add upload preset**
3. Set **Signing Mode** to `Unsigned`
4. Enable **Overwrite**
5. Name it to match `UPLOAD_PRESET` in `api_secret.h` (default: `esp32_preset`)
6. Save

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
```

The Xtensa/GCC flags must be removed because clang rejects them.

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
