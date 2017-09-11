#include "RunningMedian.h"
#include "utils.h"
#include "ADC_Sampler.h"
#include "LED_Path.h"

#define ARM_MATH_CM0
#include "arm_math.h"

// update rate in Hz
#define UPDATE_RATE 60

#define ADC_BITS 10

// some pin defines
#define POTI_PIN A0
#define MIC_PIN A1
#define LED_PIN 6
#define BATTERY_PIN A7

#define SERIAL_BUFSIZE 128

constexpr float ADC_MAX = (1 << ADC_BITS) - 1.f;

char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

// mic sampling
const uint32_t g_mic_sample_window = 100;
volatile uint32_t g_mic_signal_max = 0;
volatile uint32_t g_mic_signal_min = ADC_MAX;

// start of sample window
uint32_t g_mic_start_millis = 0;

// peak-to-peak level
uint32_t g_mic_peak_to_peak = 0;

// current mic-level in range: [0, 1]
float g_mic_lvl = 0.f;

float g_gain = 3.f;

// lvl decay per sec
float g_mic_decay = 2.f;

// continuous sampling with timer interrupts and custom ADC settings
ADC_Sampler g_adc_sampler;

// Run FFT on sample data.
arm_cfft_radix4_instance_f32 fft_inst;

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;

// update interval in millis
const int g_update_interval = 1000 / UPDATE_RATE;

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

// LEDs
LED_Path g_path(LED_PIN, 8);

// Poti
constexpr uint8_t g_num_poti_samples = 3;
RunningMedian g_pot_vals(g_num_poti_samples);

float battery_lvl()
{
    constexpr uint32_t measure_interval = 10000;
    static uint32_t last_measure = measure_interval;
    static float ret = 0.f;
    uint32_t delta_time = millis() - last_measure;
    
    if(delta_time >= measure_interval)
    {
        ret = analogRead(BATTERY_PIN);
        last_measure = millis();
        // voltage is divided by 2, so multiply back
        ret *= 2 * 3.3f / ADC_MAX;
        ret = map_value<float>(ret, 3.3f, 4.2f, 0.f, 1.f);
        // Serial.print("VBat: " );
        // Serial.println(measuredvbat);
    }
    return ret;
}

//! value callback from ADC_Sampler ISR
void adc_callback(uint32_t the_sample)
{
    if(the_sample <= ADC_MAX)
    {
        g_mic_signal_min = min(g_mic_signal_min, the_sample);
        g_mic_signal_max = max(g_mic_signal_max, the_sample);
    }
}

void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    pinMode(MIC_PIN, INPUT);

    // set ADC resolution (default 10 bits, maximum 12 bits)
    analogReadResolution(ADC_BITS);

    // while(!Serial){ delay(10); }
    Serial.begin(115200);

    // start mic sampling
    g_adc_sampler.set_adc_callback(&adc_callback);
    // g_adc_sampler.begin(MIC_PIN, 22050);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    float bat_lvl = 0.f;

    adc_callback(analogRead(MIC_PIN));

    // scope is interrupt free
    // {
    //     no_interrupt ni;
    //
    //     // update poti-lvl
    //     for(int i = 0; i < g_num_poti_samples; ++i)
    //     {
    //         g_pot_vals.add(analogRead(POTI_PIN));
    //     }
    //
    //     bat_lvl = battery_lvl();
    // }
    // float current_pot = map_value<float>(g_pot_vals.getMedian(), 133, 858, 0.f, 1.f);
    // Serial.println((int)g_pot_vals.getMedian());

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        digitalWrite(13, g_indicator);
        g_indicator = !g_indicator;

        // read debug inputs
        process_serial_input();

        // update current microphone value
        process_mic_input(g_time_accum);

        // logic goes here
        g_path.set_all_segments(BLACK);
        int segs_bat = battery_lvl() * g_path.num_segments();
        int segs_mic = g_mic_lvl * g_path.num_segments();

        for(int i = 0; i < g_path.num_segments(); ++i)
        {
            if(i < segs_bat){ g_path.segment(i)->set_color(ORANGE); }
            if(i < segs_mic){ g_path.segment(i)->set_color(AQUA); }
        }
        g_path.update(g_time_accum);

        // clear time accumulator
        g_time_accum = 0;
    }
}

void process_mic_input(uint32_t the_delta_time)
{
    // decay
    float decay = g_mic_decay * the_delta_time / 1000.f;
    g_mic_lvl = max(0, g_mic_lvl - decay);

    if(millis() > g_mic_start_millis + g_mic_sample_window)
    {
        constexpr uint32_t min_thresh = 5;

        // max - min = peak-peak amplitude
        g_mic_peak_to_peak = max(0, g_mic_signal_max - g_mic_signal_min);
        g_mic_peak_to_peak = g_mic_peak_to_peak > min_thresh ? g_mic_peak_to_peak : 0;

        g_mic_signal_max = 0;
        g_mic_signal_min = ADC_MAX;
        g_mic_start_millis = millis();

        // read mic val
        float v = clamp<float>(g_gain * g_mic_peak_to_peak / 31.f, 0.f, 1.f);

        g_mic_lvl = max(g_mic_lvl, v);
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
        }
    }
}
