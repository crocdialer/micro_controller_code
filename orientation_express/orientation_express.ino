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

#define LED_PIN 12
#define NUM_LEDS 8
#define BRIGHTNESS 50
#define BAT_PIN A7
#define MIC_PIN A1
#define POTI_PIN A2

const int g_update_interval = 20;

char g_serial_buf[512];

// mic sampling
int g_mic_sample_window = 10;
unsigned long g_mic_start_millis = 0;  // start of sample window
volatile unsigned int g_mic_peak_to_peak = 0;   // peak-to-peak level
unsigned int g_mic_signal_max = 0;
unsigned int g_mic_signal_min = 4096;
float g_mic_lvl = 0.f; // 0.0 ... 1.0

//! time management
const int g_update_interval_params = 2000;
int g_time_accum = 0, g_time_accum_params = 0;
long g_last_time_stamp = 0;

bool g_bt_initialized = false;

// i2c
Adafruit_LSM9DS0 g_lsm = Adafruit_LSM9DS0();

// You can also use software SPI
//Adafruit_LSM9DS0 lsm = Adafruit_LSM9DS0(13, 12, 11, 10, 9);
// Or hardware SPI! In this case, only CS pins are passed in
//Adafruit_LSM9DS0 lsm = Adafruit_LSM9DS0(10, 9);

const uint16_t g_num_samples = 3;
const uint16_t g_sense_interval = 2;
RunningMedian g_running_median = RunningMedian(g_num_samples);
uint32_t g_current_distance = 0;

enum SharpIR_Model{ GP2Y0A21Y, GP2Y0A02YK, GP2Y0A710K0F };
uint32_t convert_distance(uint32_t the_measurement, SharpIR_Model the_model = GP2Y0A02YK);

// Color defines (BRGW)
static const uint32_t
WHITE = Adafruit_NeoPixel::Color(0, 0, 0, 255),
PURPLE = Adafruit_NeoPixel::Color(150, 235, 0, 0),
ORANGE = Adafruit_NeoPixel::Color(0, 255, 50, 30),
BLACK = 0;

uint32_t *g_pixels = nullptr;

static const uint8_t r_offset = 1, g_offset = 0, b_offset = 2, w_offset = 3;

float smoothstep(float edge0, float edge1, float x)
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
    measuredvbat *= 2 * 3.3f / 4095.f;
    Serial.print("VBat: " );
    Serial.println(measuredvbat);
}

static inline uint32_t mix_colors(uint32_t lhs, uint32_t rhs, float ratio)
{
    uint8_t *ptr_lhs = (uint8_t*) &lhs, *ptr_rhs = (uint8_t*) &rhs;

    return  mix<uint32_t>(ptr_lhs[w_offset], ptr_rhs[w_offset], ratio) << 24 |
            mix<uint32_t>(ptr_lhs[b_offset], ptr_rhs[b_offset], ratio) << 16 |
            mix<uint32_t>(ptr_lhs[r_offset], ptr_rhs[r_offset], ratio) << 8 |
            mix<uint32_t>(ptr_lhs[g_offset], ptr_rhs[g_offset], ratio);
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

void setup()
{
    // drives our status LED
    pinMode(13, OUTPUT);

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

    // set ADC resolution (up to 12 bits (0 - 4095))
    analogReadResolution(12);

    digitalWrite(13, LOW);
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

    process_mic_input();

    if(g_time_accum >= g_update_interval)
    {
        g_time_accum = 0;

        float gain = map_value<float>((analogRead(POTI_PIN) - 1) / 4095.f,
                                      0.f, 1.f, 0.f, 8.f);
        float val = clamp<float>(g_mic_lvl * gain, 0.f, 1.f);
        int num_leds = val * NUM_LEDS;

        //WBRG
        // uint32_t color = 0 << 24 | 0 << 16 | 0 << 8 | 255;
        uint32_t color = PURPLE;
        color = fade_color(ORANGE, val);
        // print_battery_lvl();

        for(int i = 0; i < NUM_LEDS; i++){ g_pixels[i] = i < num_leds ? color : 0; }
        g_strip->show();

        // sprintf(g_serial_buf, "peak to peak: %d\nmic: ", g_mic_peak_to_peak);
        // Serial.write(g_serial_buf);
        // Serial.println(g_mic_lvl);
    }
}

uint32_t convert_distance(uint32_t the_measurement, SharpIR_Model the_model)
{
    const uint32_t ADC_maxval = 4095;
    const float ARef_volt = 3.3;

    uint32_t ret = 0;
    float volts = the_measurement / (float)ADC_maxval * ARef_volt;

    switch(the_model)
    {
        case GP2Y0A02YK:
            ret = 65.f * pow(volts, -1.1f);
            break;

        case GP2Y0A710K0F:
            // ret = map(the_measurement, 0, ADC_maxval, 0, ARef_mvolt);
            break;
    }
    return ret;
}

void process_mic_input()
{
  uint16_t sample = analogRead(MIC_PIN);

  // toss out spurious readings
  if(sample < 4096)
  {
      g_mic_signal_min = min(g_mic_signal_min, sample);  // save just the min levels
      g_mic_signal_max = max(g_mic_signal_max, sample);  // save just the max levels
  }

  if(millis() > g_mic_start_millis + g_mic_sample_window)
  {
      g_mic_peak_to_peak = g_mic_signal_max - g_mic_signal_min;  // max - min = peak-peak amplitude
      g_mic_signal_max = 0;
      g_mic_signal_min = 4096;
      g_mic_start_millis = millis();
      g_mic_lvl *= .97f;

      // read mic val
      float v = map(clamp<int>(g_mic_peak_to_peak, 10, 60), 10, 60, 0, 1023) / 1023.f;
      g_mic_lvl = max(g_mic_lvl, v);
  }
}
