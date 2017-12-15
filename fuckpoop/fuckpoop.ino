#include <ArduinoSound.h>
#include "ModeHelpers.h"
#include "Timer.hpp"
#include "LED_Path.h"
#include "ModeHelpers.h"

// create an amplitude analyzer to be used with the I2S input
AmplitudeAnalyzer g_amplitude_analyzer;

// update rate in Hz
#define UPDATE_RATE 30
#define SERIAL_BUFSIZE 128

char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

// helper variables for time measurement
long g_last_time_stamp = 0, g_last_mic_reading = 0;
uint32_t g_time_accum = 0;

// update interval in millis
const int g_update_interval = 1000 / UPDATE_RATE;

// an array of Timer objects is provided
constexpr uint32_t g_num_timers = 1;
enum TimerEnum
{
    TIMER_PLACE_HOLDER = 1
};
kinski::Timer g_timer[g_num_timers];

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

//! define our run-modes here
enum RunMode
{
    MODE_DEBUG = 1 << 0,
    MODE_RUNNING = 1 << 1
};
uint32_t g_run_mode = MODE_RUNNING;

constexpr uint8_t g_num_paths = 1;
constexpr uint8_t g_path_length = 2;
const uint8_t g_led_pins[] = {5};

LED_Path* g_path[g_num_paths];

ModeHelper *g_mode_sinus = nullptr, *g_mode_colour = nullptr, *g_mode_current = nullptr;
CompositeMode *g_mode_composite = nullptr;

void blink_status_led()
{
    digitalWrite(13, LOW);
    delay(500);
    digitalWrite(13, HIGH);
    delay(500);
}

bool init_audio()
{
    g_last_mic_reading = 0;
    
     // setup the I2S audio input for 44.1 kHz with 32-bits per sample
    return AudioInI2S.begin(16000, 32) && g_amplitude_analyzer.input(AudioInI2S);
}

void setup()
{
    srand(analogRead(A7));

    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // while(!Serial){ delay(10); }
    Serial.begin(115200);

    // init path objects with pin array
    for(uint8_t i = 0; i < g_num_paths; ++i)
    {
         g_path[i] = new LED_Path(g_led_pins[i], g_path_length, 8);
    }

    // create ModeHelper objects
    g_mode_colour = new Mode_ONE_COLOR();
    g_mode_sinus = new SinusFill();
    g_mode_current = g_mode_composite = new CompositeMode();
    g_mode_composite->add_mode(g_mode_colour);
    g_mode_composite->add_mode(g_mode_sinus);

    while(!init_audio()){ blink_status_led(); }
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // poll Timer objects
    for(uint32_t i = 0; i < g_num_timers; ++i){ g_timer[i].poll(); }

    int amplitude = 0;

    if(g_amplitude_analyzer.available())
    {
        // read the new amplitude
        amplitude = g_amplitude_analyzer.read();

        g_indicator = !g_indicator;

        // print out the amplititude to the serial monitor
        if(amplitude){ Serial.println(amplitude); }

        g_last_mic_reading = g_last_time_stamp;
    }
    else if(g_last_time_stamp - g_last_mic_reading > 100)
    {
        while(!init_audio()){ blink_status_led(); }
    }

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        digitalWrite(13, g_indicator);
        // g_indicator = !g_indicator;

        for(uint8_t i = 0; i < g_num_paths; ++i)
        {
            // g_path[i]->set_current_max(i / 8.f);
            g_path[i]->update(g_time_accum);
            g_mode_current->process(g_path[i], g_time_accum);
        }

        // clear time accumulator
        g_time_accum = 0;
    }


}
