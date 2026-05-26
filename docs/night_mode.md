# Night Mode

When the calibration sweep exhausts all `MAX_ITERATIONS` without converging, the scene is too dark for normal exposure settings. The system automatically enters **night mode** — a forced configuration that stacks every available exposure lever to extract the maximum possible light from the sensor.

---

## Trigger

```cpp
result.night_mode = !converged;   // true when MAX_ITERATIONS reached
```

Night mode is selected once and stays active until the next 30-minute recalibration. If conditions have changed (e.g. dawn), the next calibration cycle will converge normally and revert to a regular exposure.

---

## What Changes vs Normal Mode

| Lever | Normal | Night | Mechanism |
|-------|--------|-------|-----------|
| `xclk_freq_hz` | 20 MHz | **6 MHz** | `camera_config_t` before `esp_camera_init()` |
| VTS (lines/frame) | ~2 000 | **4 000** | Direct register write `0x380e / 0x380f` |
| AEC (exposure lines) | calibrated (0–1200) | **3 992** | Direct register write `0x3500–0x3502` |
| AGC gain | calibrated (0–30) | **30** | `set_agc_gain()` |
| `aec2` | OFF | **ON** | `set_aec2()` — OV3660 HW night-mode bit |
| Gain ceiling | default | **128×** | `set_gainceiling(GAINCEILING_128X)` |

---

## The Three Main Levers

### 1 — XCLK: 20 MHz → 6 MHz

XCLK drives the sensor's internal pixel clock. Halving it stretches every line period in real time:

$$
t_{\text{line}} \propto \frac{1}{f_{\text{XCLK}}}
\qquad\Rightarrow\qquad
\frac{t_{\text{line,night}}}{t_{\text{line,normal}}} = \frac{20}{6} \approx 3.3\times
$$

6 MHz is the documented minimum safe clock for the OV3660.

```cpp
camera_config_t config = AcquisitionManager::make_config(PIXFORMAT_JPEG, FRAMESIZE_QSXGA, 1);
config.xclk_freq_hz = 6000000;
esp_camera_init(&config);
```

---

### 2 — VTS: ~2 000 → 4 000 lines

VTS (Vertical Total Size) is the total number of line periods per frame — active lines plus vertical blanking. Increasing it reduces frame rate and allows the AEC register to specify more integration lines:

$$
\text{frame rate} = \frac{f_{\text{XCLK}}}{\text{HTS} \times \text{VTS}}
$$

Written directly to OV3660 registers because the `esp_camera` API has no VTS setter:

```cpp
constexpr uint16_t NIGHT_VTS = 4000;

s->set_reg(s, 0x380e, 0xff, (NIGHT_VTS >> 8) & 0xff);   // VTS[15:8]
s->set_reg(s, 0x380f, 0xff,  NIGHT_VTS        & 0xff);   // VTS[7:0]
```

---

### 3 — AEC: 1 200 → 3 992 lines (direct register)

The `esp_camera` API caps `set_aec_value()` at **1 200** in the OV3660 driver. Internally it just does `value << 4` and writes to three registers — so writing those registers directly bypasses the cap:

```cpp
constexpr uint32_t NIGHT_AEC_REG = (NIGHT_VTS - 8U) * 16U;  // 1/16-line units
//  NIGHT_VTS - 8  →  3992 lines  (8-line blanking margin)
//  × 16           →  63872 = 0xF980

s->set_reg(s, 0x3500, 0x0f, (NIGHT_AEC_REG >> 16) & 0x0f);  // AEC[19:16]
s->set_reg(s, 0x3501, 0xff, (NIGHT_AEC_REG >>  8) & 0xff);  // AEC[15:8]
s->set_reg(s, 0x3502, 0xff,  NIGHT_AEC_REG         & 0xff); // AEC[7:0]
```

The 8-line margin below VTS is the minimum vertical blanking the OV3660 requires before it can begin the next frame.

> **Critical:** `set_aec_value()` is intentionally **not called** after `initNight()`. Calling it with any value (including 1200) would silently overwrite the direct register values above.

---

## Combined Exposure Gain

$$
\text{gain}_{\text{total}}
= \underbrace{\frac{20}{6}}_{\text{XCLK}} \times \underbrace{\frac{4000}{2000}}_{\text{VTS}} \times \underbrace{\frac{3992}{1200}}_{\text{AEC}}
\approx 3.3 \times 2 \times 3.3
\approx \mathbf{22\times}
$$

compared to the default daytime configuration (20 MHz, AEC = 1 200).

---

## `aec2` and Gain Ceiling

On top of the fixed manual settings, two sensor-side helpers are enabled:

- **`aec2 = 1`** — OV3660 hardware night-mode bit. Allows the sensor's internal AEC logic to stretch integration time further when the frame is still underexposed, even in manual-AEC mode.
- **`gainceiling = GAINCEILING_128X`** — raises the maximum gain the internal AGC is allowed to apply from its default to ×128, giving the sensor more headroom to amplify weak signals.

```cpp
sn->set_aec2(sn, 1);
sn->set_gainceiling(sn, GAINCEILING_128X);
```

---
