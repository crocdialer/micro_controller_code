#include <Wire.h>
#include "Adafruit_MPR121.h"
#include "RunningMedian.h"

#include "device_id.h"
#include "utils.h"
#include "Timer.hpp"

#define USE_WIFI

const int g_update_interval = 33;
char g_serial_buf[512];

//! time management
const int g_update_interval_params = 2000;
int g_time_accum = 0, g_time_accum_params = 0;
long g_last_time_stamp;

constexpr uint32_t g_num_timers = 1;
kinski::Timer g_timer[g_num_timers];
enum TimerEnum{TIMER_UDP_BROADCAST = 0};

#ifdef USE_WIFI
#include "WifiHelper.h"

// network SSID
static constexpr uint32_t g_num_known_networks = 2;
static const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "egligeil2.4", "#LoftFlower!",
    "Sunrise_2.4GHz_BA25E8", "sJ4C257yyukZ",
};
WifiHelper* g_wifi_helper = WifiHelper::get();

// UDP broadcast
constexpr float g_udp_broadcast_interval = 2.f;
uint16_t g_udp_broadcast_port = 55555;

//TCP Server
uint16_t g_tcp_listening_port = 33333;

void send_udp_broadcast()
{
     WifiHelper::get()->send_udp_broadcast(DEVICE_ID, g_udp_broadcast_port);
};
#endif

//! number of capacitve touch sensors used
#define NUM_SENSORS 1

// up to 4 on one i2c bus
// Default address is 0x5A, if tied to 3.3V its 0x5B
// If tied to SDA its 0x5C and if SCL then 0x5D
const uint8_t g_i2c_adresses[] = {0x5A, 0x5B, 0x5C, 0x5D};

Adafruit_MPR121 g_cap_sensors[NUM_SENSORS];

const uint16_t g_num_samples = 5;
RunningMedian g_proxy_medians[NUM_SENSORS];

//! contains a continuous measure for proximity
float g_proxy_values[NUM_SENSORS];

//! bitmasks for touch status
uint16_t g_touch_buffer[NUM_SENSORS];

//! adjust these for desired sensitivity
const uint8_t g_thresh_touch = 12;
const uint8_t g_thresh_release = 6;
const uint8_t g_charge_time = 0;

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

    for(uint8_t i = 0; i < NUM_SENSORS; ++i)
    {
        if(!g_cap_sensors[i].begin(g_i2c_adresses[i]))
        {
            // will block here if address is not found on i2c bus
        }
        g_cap_sensors[i].setThresholds(g_thresh_touch, g_thresh_release);

        // set global charge current (0 ... 63uA)
        g_cap_sensors[i].setChargeCurrentAndTime(63, g_charge_time);

        // enable proximity mode
        g_cap_sensors[i].setMode(Adafruit_MPR121::PROXI_01);

        g_touch_buffer[i] = 0;

        g_proxy_medians[i] = RunningMedian(g_num_samples);
    }
    Serial.begin(115200);
    // while(!has_uart()){ blink_status_led(); }

#ifdef USE_WIFI
    if(g_wifi_helper->setup_wifi(g_wifi_known_networks, g_num_known_networks))
    {
        g_wifi_helper->set_tcp_listening_port(g_tcp_listening_port);
        g_timer[TIMER_UDP_BROADCAST].expires_from_now(g_udp_broadcast_interval);
        g_timer[TIMER_UDP_BROADCAST].set_periodic();
        g_timer[TIMER_UDP_BROADCAST].set_callback(&::send_udp_broadcast);
    }
#endif
    digitalWrite(13, LOW);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;
    g_time_accum_params += delta_time;

    // poll Timer objects
    for(uint32_t i = 0; i < g_num_timers; ++i){ g_timer[i].poll(); }

    uint16_t touch_states = 0;

    for(uint8_t i = 0; i < NUM_SENSORS; i++)
    {
        int16_t base_value = g_cap_sensors[i].baselineData(12);
        g_proxy_medians[i].add(base_value - (int16_t)g_cap_sensors[i].filteredData(12));
        int16_t diff_to_base = g_proxy_medians[i].getMedian();

        const float decay_secs = .35f;
        float decay = 1.f / decay_secs * delta_time / 1000.f;
        g_proxy_values[i] = max(0, g_proxy_values[i] - decay);

        float val = clamp<float>(diff_to_base / 100.f, 0.f, 1.f);
        if(g_proxy_values[i] < val){ g_proxy_values[i] = val; }

        // touch_states |= (g_cap_sensors[i].touched() ? 1 : 0) << i;

        // uncomment to override: use inbuilt touch mechanism
        touch_states = g_cap_sensors[i].touched();

        // register all touch-events and keep them
        g_touch_buffer[i] |= touch_states;
    }


    if(g_time_accum >= g_update_interval)
    {
        char fmt_buf[32];
        char *buf_ptr = g_serial_buf;

        for(uint8_t i = 0; i < NUM_SENSORS; i++)
        {
            buf_ptr += sprintf(buf_ptr, "%d", g_touch_buffer[0]);

            fmt_real_to_str(fmt_buf, g_proxy_values[i]);
            buf_ptr += sprintf(buf_ptr, " %s ", fmt_buf);
        }
        sprintf(buf_ptr - 1, "\n");

        g_time_accum = 0;
        g_touch_buffer[0] = 0;

        // IO -> Serial
        process_input(Serial);
        Serial.write(g_serial_buf);

#ifdef USE_WIFI
        // IO -> TCP
        g_wifi_helper->update_connections();
        uint32_t num_connections = 0;
        auto wifi_clients = g_wifi_helper->connected_clients(&num_connections);

        for(uint32_t i = 0; i < num_connections; ++i)
        {
             process_input(*wifi_clients[i]);
             wifi_clients[i]->write(g_serial_buf);
        }
        g_wifi_helper->tcp_server().write(g_serial_buf);
#endif

    }
}

template <typename T> void process_input(T& the_device)
{
    uint16_t buf_idx = 0;

    while(the_device.available())
    {
        // get the new byte:
        char c = the_device.read();

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
        #ifdef USE_BLUETOOTH
            g_bt_serial.write(buf);
        #endif
        return true;
    }
    return false;
}

void parse_line(char *the_line)
{
    const char* delim = " ";
    const size_t elem_count = 3;
    char *token = strtok(the_line, delim);
    int num_buf[elem_count];
    uint16_t i = 0;

    for(; token && (i < elem_count); i++)
    {
        if(check_for_cmd(token)){ break; }
        else{ num_buf[i] = atoi(token); }
        token = strtok(nullptr, delim);
    }

    if(i == elem_count)
    {
        for(uint8_t i = 0; i < NUM_SENSORS; ++i)
        {
            g_cap_sensors[i].setThresholds(num_buf[0], num_buf[1]);
            g_cap_sensors[i].setChargeCurrentAndTime(num_buf[2], g_charge_time);
        }
    }
}
