#pragma once

#include "LED_Path.h"
#include "ColorDefines.h"

// path variables
static LED_Path g_path = LED_Path(LED_PIN, PATH_LENGTH);

class ModeHelper
{
public:
    virtual void process(uint32_t the_delta_time) = 0;
    virtual void reset() = 0;

    void set_trigger_time(uint32_t the_min, uint32_t the_max);

protected:

    uint32_t m_time_accum = 0;
    uint32_t m_trigger_time = 0;
    uint32_t m_trigger_time_min, m_trigger_time_max;
};

class Mode_ONE_COLOR : public ModeHelper
{
public:
    Mode_ONE_COLOR();
    void process(uint32_t the_delta_time) override;
    void reset() override;

private:
    uint32_t m_next_color = GREEN;
};

class ModeFlash : public ModeHelper
{
public:
    ModeFlash();
    void process(uint32_t the_delta_time) override;
    void reset() override;
};

class Mode_Segments : public ModeHelper
{
public:

    Mode_Segments();
    void process(uint32_t the_delta_time) override;
    void reset() override;
};

class CompositeMode : public ModeHelper
{
public:

    CompositeMode();
    void process(uint32_t the_delta_time) override;
    void reset() override;

private:
    uint32_t m_num_mode_helpers = 2;
    ModeHelper* m_mode_helpers[3] = {new ModeFlash(), new Mode_ONE_COLOR(), new Mode_Segments()};
    bool m_shorter_duration = false;
};
