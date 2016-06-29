/*
* connect and read a LIS3DH 3-axis accelerometer via I2C
* (connect and read a chestbelt (elongation) sensor via Analog Input)
* communicate measurements via Serial
*/

// #include <Wire.h>
// #include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#include "utils.h"
#include "RunningMedian.h"

#define SERIAL_START_CODE 0x7E
#define SERIAL_END_CODE 0xE7
#define SERIAL_BUFSIZE 512
char g_serial_buf[SERIAL_BUFSIZE];

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_update_interval = 16;
bool g_indicator = false;

RunningMedian g_running_median[3] =
{
  RunningMedian(10),
  RunningMedian(10),
  RunningMedian(10)
};

const uint8_t g_cs_pins[] = {5};
Adafruit_LIS3DH g_accelo = Adafruit_LIS3DH(g_cs_pins[0]);
sensors_event_t g_sensor_event;

const uint8_t g_sense_pin = A0;
const uint16_t g_sense_interval = 1;
const uint16_t g_num_samples = 5;

float g_value_buf = 0.f;

void setup()
{
  memset(g_serial_buf, 0, SERIAL_BUFSIZE);

  //setup accelerometer
  // change this to 0x19 for alternative i2c address
  if (!g_accelo.begin(0x18))
  {
    digitalWrite(13, HIGH);
    while (1);
  }
  // 2, 4, 8 or 16 G!
  g_accelo.setRange(LIS3DH_RANGE_4_G);

  // initialize the serial communication:
  Serial.begin(57600);
}

////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

void loop()
{
  // time measurement
  int delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  for(uint8_t i = 0; i < g_num_samples; i++)
  {
    // sensor reading
    g_accelo.getEvent(&g_sensor_event);

    for(uint8_t j = 0; j < 3; j++)
    {
      g_running_median[j].add(g_sensor_event.acceleration.v[j]);
    }

    // wait the desired interval
    delay(g_sense_interval);
  }

  // average of the middle n Median values
  // removes noise from outliers
  vec3 val = vec3(g_running_median[0].getAverage(2),
                  g_running_median[1].getAverage(2),
                  g_running_median[2].getAverage(2));

  const float base_g_2 = (9.81f * 9.81f);
  float abs_g = val.length2() / base_g_2;
  g_value_buf = max(g_value_buf, fabs(abs_g - 1.f));

  // uint16_t val_16 = 0xFFFF * abs_diff;

  if(g_time_accum >= g_update_interval)
  {
    // send values as byte array
    Serial.write(SERIAL_START_CODE);
    Serial.write((uint8_t*)&g_value_buf, sizeof(g_value_buf));
    Serial.write(SERIAL_END_CODE);
    // Serial.print((int)(g_value_buf * 100));Serial.print(" %\n");

    g_value_buf = 0.f;
    g_time_accum = 0;
    g_indicator = !g_indicator;
    // digitalWrite(13, g_indicator);
  }
}


void serialEvent()
{
  while (Serial.available())
  {
    // get the new byte:
    char c = Serial.read();

    if(c == '\0'){ continue; }
  }
}
