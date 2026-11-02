#include "calibration_manager.h"

#include <Arduino.h>
#include "esp_camera.h"
#include "../acquisition_manager.h"
#include "../led_indicator.h"

// Discard `count` frames to let the sensor stabilise after an exposure change.
static void flush_frames(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb)
            esp_camera_fb_return(fb);
        delay(80);
    }
}

// Average luminance of a PIXFORMAT_GRAYSCALE frame.
// Each byte in fb->buf is a direct Y (luminance) value 0–255.
static uint8_t avg_luminance(const camera_fb_t *fb)
{
    const uint32_t n = (uint32_t)fb->width * fb->height;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
        sum += fb->buf[i];
    return (uint8_t)(sum / n);
}

ExposureSettings CalibrationManager::calibrate()
{
    ExposureSettings result{};

    // LED stays lit for the entire calibration procedure so it's visible
    // as a distinct, longer indication than a single capture flash.
    LedIndicator::on();

    // Full deinit + reinit with GRAYSCALE/QVGA.
    // Simply calling set_pixformat() at runtime only writes sensor registers;
    // it does NOT reconfigure the DMA buffers, causing fb_get() to return
    // nullptr consistently.  A proper deinit/reinit cycle fixes this.
    esp_camera_deinit();

    camera_config_t cal_config = AcquisitionManager::make_config(
        PIXFORMAT_GRAYSCALE, FRAMESIZE_QVGA, 1);

    esp_err_t err = esp_camera_init(&cal_config);
    if (err != ESP_OK)
    {
        Serial.printf("[Calibration] ERROR: camera reinit failed (0x%x) — restoring\n", err);
        AcquisitionManager::init();
        LedIndicator::off();
        return result;
    }

    Serial.println("[Calibration] Starting exposure sweep...");

    sensor_t *s = esp_camera_sensor_get();

    // Manual exposure and gain — keep AEC/AGC out of the sweep.
    s->set_exposure_ctrl(s, 0);
    s->set_gain_ctrl(s, 0);

    // Moderate starting point biased toward night conditions.
    uint16_t aec  = 300;
    uint8_t  gain = 0;
    s->set_aec_value(s, aec);
    s->set_agc_gain(s, gain);

    flush_frames(3);   // let the sensor stabilise after init

    bool converged = false;

    for (uint8_t iter = 0; iter < MAX_ITERATIONS; iter++)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.printf("[Calibration] iter %u: frame capture failed\n", iter + 1);
            break;
        }

        const uint8_t avg = avg_luminance(fb);
        esp_camera_fb_return(fb);

        Serial.printf("[Calibration] iter=%u  aec=%u  gain=%u  avg_lum=%u  target=%u\n",
                      iter + 1, aec, gain, avg, TARGET_GRAY);

        if (abs((int)avg - (int)TARGET_GRAY) <= TOLERANCE)
        {
            Serial.println("[Calibration] Converged.");
            converged = true;
            break;
        }

        if (avg < 4)
        {
            // Nearly black: exhaust AEC range first, then push gain.
            if (aec < 1200)
                aec = (uint16_t)min((uint32_t)aec * 4, (uint32_t)1200);
            else
                gain = (uint8_t)min((int)gain + 8, 30);
        }
        else
        {
            // Proportional correction: new_aec = current_aec * (target / avg).
            uint32_t new_aec = (uint32_t)aec * TARGET_GRAY / avg;
            if (new_aec > 1200)
            {
                // AEC ceiling reached — spill the remainder into gain.
                aec          = 1200;
                uint8_t bump = (uint8_t)min((new_aec - 1200) / 40 + 1, (uint32_t)5);
                gain         = (uint8_t)min((int)gain + bump, 30);
            }
            else
            {
                aec = (uint16_t)new_aec;
            }
        }

        s->set_aec_value(s, aec);
        s->set_agc_gain(s, gain);
        flush_frames(2);   // wait for new exposure to propagate
    }

    if (!converged)
        Serial.println("[Calibration] WARNING: did not converge within MAX_ITERATIONS — forcing night mode");

    result.exposure_time = aec;
    result.gain          = gain;
    result.night_mode    = !converged;

    // Restore for normal acquisition — night path uses halved XCLK.
    esp_camera_deinit();
    if (result.night_mode)
        AcquisitionManager::initNight();
    else
        AcquisitionManager::init();

    // Re-apply the calibrated (or forced-maximum) values to the freshly-inited sensor.
    sensor_t *sn = esp_camera_sensor_get();
    if (sn)
    {
        sn->set_exposure_ctrl(sn, 0);
        sn->set_gain_ctrl(sn, 0);
        sn->set_agc_gain(sn, gain);

        if (result.night_mode)
        {
            // initNight() already wrote VTS and AEC via direct registers —
            // do NOT call set_aec_value() here or it would overwrite them.
            sn->set_aec2(sn, 1);                       // OV3660 night-mode bit
            sn->set_gainceiling(sn, GAINCEILING_128X); // raise AGC ceiling to ×128
            Serial.println("[Calibration] Night mode active — XCLK=6MHz  VTS=4000  AEC_lines=3992  GAIN=30  aec2=ON  gainceiling=128x");
        }
        else
        {
            sn->set_aec_value(sn, aec);
        }
    }

    Serial.printf("[Calibration] Done — aec=%u  gain=%u  night=%d\n",
                  aec, gain, (int)result.night_mode);

    LedIndicator::off();

    return result;
}
