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

    // Publishes a (non-retained) message on the event topic. Returns false if not connected.
    static bool publishEvent(const char *event);

    // Publishes the upload progress (e.g. "40%"), non-retained. Returns false if not connected.
    static bool publishUploadStatus(const char *status);

    // Publishes an error message on the error topic. Returns false if not connected.
    static bool publishError(const char *error);
};

#endif // MQTT_MANAGER_H
