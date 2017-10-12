#pragma once

#include <Adafruit_NeoPixel.h>

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
GREEN = Adafruit_NeoPixel::Color(60, 50, 255, 40),
// AQUA = Adafruit_NeoPixel::Color(255, 0, 120, 40),
AQUA = Adafruit_NeoPixel::Color(255, 80, 120, 40),
PURPLE_ISH = Adafruit_NeoPixel::Color(255, 140, 70, 40),
DARK_ORANGE = Adafruit_NeoPixel::Color(0, 200, 40, 40),
TOKYO_CHERRY = Adafruit_NeoPixel::Color(60, 225, 90, 40),
ALL_IN = Adafruit_NeoPixel::Color(255, 255, 255, 255),
BLACK = 0;

static const uint32_t g_colors[] = {PURPLE_ISH, TOKYO_CHERRY, GREEN, DARK_ORANGE, ORANGE, WHITE};
static const uint32_t g_num_colors = 1;
