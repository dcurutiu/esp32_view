#include "camera/acquisition_manager.h"
#include "camera/board_config.h"
#include "led/led_manager.h"
#include "supabase/supabase_manager.h"
#include "wifi/wifi_manager.h"

void setup()
{
    Serial.begin(115200);
    LEDManager::setup();
    WiFiManager::setup();
    if (!AcquisitionManager::init())
    {
        Serial.println("Camera initialization failed");
        while (true)
        {
            LEDManager::blinkRGB(LEDManager::COLOR_RED); // red
            delay(1000);
        }
    }
}

void loop()
{
    Serial.println("alive");
    bool pingResult = WiFiManager::testPing();
    if (pingResult)
    {
        LEDManager::blinkRGB(LEDManager::COLOR_BLUE); // blue
        // do only when I press the button
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            const auto image = AcquisitionManager::acquireImage();
            if (image)
            {
                SupabaseManager::uploadImage(*image);
            } // image goes out of scope here → buffer freed automatically
            delay(5000);
        }
    }
}