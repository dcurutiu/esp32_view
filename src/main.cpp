#include "camera/acquisition_manager.h"
#include "camera/board_config.h"
#include "camera/calibration/calibration_manager.h"
#include "cloud_storage/cloudinary_manager.h"
#include "led/led_manager.h"
#include "wifi/wifi_manager.h"

static constexpr uint32_t CALIBRATION_INTERVAL_MS = 30UL * 60UL * 1000UL;
static uint32_t lastCalibrationMs = 0;

void setup()
{
    Serial.begin(115200);
    LEDManager::setup();
    WiFiManager::setup();
    if (!AcquisitionManager::init())
    {
        Serial.println("[main] Camera initialization failed");
        while (true)
        {
            LEDManager::blinkRGB(LEDManager::COLOR_RED);
            delay(1000);
        }
    }

    CalibrationManager::calibrate();
    lastCalibrationMs = millis();
}

void loop()
{
    if (millis() - lastCalibrationMs >= CALIBRATION_INTERVAL_MS)
    {
        Serial.println("[main] 30-minute interval — recalibrating exposure");
        CalibrationManager::calibrate();
        lastCalibrationMs = millis();
    }

    bool pingResult = WiFiManager::testPing();
    if (pingResult)
    {
        LEDManager::blinkRGB(LEDManager::COLOR_BLUE);

        static uint8_t uploadFailures = 0;

        // If the latest image already exists on Cloudinary, skip capture/upload to save resources.
        if (CloudinaryManager::imageExists())
        {
            Serial.println("[main] Image already on Cloudinary — sleeping");
            delay(60000);
            return;
        }

        // // check again: most of the time false negatives are due to transient network issues
        //     // and the image is actually there. 
        // if (CloudinaryManager::imageExists())
        // {
        //     Serial.println("[main] Image already on Cloudinary [2] — sleeping");
        //     delay(60000);
        //     return;
        // }

        const auto image = AcquisitionManager::acquireImage();
        if (image)
        {
            Serial.printf("[main] Captured image: %u bytes\n", (unsigned)image->len);
            Serial.println("[main] Uploading image to Cloudinary...");
            bool ok = CloudinaryManager::uploadImage(*image);
            if (ok)
            {
                uploadFailures = 0;
            }
            else
            {
                uploadFailures++;
                Serial.printf("[main] Upload failed (%u/2)\n", uploadFailures);
                if (uploadFailures >= 2)
                {
                    Serial.println("[main] 2 consecutive failures — restarting!");
                    delay(500);
                    ESP.restart();
                }
            }
        }
        delay(60000);
    }
    else
    {
        delay(25000);
        Serial.println("[main] No WiFi connection — restarting!");
        if (!WiFiManager::testPing())
        {
            Serial.println("[main] Still no connection after delay — restarting now!");
            ESP.restart();
        }
    }
}