#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <cstdint>

struct ExposureSettings
{
    uint32_t exposure_time = 0;   // AEC value (0–1200)
    uint32_t gain          = 0;   // AGC gain  (0–30)
};

struct CalibrationManager
{
    // Target average luminance (0–255).
    // 100 is chosen instead of 128: a slightly-below-neutral target avoids
    // burning out the point light sources (streetlights, windows) common in
    // night scenes while still producing a usable exposure.
    static constexpr uint8_t TARGET_GRAY    = 100;
    static constexpr uint8_t TOLERANCE      = 10;   // ±10 counts — good enough
    static constexpr uint8_t MAX_ITERATIONS = 30;

    // Sweep-calibrates the sensor exposure in GRAYSCALE/QVGA mode (fast),
    // then restores JPEG + QSXGA for normal acquisition.
    // Must be called after AcquisitionManager::init().
    // Leaves the sensor in manual AEC/AGC mode at the calibrated values.
    static ExposureSettings calibrate();
};

#endif // CALIBRATION_MANAGER_H