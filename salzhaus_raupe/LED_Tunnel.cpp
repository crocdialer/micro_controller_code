#include "LED_Tunnel.h"
#include "utils.h"

//! the default-value will cause no brightness adjustment
uint8_t g_brightness = 150;

Tunnel::Tunnel()
{
    memset(m_strips, 0, sizeof(m_strips));
}

void Tunnel::init()
{
    for(uint8_t i = 0; i < g_num_pins; ++i)
    {
        if(m_strips[i]){ delete m_strips[i]; m_strips[i] = nullptr; }
    }

    m_strips[0] = new Adafruit_NeoPixel(m_num_gates * 15, g_led_pins[0], NEO_GRBW + NEO_KHZ800);
    // m_strips[1] = new Adafruit_NeoPixel(42 + 34 + 42, g_led_pins[1], NEO_GRBW + NEO_KHZ800);
    // m_strips[2] = new Adafruit_NeoPixel(6 * 3 * 42, g_led_pins[2], NEO_GRBW + NEO_KHZ800);
    uint8_t* data_start[g_num_pins];

    char buf[128], *ptr = buf;

    for(uint8_t i = 0; i < g_num_pins; ++i)
    {
        m_strips[i]->begin();
        m_strips[i]->setBrightness(50);
        m_strips[i]->show(); // Initialize all pixels to 'off'
        data_start[i] = (uint8_t*)m_strips[i]->getPixels();
        ptr += sprintf(ptr, "strip[%d]: %d\n", i, (int)data_start[i]);
    }
    Serial.write(buf);

    const size_t gate_num_bytes = 15 * sizeof(uint32_t);

    for(int i = 0; i < m_num_gates; ++i)
    {
        m_gates[i] = Gate(data_start[0] + i * gate_num_bytes, 5, 5, 5, Gate::NORMAL);
    }

    // PIN 10
    // m_gates[0] = Gate(data_start[0] + 5 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);
    // m_gates[1] = Gate(data_start[0] + 4 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    // m_gates[2] = Gate(data_start[0] + 3 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);
    // m_gates[3] = Gate(data_start[0] + 2 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    // m_gates[4] = Gate(data_start[0] + 1 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);
    // m_gates[5] = Gate(data_start[0] + 0 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    //
    // // PIN 11
    // m_gates[6] = Gate(data_start[1], 42, 34, 42, Gate::NORMAL);
    //
    // // PIN 12
    // m_gates[7] = Gate(data_start[2] + 0 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    // m_gates[8] = Gate(data_start[2] + 1 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);
    // m_gates[9] = Gate(data_start[2] + 2 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    // m_gates[10] = Gate(data_start[2] + 3 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);
    // m_gates[11] = Gate(data_start[2] + 4 * gate_num_bytes, 42, 42, 42, Gate::NORMAL);
    // m_gates[12] = Gate(data_start[2] + 5 * gate_num_bytes, 42, 42, 42, Gate::REVERSE);

    m_num_leds = 0;
    for(uint32_t i = 0; i < m_num_gates; ++i){ m_num_leds += m_gates[i].num_leds(); }
    if(m_pixel_time_buf){ delete[] m_pixel_time_buf; }
    m_pixel_time_buf = new unsigned long[m_num_leds];
    memset(m_pixel_time_buf, 0, m_num_leds * sizeof(unsigned long));

    sprintf(buf, "num leds: %d\n", m_num_leds);
    Serial.write(buf);
}

uint8_t Tunnel::brightness() const
{
    return g_brightness;
}

void Tunnel::set_brightness(uint8_t the_brightness)
{
    g_brightness = the_brightness;
}

void Tunnel::clear()
{
    for(uint8_t i = 0; i < m_num_gates; ++i)
    {
        m_gates[i].set_all_pixels(0);
    }
}

void Tunnel::add_random_pixels(uint16_t the_count, uint32_t the_delay_millis)
{
    if(!m_pixel_time_buf){ return; }
    unsigned long time_stamp = millis();

    for(uint16_t i = 0; i < the_count; ++i)
    {
        int rnd_index = random<int>(0, m_num_leds);
        int rnd_time = random<int>(0, the_delay_millis / 2);
        m_pixel_time_buf[rnd_index] = time_stamp + the_delay_millis + rnd_time;
    }
}

void Tunnel::update(uint32_t the_delta_time)
{
    uint32_t pix_idx = 0;
    uint32_t time_stamp = millis();

    // iterate timestamp array and set pixel colors
    for(uint32_t i = 0; i < m_num_gates; ++i)
    {
        uint32_t num_gate_leds = m_gates[i].num_leds();
        uint32_t *gate_data = (uint32_t*)m_gates[i].data();

        for(uint32_t j = 0; j < num_gate_leds; ++j)
        {
            if(m_pixel_time_buf[pix_idx] > time_stamp)
            {
                gate_data[j] = gate_data[j] ? color_add(gate_data[j], ORANGE) : ORANGE;
            }
            pix_idx++;
        }
    }
    for(int i = 0; i < g_num_pins; ++i){ m_strips[i]->show(); }
}

Gate::Gate():
m_data(nullptr),
m_num_leds(0),
m_direction(NORMAL)
{}

Gate::Gate(uint8_t *data_start, uint16_t num_left, uint16_t num_top, uint16_t num_right,
           Direction the_dir):
m_data(data_start),
m_num_leds(num_left + num_top + num_right),
m_direction(the_dir)
{
    m_seq_length[0] = num_left;
    m_seq_length[1] = num_top;
    m_seq_length[2] = num_right;
}

void Gate::set_pixel(uint32_t the_index, uint32_t the_color)
{
    if(m_data && the_index < m_num_leds)
    {
        the_index = m_direction == NORMAL ?
            the_index : (m_num_leds - 1 - the_index);

        the_color = fade_color(the_color, g_brightness / 255.f);
        uint32_t *ptr = (uint32_t*)m_data;
        ptr[the_index] = the_color;
    }
}

void Gate::set_all_pixels(uint32_t the_color)
{
    if(!m_data) return;
    the_color = fade_color(the_color, g_brightness / 255.f);
    uint32_t *ptr = (uint32_t*)m_data,
    *end_ptr = ptr + m_num_leds;

    for(; ptr < end_ptr; ++ptr){ *ptr = the_color; }
}
