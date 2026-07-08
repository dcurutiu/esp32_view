#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

// Simple on/off wrapper around the activity indicator LED so capture and
// calibration code share the same pin setup instead of duplicating it.
struct LedIndicator
{
    static constexpr int GPIO_NUM = 1;

    static void on();
    static void off();
};

#endif // LED_INDICATOR_H
