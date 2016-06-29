/* read a sharp distance sensor,
* read a poti value as threshold
* light an indicator-LED, send status via Serial
*/

#include "RunningMedian.h"

#define SIGNAL_PIN A0
#define POTI_PIN A1
#define LED_PIN 4

char g_serial_buf[512];

uint8_t g_state_buf = 0;

//! time managment
int g_update_interval = 100;
int g_time_accum = 0;
long g_last_time_stamp;

const uint16_t g_sense_interval = 2;
const uint16_t g_num_samples = 5;

RunningMedian g_running_median = RunningMedian(20);

void setup()
{
    // pinMode(SIGNAL_PIN, INPUT);
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
        g_running_median.add(analogRead(SIGNAL_PIN));
        delay(g_sense_interval);
    }

    int distance_val = g_running_median.getAverage(4);
    int pot_val = analogRead(POTI_PIN);

    bool has_detect = true;//distance_val > pot_val;
    g_state_buf |= has_detect;

    // Serial.println(has_detect);
    digitalWrite(LED_PIN, g_state_buf);

    if(g_time_accum >= g_update_interval)
    {
        sprintf(g_serial_buf, "%d\n", g_state_buf ? distance_val : 0);
        Serial.write(g_serial_buf);

        g_time_accum = 0;
        g_state_buf = 0;
    }

  // delay(16);
}
