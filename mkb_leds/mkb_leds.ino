/*
* drive WS2812 RGBW LED stripes
* receive color values via Serial
* author: Fabian - crocdialer@googlemail.com
*/

#include <Adafruit_NeoPixel.h>

#define CMD_QUERY_ID "ID"
#define CMD_START "START"
#define CMD_STOP "STOP"
#define CMD_RESET "RESET"
#define DEVICE_ID "LED_CONTROL"

#define SERIAL_END_CODE '\n'
#define SERIAL_BUFSIZE 512
char g_serial_buf[SERIAL_BUFSIZE];

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_update_interval = 10;
bool g_indicator = false;

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
PURPLE = Adafruit_NeoPixel::Color(150, 235, 0, 20),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
BLACK = 0;

const uint8_t g_num_stripes = 2;
const uint8_t g_led_pins[g_num_stripes] = {6, 10};
const uint8_t g_led_counts[g_num_stripes] = {182, 90};
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

void blink_status_led()
{
    digitalWrite(13, LOW);
    delay(500);
    digitalWrite(13, HIGH);
    delay(500);
}

void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);

    // initialize the serial communication:
    while(!Serial){ blink_status_led(); }
    Serial.begin(57600);
    Serial.println(DEVICE_ID);

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

        // for(int i = 0; i < g_num_stripes; i++)
        {
            for(int p = 0; p < g_stripes[0]->numPixels(); ++p)
            {
                g_stripes[0]->setPixelColor(p, g_current_color);
            }
            g_stripes[0]->show();

            for(int p = 0; p < g_stripes[1]->numPixels(); ++p)
            {
                g_stripes[1]->setPixelColor(p, WHITE);
            }
            g_stripes[1]->setBrightness( 175 + 80 * sin(0.0005f * g_last_time_stamp));
            g_stripes[1]->show();
        }
    }
}

template <typename T> void process_serial_input(T& the_serial)
{
    uint32_t bytes_to_read = min(the_serial.available(), SERIAL_BUFSIZE);
    the_serial.readBytes(g_serial_buf, bytes_to_read);
    char* buf_ptr = g_serial_buf;

    for(uint32_t i = 0; i < bytes_to_read; i++)
    {
        switch(g_serial_buf[i])
        {
            case '\n':
                g_serial_buf[i] = '\0';
                parse_line(buf_ptr);
                buf_ptr = g_serial_buf + i + 1;
                break;

            default:
            // case '\r':
            // case '\0':
                break;
        }
    }
}

bool check_for_cmd(const char* the_str)
{
    if(strcmp(the_str, CMD_QUERY_ID) == 0)
    {
        char buf[32];
        sprintf(buf, "%s %s\n", the_str, DEVICE_ID);
        Serial.write(buf);
        return true;
    }
    return false;
}

void parse_line(char *the_line)
{
    const char* delim = " ";

    // we expect 4 ints
    const size_t elem_count = 4;
    int parsed_ints[elem_count];

    char *token = strtok(the_line, delim);
    uint32_t num_tokens = 0;

    for(; token && (num_tokens < elem_count);)
    {
        if(check_for_cmd(token)){}
        else{ parsed_ints[num_tokens] = atoi(token); ++num_tokens; }

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

        default:
            break;
    }
}
