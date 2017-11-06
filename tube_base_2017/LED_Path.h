#ifndef LED_PATH
#define LED_PATH

#include "utils.h"
#include "ColorDefines.h"
#include <Adafruit_NeoPixel_ZeroDMA.h>

// using LedType = Adafruit_NeoPixel_ZeroDMA;
using LedType = Adafruit_NeoPixel;

// #define CURRENT_LED_TYPE (NEO_RGB + NEO_KHZ800)
// #define BYTES_PER_PIXEL 3

#define CURRENT_LED_TYPE (NEO_RGBW + NEO_KHZ800)
#define BYTES_PER_PIXEL 4

#define TUBE_LENGTH 58
#define SEGMENT_LENGTH TUBE_LENGTH // (2 tubes, each 58 px)

class Segment
{
public:
    Segment(uint32_t the_length);
    inline uint32_t length(){return m_length; }
    inline uint32_t color() const { return m_color; }
    inline void set_color(uint32_t the_color){ m_color = the_color; }
    inline void set_active(bool b){ m_active = b; }
    inline bool active() const{ return m_active; }
private:
    uint32_t m_length;
    uint32_t m_color = AQUA;
    bool m_active = true;
};

class LED_Path
{
public:
    LED_Path(){};
    LED_Path(uint32_t the_pin, uint32_t the_num_segments);
    ~LED_Path();

    inline uint32_t num_leds() const{ return num_segments() * SEGMENT_LENGTH; }
    inline uint32_t num_segments() const{ return m_num_segments; };
    inline Segment* segment(uint32_t the_index){ return m_segments[the_index]; };

    void set_all_segments(uint32_t the_color);
    inline float brightness(){ return m_brightness; }
    void set_brightness(float the_brightness);

    void clear();
    void update(uint32_t the_delta_time);

    inline const uint8_t* data() const { return m_data; };

    void set_current_max(uint32_t the_max){ m_current_max = the_max; }
    void set_flash_speed(float the_speed){ m_flash_speed = the_speed; }
    void set_flash_direction(bool b){ m_flash_forward = b; }
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

    uint8_t* m_data = nullptr;
    LedType* m_strip = nullptr;
    uint32_t m_num_segments;
    Segment** m_segments = nullptr;
    float m_brightness = .4f;

    float m_current_max;
    float m_flash_speed = 800.f;
    bool m_flash_forward = true;
};
#endif
