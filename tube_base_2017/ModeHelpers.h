#pragma once

#include "utils.h"
#include "LED_Path.h"
#include "ColorDefines.h"

// path variables
// static LED_Path g_path = LED_Path(LED_PIN, PATH_LENGTH);

class ModeHelper
{
public:
    ModeHelper(LED_Path* the_path);
    virtual void process(uint32_t the_delta_time) = 0;
    virtual void reset() = 0;

    virtual void set_trigger_time(uint32_t the_min, uint32_t the_max);

    inline LED_Path* path(){ return m_path; };

protected:

    LED_Path* m_path;
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min = 0, m_trigger_time_max = 0;
};

class Mode_ONE_COLOR : public ModeHelper
{
public:
    Mode_ONE_COLOR(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;

private:
    uint32_t m_next_color = g_colors[0];
};

class ModeFlash : public ModeHelper
{
public:
    ModeFlash(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;

    inline void set_flash_speed(float the_speed){ m_flash_speed = the_speed; }
    inline void set_flash_direction(bool b){ m_flash_forward = b; }

private:
    
    float m_flash_speed = 800.f;
    bool m_flash_forward = true;
};

class Mode_Segments : public ModeHelper
{
public:

    Mode_Segments(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;
};

class SinusFill : public ModeHelper
{
public:
    SinusFill(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;

    void set_sinus_offsets(float a, float b){ m_sinus_offsets[0] = a; m_sinus_offsets[1] = b;}

private:
    float m_sinus_factors[2] = {PI_2, PI * 7.3132f};
    float m_sinus_speeds[2] = {-15, -2};
    float m_sinus_offsets[2] = {0, 211};

    static FastSinus s_fast_sin;

    inline float create_sinus_val(uint32_t the_index)
    {
        float ret = 0.f;
        constexpr uint32_t num_sinus = 2;

        for(uint32_t i = 0; i < num_sinus; ++i)
        {
            float val = m_sinus_factors[i] * (the_index + m_sinus_offsets[i]) / (TUBE_LENGTH /*/ 5.f*/);
            ret += s_fast_sin(val);
        }
        return clamp((ret / num_sinus + 1.f) / 2.f, 0.05f, 1.f);
    }
};

class CompositeMode : public ModeHelper
{
public:

    CompositeMode(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;

private:
    uint32_t m_num_mode_helpers = 2;
    ModeHelper* m_mode_helpers[3] = {nullptr, nullptr, nullptr};
    bool m_shorter_duration = false;
};
