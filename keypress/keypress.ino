#include "Keyboard.h"

constexpr uint32_t g_led_pin = 13;
constexpr uint32_t g_button_pin = 6;
constexpr uint8_t g_key = 'x';

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;

// keypress interval in ms
uint32_t g_keypress_interval = 200;

void setup()
{
    pinMode(g_led_pin, OUTPUT);
    pinMode(g_button_pin, INPUT_PULLUP);
    Keyboard.begin();
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // button pressed
    if(!digitalRead(g_button_pin))
    {
        digitalWrite(g_led_pin, true);

        if(g_time_accum >= g_keypress_interval)
        {
            Keyboard.write('x');
            g_time_accum = 0;
        }
    }
    else
    {
        // Keyboard.release('x');
        digitalWrite(g_led_pin, false);
    }
    delay(10);
}
