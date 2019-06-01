#include <I2S.h>
#include "ModeHelpers.h"
#include "Timer.hpp"
#include "LED_Path.h"
#include "ModeHelpers.h"

// create an amplitude analyzer to be used with the I2S input
// AmplitudeAnalyzer g_amplitude_analyzer;

#define ARM_MATH_CM0PLUS
#include <arm_math.h>

// update rate in Hz
#define UPDATE_RATE 25
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

// sampling configuration
using sample_t = int32_t;
constexpr uint32_t g_bits_per_sample = 16;
constexpr uint32_t g_sample_rate = 22050;
sample_t g_variance = 0, g_mean = 0, g_peak_to_peak = 0;

// peak calculations
sample_t g_amplitude_min = 1, g_current_amplitude_max = 0, g_amplitude = 0;
sample_t g_decay_per_msec = 4000;

float g_mic_lvl = 0.f;

uint8_t g_sample_buffer[512];

void blink_status_led()
{
    digitalWrite(13, LOW);
    delay(1000);
    digitalWrite(13, HIGH);
    delay(1000);
}

inline int32_t calculate_amplitude(uint8_t* buffer, size_t num_bytes)
{
    sample_t analysis = 0;
    if(g_bits_per_sample == 16){ arm_rms_q15((q15_t*)buffer, num_bytes / 2, (q15_t*)&analysis); }
    else if(g_bits_per_sample == 32){ arm_rms_q31((q31_t*)buffer, num_bytes / 4, &analysis); }
    return analysis;
}

void on_i2s_receive()
{
    int num = I2S.read(g_sample_buffer, sizeof(g_sample_buffer));

    if(num)
    {
        g_last_mic_reading = millis();
        g_indicator = false;

        // amplitude calculation
        g_amplitude = calculate_amplitude(g_sample_buffer, num);

        // mean
        if(g_bits_per_sample == 16){ arm_mean_q15((q15_t*)g_sample_buffer, num / 2, (q15_t*)&g_mean); }
        else if(g_bits_per_sample == 32){ arm_mean_q31((q31_t*)g_sample_buffer, num / 4, &g_mean); }

        // variance
        // sample_t variance;
        // if(g_bits_per_sample == 16){ arm_var_q15((q15_t*)g_sample_buffer, num / 2, (q15_t*)&g_variance); }
        // else if(g_bits_per_sample == 32){ arm_var_q31((q31_t*)g_sample_buffer, num / 4, &g_variance); }

        // iterate samples
        // sample_t *ptr = (sample_t*)g_sample_buffer, *end = (sample_t*)(g_sample_buffer + num);
        // for(; ptr < end; ++ptr){ ptr -= g_mean; }

        // peak to peak
        uint32_t index;
        sample_t min, max;
        arm_min_q31((q31_t*)g_sample_buffer, num / 4, &min, &index);
        arm_max_q31((q31_t*)g_sample_buffer, num / 4, &max, &index);
        g_peak_to_peak = abs(max - min);

        int16_t* ptr = (int16_t*)g_sample_buffer, *end = (int16_t*)(g_sample_buffer + sizeof(g_sample_buffer));
        for(; ptr < end; ++ptr){ Serial.println(*ptr); }
    }
}

bool init_audio()
{
    bool ret = I2S.begin(I2S_PHILIPS_MODE, g_sample_rate, g_bits_per_sample);

    // add the receiver callback
    I2S.onReceive(on_i2s_receive);

    // trigger a read to kick things off
    I2S.read();

    g_last_mic_reading = millis();
    return ret;
}

void process_mic_input(uint32_t the_delta_time)
{
    // lvl decay
    const float decay_secs = .6f;
    float decay = the_delta_time / 1000.f / decay_secs;
    g_mic_lvl = max(0, g_mic_lvl - decay);

    // peak decay
    g_current_amplitude_max -= g_current_amplitude_max * the_delta_time / 1000.f / 10.f;
    g_current_amplitude_max = max(g_peak_to_peak, g_current_amplitude_max);
    g_current_amplitude_max = max(g_current_amplitude_max, 6710);

    // read mic val
    float v = map(g_peak_to_peak, 6700, g_current_amplitude_max, 0, 1000);
    g_mic_lvl = clamp(max(g_mic_lvl, v / 1000.f), 0.f, 1.f);

    // Run FFT on sample data.
    // arm_cfft_radix4_instance_f32 fft_inst;
}

void setup()
{
    srand(analogRead(A7));

    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    while(!Serial){ delay(10); }
    Serial.begin(115200);

    while(!init_audio()){ blink_status_led(); }

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
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // poll Timer objects
    for(uint32_t i = 0; i < g_num_timers; ++i){ g_timer[i].poll(); }

    // reinit audio if necessary
    if(g_last_time_stamp - g_last_mic_reading > 100)
    {
        I2S.end();
        while(!init_audio()){ blink_status_led(); }
    }

    process_mic_input(delta_time);
    // Serial.println(g_peak_to_peak);
    // Serial.println(g_mic_lvl);
    // Serial.println(g_amplitude);

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        digitalWrite(13, g_indicator);

        // for(uint8_t i = 0; i < g_num_paths; ++i)
        // {
        //     uint32_t vol_index = g_path[i]->num_leds() * g_mic_lvl;
        //     g_path[i]->set_current_max(vol_index);
        //     g_mode_current->process(g_path[i], g_time_accum);
        //     g_path[i]->update(g_time_accum);
        // }
        // clear time accumulator
        g_time_accum = 0;

        // reset status led
        g_indicator = true;
    }
    // else{ delay(g_update_interval - g_time_accum); }
    else{ delay(2); }
}
