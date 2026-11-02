#include "mqtt_manager.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../cloud_storage/api_secret.h"

static constexpr uint16_t KEEP_ALIVE_S = 90;
static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

static WiFiClientSecure net;
static PubSubClient mqtt(net);
static uint32_t lastAttemptMs = 0;

static MqttManager::CommandHandler commandHandler = nullptr;

static void onMessage(char *topic, uint8_t *payload, unsigned int length)
{
    char cmd[32];
    size_t n = length < sizeof(cmd) - 1 ? length : sizeof(cmd) - 1;
    memcpy(cmd, payload, n);
    cmd[n] = '\0';
    Serial.printf("[MQTT] Command received: %s\n", cmd);
    if (commandHandler)
        commandHandler(cmd);
}

void MqttManager::setCommandHandler(CommandHandler handler)
{
    commandHandler = handler;
}

void MqttManager::setup()
{
    mqtt.setCallback(onMessage);
    net.setInsecure();
    mqtt.setServer(ApiSecret::MQTT_HOST, ApiSecret::MQTT_PORT);
    mqtt.setKeepAlive(KEEP_ALIVE_S);
}

bool MqttManager::isConnected()
{
    return mqtt.connected();
}

void MqttManager::loop()
{
    if (!mqtt.connected())
    {
        if (WiFi.status() != WL_CONNECTED)
            return;
        if (lastAttemptMs != 0 && millis() - lastAttemptMs < RECONNECT_INTERVAL_MS)
            return;
        lastAttemptMs = millis();

        char clientId[32];
        snprintf(clientId, sizeof(clientId), "esp32_view-%04X", (uint16_t)(ESP.getEfuseMac() >> 32));

        // flespi: username = token, empty password. Last will = "offline" (retained).
        if (mqtt.connect(clientId, ApiSecret::MQTT_TOKEN, "", ApiSecret::MQTT_STATUS_TOPIC, 1, true, "offline"))
        {
            mqtt.publish(ApiSecret::MQTT_STATUS_TOPIC, "online", true);
            mqtt.subscribe(ApiSecret::MQTT_COMMAND_TOPIC, 1);
            Serial.println("[MQTT] Connected, status: online");
        }
        else
        {
            Serial.printf("[MQTT] Connect failed, rc=%d\n", mqtt.state());
        }
        return;
    }
    mqtt.loop();
}

void MqttManager::delayMs(uint32_t ms)
{
    uint32_t start = millis();
    while (millis() - start < ms)
    {
        loop();
        delay(50);
    }
}
