#pragma once

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

protected:

    LED_Path* m_path;
    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min, m_trigger_time_max;
};

class Mode_ONE_COLOR : public ModeHelper
{
public:
    Mode_ONE_COLOR(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;

private:
    uint32_t m_next_color = GREEN;
};

class ModeFlash : public ModeHelper
{
public:
    ModeFlash(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;
};

class Mode_Segments : public ModeHelper
{
public:

    Mode_Segments(LED_Path* the_path);
    void process(uint32_t the_delta_time) override;
    void reset() override;
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
