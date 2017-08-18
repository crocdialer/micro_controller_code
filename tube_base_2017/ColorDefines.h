#pragma once

#include <Adafruit_NeoPixel.h>

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
GREEN = Adafruit_NeoPixel::Color(60, 50, 255, 40),
AQUA = Adafruit_NeoPixel::Color(255, 0, 120, 40),
BLACK = 0;

static const uint32_t g_colors[4] = {ORANGE, AQUA, GREEN, WHITE};
static const uint32_t g_num_colors = 4;
