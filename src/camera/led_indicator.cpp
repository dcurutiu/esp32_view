#include "led_indicator.h"

#include <Arduino.h>

static bool s_initialized = false;

static void ensure_init()
{
    if (!s_initialized)
    {
        pinMode(LedIndicator::GPIO_NUM, OUTPUT);
        digitalWrite(LedIndicator::GPIO_NUM, LOW);
        s_initialized = true;
    }
}

void LedIndicator::on()
{
    ensure_init();
    digitalWrite(GPIO_NUM, HIGH);
}

void LedIndicator::off()
{
    ensure_init();
    digitalWrite(GPIO_NUM, LOW);
}
