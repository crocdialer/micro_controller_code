/* read a sharp distance sensor,
* read a poti value as threshold
* light an indicator-LED, send status via Serial
*/
#include "RunningMedian.h"

#define CMD_QUERY_ID "ID"
#define CMD_START "START"
#define CMD_STOP "STOP"
#define CMD_RESET "RESET"
#define DEVICE_ID "DISTANCE_SENSOR"

#define DISTANCE_PIN A5
// #define POTI_PIN A1
#define LED_PIN 13

const uint8_t g_num_pirs = 1;
const uint8_t g_pir_pins[g_num_pirs] = {6};

char g_serial_buf[512];

enum State
{
    STATE_INACTIVE = 0,
    STATE_ACTIVE = 1
};

uint32_t g_state_buf = STATE_INACTIVE;

//! time managment
int g_update_interval = 33;
int g_time_accum = 0;
long g_last_time_stamp;

const int g_distance_thresh = 350;
const uint16_t g_sense_interval = 1;
const uint16_t g_num_samples = 5;

RunningMedian g_running_median = RunningMedian(g_num_samples);

volatile uint32_t g_motion_timestamp = 0;
volatile bool g_pir_active = false;
const uint32_t g_motion_timeout = 120000;

void motion_ISR()
{
    g_motion_timestamp = millis();
    g_pir_active = false;
    for(int i = 0; i < g_num_pirs; ++i){ g_pir_active |= digitalRead(g_pir_pins[i]); }
}

void setup()
{
    for(uint8_t i = 0; i < g_num_pirs; i++)
    {
        pinMode(g_pir_pins[i], INPUT);
        attachInterrupt(g_pir_pins[i], motion_ISR, CHANGE);
    }
    pinMode(DISTANCE_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(57600);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    for(uint8_t i = 0; i < g_num_samples; i++)
    {
        g_running_median.add(analogRead(DISTANCE_PIN));
        delay(g_sense_interval);
    }
    int distance_val = g_running_median.getMedian();

    if(distance_val > g_distance_thresh)
    {
         g_state_buf = STATE_ACTIVE;
         g_motion_timestamp = millis();
    }

    // evaluate motion timeout
    bool is_timer_running = millis() - g_motion_timestamp < g_motion_timeout;
    if(!is_timer_running){ g_state_buf = STATE_INACTIVE; }

    int millis_left = g_motion_timeout - millis() + g_motion_timestamp;

    if(g_time_accum >= g_update_interval)
    {
        // sprintf(g_serial_buf, "active: %d -- distance: %d -- pir: %d\n",
        //         g_state_buf, distance_val, g_pir_active);
        sprintf(g_serial_buf, "%d\n", g_state_buf ? millis_left : 0);

        Serial.write(g_serial_buf);
        g_time_accum = 0;

        process_serial_input(Serial);
    }
    digitalWrite(LED_PIN, g_state_buf);
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
    char *token = strtok(the_line, delim);
    uint16_t i = 0;

    while(token)
    {
        check_for_cmd(token);
        token = strtok(nullptr, delim);
    }
}
