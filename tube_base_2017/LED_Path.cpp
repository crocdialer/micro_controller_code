#include "LED_Path.h"
#include "utils.h"

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

uint32_t LED_Path::num_leds() const
{
    uint32_t sum = 0;
    Segment *ptr = m_segments[0], *end_ptr = m_segments[0] + num_segments();

    for(;ptr < end_ptr;++ptr){ sum += ptr->length(); }
    return sum;
}

void LED_Path::clear()
{
    memset(m_data, 0, num_leds() * 4);
}

void LED_Path::update(uint32_t the_delta_time)
{
    clear();

    uint32_t count = 0;

    for(uint32_t i = 0; i < m_num_segments; ++i)
    {
        uint32_t c = m_segments[i]->color();
        uint32_t *ptr = (uint32_t*)m_data, *end_ptr = ptr + m_segments[i]->length();

        for(;ptr < end_ptr;++ptr)
        {
            float sin_val = 1.f;
            *ptr = fade_color(c, m_brightness * sin_val);
            ++count;

            if(count >= m_current_max){ goto finished; }
        }
    }

    //TODO: plasma sinus here

finished:

    m_strip->show();
    m_current_max = min(num_leds() - 1, m_current_max + m_grow_speed * the_delta_time / 1000.f);
}

void LED_Path::set_brightness(float the_brightness)
{
     m_brightness = the_brightness;
     m_strip->setBrightness(255 * m_brightness);
}
