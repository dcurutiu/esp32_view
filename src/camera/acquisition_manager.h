#ifndef ACQUISITION_MANAGER_H
#define ACQUISITION_MANAGER_H

#include <cstdlib>
#include <optional>

#include "board_config.h"
#include "camera_index.h"
#include "esp32-hal-ledc.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "img_converters.h"
#include "sdkconfig.h"

struct CapturedImage
{
    uint8_t *data = nullptr;
    size_t len = 0;
    uint16_t width = 0;
    uint16_t height = 0;

    CapturedImage() = default;

    ~CapturedImage() { free(data); }

    CapturedImage(const CapturedImage &) = delete;
    CapturedImage &operator=(const CapturedImage &) = delete;

    CapturedImage(CapturedImage &&o) noexcept : data(o.data), len(o.len), width(o.width), height(o.height)
    {
        o.data = nullptr;
        o.len = 0;
    }

    CapturedImage &operator=(CapturedImage &&o) noexcept
    {
        if (this != &o)
        {
            free(data);
            data = o.data;
            o.data = nullptr;
            len = o.len;
            o.len = 0;
            width = o.width;
            height = o.height;
        }
        return *this;
    }
};

struct AcquisitionManager
{
    // Builds a camera_config_t with the board pin assignments and the given
    // format / frame-size / fb_count.  Does NOT call esp_camera_init().
    static camera_config_t make_config(pixformat_t fmt, framesize_t fs, uint8_t fb_count = 1);

    static bool init();

    // Night-mode init: same as init() but XCLK is halved (10 MHz) so each
    // line period takes twice as long, doubling real-world exposure at AEC=1200.
    static bool initNight();

    static std::optional<CapturedImage> acquireImage();
};

#endif // ACQUISITION_MANAGER_H