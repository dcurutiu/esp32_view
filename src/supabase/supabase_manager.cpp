#include "supabase_manager.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include "api_secret.h"

static constexpr const char *BOUNDARY = "ESP32Boundary42";

void SupabaseManager::uploadImage(const CapturedImage &img)
{
    Serial.printf("Supabase: connecting to %s:443 ...\n", ApiSecret::SUPABASE_HOST);
    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect(ApiSecret::SUPABASE_HOST, 443))
    {
        Serial.println("Supabase: TCP/TLS connect FAILED (DNS or TLS handshake)");
        return;
    }
    Serial.println("Supabase: TCP/TLS connected OK");
    Serial.printf("Supabase: sending POST %s  (payload %u bytes)\n", ApiSecret::FUNCTION_PATH, (unsigned)img.len);

    char partHead[256];
    snprintf(
        partHead,
        sizeof(partHead),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"latest.bmp\"\r\n"
        "Content-Type: image/bmp\r\n\r\n",
        BOUNDARY
    );

    char partTail[64];
    snprintf(partTail, sizeof(partTail), "\r\n--%s--\r\n", BOUNDARY);

    size_t contentLength = strlen(partHead) + img.len + strlen(partTail);

    Serial.printf("strlen(partHead)=%u\n", (unsigned)strlen(partHead));
    Serial.printf("img.len=%u\n", (unsigned)img.len);
    Serial.printf("strlen(partTail)=%u\n", (unsigned)strlen(partTail));
    Serial.printf("contentLength=%u\n", (unsigned)contentLength);

    client.printf("POST %s HTTP/1.1\r\n", ApiSecret::FUNCTION_PATH);
    client.printf("Host: %s\r\n", ApiSecret::SUPABASE_HOST);
    client.printf("Authorization: Bearer %s\r\n", ApiSecret::SUPABASE_KEY);
    client.printf("apikey: %s\r\n", ApiSecret::SUPABASE_KEY);
    client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
    client.printf("Content-Length: %u\r\n", contentLength);
    client.print("Connection: close\r\n\r\n");
    client.print(partHead);
    client.write(img.data, img.len);
    client.print(partTail);

    // --- Read response ---
    uint32_t deadline = millis() + 10000;
    while (!client.available() && millis() < deadline) delay(10);

    if (!client.available())
    {
        Serial.println("Supabase: no response received, timing out");
        client.stop();
        return;
    }

    // Status line: "HTTP/1.1 200 OK"
    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    int httpCode = 0;
    {
        int sp1 = statusLine.indexOf(' ');
        int sp2 = statusLine.indexOf(' ', sp1 + 1);
        if (sp1 > 0 && sp2 > sp1)
        {
            httpCode = statusLine.substring(sp1 + 1, sp2).toInt();
            String reason = statusLine.substring(sp2 + 1);
            Serial.printf("Supabase: HTTP %d %s\n", httpCode, reason.c_str());
        }
        else
        {
            Serial.printf("Supabase: malformed status line: %s\n", statusLine.c_str());
        }
    }

    // Skip headers (read until blank line)
    while (client.connected())
    {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty())
        {
            break;
        }
    }

    // Read body (capped to avoid OOM)
    constexpr size_t BODY_CAP = 512;
    char body[BODY_CAP + 1] = {};
    size_t bodyLen = 0;
    deadline = millis() + 10000;
    while ((client.connected() || client.available()) && bodyLen < BODY_CAP && millis() < deadline)
    {
        if (client.available())
        {
            body[bodyLen++] = (char)client.read();
        }
        else
        {
            delay(10);
        }
    }
    body[bodyLen] = '\0';

    if (httpCode >= 200 && httpCode < 300)
    {
        Serial.println("Supabase: upload OK");
    }
    else
    {
        // Extract "error" and "message" values from JSON without a full parser
        auto extractJsonString = [](const char *json, const char *key) -> String
        {
            char search[64];
            snprintf(search, sizeof(search), "\"%s\"", key);
            const char *pos = strstr(json, search);
            if (!pos)
            {
                return String();
            }
            pos += strlen(search);
            while (*pos == ' ' || *pos == ':')
            {
                pos++;
            }
            if (*pos == '"')
            {
                pos++;
                const char *end = strchr(pos, '"');
                if (end)
                {
                    return String(pos).substring(0, end - pos);
                }
            }
            return String();
        };

        String errField = extractJsonString(body, "error");
        String msgField = extractJsonString(body, "message");

        if (errField.length())
        {
            Serial.printf("Supabase error  : %s\n", errField.c_str());
        }
        if (msgField.length())
        {
            Serial.printf("Supabase message: %s\n", msgField.c_str());
        }
        if (!errField.length() && !msgField.length() && bodyLen > 0)
        {
            Serial.printf("Supabase body   : %s\n", body); // fallback if unknown format
        }
    }

    client.stop();
}