#include "ModeHelpers.h"
#include "Timer.hpp"
#include "device_id.h"

// accelerometer
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include "RunningMedian.h"
#include "utils.h"
#include "vec3.h"

// #define USE_NETWORK
// #define NO_ETHERNET
// #define NO_WIFI
//
// #ifdef USE_NETWORK
// #include "NetworkHelper.h"
//
// // Ethernet MAC adress
// uint8_t g_mac_adress[6] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x69};
//
// // network SSID
// static constexpr uint32_t g_num_known_networks = 3;
// static const char* g_wifi_known_networks[2 * g_num_known_networks] =
// {
//     "whoopy", "senftoast",
//     "Trendsetter", "trendsetter69",
//     "egligeil2.4", "#LoftFlower!"
// };
// NetworkHelper* g_net_helper = NetworkHelper::get();
//
// // UDP broadcast
// constexpr float g_udp_broadcast_interval = 2.f;
// uint16_t g_udp_broadcast_port = 55555;
//
// //TCP Server
// uint16_t g_tcp_listening_port = 44444;
//
// void send_udp_broadcast()
// {
//      NetworkHelper::get()->send_udp_broadcast(DEVICE_ID, g_udp_broadcast_port);
// };
//
// #endif

// update rate in Hz
#define UPDATE_RATE 30
#define SERIAL_BUFSIZE 128

char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;

// update interval in millis
const int g_update_interval = 1000 / UPDATE_RATE;

// an array of Timer objects is provided
constexpr uint32_t g_num_timers = 3;
enum TimerEnum
{
    TIMER_UDP_BROADCAST = 0,
    TIMER_RUNMODE = 1,
    TIMER_BRIGHTNESS_MEASURE = 2
};
kinski::Timer g_timer[g_num_timers];

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

bool g_use_indicator = false;

//! define our run-modes here
enum RunMode
{
    MODE_DEBUG = 1 << 0,
    MODE_RUNNING = 1 << 1,
    MODE_STREAMING = 1 << 2
};
uint32_t g_run_mode = MODE_RUNNING;

constexpr uint8_t g_num_paths = 1;
constexpr uint8_t g_path_lengths[] = {1};
const uint8_t g_led_pins[] = {5};

LED_Path* g_path[g_num_paths];
ModeHelper *g_mode_sinus = nullptr, *g_mode_colour = nullptr, *g_mode_current = nullptr;
CompositeMode *g_mode_composite = nullptr;

// brightness measuring
constexpr uint8_t g_photo_pin = A5;
constexpr uint16_t g_photo_thresh = 60;

// acceleration measuring
Adafruit_LIS3DH g_accel;
constexpr lis3dh_range_t g_sensor_range = LIS3DH_RANGE_4_G;
sensors_event_t g_sensor_event;
const uint16_t g_sense_interval = 5;
float g_last_accel_val = 0;

const uint16_t g_num_samples = 5;
RunningMedian g_running_median = RunningMedian(g_num_samples);

//! timer callback to reset the runmode after streaming
void set_running(){ g_run_mode = MODE_RUNNING; }

void setup()
{
    // while(!Serial){ delay(10); }
    Serial.begin(2000000);

    srand(analogRead(A0));

    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, 0);

    // enable photo sense pin
    pinMode(g_photo_pin, INPUT);

    //setup accelerometer
    g_accel = Adafruit_LIS3DH();
    if(!g_accel.begin()){ Serial.print("could not connect accelerometer ..."); }
    g_accel.setRange(g_sensor_range);

    // init path objects with pin array
    for(uint8_t i = 0; i < g_num_paths; ++i)
    {
         g_path[i] = new LED_Path(g_led_pins[i], g_path_lengths[i]);
    }

    // create ModeHelper objects
    g_mode_colour = new Mode_ONE_COLOR();
    g_mode_sinus = new SinusFill();
    g_mode_current = g_mode_composite = new CompositeMode();
    g_mode_composite->add_mode(g_mode_colour);
    g_mode_composite->add_mode(g_mode_sinus);

#ifdef USE_NETWORK
    if( g_net_helper->setup_ethernet(g_mac_adress) ||
        g_net_helper->setup_wifi(g_wifi_known_networks, g_num_known_networks))
    {
        g_net_helper->set_tcp_listening_port(g_tcp_listening_port);
        g_timer[TIMER_UDP_BROADCAST].expires_from_now(g_udp_broadcast_interval);
        g_timer[TIMER_UDP_BROADCAST].set_periodic();
        g_timer[TIMER_UDP_BROADCAST].set_callback(&::send_udp_broadcast);
    }
#endif

    g_timer[TIMER_RUNMODE].set_callback(set_running);

    // brightness measuring
    g_timer[TIMER_BRIGHTNESS_MEASURE].set_callback([]()
    {
        auto val = analogRead(g_photo_pin);
        bool use_leds = val < g_photo_thresh;

        // init path objects with pin array
        for(uint8_t i = 0; i < g_num_paths; ++i)
        {
            for(uint8_t j = 0; j < g_path[i]->num_segments(); ++j)
            {
                Segment *s = g_path[i]->segment(j);
                s->set_active(use_leds);
            }
        }

        // Serial.println(val);
    });
    g_timer[TIMER_BRIGHTNESS_MEASURE].set_periodic();
    g_timer[TIMER_BRIGHTNESS_MEASURE].expires_from_now(.2f);
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // measure acceleration
    for(uint8_t i = 0; i < g_num_samples; i++)
    {
        // sensor reading
        g_accel.getEvent(&g_sensor_event);
        vec3 measurement(g_sensor_event.acceleration.v);
        g_running_median.add(measurement.length2());

        // wait the desired interval
        delay(g_sense_interval);
    }
    constexpr float base_g =  9.82f;
    float g_factor = sqrt(g_running_median.getMedian()) / base_g;
    g_last_accel_val = max(g_last_accel_val, max(g_factor - 1.f, 0.f));

    // poll Timer objects
    for(uint32_t i = 0; i < g_num_timers; ++i){ g_timer[i].poll(); }

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        if(g_use_indicator){ digitalWrite(13, g_indicator); }
        g_indicator = !g_indicator;

        Serial.println(g_last_accel_val);
        g_last_accel_val = 0;

        if(!(g_run_mode & MODE_STREAMING))
        {
            for(uint8_t i = 0; i < g_num_paths; ++i)
            {
                g_mode_current->process(g_path[i], g_time_accum);
                g_path[i]->update(g_time_accum);
            }
        }
        // clear time accumulator
        g_time_accum = 0;
    }
}
