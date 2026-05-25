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

    sensor_t *s = esp_camera_sensor_get();
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

    return true;
}

std::optional<CapturedImage> AcquisitionManager::acquireImage()
{
    Serial.printf("[Mem] acquireImage start       heap=%u  PSRAM=%u\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("[AcquisitionManager] Camera capture failed!");
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
        return std::nullopt;
    }
    memcpy(buf, fb->buf, buf_len);
    esp_camera_fb_return(fb);

    Serial.printf("[Mem] after jpeg copy           heap=%u  PSRAM=%u  jpeg=%u bytes\n",
                  ESP.getFreeHeap(), ESP.getFreePsram(), (unsigned)buf_len);

    CapturedImage img;
    img.data = buf;
    img.len = buf_len;
    img.width = w;
    img.height = h;
    return img;
}