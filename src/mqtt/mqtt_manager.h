#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdint.h>

// Publishes a retained "online" status to flespi's MQTT broker; the broker
// publishes "offline" (last will) if the device disappears.
struct MqttManager
{
    using CommandHandler = void (*)(const char *command);

    static void setup();

    // Called (from loop()/delayMs()) with the payload of each message on the command topic.
    static void setCommandHandler(CommandHandler handler);

    // Reconnects when needed and services the MQTT client. Call often.
    static void loop();

    // delay() replacement that keeps the MQTT connection alive while waiting.
    static void delayMs(uint32_t ms);

    static bool isConnected();
};

#endif // MQTT_MANAGER_H
