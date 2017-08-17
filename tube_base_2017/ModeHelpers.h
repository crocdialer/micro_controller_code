#pragma once

#include "LED_Path.h"

#define PATH_LENGTH 8

// some pin defines
#define LED_PIN 6

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
GREEN = Adafruit_NeoPixel::Color(60, 50, 255, 40),
BLACK = 0;

static const uint32_t g_colors[3] = {ORANGE, WHITE, GREEN};
static const uint32_t g_num_colors = 3;

// path variables
LED_Path g_path = LED_Path(LED_PIN, PATH_LENGTH);

class ModeHelper
{
public:
    virtual void process(uint32_t the_delta_time) = 0;
    virtual void reset() = 0;
};

class Mode_ONE_COLOR : public ModeHelper
{
public:

    void process(uint32_t the_delta_time) override
    {
        m_time_accum += the_delta_time;

        if(m_time_accum > m_trigger_time)
        {
            g_path.set_all_segments(m_next_color);

            uint32_t col_index = clamp<uint32_t>(random<uint32_t>(0, g_num_colors + 1),
                                                 0, g_num_colors - 1);
            m_next_color = g_colors[col_index];

            // remove timing values from here
            m_trigger_time = random<uint32_t>(2000, 10000);
            m_time_accum = 0;
        }
    };

    void reset() override
    {

    };

private:
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_next_color = GREEN;
};

class FlashModeHelper : public ModeHelper
{
public:

    virtual void process(uint32_t the_delta_time) override
    {
        m_time_accum += the_delta_time;

        if(m_time_accum > m_trigger_time)
        {
            g_path.set_current_max(0);
            m_trigger_time = random<uint32_t>(2000, 10000);
            m_time_accum = 0;
        }
    }

    virtual void reset() override
    {

    }

private:
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
};

ModeHelper* g_helper_one_color = new Mode_ONE_COLOR();
ModeHelper* g_helper_flash = new FlashModeHelper();
