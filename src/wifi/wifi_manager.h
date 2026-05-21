#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

struct WiFiManager
{
    static void setup();
    static bool testPing();
};

#endif // WIFI_MANAGER_H
