#include "src/camera/acquisition_manager.h"
#include "src/camera/board_config.h"
#include "src/camera/calibration/calibration_manager.h"
#include "src/cloud_storage/cloudinary_manager.h"
#include "src/led/led_manager.h"
#include "src/mqtt/mqtt_manager.h"
#include "src/wifi/wifi_manager.h"

static constexpr uint32_t PING_INTERVAL_MS = 60UL * 1000UL;

static volatile bool acquireRequested = false;
static volatile bool calibrateRequested = false;
static uint8_t uploadFailures = 0;

static void onCommand(const char *command)
{
    if (strcmp(command, "acquire") == 0)
        acquireRequested = true;
    else if (strcmp(command, "calibrate") == 0)
        calibrateRequested = true;
    else
        Serial.printf("[main] Unknown command: %s\n", command);
}

static void calibrateCamera()
{
    MqttManager::publishEvent("calibrating");
    CalibrationManager::calibrate();
    MqttManager::publishEvent("calibrated");
}

static void acquireAndUpload()
{
    LEDManager::blinkRGB(LEDManager::COLOR_BLUE);
    const auto image = AcquisitionManager::acquireImage();
    if (!image)
        return;

    Serial.printf("[main] Captured image: %u bytes\n", (unsigned)image->len);
    Serial.println("[main] Uploading image to Cloudinary...");
    MqttManager::publishEvent("uploading");
    if (CloudinaryManager::uploadImage(*image))
    {
        uploadFailures = 0;
        MqttManager::publishEvent("uploaded");
        return;
    }

    uploadFailures++;
    Serial.printf("[main] Upload failed (%u/2)\n", uploadFailures);
    if (uploadFailures >= 2)
    {
        Serial.println("[main] 2 consecutive failures — restarting!");
        delay(500);
        ESP.restart();
    }
}

void setup()
{
    Serial.begin(115200);
    LEDManager::setup();
    WiFiManager::setup();
    MqttManager::setCommandHandler(onCommand);
    MqttManager::setup();
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
    static uint32_t lastPingMs = 0;

    MqttManager::loop();

    if (acquireRequested)
    {
        acquireRequested = false;
        acquireAndUpload();
    }

    if (calibrateRequested)
    {
        calibrateRequested = false;
        calibrateCamera();
    }

    if (millis() - lastPingMs >= PING_INTERVAL_MS)
    {
        lastPingMs = millis();
        if (!WiFiManager::testPing())
        {
            Serial.println("[main] No WiFi connection — retrying shortly");
            MqttManager::delayMs(25000);
            if (!WiFiManager::testPing())
            {
                Serial.println("[main] Still no connection — restarting now!");
                ESP.restart();
            }
        }
    }

    delay(20);
}
