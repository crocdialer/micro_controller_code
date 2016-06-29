#include "ADCTouch.h"
#include "fab_utils/fab_utils.h"

//! serial communication
#define SERIAL_START_CODE 0x7E
#define SERIAL_END_CODE 0xE7
char g_serial_buf[512];

//! time managment
int g_update_interval = 250;
int g_time_accum = 0;
long g_last_time_stamp;

//! number of capacitve touch sensors used
#define NUM_SENSORS 1

//! number of repeated measurements for one reading
#define NUM_SAMPLES 100

//! timeout in ms, before a release event is forced
#define TIMEOUT_RELEASE 5000

const uint8_t g_sensor_pins[] = {A0, A1, A2, A3, A4, A5};

ADCTouchClass g_adc_touch;

float g_base_values[NUM_SENSORS];
long g_time_stamps[NUM_SENSORS];
uint16_t g_touch_states = 0, g_touch_buffer = 0;

//! adjust these for desired sensitivity
const int g_thresh_touch = 32;
const int g_thresh_release = 24;

CircularBuffer<int> g_circ_bufs[NUM_SENSORS];

void setup()
{
  Serial.begin(57600);

  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    g_circ_bufs[i].set_capacity(5);
    g_base_values[i] = 0;
    g_time_stamps[i] = millis();
  }
}

void loop()
{
  // time measurement
  uint32_t delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    uint16_t mask = 1 << i;
    int val = g_adc_touch.read(g_sensor_pins[i], NUM_SAMPLES);
    g_circ_bufs[i].push(val);

    // circ buffer filled up and ready
    // if(g_circ_bufs[i].size() == g_circ_bufs[i].capacity())
    // {
    //     val = g_circ_bufs[i][4] * 0.4f + g_circ_bufs[i][3] * 0.3f +
    //       g_circ_bufs[i][2] * 0.2f + g_circ_bufs[i][1] * 0.1f;
    // }

    g_base_values[i] = mix<float>(g_base_values[i], val, 0.02f);
    int diff = val - g_base_values[i];

    // TODO: still loosing release events measuring like this
    if(diff > g_thresh_touch && !(mask & g_touch_states))
    {
      g_touch_states |= mask;
      g_time_stamps[i] = g_last_time_stamp;
      // Serial.print(i + 1); Serial.print(" touched\n");
    }
    else if((diff <= -g_thresh_release && (mask & g_touch_states)) ||
            g_last_time_stamp - TIMEOUT_RELEASE > g_time_stamps[i])
    {
      g_touch_states ^= mask;
      // Serial.print(i + 1); Serial.print(" released\n");
    }

    if(g_time_accum >= g_update_interval)
    {
      sprintf(g_serial_buf, "base: %d -- current value: %d\n", (int)g_base_values[i], val);
      Serial.print(g_serial_buf);
    }
  }

  // register all touch-events and keep them
  g_touch_buffer |= g_touch_states;

  if(g_time_accum >= g_update_interval)
  {
    // send values as byte array
    // Serial.write(SERIAL_START_CODE);
    // Serial.write((uint8_t*)&g_touch_buffer, sizeof(g_touch_buffer));
    // Serial.write(SERIAL_END_CODE);

    g_time_accum = 0;
    g_touch_buffer = 0;
  }
}
