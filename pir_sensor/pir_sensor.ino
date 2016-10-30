/* read a sharp distance sensor,
* read a poti value as threshold
* light an indicator-LED, send status via Serial
*/
#include "RunningMedian.h"

#define DISTANCE_PIN A0
#define POTI_PIN A1
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
const uint32_t g_motion_timeout = 5000;

void motion_ISR()
{
    g_motion_timestamp = millis();
}

void setup()
{
    for(uint8_t i = 0; i < g_num_pirs; i++)
    {
        pinMode(g_pir_pins[i], INPUT);
        attachInterrupt(g_pir_pins[i], motion_ISR, RISING);
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

    // eveluate motion timeout
    bool motion_detected = millis() - g_motion_timestamp < g_motion_timeout;
    if(!motion_detected){ g_state_buf = STATE_INACTIVE; }

    if(g_time_accum >= g_update_interval)
    {
        sprintf(g_serial_buf, "%d %s\n",
                g_state_buf ? distance_val : 0,
                motion_detected ? "PIR" : "");
        Serial.write(g_serial_buf);
        g_time_accum = 0;
    }
    digitalWrite(LED_PIN, g_state_buf);
    // delay(250);
}
