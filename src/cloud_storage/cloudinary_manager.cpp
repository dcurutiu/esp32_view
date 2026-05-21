#include "cloudinary_manager.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include "api_secret.h"

static constexpr const char *BOUNDARY = "ESP32Boundary42";

// Writes the first string value for `key` from a flat JSON object into `out`.
// Returns true on success.
static bool extractJsonString(const char *json, const char *key, char *out, size_t outSize)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return false;
    pos += strlen(search);
    while (*pos == ' ' || *pos == ':')
        pos++;
    if (*pos == '"')
    {
        pos++;
        const char *end = strchr(pos, '"');
        if (end)
        {
            size_t len = min((size_t)(end - pos), outSize - 1);
            strncpy(out, pos, len);
            out[len] = '\0';
            return true;
        }
    }
    return false;
}

static void printHeap(const char *tag)
{
    Serial.printf("[Mem] %-30s  heap=%u  PSRAM=%u\n",
                  tag, ESP.getFreeHeap(), ESP.getFreePsram());
}

bool CloudinaryManager::uploadImage(const CapturedImage &img)
{
    printHeap("uploadImage start");

    char uploadPath[64];
    snprintf(uploadPath, sizeof(uploadPath), "/v1_1/%s/image/upload", ApiSecret::CLOUD_NAME);

    Serial.printf("[Cloudinary] Connecting to %s:443 ...\n", ApiSecret::CLOUDINARY_HOST);

    // Scope block so WiFiClientSecure is destroyed (and its TLS heap freed)
    // before we print the final heap snapshot.
    int httpCode = 0;
    {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(30);

        uint32_t t0 = millis();
        if (!client.connect(ApiSecret::CLOUDINARY_HOST, 443))
        {
            Serial.println("[Cloudinary] TCP/TLS connect FAILED");
            return false;
        }
        Serial.printf("[Cloudinary] Connected in %u ms\n", millis() - t0);
        printHeap("after TLS connect");

        // --- Build multipart parts ---
        char presetPart[160];
        snprintf(presetPart, sizeof(presetPart),
                 "--%s\r\n"
                 "Content-Disposition: form-data; name=\"upload_preset\"\r\n\r\n"
                 "%s\r\n",
                 BOUNDARY, ApiSecret::UPLOAD_PRESET);

        char publicIdPart[160];
        snprintf(publicIdPart, sizeof(publicIdPart),
                 "--%s\r\n"
                 "Content-Disposition: form-data; name=\"public_id\"\r\n\r\n"
                 "%s\r\n",
                 BOUNDARY, ApiSecret::PUBLIC_ID);

        char filePartHead[160];
        snprintf(filePartHead, sizeof(filePartHead),
                 "--%s\r\n"
                 "Content-Disposition: form-data; name=\"file\"; filename=\"QSXGA_latest.jpg\"\r\n"
                 "Content-Type: image/jpeg\r\n\r\n",
                 BOUNDARY);

        char partTail[64];
        snprintf(partTail, sizeof(partTail), "\r\n--%s--\r\n", BOUNDARY);

        size_t contentLength = strlen(presetPart) + strlen(publicIdPart) +
                               strlen(filePartHead) + img.len + strlen(partTail);

        Serial.printf("[Cloudinary] POST %s  content-length=%u\n", uploadPath, (unsigned)contentLength);

        // --- Send HTTP headers ---
        client.printf("POST %s HTTP/1.1\r\n", uploadPath);
        client.printf("Host: %s\r\n", ApiSecret::CLOUDINARY_HOST);
        client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
        client.printf("Content-Length: %u\r\n", (unsigned)contentLength);
        client.print("Connection: close\r\n\r\n");

        // --- Send text fields ---
        client.print(presetPart);
        client.print(publicIdPart);
        client.print(filePartHead);
        Serial.println("[Cloudinary] Text fields sent, streaming image ...");

        // --- Stream binary image with deadline ---
        static constexpr size_t CHUNK = 4096;
        static constexpr uint32_t STREAM_TIMEOUT_MS = 120000; // 2 min
        size_t sent = 0;
        size_t lastPrint = 0;
        t0 = millis();
        uint32_t deadline = t0 + STREAM_TIMEOUT_MS;

        while (sent < img.len)
        {
            if (millis() > deadline)
            {
                Serial.printf("[Cloudinary] Stream timed out after %u ms at byte %u/%u\n",
                              (unsigned)(millis() - t0), (unsigned)sent, (unsigned)img.len);
                return false;
            }
            if (!client.connected())
            {
                Serial.printf("[Cloudinary] Connection dropped at byte %u/%u!\n",
                              (unsigned)sent, (unsigned)img.len);
                return false;
            }

            size_t toSend = min(CHUNK, img.len - sent);
            size_t written = client.write(img.data + sent, toSend);
            if (written == 0)
            {
                delay(10); // transient stall — retry
                continue;
            }
            sent += written;

            // Progress log every 8 KB
            if (sent - lastPrint >= 8192)
            {
                Serial.printf("[Cloudinary] Sent %u/%u bytes (%.0f%%)\n",
                              (unsigned)sent, (unsigned)img.len,
                              100.0f * sent / img.len);
                lastPrint = sent;
            }
        }
        Serial.printf("[Cloudinary] Image fully sent in %u ms\n", millis() - t0);

        client.print(partTail);
        Serial.println("[Cloudinary] Waiting for response ...");

        // --- Read response ---
        t0 = millis();
        uint32_t responseDeadline = millis() + 50000;
        while (!client.available() && client.connected() && millis() < responseDeadline)
            delay(10);

        if (!client.available())
        {
            Serial.printf("[Cloudinary] No response after %u ms (connected=%d)\n",
                          millis() - t0, (int)client.connected());
            return false;
        }
        Serial.printf("[Cloudinary] Response in %u ms\n", millis() - t0);

        // --- Status line (no String — fixed char buffer) ---
        char statusBuf[128] = {};
        int statusLen = client.readBytesUntil('\n', statusBuf, sizeof(statusBuf) - 1);
        if (statusLen > 0 && statusBuf[statusLen - 1] == '\r')
            statusBuf[--statusLen] = '\0';

        {
            char *sp1 = strchr(statusBuf, ' ');
            if (sp1)
            {
                httpCode = atoi(sp1 + 1);
                Serial.printf("[Cloudinary] %s\n", statusBuf);
            }
        }

        // --- Response headers (no String — reuse one fixed buffer) ---
        char lineBuf[256];
        while (client.connected())
        {
            int len = client.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
            lineBuf[len] = '\0';
            if (len > 0 && lineBuf[len - 1] == '\r')
                lineBuf[--len] = '\0';
            if (len == 0)
                break;
            Serial.printf("[Cloudinary] hdr: %s\n", lineBuf);
        }

        // --- Body ---
        constexpr size_t BODY_CAP = 512;
        char body[BODY_CAP + 1] = {};
        size_t bodyLen = 0;
        responseDeadline = millis() + 10000;
        while ((client.connected() || client.available()) && bodyLen < BODY_CAP && millis() < responseDeadline)
        {
            if (client.available())
                body[bodyLen++] = (char)client.read();
            else
                delay(10);
        }
        body[bodyLen] = '\0';

        char valueBuf[256] = {};
        if (httpCode >= 200 && httpCode < 300)
        {
            if (extractJsonString(body, "secure_url", valueBuf, sizeof(valueBuf)))
                Serial.printf("[Cloudinary] Upload OK → %s\n", valueBuf);
            else
                Serial.println("[Cloudinary] Upload OK");
        }
        else
        {
            if (!extractJsonString(body, "message", valueBuf, sizeof(valueBuf)))
                extractJsonString(body, "error", valueBuf, sizeof(valueBuf));
            if (valueBuf[0])
                Serial.printf("[Cloudinary] Error: %s\n", valueBuf);
            else if (bodyLen > 0)
                Serial.printf("[Cloudinary] Body: %s\n", body);
        }

        client.stop();
    } // WiFiClientSecure destroyed here → TLS heap freed

    printHeap("uploadImage end");
    return httpCode >= 200 && httpCode < 300;
}

