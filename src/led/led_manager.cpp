#include "led_manager.h"

Adafruit_NeoPixel LEDManager::rgb_led(LEDManager::NUM_RGB_LEDS, LEDManager::RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void LEDManager::setup()
{
    pinMode(LED_TX_PIN, OUTPUT);
    pinMode(LED_RX_PIN, OUTPUT);
    rgb_led.begin();
    rgb_led.show();
}

void LEDManager::blinkRX()
{
    digitalWrite(LED_RX_PIN, HIGH);
    delay(500);
    digitalWrite(LED_RX_PIN, LOW);
    delay(500);
}

void LEDManager::blinkTX()
{
    digitalWrite(LED_TX_PIN, HIGH);
    delay(500);
    digitalWrite(LED_TX_PIN, LOW);
    delay(500);
}

void LEDManager::blinkRGB(uint8_t r, uint8_t g, uint8_t b)
{
    rgb_led.setPixelColor(0, rgb_led.Color(r, g, b));
    rgb_led.show();
    delay(500);
    rgb_led.setPixelColor(0, rgb_led.Color(0, 0, 0));
    rgb_led.show();
    delay(500);
}

void LEDManager::blinkRGB(uint32_t color)
{
    {
        if (color == 0)
        {
            blinkRGB(0, 0, 0); // off
            return;
        }
        if (color > 0xFFFFFF)
        {
            color = 0xFFFFFF; // cap to white
        }
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        blinkRGB(r, g, b);
    }
}