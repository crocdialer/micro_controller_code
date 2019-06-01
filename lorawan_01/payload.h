#pragma once

#include <stdint.h>

struct payload_t
{
    // GRBW
    uint8_t color[4] = {0, 0, 0, 0};

    uint8_t brightness = 255;

    uint8_t num_leds = 8;

    uint8_t battery = 0;
};
