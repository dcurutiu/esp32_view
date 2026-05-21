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

## Build System

This project is configured for **PlatformIO**, but in practice PlatformIO can be notoriously sluggish — slow indexing, unresponsive IDE integration, and unpredictable build times are common pain points. If you hit a wall with PlatformIO, switching to **arduino-cli** is a practical and no-frills alternative.

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
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### 2. Install dependencies

```bash
arduino-cli lib install "Adafruit NeoPixel@1.15.5"
arduino-cli lib install "ESPping"
```

### 3. Compile

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --board-options PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom \
  .
```

### 4. Upload

```bash
arduino-cli upload \
  -p COM6 \
  --fqbn esp32:esp32:esp32s3 \
  --board-options PSRAM=opi,CDCOnBoot=cdc,PartitionScheme=custom \
  .
```

> Adjust `-p COM6` to match your actual serial port.

---

## Serial Monitoring

To monitor serial output without PlatformIO:

```bash
arduino-cli monitor -p COM6 -c baudrate=115200
```

---

## Custom Partition Table

When building with arduino-cli, the `PartitionScheme=custom` board option will use the `partitions.csv` file at the root of the project. Make sure it is present before compiling — it is already included in this repository.

---

## Downsides of arduino-cli + VS Code

Using arduino-cli with a plain VS Code setup is functional but spartan:

- **No linting** — syntax and semantic errors are only caught at compile time.
- **No "Go to Definition / Declaration"** — `Ctrl+Click` navigation does not work without a proper language server configured for Arduino/C++.

---

## Alternative: Arduino IDE

Using the [Arduino IDE](https://www.arduino.cc/en/software) is also a valid option. It provides a simpler integrated experience (compilation, upload, and serial monitor in one window) and handles board/library management through a GUI, which can be easier to set up than arduino-cli.
