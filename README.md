# esp32_view

A lightweight embedded application that periodically captures photos and uploads them to [Supabase](https://supabase.com/) storage. Runs on an ESP32-S3 and is designed for unattended, continuous operation.

## Hardware

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB OPI PSRAM) |
| Camera | OV3660 |

## Build System

This project is configured for **PlatformIO**, but in practice PlatformIO can be notoriously sluggish — slow indexing, unresponsive IDE integration, and unpredictable build times are common pain points. If you hit a wall with PlatformIO, switching to **arduino-cli** is a practical and no-frills alternative.

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
