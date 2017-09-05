#include "LED_Path.h"

Segment::Segment(uint32_t the_length):
m_length(the_length)
{

}

LED_Path::LED_Path(uint32_t the_pin, uint32_t the_num_segments):
m_num_segments(the_num_segments)
{
    m_segments = new Segment*[the_num_segments];

    for(uint32_t i = 0; i < the_num_segments; ++i)
    {
        m_segments[i] = new Segment(SEGMENT_LENGTH);
    }

    m_strip = new Adafruit_NeoPixel(SEGMENT_LENGTH * the_num_segments, the_pin, NEO_GRBW + NEO_KHZ800);
    m_strip->begin();
    m_strip->setBrightness(255 * m_brightness);
    m_strip->show(); // Initialize all pixels to 'off'

    m_data = (uint8_t*)m_strip->getPixels();

    m_current_max = num_leds();
}

LED_Path::~LED_Path()
{
    delete m_strip;
    for(uint32_t i = 0; i < m_num_segments; ++i){ delete m_segments[i]; }
    delete[] m_segments;
}

void LED_Path::clear()
{
    memset(m_data, 0, num_leds() * 4);
}

void LED_Path::update(uint32_t the_delta_time)
{
    clear();

    for(uint32_t i = 0; i < m_num_segments; ++i)
    {
        if(!m_segments[i]->active()){ continue; }
        uint32_t c = m_segments[i]->color();
        uint32_t *ptr = (uint32_t*)m_data + i * SEGMENT_LENGTH;
        uint32_t *end_ptr = ptr + SEGMENT_LENGTH;

        for(;ptr < end_ptr;++ptr)
        {
            uint32_t current_index = ptr - (uint32_t*)m_data;
            if(current_index >= m_current_max){ goto finished; }

            float sin_val = create_sinus_val(current_index);
            *ptr = fade_color(c, m_brightness * sin_val);
        }
    }

finished:

    m_strip->show();
    m_current_max = min(num_leds(), m_current_max + m_flash_speed * the_delta_time / 1000.f);

    // advance sinus offsets
    for(uint32_t i = 0; i < 2; ++i)
    {
        m_sinus_offsets[i] += m_sinus_speeds[i] * the_delta_time / 1000.f;
    }
}

void LED_Path::set_brightness(float the_brightness)
{
     m_brightness = the_brightness;
     m_strip->setBrightness(255 * m_brightness);
}

void LED_Path::set_all_segments(uint32_t the_color)
{
    for(uint32_t i = 0; i < m_num_segments; ++i)
    {
        m_segments[i]->set_color(the_color);
    }
}
