#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Adafruit_NeoPixel.h>

struct LEDManager
{
    static constexpr uint8_t LED_TX_PIN = 43;
    static constexpr uint8_t LED_RX_PIN = 44;
    static constexpr uint8_t RGB_LED_PIN = 48;
    static constexpr uint8_t NUM_RGB_LEDS = 1;

    static constexpr uint32_t COLOR_OFF = 0x000000;
    static constexpr uint32_t COLOR_RED = 0xFF0000;
    static constexpr uint32_t COLOR_GREEN = 0x00FF00;
    static constexpr uint32_t COLOR_BLUE = 0x0000FF;
    static constexpr uint32_t COLOR_WHITE = 0xFFFFFF;

    static Adafruit_NeoPixel rgb_led;

    static void setup();
    static void blinkRX();
    static void blinkTX();
    static void blinkRGB(uint8_t r, uint8_t g, uint8_t b);
    static void blinkRGB(uint32_t color);
};

#endif // LED_MANAGER_H