#include "wifi_manager.h"

#include <ESPping.h>
#include <WiFi.h>

#include "src/cloud_storage/api_secret.h"
#include "src/led/led_manager.h"
#include "wifi_passwd.h"

void WiFiManager::setup()
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WifiPasswd::ssid, WifiPasswd::password);
    configTime(0, 0, "time.google.com"); // NTP, required for signed Cloudinary uploads
}

bool WiFiManager::testPing()
{
    bool ok = Ping.ping(ApiSecret::PING_HOST, 4);
    if (ok)
    {
        Serial.println("Ping successful!");
        return true;
    }
    Serial.println("Ping failed!");
    return false;
}