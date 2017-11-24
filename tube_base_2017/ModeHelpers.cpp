#include "ModeHelpers.h"

ModeHelper::ModeHelper(LED_Path* the_path):
m_path(the_path){}

void ModeHelper::set_trigger_time(uint32_t the_min, uint32_t the_max)
{
    m_trigger_time_min = the_min;
    m_trigger_time_max = the_max;
}

///////////////////////////////////////////////////////////////////////////////

Mode_ONE_COLOR::Mode_ONE_COLOR(LED_Path* the_path):ModeHelper(the_path)
{
    set_trigger_time(5000, 25000);
}

void Mode_ONE_COLOR::process(uint32_t the_delta_time)
{
    m_time_accum += the_delta_time;

    if(m_time_accum > m_trigger_time)
    {
        m_path->set_all_segments(m_next_color);

        uint32_t col_index = clamp<uint32_t>(random<uint32_t>(0, g_num_colors),
                                             0, g_num_colors - 1);
        m_next_color = g_colors[col_index];

        // Serial.println((int)(col_index));
        m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
        m_time_accum = 0;
    }
}

void Mode_ONE_COLOR::reset()
{
    m_time_accum = m_trigger_time = 0;

    for(uint32_t i = 0; i < m_path->num_segments(); ++i)
    {
        m_path->segment(i)->set_active(true);
    }
}

///////////////////////////////////////////////////////////////////////////////

ModeFlash::ModeFlash(LED_Path* the_path):ModeHelper(the_path)
{
    set_trigger_time(8000, 15000);
}

void ModeFlash::process(uint32_t the_delta_time)
{
    m_time_accum += the_delta_time;

    if(m_time_accum > m_trigger_time)
    {
        path()->set_current_max(0);
        set_flash_speed(random<uint32_t>(path()->num_leds(), path()->num_leds() * 4));//950, 5500
        m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
        m_time_accum = 0;
    }

    auto new_max_index = min(path()->num_leds(),
                             path()->current_max() + m_flash_speed * the_delta_time / 1000.f);
    path()->set_current_max(new_max_index);
}

void ModeFlash::reset()
{

}

///////////////////////////////////////////////////////////////////////////////

Mode_Segments::Mode_Segments(LED_Path* the_path):ModeHelper(the_path)
{
    set_trigger_time(1200 , 6000);
}

void Mode_Segments::process(uint32_t the_delta_time)
{
    m_time_accum += the_delta_time;

    if(m_time_accum > m_trigger_time)
    {
        for(uint32_t i = 0; i < m_path->num_segments(); ++i)
        {
            m_path->segment(i)->set_active(random<float>(0, 1) > .5f);

            uint32_t col_index = clamp<uint32_t>(random<uint32_t>(0, g_num_colors),
                                                 0, g_num_colors - 1);
            m_path->segment(i)->set_color(g_colors[col_index]);
        }
        m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
        m_time_accum = 0;
    }
}

void Mode_Segments::reset()
{
    m_time_accum = m_trigger_time = 0;

    for(uint32_t i = 0; i < m_path->num_segments(); ++i)
    {
        m_path->segment(i)->set_active(true);
    }
}

///////////////////////////////////////////////////////////////////////////////

FastSinus SinusFill::s_fast_sin;

SinusFill::SinusFill(LED_Path* the_path):ModeHelper(the_path)
{
    set_sinus_offsets(random<int>(0, 256), random<int>(0, 256));
}

void SinusFill::process(uint32_t the_delta_time)
{
    path()->clear();
    auto max_index = path()->current_max();
    float brightness = path()->brightness();

    for(uint32_t i = 0; i < path()->num_segments(); ++i)
    {
        Segment *seg = path()->segment(i);

        if(!seg->active()){ continue; }
        uint32_t c = seg->color();
        swap(((uint8_t*) &c)[0], ((uint8_t*) &c)[1]);

        uint8_t *ptr = seg->data();
        uint8_t *end_ptr = ptr + seg->length() * BYTES_PER_PIXEL;

        for(;ptr < end_ptr; ptr += BYTES_PER_PIXEL)
        {
            uint32_t current_index = (ptr - path()->data()) / BYTES_PER_PIXEL;
            if(current_index >= max_index){ goto finished; }

            float sin_val = create_sinus_val(current_index);
            uint32_t fade_col = fade_color(c, brightness * sin_val);
            memcpy(ptr, &fade_col, BYTES_PER_PIXEL);
        }
    }

finished:

    // m_strip->show();
    // m_current_max = min(num_leds(), m_current_max + m_flash_speed * the_delta_time / 1000.f);

    // advance sinus offsets
    for(uint32_t i = 0; i < 2; ++i)
    {
        m_sinus_offsets[i] += m_sinus_speeds[i] * the_delta_time / 1000.f;
    }
}

void SinusFill::reset()
{

}

///////////////////////////////////////////////////////////////////////////////

CompositeMode::CompositeMode(LED_Path* the_path):ModeHelper(the_path)
{
    set_trigger_time(1000 * 30 , 1000 * 120);
    m_mode_helpers[0] = new Mode_ONE_COLOR(the_path);
    m_mode_helpers[1] = new SinusFill(the_path);
    m_mode_helpers[2] = new ModeFlash(the_path);
}

void CompositeMode::process(uint32_t the_delta_time)
{
    m_time_accum += the_delta_time;

    // process child modes
    for(int i = 0; i < m_num_mode_helpers; ++i)
    {
         if(m_mode_helpers[i]){ m_mode_helpers[i]->process(the_delta_time); }
    }

    if(m_time_accum > m_trigger_time)
    {
        // // change mode here
        // if(m_mode_helpers[2])
        // {
        //     m_mode_helpers[1]->reset();
        //     swap(m_mode_helpers[1], m_mode_helpers[2]);
        //     m_shorter_duration = !m_shorter_duration;
        // }
        //
        // m_trigger_time = random<uint32_t>(m_trigger_time_min, m_trigger_time_max);
        // if(m_shorter_duration){ m_trigger_time /= 3; }

        m_time_accum = 0;
    }
};

void CompositeMode::reset()
{
    m_time_accum = m_trigger_time = 0;
    m_shorter_duration = false;
    for(int i = 0; i < m_num_mode_helpers; ++i){ m_mode_helpers[i]->reset(); }
}
