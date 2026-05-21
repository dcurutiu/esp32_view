#include "wifi_manager.h"

#include <ESPping.h>
#include <WiFi.h>

#include "wifi_passwd.h"

#include "../led/led_manager.h"
#include "../supabase/supabase_manager.h"
#include "../supabase/api_secret.h"

void WiFiManager::setup()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WifiPasswd::ssid, WifiPasswd::password);
}

bool WiFiManager::testPing()
{
    bool ok = Ping.ping(ApiSecret::SUPABASE_HOST, 4);

    if (ok)
    {
        LEDManager::blinkRGB(LEDManager::COLOR_GREEN); // green
        Serial.println("Ping successful!");
        return true;
    }

    LEDManager::blinkRGB(LEDManager::COLOR_RED); // red
    Serial.println("Ping failed!");
    return false;
}