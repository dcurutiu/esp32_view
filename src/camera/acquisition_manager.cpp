#include "acquisition_manager.h"

#include <Arduino.h>

camera_config_t AcquisitionManager::make_config(pixformat_t fmt, framesize_t fs, uint8_t fb_count)
{
    camera_config_t c{};
    c.ledc_channel  = LEDC_CHANNEL_0;
    c.ledc_timer    = LEDC_TIMER_0;
    c.pin_d0        = Y2_GPIO_NUM;
    c.pin_d1        = Y3_GPIO_NUM;
    c.pin_d2        = Y4_GPIO_NUM;
    c.pin_d3        = Y5_GPIO_NUM;
    c.pin_d4        = Y6_GPIO_NUM;
    c.pin_d5        = Y7_GPIO_NUM;
    c.pin_d6        = Y8_GPIO_NUM;
    c.pin_d7        = Y9_GPIO_NUM;
    c.pin_xclk      = XCLK_GPIO_NUM;
    c.pin_pclk      = PCLK_GPIO_NUM;
    c.pin_vsync     = VSYNC_GPIO_NUM;
    c.pin_href      = HREF_GPIO_NUM;
    c.pin_sccb_sda  = SIOD_GPIO_NUM;
    c.pin_sccb_scl  = SIOC_GPIO_NUM;
    c.pin_pwdn      = PWDN_GPIO_NUM;
    c.pin_reset     = RESET_GPIO_NUM;
    c.xclk_freq_hz  = 20000000;
    c.pixel_format  = fmt;
    c.frame_size    = fs;
    c.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
    c.fb_location   = CAMERA_FB_IN_PSRAM;
    c.jpeg_quality  = 12;
    c.fb_count      = fb_count;
    return c;
}

// Applies board-specific sensor defaults that are independent of XCLK / format.
static void apply_sensor_defaults(sensor_t *s)
{
    if (s->id.PID == OV3660_PID)
    {
        s->set_vflip(s, 1);
        s->set_saturation(s, 1);
    }
    s->set_brightness(s, -1);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 1);
#endif
}

bool AcquisitionManager::init()
{
    Serial.println("[AcquisitionManager] Initializing camera...");

    camera_config_t config = make_config(PIXFORMAT_JPEG, FRAMESIZE_QSXGA, 1);

    if (psramFound())
    {
        config.fb_count  = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("[AcquisitionManager] Camera init failed with error 0x%x\n", err);
        return false;
    }
    Serial.println("[AcquisitionManager] Camera init successful!");

    apply_sensor_defaults(esp_camera_sensor_get());
    return true;
}

bool AcquisitionManager::initNight()
{
    Serial.println("[AcquisitionManager] Initializing camera (night mode)...");

    // 6 MHz XCLK: minimum safe clock for the OV3660.  Each line period is
    // 20/6 ≈ 3.3× longer than the standard 20 MHz, giving proportionally
    // more real-world exposure time for the same AEC line count.
    camera_config_t config = make_config(PIXFORMAT_JPEG, FRAMESIZE_QSXGA, 1);
    config.xclk_freq_hz = 6000000;

    if (psramFound())
    {
        config.fb_count  = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else
    {
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("[AcquisitionManager] Night init failed with error 0x%x\n", err);
        return false;
    }
    Serial.println("[AcquisitionManager] Night camera init successful!");

    sensor_t *s = esp_camera_sensor_get();
    apply_sensor_defaults(s);

    // Push exposure beyond the esp_camera API ceiling (AEC = 1200 lines) by
    // writing OV3660 registers directly via the driver's set_reg interface.
    //
    // VTS (Vertical Total Size) — registers 0x380e / 0x380f:
    //   Controls total lines per frame (active + blanking).  Default ≈ 2000.
    //   Doubling it halves the frame rate and allows twice the integration time.
    //
    // AEC — registers 0x3500[3:0] / 0x3501 / 0x3502:
    //   Exposure duration in 1/16 line-period units.
    //   Maximum meaningful value is VTS − 8 (leave 8-line margin for blanking).
    //
    // Net result at 6 MHz XCLK + VTS = 4000 + AEC = 3992 lines:
    //   ≈ 22× the exposure of the default 20 MHz / AEC = 1200 config.

    constexpr uint16_t NIGHT_VTS       = 4000;
    constexpr uint32_t NIGHT_AEC_REG   = (NIGHT_VTS - 8U) * 16U;  // in 1/16-line units

    s->set_reg(s, 0x380e, 0xff, (NIGHT_VTS     >>  8) & 0xff);  // VTS[15:8]
    s->set_reg(s, 0x380f, 0xff,  NIGHT_VTS              & 0xff); // VTS[7:0]

    s->set_reg(s, 0x3500, 0x0f, (NIGHT_AEC_REG >> 16) & 0x0f);  // AEC[19:16]
    s->set_reg(s, 0x3501, 0xff, (NIGHT_AEC_REG >>  8) & 0xff);  // AEC[15:8]
    s->set_reg(s, 0x3502, 0xff,  NIGHT_AEC_REG         & 0xff); // AEC[7:0]

    Serial.printf("[AcquisitionManager] Night registers written — VTS=%u  AEC_lines=%u  AEC_reg=0x%05lx\n",
                  NIGHT_VTS, NIGHT_VTS - 8U, (unsigned long)NIGHT_AEC_REG);
    return true;
}

std::optional<CapturedImage> AcquisitionManager::acquireImage()
{
    Serial.printf("[Mem] acquireImage start       heap=%u  PSRAM=%u\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    LedIndicator::on();
    delay(1300);   // visible warning before the shutter fires

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("[AcquisitionManager] Camera capture failed!");
        LedIndicator::off();
        return std::nullopt;
    }

    uint16_t w = fb->width;
    uint16_t h = fb->height;

    size_t buf_len = fb->len;
    uint8_t *buf = (uint8_t *)ps_malloc(buf_len);
    if (!buf)
    {
        Serial.println("[AcquisitionManager] ps_malloc failed for JPEG buffer");
        esp_camera_fb_return(fb);
        LedIndicator::off();
        return std::nullopt;
    }
    memcpy(buf, fb->buf, buf_len);
    esp_camera_fb_return(fb);

    delay(1300);   // keep the LED visible for a second after the shot too
    LedIndicator::off();

    Serial.printf("[Mem] after jpeg copy           heap=%u  PSRAM=%u  jpeg=%u bytes\n",
                  ESP.getFreeHeap(), ESP.getFreePsram(), (unsigned)buf_len);

    CapturedImage img;
    img.data = buf;
    img.len = buf_len;
    img.width = w;
    img.height = h;
    return img;
}