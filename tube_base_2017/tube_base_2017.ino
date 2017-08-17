#include "LED_Path.h"

// update rate in Hz
#define UPDATE_RATE 60

#define PATH_LENGTH 8

// some pin defines
#define LED_PIN 6

#define SERIAL_BUFSIZE 128

static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 40),
BLACK = 0;

char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;

// update interval in millis
const int g_update_interval = 1000 / UPDATE_RATE;

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

// path variables
LED_Path g_path = LED_Path(LED_PIN, PATH_LENGTH);
uint32_t g_current_index = 0;

//! define our run-modes here
enum RunMode
{
    MODE_DEBUG = 1 << 0,
    MODE_ONE_COLOR = 1 << 1,
};
uint32_t g_run_mode = MODE_ONE_COLOR;

void process_mode(uint32_t the_run_mode)
{
    switch (the_run_mode)
    {
        case MODE_ONE_COLOR:

        break;

        default:
            break;
    }

}
void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // while(!Serial){ delay(10); }
    Serial.begin(115200);

}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        digitalWrite(13, g_indicator);
        g_indicator = !g_indicator;

        // read debug inputs
        process_serial_input();

        // do nothing here while debugging
        if(g_run_mode & MODE_DEBUG){ return; }

        // clear everything to black
        g_path.clear();

        // do mode stuff here
        process_mode(g_run_mode);

        g_path.update(g_time_accum);

        // clear time accumulator
        g_time_accum = 0;
    }
}

void process_serial_input()
{
    while(Serial.available())
    {
        // get the new byte:
        uint8_t c = Serial.read();
        if(c == '\0'){ continue; }

        // add it to the buf
        g_serial_buf[g_buf_index % SERIAL_BUFSIZE] = c;
        g_buf_index = (g_buf_index + 1) % SERIAL_BUFSIZE;

        // if the incoming character is a newline, set a flag
        if (c == '\n')
        {
            int index = atoi((const char*)g_serial_buf);
            g_buf_index = 0;
            memset(g_serial_buf, 0, SERIAL_BUFSIZE);

            Serial.println(index);

            if(index >= 0 && index < g_path.num_segments())
            {
                g_run_mode = MODE_DEBUG;
                g_path.clear();
                g_path.segment(index)->set_color(ORANGE);
                g_path.update(0);
            }
            else{ g_run_mode = MODE_ONE_COLOR; }
        }
    }
}
