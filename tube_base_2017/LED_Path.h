#ifndef LED_PATH
#define LED_PATH

#include <Adafruit_NeoPixel.h>

#define SEGMENT_LENGTH 116 // (2 tubes, each 58 px)

class Segment
{
public:
    Segment(uint32_t the_length);
    inline uint32_t length(){return m_length; }
    inline uint32_t color(){ return m_color; }
    inline void set_color(uint32_t the_color){ m_color = the_color; }
private:
    uint32_t m_length;
    uint32_t m_color = 0;
};

class LED_Path
{
public:
    LED_Path(uint32_t the_pin, uint32_t the_num_segments);
    ~LED_Path();

    uint32_t num_leds() const;
    inline uint32_t num_segments() const{ return m_num_segments; };
    inline Segment* segment(uint32_t the_index){ return m_segments[the_index]; };

    inline float brightness(){ return m_brightness; }
    void set_brightness(float the_brightness);

    void clear();
    void update(uint32_t the_delta_time);

    inline const uint8_t* data() const { return m_data; };

private:
    uint8_t* m_data;
    Adafruit_NeoPixel* m_strip;
    uint32_t m_num_segments;
    Segment** m_segments;
    float m_brightness = 1.f;

    float m_current_max;
    float m_grow_speed;
};
#endif
