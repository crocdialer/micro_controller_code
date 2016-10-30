#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_LSM9DS0.h>
#include <Adafruit_Sensor.h>  // not used in this demo but required!
#include <RunningMedian.h>

#define USE_BLUETOOTH

// bluetooth communication
#ifdef USE_BLUETOOTH
    #include "Adafruit_BluefruitLE_SPI.h"
    #define BLUEFRUIT_SPI_CS 8
    #define BLUEFRUIT_SPI_IRQ 7
    #define BLUEFRUIT_SPI_RST 4    // Optional but recommended, set to -1 if unused
    Adafruit_BluefruitLE_SPI g_bt_serial(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

    const int g_update_interval_bt = 100;
    int g_time_accum_bt = 0;
#else

#endif
#include "utils.h"
#include "ADC_Sampler.h"

#define LED_PIN 12
#define NUM_LEDS 64
#define BRIGHTNESS 50
#define BAT_PIN A7
#define MIC_PIN A1
#define POTI_PIN A2
#define BARRIER_INTERRUPT_PIN 6

#define ADC_BITS 10
const float ADC_MAX = (1 << ADC_BITS) - 1.f;

// #define ARM_MATH_CM0
// #include <arm_math.h>} // this header appears to have screwed braces!?

const int g_update_interval = 20;

char g_serial_buf[512];

// mic sampling
uint32_t g_mic_sample_window = 10;
uint32_t g_mic_start_millis = 0;  // start of sample window
volatile uint16_t g_mic_peak_to_peak = 0;   // peak-to-peak level
volatile uint16_t g_mic_signal_max = 0;
volatile uint16_t g_mic_signal_min = ADC_MAX;
float g_mic_lvl = 0.f; // 0.0 ... 1.0

ADC_Sampler g_adc_sampler;

//! time management
const uint32_t g_update_interval_params = 2000;
uint32_t g_time_accum = 0, g_time_accum_params = 0;
long g_last_time_stamp = 0;

bool g_bt_initialized = false;

// i2c
Adafruit_LSM9DS0 g_lsm = Adafruit_LSM9DS0();

// You can also use software SPI
//Adafruit_LSM9DS0 lsm = Adafruit_LSM9DS0(13, 12, 11, 10, 9);
// Or hardware SPI! In this case, only CS pins are passed in
//Adafruit_LSM9DS0 lsm = Adafruit_LSM9DS0(10, 9);

const uint16_t g_num_samples = 3;
const uint16_t g_sense_interval = 0;
RunningMedian g_running_median = RunningMedian(g_num_samples);

enum SharpIR_Model{ GP2Y0A21Y, GP2Y0A02YK, GP2Y0A710K0F };
uint32_t convert_distance(uint32_t the_measurement, SharpIR_Model the_model = GP2Y0A02YK);

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
PURPLE = Adafruit_NeoPixel::Color(150, 235, 0, 0),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, g_gamma[40]),
BLACK = 0;

uint32_t *g_pixels = nullptr;

static const uint8_t r_offset = 1, g_offset = 0, b_offset = 2, w_offset = 3;

/*! smoothstep performs smooth Hermite interpolation between 0 and 1,
 *  when edge0 < x < edge1.
 *  This is useful in cases where a threshold function with a smooth transition is desired
 */
inline float smoothstep(float edge0, float edge1, float x)
{
    float t = clamp<float>((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3.0 - 2.0 * t);
}

void print_color(uint32_t the_color)
{
    uint8_t *ptr = (uint8_t*) &the_color;
    sprintf(g_serial_buf, "R: %d - G: %d - B: %d - W: %d\n", ptr[r_offset], ptr[g_offset],
            ptr[b_offset], ptr[w_offset]);
    Serial.write(g_serial_buf);
}

void print_battery_lvl()
{
    float measuredvbat = analogRead(BAT_PIN);

    // voltage is divided by 2, so multiply back
    measuredvbat *= 2 * 3.3f / ADC_MAX;
    Serial.print("VBat: " );
    Serial.println(measuredvbat);
}

static inline uint32_t colors_mix(uint32_t lhs, uint32_t rhs, float ratio)
{
    uint8_t *ptr_lhs = (uint8_t*) &lhs, *ptr_rhs = (uint8_t*) &rhs;

    return  mix<uint32_t>(ptr_lhs[w_offset], ptr_rhs[w_offset], ratio) << 24 |
            mix<uint32_t>(ptr_lhs[b_offset], ptr_rhs[b_offset], ratio) << 16 |
            mix<uint32_t>(ptr_lhs[r_offset], ptr_rhs[r_offset], ratio) << 8 |
            mix<uint32_t>(ptr_lhs[g_offset], ptr_rhs[g_offset], ratio);
}

static inline uint32_t color_add(uint32_t lhs, uint32_t rhs)
{
    uint8_t *ptr_lhs = (uint8_t*) &lhs, *ptr_rhs = (uint8_t*) &rhs;
    return  min((uint32_t)ptr_lhs[w_offset] + (uint32_t)ptr_rhs[w_offset], 255) << 24 |
            min((uint32_t)ptr_lhs[b_offset] + (uint32_t)ptr_rhs[b_offset], 255) << 16 |
            min((uint32_t)ptr_lhs[r_offset] + (uint32_t)ptr_rhs[r_offset], 255) << 8 |
            min((uint32_t)ptr_lhs[g_offset] + (uint32_t)ptr_rhs[g_offset], 255);
}

static inline uint32_t fade_color(uint32_t the_color, float the_fade_value)
{
    float val = clamp<float>(the_fade_value, 0.f, 1.f);

    uint8_t *ptr = (uint8_t*) &the_color;
    return  (uint32_t)(ptr[w_offset] * val) << 24 |
            (uint32_t)(ptr[b_offset] * val) << 16 |
            (uint32_t)(ptr[r_offset] * val) << 8 |
            (uint32_t)(ptr[g_offset] * val);
}

static inline uint32_t fast_8bit_scale(uint32_t the_color, uint8_t the_scale_value)
{
    if(!the_scale_value){ return 0; }
    uint8_t *ptr = (uint8_t*) &the_color;
    return  ((uint32_t)(ptr[w_offset]) << 8 / the_scale_value) << 24 |
            ((uint32_t)(ptr[b_offset]) << 8 / the_scale_value) << 16 |
            ((uint32_t)(ptr[r_offset]) << 8 / the_scale_value) << 8 |
            ((uint32_t)(ptr[g_offset]) << 8 / the_scale_value);
}

void setup_sensor()
{
  // 1.) Set the accelerometer range
  g_lsm.setupAccel(Adafruit_LSM9DS0::LSM9DS0_ACCELRANGE_2G);
  //lsm.setupAccel(lsm.LSM9DS0_ACCELRANGE_4G);
  //lsm.setupAccel(lsm.LSM9DS0_ACCELRANGE_6G);
  //lsm.setupAccel(lsm.LSM9DS0_ACCELRANGE_8G);
  //lsm.setupAccel(lsm.LSM9DS0_ACCELRANGE_16G);

  // 2.) Set the magnetometer sensitivity
  g_lsm.setupMag(Adafruit_LSM9DS0::LSM9DS0_MAGGAIN_2GAUSS);
  //lsm.setupMag(lsm.LSM9DS0_MAGGAIN_4GAUSS);
  //lsm.setupMag(lsm.LSM9DS0_MAGGAIN_8GAUSS);
  //lsm.setupMag(lsm.LSM9DS0_MAGGAIN_12GAUSS);

  // 3.) Setup the gyroscope
  g_lsm.setupGyro(Adafruit_LSM9DS0::LSM9DS0_GYROSCALE_245DPS);
  //lsm.setupGyro(lsm.LSM9DS0_GYROSCALE_500DPS);
  //lsm.setupGyro(lsm.LSM9DS0_GYROSCALE_2000DPS);
}

Adafruit_NeoPixel* g_strip = new Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

bool has_uart()
{
    #ifdef USE_BLUETOOTH
        // bluetooth config
        if(g_bt_initialized || g_bt_serial.begin(false))
        {
            g_bt_serial.echo(false);
            return true;
        }
    #endif
    return Serial;
}

void blink_status_led()
{
    digitalWrite(13, LOW);
    delay(500);
    digitalWrite(13, HIGH);
    delay(500);
}

// lightbarrier handling
volatile bool g_barrier_lock = false;
volatile uint32_t g_barrier_timestamp = 0;
volatile uint32_t g_tick_count = 0;
uint32_t g_current_sample_rate = 0;

void process_mic_input(uint32_t the_delta_time);

class no_interrupt
{
public:
    no_interrupt(){ noInterrupts(); }
    ~no_interrupt(){ interrupts(); }
};

void barrier_ISR()
{
     g_barrier_lock = digitalRead(BARRIER_INTERRUPT_PIN);
     if(g_barrier_lock){ g_barrier_timestamp = millis(); }
}

void adc_callback(uint32_t the_sample)
{
    if(the_sample <= ADC_MAX)
    {
        g_mic_signal_min = min(g_mic_signal_min, the_sample);
        g_mic_signal_max = max(g_mic_signal_max, the_sample);
    }
    g_tick_count++;
}

void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);
    pinMode(BARRIER_INTERRUPT_PIN, INPUT);
    digitalWrite(BARRIER_INTERRUPT_PIN, 0);
    attachInterrupt(BARRIER_INTERRUPT_PIN, barrier_ISR, CHANGE);

    // indicate "not ready"
    digitalWrite(13, HIGH);

    // g_strip->setBrightness(BRIGHTNESS);
    g_strip->begin();
    g_strip->show(); // Initialize all pixels to 'off'
    g_pixels = (uint32_t*) g_strip->getPixels();

    // Try to initialise and warn if we couldn't detect the chip
    // if(!g_lsm.begin())
    // {
    //     while(1){ blink_status_led(); };
    //     setup_sensor();
    // }

    // while(!has_uart()){ blink_status_led(); }
    Serial.begin(57600);

    // set ADC resolution (up to 12 bits (0 - ADC_MAX))
    analogReadResolution(ADC_BITS);
    digitalWrite(13, LOW);

    g_adc_sampler.set_adc_callback(&adc_callback);
    g_adc_sampler.begin(MIC_PIN, 22050);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;
    g_time_accum_params += delta_time;

    #ifdef USE_BLUETOOTH
    g_time_accum_bt += delta_time;
    #endif

    // sensors_event_t accel, mag, gyro, temp;
    // g_lsm.getEvent(&accel, &mag, &gyro, &temp);

    process_mic_input(delta_time);

    // g_mic_lvl = g_barrier_lock ? 1.f : 0.f;

    if(g_time_accum >= g_update_interval)
    {
        g_time_accum = 0;

        float gain = 12.f;
        uint32_t pot_val = 0;
        {
            // scope is interrupt free
            no_interrupt ni;
            pot_val = adc_read(POTI_PIN);
            gain = map_value<float>(pot_val, 145, 885, 0.f, 12.f);
        }
        float val = smoothstep(0.f, 1.f, clamp<float>(g_mic_lvl * gain, 0.f, 1.f));
        int num_leds = val * NUM_LEDS;

        //WBRG
        // uint32_t color = 0 << 24 | 0 << 16 | 0 << 8 | 255;
        uint32_t color = color_add(PURPLE, ORANGE);
        color = fade_color(ORANGE, val);
        // print_battery_lvl();

        for(int i = 0; i < NUM_LEDS; i++){ g_pixels[i] = i < num_leds ? color : 0; }
        g_strip->show();

        // sprintf(g_serial_buf, "sample rate: %d\n", g_current_sample_rate);
        // sprintf(g_serial_buf, "poti: %d\n", analogRead(A3));
        // Serial.write(g_serial_buf);
    }
}

uint32_t convert_distance(uint32_t the_measurement, SharpIR_Model the_model)
{
    const float ARef_volt = 3.3;

    uint32_t ret = 0;
    float volts = the_measurement / ADC_MAX * ARef_volt;

    switch(the_model)
    {
        case GP2Y0A02YK:
            ret = 65.f * pow(volts, -1.1f);
            break;
    }
    return ret;
}

void process_mic_input(uint32_t the_delta_time)
{
    // decay
    const float decay_secs = 1.f;
    float decay = 1.f / decay_secs * the_delta_time / 1000.f;
    g_mic_lvl = max(0, g_mic_lvl - decay);

    if(millis() > g_mic_start_millis + g_mic_sample_window)
    {
        g_mic_peak_to_peak = g_mic_signal_max - g_mic_signal_min;  // max - min = peak-peak amplitude
        g_mic_signal_max = 0;
        g_mic_signal_min = ADC_MAX;
        g_mic_start_millis = millis();

        g_mic_peak_to_peak = max(0, g_mic_peak_to_peak - 2);

        // read mic val
        float v = clamp<float>(g_mic_peak_to_peak / 100.f, 0.f, 1.f);
        g_mic_lvl = max(g_mic_lvl, v);

        g_current_sample_rate = 1000 * g_tick_count / g_mic_sample_window;
        g_tick_count = 0;

        // Run FFT on sample data.
        // arm_cfft_radix4_instance_f32 fft_inst;
    }
}
