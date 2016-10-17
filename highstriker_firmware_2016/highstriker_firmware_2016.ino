/*
* connect and read an array of LIS3DH 3-axis accelerometers via SPI
* communicate measurements via Serial
* author: Fabian - crocdialer@googlemail.com
*/

#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#include "RunningMedian.h"
#include "utils.h"
#include "vec3.h"

#define USE_SPI 0

#define SERIAL_END_CODE '\n'
#define SERIAL_BUFSIZE 512
char g_serial_buf[SERIAL_BUFSIZE], g_num_buf[32];

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_update_interval = 33;
bool g_indicator = false;

// the number of attached SPI sensors
const uint8_t g_max_num_sensors = 1;
uint8_t g_num_sensors = 0;
const uint32_t g_sensor_range = LIS3DH_RANGE_4_G;

// the array of SPI chip-select pins
const uint8_t g_cs_pins[] = {9, 10, 11, 12};

Adafruit_LIS3DH g_sensors[g_max_num_sensors];
sensors_event_t g_sensor_event;
const uint16_t g_sense_interval = 0;
float g_value_buf[g_max_num_sensors];

const uint16_t g_num_samples = 5;

RunningMedian g_running_median = RunningMedian(g_num_samples);

void setup()
{
    // initialize the serial communication:
    Serial.begin(57600);

    memset(g_serial_buf, 0, SERIAL_BUFSIZE);
    memset(g_value_buf, 0, sizeof(g_value_buf));

    //setup accelerometers
    for(uint8_t i = 0; i < g_max_num_sensors; i++)
    {
        g_sensors[g_num_sensors] = USE_SPI ? Adafruit_LIS3DH(g_cs_pins[i]) : Adafruit_LIS3DH();

        // passed in adress has no effect, since its SPI
        if(!g_sensors[g_num_sensors].begin())
        {
            Serial.print("Sensor not connected: ");
            Serial.println(i);
        }else{ g_num_sensors++; }
    }

    for(uint8_t i = 0; i < g_num_sensors; i++)
    {
        // 2, 4, 8 or 16 G!
        g_sensors[i].setRange(g_sensor_range);
    }

    if(!g_num_sensors)
    {
        digitalWrite(13, HIGH);
        while(1);
    }
}

////////////////////////////////////////////////////////////////////////

void loop()
{
    // time measurement
    int delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    for(uint8_t s = 0; s < g_num_sensors; s++)
    {
        for(uint8_t i = 0; i < g_num_samples; i++)
        {
            // sensor reading
            g_sensors[s].getEvent(&g_sensor_event);
            vec3 measurement(g_sensor_event.acceleration.v[0],
                             g_sensor_event.acceleration.v[1],
                             g_sensor_event.acceleration.v[2]);
            g_running_median.add(measurement.length2());

            // wait the desired interval
            delay(g_sense_interval);
        }
        const float base_g =  9.80665f;
        float g_factor = sqrt(g_running_median.getMedian()) / base_g;
        g_value_buf[s] = max(g_value_buf[s], max(g_factor - 1.f, 0.f));
    }

    if(g_time_accum >= g_update_interval)
    {
        for(uint8_t s = 0; s < g_num_sensors; s++)
        {
            fmt_real_to_str(g_num_buf, g_value_buf[s]);
            strcat(g_serial_buf, g_num_buf);
            strcat(g_serial_buf, " ");
        }
        g_serial_buf[strlen(g_serial_buf) - 1] = '\n';
        // strcat(g_serial_buf, "\n");

        // sprintf(g_serial_buf, "%s\n", g_num_buf);
        Serial.write(g_serial_buf);

        g_serial_buf[0] = '\0';
        memset(g_value_buf, 0, sizeof(g_value_buf));
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
