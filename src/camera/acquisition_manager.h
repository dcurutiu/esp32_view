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

// RAII wrapper around a BMP frame buffer.
// Move-only: buffer is freed automatically when the object is destroyed.
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
    // Returns the captured BMP image, or std::nullopt on failure.
    // Buffer is freed automatically when the returned optional is destroyed.
    static bool init();
    static std::optional<CapturedImage> acquireImage();
};

#endif // ACQUISITION_MANAGER_H