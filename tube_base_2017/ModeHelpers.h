#pragma once

#include "LED_Path.h"
#include "ColorDefines.h"

#define PATH_LENGTH 8

// some pin defines
#define LED_PIN 6

static const uint32_t g_colors[3] = {ORANGE, AQUA, GREEN};
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

            m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
            m_time_accum = 0;
        }
    };

    void reset() override
    {
        m_time_accum = m_trigger_time = 0;

        for(uint32_t i = 0; i < g_path.num_segments(); ++i)
        {
            g_path.segment(i)->set_active(true);
        }
    };

    void set_trigger_time(uint32_t the_min, uint32_t the_max)
    {
        m_trigger_time_min = the_min;
        m_trigger_time_max = the_max;
    }

private:
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min = 5000, m_trigger_time_max = 25000;
    uint32_t m_next_color = GREEN;
};

class ModeFlash : public ModeHelper
{
public:

    virtual void process(uint32_t the_delta_time) override
    {
        m_time_accum += the_delta_time;

        if(m_time_accum > m_trigger_time)
        {
            g_path.set_current_max(0);
            g_path.set_flash_speed(random<uint32_t>(750, 2500));
            m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
            m_time_accum = 0;
        }
    }

    virtual void reset() override
    {

    }

private:
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min = 8000, m_trigger_time_max = 15000;
};

class Mode_Segments : public ModeHelper
{
public:

    void process(uint32_t the_delta_time) override
    {
        m_time_accum += the_delta_time;

        if(m_time_accum > m_trigger_time)
        {
            for(uint32_t i = 0; i < g_path.num_segments(); ++i)
            {
                g_path.segment(i)->set_active(random<float>(0, 1) > .5f);
            }
            m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
            m_time_accum = 0;
        }
    };

    void reset() override
    {
        m_time_accum = m_trigger_time = 0;

        for(uint32_t i = 0; i < g_path.num_segments(); ++i)
        {
            g_path.segment(i)->set_active(true);
        }
    };

    void set_trigger_time(uint32_t the_min, uint32_t the_max)
    {
        m_trigger_time_min = the_min;
        m_trigger_time_max = the_max;
    }

private:
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min = 100, m_trigger_time_max = 800;
};

class CompositeMode : public ModeHelper
{
public:

    void process(uint32_t the_delta_time) override
    {
        m_time_accum += the_delta_time;

        // process child modes
        for(int i = 0; i < m_num_mode_helpers; ++i){ m_mode_helpers[i]->process(the_delta_time); }

        if(m_time_accum > m_trigger_time)
        {
            // change mode here

            m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
            m_time_accum = 0;
        }
    };

    void reset() override
    {
        m_time_accum = m_trigger_time = 0;

        for(uint32_t i = 0; i < g_path.num_segments(); ++i)
        {
            g_path.segment(i)->set_active(true);
        }
    };

    void set_trigger_time(uint32_t the_min, uint32_t the_max)
    {
        m_trigger_time_min = the_min;
        m_trigger_time_max = the_max;
    }

private:
    uint32_t m_num_mode_helpers = 2;
    ModeHelper* m_mode_helpers[2] = {new Mode_ONE_COLOR(), new ModeFlash()};
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min = 100, m_trigger_time_max = 800;
};

ModeHelper* g_mode_helper = new CompositeMode();

// ModeHelper* g_helper_one_color = new Mode_ONE_COLOR();
// ModeHelper* g_helper_flash = new ModeFlash();
// ModeHelper* g_helper_segments = new Mode_Segments();
