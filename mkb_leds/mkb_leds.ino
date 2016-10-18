/*
* drive WS2812 RGBW LED stripes
* receive color values via Serial
* author: Fabian - crocdialer@googlemail.com
*/

#include <Adafruit_NeoPixel.h>

#define QUERY_ID_CMD "ID"
#define DEVICE_ID "LEDS"

#define SERIAL_END_CODE '\n'
#define SERIAL_BUFSIZE 512
char g_serial_buf[SERIAL_BUFSIZE];

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_update_interval = 33;
bool g_indicator = false;

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
PURPLE = Adafruit_NeoPixel::Color(150, 235, 0, 20),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
BLACK = 0;

const uint8_t g_num_stripes = 1;
const uint8_t g_led_pins[g_num_stripes] = {12};
const uint8_t g_led_counts[g_num_stripes] = {64};
Adafruit_NeoPixel* g_stripes[g_num_stripes];
uint32_t g_current_color = BLACK;

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

void setup()
{
    // initialize the serial communication:
    Serial.begin(57600);
    memset(g_serial_buf, 0, SERIAL_BUFSIZE);

    for(int i = 0; i < g_num_stripes; i++)
    {
        g_stripes[i] = new Adafruit_NeoPixel(g_led_counts[i], g_led_pins[i], NEO_GRBW + NEO_KHZ800);
        g_stripes[i]->begin();
        g_stripes[i]->show();
    }
}

////////////////////////////////////////////////////////////////////////

void loop()
{
    // time measurement
    int delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    if(g_time_accum >= g_update_interval)
    {
        // check for input
        process_serial_input(Serial);

        for(int i = 0; i < g_num_stripes; i++)
        {
            for(int p = 0; p < g_stripes[i]->numPixels(); ++p)
            {
                g_stripes[i]->setPixelColor(p, g_current_color);
            }
            g_stripes[i]->show();
        }
    }
}

template <typename T> void process_serial_input(T& the_serial)
{
    uint16_t buf_idx = 0;

    while(the_serial.available())
    {
        // get the new byte:
        char c = the_serial.read();

        switch(c)
        {
            case '\r':
            case '\0':
                continue;

            case '\n':
                g_serial_buf[buf_idx] = '\0';
                buf_idx = 0;
                parse_line(g_serial_buf);
                break;

            default:
                g_serial_buf[buf_idx++] = c;
                break;
        }
    }
}

void parse_line(char *the_line)
{
    const char* delim = " ";

    // we expect 4 ints
    const size_t elem_count = 4;
    int parsed_ints[elem_count];

    char *token = strtok(the_line, delim);
    uint32_t num_tokens = 0;

    for(; token && (num_tokens < elem_count); ++num_tokens)
    {
        if(strcmp(token, QUERY_ID_CMD) == 0)
        {
            char buf[32];
            sprintf(buf, "%s\n", DEVICE_ID);
            Serial.write(buf);
        }
        else{ parsed_ints[num_tokens] = atoi(token); }

        // fetch next token
        token = strtok(nullptr, delim);
    }

    switch(num_tokens)
    {
        case 1:
            g_current_color = Adafruit_NeoPixel::Color(0, 0, 0, g_gamma[parsed_ints[0]]);
            break;
        case 3:
            g_current_color = Adafruit_NeoPixel::Color(parsed_ints[0], parsed_ints[1],
                                                       parsed_ints[2], 0);
            break;
        case 4:
            g_current_color = Adafruit_NeoPixel::Color(parsed_ints[0], parsed_ints[1],
                                                       parsed_ints[2], g_gamma[parsed_ints[3]]);
            break;
            
        case 0:
        case 2:
        default:
            break;
    }
}
