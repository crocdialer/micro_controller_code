#include "CapacitiveSensor.h"
#include "fab_utils/fab_utils.h"

//! serial communication
#define SERIAL_START_CODE 0x7E
#define SERIAL_END_CODE 0xE7
char g_serial_buf[512];

//! time managment
int g_update_interval = 16;
int g_time_accum = 0;
long g_last_time_stamp;

//! number of capacitve touch sensors used
#define NUM_SENSORS 2

//! number of repeated measurements for one reading
#define NUM_SAMPLES 5

//! timeout in ms, before a release event is forced
#define TIMEOUT_RELEASE 5000

const uint8_t g_send_pin = 6;
const uint8_t g_receive_pins[] = {7, 8, 9};

CapacitiveSensor g_cap_sensors[] =
{
  CapacitiveSensor(g_send_pin, 7),
  CapacitiveSensor(g_send_pin, 8),
  CapacitiveSensor(g_send_pin, 9)
};

float g_base_values[NUM_SENSORS];
long g_time_stamps[NUM_SENSORS];
uint16_t g_touch_states = 0, g_touch_buffer = 0;

const uint8_t g_led_pin = 12;

//! adjust these for desired sensitivity
const int g_thresh_touch = 500;
const int g_thresh_release = 200;
float g_thresh_frac = 0.1f;

CircularBuffer<long> g_circ_bufs[NUM_SENSORS];

void setup()
{
  Serial.begin(57600);

  pinMode(A0, INPUT);
  pinMode(g_led_pin, OUTPUT);

  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    // g_cap_sensors[i] = CapacitiveSensor(g_send_pin, g_receive_pins[i]);
    g_circ_bufs[i].set_capacity(5);
    g_base_values[i] = 0;
    g_time_stamps[i] = millis();

    // g_cap_sensors[i].set_CS_AutocaL_Millis(0xFFFFFFFF);
  }
}

void loop()
{
  // time measurement
  uint32_t delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  g_thresh_frac = analogRead(A0) / 1023.f;

  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    uint16_t mask = 1 << i;
    long val = g_cap_sensors[i].capacitiveSensor(NUM_SAMPLES);
    g_circ_bufs[i].push(val);
    long sum = 0;

    for(uint8_t j = 0; j < g_circ_bufs[i].size(); j++)
    {
      sum += g_circ_bufs[i][j];
    }
    val = sum / g_circ_bufs[i].size();

    g_base_values[i] = mix<float>(g_base_values[i], val, 0.02f);
    int diff = val - g_base_values[i];

    int thresh_touch = g_base_values[i] * g_thresh_frac;
    int thresh_release = 2 * thresh_touch / 5;
    // int thresh_touch = g_thresh_touch * g_thresh_frac;
    // int thresh_release = g_thresh_release * g_thresh_frac;

    // TODO: still loosing release events measuring like this
    if(diff > thresh_touch && !(mask & g_touch_states))
    {
      g_touch_states |= mask;
      g_time_stamps[i] = g_last_time_stamp;
      // Serial.print(i + 1); Serial.print(" touched\n");
    }
    else if((mask & g_touch_states) && ((diff <= -thresh_release )||
            g_last_time_stamp - TIMEOUT_RELEASE > g_time_stamps[i]))
    {
      g_touch_states ^= mask;
      // Serial.print(i + 1); Serial.print(" released\n");
    }

    // if(g_time_accum >= g_update_interval)
    // {
    //   sprintf(g_serial_buf, "base: %d -- current value: %ld -- thresh: %d\n",
    //           (int)g_base_values[i], val, (int)(g_thresh_frac * 100));
    //   Serial.print(g_serial_buf);
    // }
  }

  // register all touch-events and keep them
  g_touch_buffer |= g_touch_states;

  // touch feedback LED
  digitalWrite(g_led_pin, g_touch_buffer);

  if(g_time_accum >= g_update_interval)
  {
    // send values as byte array
    Serial.write(SERIAL_START_CODE);
    Serial.write((uint8_t*)&g_touch_buffer, sizeof(g_touch_buffer));
    Serial.write(SERIAL_END_CODE);

    g_time_accum = 0;
    g_touch_buffer = 0;
  }
}
