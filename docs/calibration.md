# Camera Exposure Calibration

## Overview

The calibration routine finds the correct **AEC value** (exposure time) and
**AGC gain** so that the average luminance of a captured frame matches a
pre-defined target grey.  It runs once at boot and then every 30 minutes to
track changing light conditions.

---

## 1. The Problem

A camera sensor maps scene radiance to pixel values.  For a given scene we
want to choose an exposure `E` and gain `G` such that the mean pixel value
sits at a target luminance `L_target`:

```
mean(frame) ≈ L_target
```

On the OV3660 the two independent controls are:

| Symbol | Register | Range | Unit |
|--------|----------|-------|------|
| `E`    | `aec_value` | 0 – 1200 | line-periods |
| `G`    | `agc_gain`  | 0 – 30   | index into gain table |

---

## 2. Luminance Metric

Calibration captures in `PIXFORMAT_GRAYSCALE` at `FRAMESIZE_QVGA`
(320 × 240 = 76 800 pixels).  Each pixel byte is a direct **Y (luma)**
sample, so the metric is simply:

$$
\bar{L} = \frac{1}{N} \sum_{i=0}^{N-1} p_i
\qquad N = 320 \times 240 = 76\,800
$$

Integer arithmetic is safe because:

$$
\sum p_i \;\leq\; 255 \times 76\,800 = 19\,584\,000 \;<\; 2^{32}
$$

---

## 3. Proportional Correction (P-Controller)

### 3.1 Linear sensor model

Assume the sensor response is linear in both `E` and `G`:

$$
\bar{L} = k \cdot E \cdot g(G)
$$

where `g(G)` is the gain look-up factor and `k` is a scene-dependent
constant that absorbs reflectance, aperture, etc.

### 3.2 One-step update rule

Given a measurement `L_n` at exposure `E_n`, the ideal correction is:

$$
\boxed{E_{n+1} = E_n \cdot \frac{L_{\text{target}}}{L_n}}
$$

Substituting back into the linear model:

$$
L_{n+1}
= k \cdot E_{n+1} \cdot g(G)
= k \cdot \left(E_n \cdot \frac{L_{\text{target}}}{L_n}\right) \cdot g(G)
= L_n \cdot \frac{L_{\text{target}}}{L_n}
= L_{\text{target}}
$$

**A perfectly linear sensor converges in a single iteration.**

### 3.3 Non-linear / saturated case

Real sensors saturate at 255.  If the sensor clips, the measured `L_n` is
an underestimate of the true scene brightness.  The controller still drives
`E` in the right direction but overshoots less than a bang-bang controller
would.  Define the per-step error:

$$
e_n = \bar{L}_n - L_{\text{target}}
$$

For a saturated first frame (`L_0 = 255`):

$$
E_1 = E_0 \cdot \frac{100}{255} \approx 0.39\, E_0
$$

The reduction is large enough to pull the sensor out of saturation in the
next frame.  Convergence is **monotonically decreasing in error** as long as
`L(E)` is strictly increasing.

### 3.4 Worked example — bright scene

| Iter | `E` | `L` measured | Action |
|------|----:|-------------:|--------|
| 1 | 300 | 240 | `E = 300 × 100/240 = 125` |
| 2 | 125 | 115 | `E = 125 × 100/115 = 108` |
| 3 | 108 | 102 | `|102 − 100| = 2 ≤ 10` → **converged** |

### 3.5 Worked example — very dark scene

| Iter | `E` | `G` | `L` | Action |
|------|----:|----:|----:|--------|
| 1 | 300 | 0 | 2 | `L < 4` → `E = min(1200, 1200)` |
| 2 | 1200 | 0 | 8 | `new_E = 15 000 > 1200` → `E=1200`, `G += 5` |
| 3 | 1200 | 5 | 20 | `new_E = 6 000 > 1200` → `G += 5` |
| 4 | 1200 | 10 | 42 | `new_E = 2 857 > 1200` → `G += 5` |
| 5 | 1200 | 15 | 88 | `new_E = 1 364 > 1200` → `G += 2` |
| 6 | 1200 | 17 | 101 | `|101 − 100| = 1 ≤ 10` → **converged** |

---

## 4. Gain Cascade

When the proportional step would require `E > 1200` the excess correction
spills into gain:

```
new_E  = E_n × (L_target / L_n)

if new_E > 1200:
    E      = 1200
    bump   = min( (new_E − 1200) / 40 + 1 , 5 )   // at most +5 per iter
    G      = min( G + bump , 30 )
else:
    E      = new_E
```

The `/ 40` damping prevents gain from jumping to its ceiling in one step,
which would overshoot on the next iteration.

### 4.1 Maximum achievable brightness

At `E = 1200`, `G = 30` the sensor is at its absolute maximum.  If even
this cannot reach `L_target`, calibration ends after `MAX_ITERATIONS` and
the best-effort values are used.

---

## 5. Termination

```
converged  ←  |L_n − L_target| ≤ TOLERANCE   (TOLERANCE = 10)
max_iters  ←  MAX_ITERATIONS = 10
```

Either condition ends the sweep.

---

## 6. Why deinit / reinit Instead of set_pixformat

`set_pixformat()` writes OV3660 sensor registers but does **not** reconfigure
the ESP32 DMA engine.  The DMA buffers are sized and mapped during
`esp_camera_init()` for the chosen pixel format.  Switching format at runtime
leaves the DMA expecting the old stream layout, and every subsequent
`esp_camera_fb_get()` returns `nullptr`.

The only reliable method is a full driver cycle:

```
esp_camera_deinit()          // release DMA, I2S, frame buffers
esp_camera_init(GRAYSCALE)   // reallocate everything for new format
... calibrate ...
esp_camera_deinit()
esp_camera_init(JPEG)        // restore normal acquisition config
```

---

## 7. PlantUML Diagrams

### 7.1 Calibration Algorithm

```plantuml
@startuml
skinparam defaultFontName Monospaced
skinparam backgroundColor #FAFAFA

start

:esp_camera_deinit();
:esp_camera_init(GRAYSCALE / QVGA);
note right: reallocates DMA buffers

:set_exposure_ctrl(OFF)\nset_gain_ctrl(OFF);
:AEC ← 300,  GAIN ← 0;
:flush 3 frames;

while (iter < MAX_ITERATIONS?) is (yes)

  :fb ← esp_camera_fb_get();

  if (fb == nullptr?) then (yes)
    #LightCoral:log "frame capture failed";
    break
  endif

  :L ← avg_luminance(fb);
  :esp_camera_fb_return(fb);

  :log iter, AEC, GAIN, L, target;

  if (|L − L_target| ≤ TOLERANCE?) then (yes)
    #LightGreen:converged ← true;
    break
  endif

  if (L < 4?) then (yes)
    if (AEC < 1200?) then (yes)
      :AEC ← min(AEC × 4, 1200);
    else (no)
      :GAIN ← min(GAIN + 8, 30);
    endif
  else (no)
    :new_AEC ← AEC × (L_target / L);
    if (new_AEC > 1200?) then (yes)
      :AEC ← 1200;
      :bump ← min((new_AEC−1200)/40+1, 5);
      :GAIN ← min(GAIN + bump, 30);
    else (no)
      :AEC ← new_AEC;
    endif
  endif

  :set_aec_value(AEC)\nset_agc_gain(GAIN);
  :flush 2 frames;

endwhile (no)

:esp_camera_deinit();
:esp_camera_init(JPEG / QSXGA);
note right: restores normal acquisition

:set_aec_value(AEC)\nset_agc_gain(GAIN);
note right: lock calibrated values\nAEC/AGC remain manual

:return ExposureSettings{AEC, GAIN};

stop
@enduml
```

---

### 7.2 Deinit / Reinit Sequence

```plantuml
@startuml
skinparam defaultFontName Monospaced
skinparam backgroundColor #FAFAFA

participant "main.cpp"           as main
participant "CalibrationManager" as cal
participant "AcquisitionManager" as acq
participant "esp_camera driver"  as drv
participant "OV3660 sensor"      as sen

main  ->  cal : calibrate()

cal   ->  drv : esp_camera_deinit()
drv   ->  sen : power-down / reset
drv   ->  drv : free DMA buffers\nfree frame buffers

cal   ->  acq : make_config(GRAYSCALE, QVGA, fb=1)
acq   --> cal : camera_config_t

cal   ->  drv : esp_camera_init(grayscale_config)
drv   ->  sen : SCCB write: GRAYSCALE registers
drv   ->  drv : allocate DMA buffers\nfor QVGA grayscale

cal   ->  sen : set_exposure_ctrl(OFF)\nset_gain_ctrl(OFF)

loop iter = 1..N  (N ≤ 10)
  cal ->  sen : set_aec_value(AEC)\nset_agc_gain(GAIN)
  cal ->  drv : flush_frames(2)
  cal ->  drv : esp_camera_fb_get()
  drv --> cal : fb* (76 800 Y bytes)
  cal ->  cal : avg_luminance(fb) → L
  cal ->  drv : esp_camera_fb_return(fb)
  note right of cal: adjust AEC/GAIN\nif not converged
end

cal   ->  drv : esp_camera_deinit()
drv   ->  drv : free GRAYSCALE buffers

cal   ->  acq : init()
acq   ->  drv : esp_camera_init(jpeg_config)
drv   ->  sen : SCCB write: JPEG / QSXGA registers
drv   ->  drv : allocate DMA buffers\nfor QSXGA JPEG

cal   ->  sen : set_aec_value(AEC_cal)\nset_agc_gain(GAIN_cal)
note right of cal : calibrated values\nlocked in manual mode

cal   --> main : ExposureSettings{AEC, GAIN}
@enduml
```

---

### 7.3 Controller State Machine

```plantuml
@startuml
skinparam defaultFontName Monospaced
skinparam backgroundColor #FAFAFA

[*] --> Reinit_Grayscale

Reinit_Grayscale  : esp_camera_deinit()
Reinit_Grayscale  : esp_camera_init(GRAYSCALE/QVGA)
Reinit_Grayscale  : AEC=300, GAIN=0
Reinit_Grayscale  --> Flush      : init OK
Reinit_Grayscale  --> Restore    : init FAILED

Flush             : flush_frames(3)
Flush             --> Capture    : done

Capture           : esp_camera_fb_get()
Capture           --> Measure    : fb valid
Capture           --> Restore    : fb == nullptr

Measure           : L = avg_luminance(fb)
Measure           --> Converged  : |L − Ltarget| ≤ 10
Measure           --> AdjustAEC  : L ≥ 4
Measure           --> BoostDark  : L < 4

AdjustAEC         : new_AEC = AEC × (Ltarget / L)
AdjustAEC         --> ApplyAEC   : new_AEC ≤ 1200
AdjustAEC         --> SpillGain  : new_AEC > 1200

SpillGain         : AEC = 1200
SpillGain         : GAIN += min((new_AEC−1200)/40+1, 5)
SpillGain         --> ApplyAEC

BoostDark         : AEC = min(AEC×4, 1200) or GAIN += 8
BoostDark         --> ApplyAEC

ApplyAEC          : set_aec_value(AEC)
ApplyAEC          : set_agc_gain(GAIN)
ApplyAEC          : flush_frames(2)
ApplyAEC          --> Capture    : iter < MAX_ITERS
ApplyAEC          --> Restore    : iter == MAX_ITERS

Converged         --> Restore

Restore           : esp_camera_deinit()
Restore           : AcquisitionManager::init() [JPEG/QSXGA]
Restore           : apply calibrated AEC, GAIN
Restore           --> [*]

@enduml
```

---

## 8. Constants Reference

| Constant | Value | Rationale |
|----------|------:|-----------|
| `TARGET_GRAY` | 100 | Below neutral grey (128) to avoid blowing out point light sources (street lamps, windows) in night scenes |
| `TOLERANCE` | 10 | ±10 / 255 ≈ ±4 % — tight enough for consistent exposure |
| `MAX_ITERATIONS` | 10 | Bounds worst-case calibration time to ≈ 4 s (10 × (2 flush × 80 ms + capture)) |
| `AEC` start | 300 | Mid-range biased toward low-light; converges quickly in both directions |
| `AEC` max | 1200 | OV3660 hardware limit (full-frame integration) |
| `GAIN` max | 30 | OV3660 AGC register ceiling |
| Frame format | GRAYSCALE / QVGA | Direct Y bytes; no JPEG decode needed; 76 800 bytes fits PSRAM comfortably |
| Flush after setting change | 2 frames | DMA pipeline depth; ensures the new exposure is fully applied |
| Flush after init | 3 frames | Sensor settling time after SCCB register write burst |
