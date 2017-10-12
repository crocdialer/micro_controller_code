#include "LED_Path.h"

FastSinus LED_Path::s_fast_sin;

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

    m_strip = new Adafruit_NeoPixel(SEGMENT_LENGTH * the_num_segments, the_pin, CURRENT_LED_TYPE);
    m_strip->begin();
    m_strip->setBrightness(255 * m_brightness);
    m_strip->show(); // Initialize all pixels to 'off'

    m_data = (uint8_t*)m_strip->getPixels();

    m_current_max = num_leds();
}

LED_Path::~LED_Path()
{
    if(m_strip){ delete m_strip; }
    if(m_segments)
    {
        for(uint32_t i = 0; i < m_num_segments; ++i){ delete m_segments[i]; }
        delete[] m_segments;
    }
}

void LED_Path::clear()
{
    memset(m_data, 0, num_leds() * BYTES_PER_PIXEL);
}

void LED_Path::update(uint32_t the_delta_time)
{
    clear();

    for(uint32_t i = 0; i < m_num_segments; ++i)
    {
        if(!m_segments[i]->active()){ continue; }
        uint32_t c = m_segments[i]->color();
        swap(((uint8_t*) &c)[0], ((uint8_t*) &c)[1]);

        uint8_t *ptr = m_data + i * SEGMENT_LENGTH * BYTES_PER_PIXEL;
        uint8_t *end_ptr = ptr + SEGMENT_LENGTH * BYTES_PER_PIXEL;

        for(;ptr < end_ptr; ptr += BYTES_PER_PIXEL)
        {
            uint32_t current_index = (ptr - m_data) / BYTES_PER_PIXEL;
            if(current_index >= m_current_max){ goto finished; }

            float sin_val = create_sinus_val(current_index);
            uint32_t fade_col = fade_color(c, m_brightness * sin_val);
            memcpy(ptr, &fade_col, BYTES_PER_PIXEL);
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
