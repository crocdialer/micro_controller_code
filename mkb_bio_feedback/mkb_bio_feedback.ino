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

#define CMD_QUERY_ID "ID"
#define CMD_START "START"
#define CMD_STOP "STOP"
#define CMD_RESET "RESET"
#define DEVICE_ID "BIO_FEEDBACK"

#define USE_SPI 0
#define HALL_PIN A5

#define SERIAL_END_CODE '\n'
#define SERIAL_BUFSIZE 512
char g_serial_buf[SERIAL_BUFSIZE];

#define ADC_BITS 10
const float ADC_MAX = (1 << ADC_BITS) - 1.f;

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

float g_hall_current_value = 0;
const uint16_t g_num_samples = 5;

RunningMedian
g_accel_median = RunningMedian(g_num_samples),
g_hall_median = RunningMedian(g_num_samples);

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

    // acceleration
    for(uint8_t s = 0; s < g_num_sensors; s++)
    {
        for(uint8_t i = 0; i < g_num_samples; i++)
        {
            // sensor reading
            g_sensors[s].getEvent(&g_sensor_event);
            vec3 measurement(g_sensor_event.acceleration.v);
            g_accel_median.add(measurement.length2());

            // wait the desired interval
            delay(g_sense_interval);
        }
        const float base_g =  9.80665f;
        float g_factor = sqrt(g_accel_median.getMedian()) / base_g;
        g_value_buf[s] = max(g_value_buf[s], max(g_factor - 1.f, 0.f));
    }

    // elongation (hall sensor)
    for(uint8_t i = 0; i < g_num_samples; i++)
    {
        g_hall_median.add(analogRead(HALL_PIN));
    }
    g_hall_current_value = 1.f - (g_hall_median.getMedian() / ADC_MAX);

    if(g_time_accum >= g_update_interval)
    {
        for(uint8_t s = 0; s < g_num_sensors; s++)
        {
            strcat(g_serial_buf, fmt_real_to_str(g_value_buf[s]));
            strcat(g_serial_buf, " ");
        }
        strcat(g_serial_buf, fmt_real_to_str(g_hall_current_value));
        strcat(g_serial_buf, "\n");

        // write to serial
        Serial.print(g_serial_buf);

        g_serial_buf[0] = '\0';
        memset(g_value_buf, 0, sizeof(g_value_buf));
        g_time_accum = 0;
        g_indicator = !g_indicator;
        // digitalWrite(13, g_indicator);

        // process_serial_input(Serial);
    }
}

template <typename T> void process_serial_input(T& the_serial)
{
    uint16_t buf_idx = 0;

    while(the_serial.available())
    {
        // get the new byte:
        char c = the_serial.read();

        switch(c)
        {
            case '\r':
            case '\0':
                continue;

            case '\n':
                g_serial_buf[buf_idx] = '\0';
                buf_idx = 0;
                parse_line(g_serial_buf);
                break;

            default:
                g_serial_buf[buf_idx++] = c;
                break;
        }
    }
}

bool check_for_cmd(const char* the_str)
{
    if(strcmp(the_str, CMD_QUERY_ID) == 0)
    {
        char buf[32];
        sprintf(buf, "%s %s\n", the_str, DEVICE_ID);
        Serial.write(buf);
        return true;
    }
    return false;
}

void parse_line(char *the_line)
{
    const char* delim = " ";
    const size_t elem_count = 3;
    char *token = strtok(the_line, delim);

    while(token)
    {
        check_for_cmd(token);
        token = strtok(nullptr, delim);
    }
}
