#include "camera/acquisition_manager.h"
#include "camera/board_config.h"
#include "cloud_storage/cloudinary_manager.h"
#include "led/led_manager.h"
#include "wifi/wifi_manager.h"

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
}

void loop()
{
    bool pingResult = WiFiManager::testPing();
    if (pingResult)
    {
        LEDManager::blinkRGB(LEDManager::COLOR_BLUE);
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            const auto image = AcquisitionManager::acquireImage();
            if (image)
            {
                Serial.printf("[main] Captured image: %u bytes\n", (unsigned)image->len);
                Serial.println("[main] Uploading image to Cloudinary...");
                CloudinaryManager::uploadImage(*image);
            }
            delay(5000);
        }
    }
}