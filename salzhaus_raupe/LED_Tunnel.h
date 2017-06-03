#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

const uint8_t g_num_pins = 1;
const uint8_t g_led_pins[] = {6};

class Gate;
class Tunnel;

const uint8_t g_gamma[256] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

// Color defines (BRGW)
static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
PURPLE = Adafruit_NeoPixel::Color(150, 235, 0, 20),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
BLACK = 0;

class Gate
{
public:
    enum Sequment{LEFT, TOP, RIGHT, ALL};
    enum Direction{NORMAL, REVERSE};

    Gate();
    Gate(uint8_t *data_start, uint16_t num_left, uint16_t num_top, uint16_t num_right,
         Direction the_dir);

    //! WRGB byte order
    void set_pixel(uint32_t the_index, uint32_t the_color);
    void set_all_pixels(uint32_t the_color);

    const uint8_t* data() const { return m_data; };

    const uint16_t num_leds() const { return m_num_leds; }

private:
    uint8_t *m_data = nullptr;
    uint16_t m_num_leds = 0;
    uint16_t m_seq_length[3] = {5, 5, 5};
    Direction m_direction = NORMAL;
};

class Tunnel
{
public:

    Tunnel();

    void init();
    uint8_t brightness() const;
    void set_brightness(uint8_t the_brightness);
    void clear();
    void add_random_pixels(uint16_t the_count, uint32_t the_delay_millis);

    Gate* gates(){ return m_gates; }
    const uint16_t num_gates() const { return m_num_gates; }
    void update(uint32_t the_delta_time);

private:

    const uint16_t m_num_gates = 16;
    Gate m_gates[16];
    Adafruit_NeoPixel* m_strips[1];

    // timestamps for every pixel, needed for random blinky
    unsigned long *m_pixel_time_buf = nullptr;
    uint32_t m_num_leds = 0;
};
