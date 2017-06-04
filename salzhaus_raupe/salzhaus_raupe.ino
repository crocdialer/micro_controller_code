#include "utils.h"
#include "LED_Tunnel.h"
#include "WaveSimulation.h"

#define ADC_BITS 10

// update rate in Hz
#define UPDATE_RATE 60

// some pin defines
#define BUTTON_PIN 11
#define BUTTON_LED 12

#define SERIAL_BUFSIZE 128

const float ADC_MAX = (1 << ADC_BITS) - 1.f;

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

// tunnel variables
Tunnel g_tunnel;
uint32_t g_current_index = 0;

// lightbarrier handling (changed in ISR, must be volatile)
volatile bool g_button_pressed = false;
volatile long g_button_timestamp = 0;

const int g_charge_millis = 3000;
float g_charge = 0.f;

//! define our run-modes here
enum RunMode
{
    MODE_SPARKLE = 1 << 0,
    MODE_WAVES = 1 << 1,
    MODE_NEBULA = 1 << 2,
    MODE_DEBUG = 1 << 3
};
uint32_t g_run_mode = MODE_SPARKLE | MODE_WAVES;

// our wave simulation object
WaveSimulation g_wave_sim;

// disabled when set to 0
int32_t g_random_wave_timer = 1;
volatile bool g_emit_wave = false;

const uint32_t g_idle_timeout = 15000;

#define SPAWN_RATE_LOW 0.08f
#define SPAWN_RATE_HIGH 1.f
float g_spawn_multiplier = SPAWN_RATE_LOW;
float g_spawn_accum = 0.f;

uint32_t g_blink_interval = 1000;

//! interrupt routine for lightbarrier status
void button_ISR()
{
     bool is_pressed = !digitalRead(BUTTON_PIN);

     // button pressed
     if(!g_button_pressed && is_pressed)
     {
         g_button_pressed = true;
         g_button_timestamp = millis();
         g_emit_wave = false;
     }
     // button released
     else if(g_button_pressed && !is_pressed)
     {
         g_button_pressed = false;
         g_emit_wave = true;
     }
}

void update_sparkling(uint32_t the_delta_time)
{
    if(g_button_pressed)
    {
        g_spawn_multiplier = mix<float>(SPAWN_RATE_LOW, SPAWN_RATE_HIGH,
                                        g_charge);
    }
    else{ g_spawn_multiplier = SPAWN_RATE_LOW; }

    g_spawn_accum += g_spawn_multiplier * 75/*per sec*/ * the_delta_time / 1000.f;
    g_tunnel.add_random_pixels(g_spawn_accum, 600);
    g_spawn_accum -= (int)g_spawn_accum;

    // Serial.print("num_random_pix: "); Serial.println(num_random_pix);
}

void update_waves(uint32_t the_delta_time)
{
    // update wave simulation
    g_wave_sim.update(the_delta_time);

    g_random_wave_timer -= g_time_accum;

    if(g_emit_wave)
    {
         g_emit_wave = false;
         g_wave_sim.emit_wave(mix(0.2f, 1.f, g_charge));
         g_charge = 0.f;
    }

    // idle timeout and wave timer elapsed
    if(millis() - g_button_timestamp > g_idle_timeout &&
       g_random_wave_timer < 0)
    {
        // emit random wave
        g_wave_sim.emit_wave(random<float>(0.5f, 1.f));

        // schedule next wave
        g_random_wave_timer = random<int32_t>(5000, 15000);
    }

    // g_tunnel.set_brightness(20 + 50 * g_mic_lvl);
    auto col = Adafruit_NeoPixel::Color(150, 50, 0, g_gamma[40]);

    // start bias for 1st gate in meters
    float pos_x = 0.3;

    // distance between two gates in meters
    const float step = 0.88f;

    for(int i = 0; i < g_tunnel.num_gates(); i++)
    {
        auto fade_col = fade_color(col, g_wave_sim.intensity_at_position(pos_x));
        g_tunnel.gates()[i].set_all_pixels(fade_col);
        pos_x += step;
    }
}

void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // interrupt from button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // digitalWrite(BUTTON_PIN, HIGH);
    // attachInterrupt(BUTTON_PIN, button_ISR, CHANGE);

    // Button LED
    pinMode(BUTTON_LED, OUTPUT);

    // set ADC resolution (default 10 bits, maximum 12 bits)
    // analogReadResolution(ADC_BITS);

    // while(!Serial){ delay(10); }
    Serial.begin(115200);

    g_tunnel.init();

    g_wave_sim.set_track_length(10.f);
    g_wave_sim.set_propagation_speed(3.33);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // button state
    button_ISR();

    bool light_led;

    // button charge status and LED
    if(g_button_pressed)
    {
        g_charge = clamp((g_last_time_stamp - g_button_timestamp) / (float) g_charge_millis, 0.f, 1.f);
        light_led = (g_charge == 1.f) ? true : (g_last_time_stamp / (g_blink_interval / 8)) % 2;
    }
    else{ light_led = (g_last_time_stamp / g_blink_interval) % 2; }
    digitalWrite(BUTTON_LED, light_led);

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
        g_tunnel.clear();

        // run stages depending on current mode
        if(g_run_mode & MODE_WAVES){ update_waves(g_time_accum); }
        if(g_run_mode & MODE_SPARKLE){ update_sparkling(g_time_accum); }

        // send new color values to strips
        g_tunnel.update(delta_time);

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
            // memcpy(&col, g_serial_buf, 3);
            int index = atoi((const char*)g_serial_buf);
            g_buf_index = 0;
            memset(g_serial_buf, 0, SERIAL_BUFSIZE);

            Serial.println(index);

            if(index >= 0 && index < g_tunnel.num_gates())
            {
                g_run_mode = MODE_DEBUG;
                g_tunnel.clear();
                g_tunnel.gates()[index].set_all_pixels(ORANGE);
                g_tunnel.update(0);
            }
            else{ g_run_mode = MODE_SPARKLE | MODE_WAVES; }
        }
    }
}
