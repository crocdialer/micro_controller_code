#include "ModeHelpers.h"
#include "Timer.hpp"
#include "device_id.h"

#define USE_NETWORK
#define NO_ETHERNET
// #define NO_WIFI

#ifdef USE_NETWORK
#include "NetworkHelper.h"

// network SSID
static constexpr uint32_t g_num_known_networks = 2;
static const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "egligeil2.4", "#LoftFlower!",
    "Sunrise_2.4GHz_BA25E8", "sJ4C257yyukZ",
};
NetworkHelper* g_net_helper = NetworkHelper::get();

// UDP broadcast
constexpr float g_udp_broadcast_interval = 2.f;
uint16_t g_udp_broadcast_port = 55555;

//TCP Server
uint16_t g_tcp_listening_port = 33333;

void send_udp_broadcast()
{
     NetworkHelper::get()->send_udp_broadcast(DEVICE_ID, g_udp_broadcast_port);
};

#endif

// update rate in Hz
#define UPDATE_RATE 60
#define SERIAL_BUFSIZE 128

char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;

// update interval in millis
const int g_update_interval = 1000 / UPDATE_RATE;

// an array of Timer objects is provided
constexpr uint32_t g_num_timers = 1;
enum TimerEnum{TIMER_UDP_BROADCAST = 0};
kinski::Timer g_timer[g_num_timers];

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

//! define our run-modes here
enum RunMode
{
    MODE_DEBUG = 1 << 0,
    MODE_RUNNING = 1 << 1,
};
uint32_t g_run_mode = MODE_RUNNING;

constexpr uint8_t g_num_paths = 1;
constexpr uint8_t g_path_length = 5;
const uint8_t g_led_pins[] = {5};

LED_Path* g_path[g_num_paths];
ModeHelper* g_mode_helper[g_num_paths];

void setup()
{
    srand(analogRead(A7));

    // drives our status LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // while(!Serial){ delay(10); }
    Serial.begin(115200);

    // init path objects with pin array
    for(uint8_t i = 0; i < g_num_paths; ++i)
    {
         g_path[i] = new LED_Path(g_led_pins[i], g_path_length);
         g_path[i]->set_sinus_offsets(random<int>(0, 256), random<int>(0, 256));
         g_mode_helper[i] = new Mode_ONE_COLOR(g_path[i]);//CompositeMode
    }

#ifdef USE_NETWORK
    if(g_net_helper->setup_wifi(g_wifi_known_networks, g_num_known_networks))
    {
        g_net_helper->set_tcp_listening_port(g_tcp_listening_port);
        g_timer[TIMER_UDP_BROADCAST].expires_from_now(g_udp_broadcast_interval);
        g_timer[TIMER_UDP_BROADCAST].set_periodic();
        g_timer[TIMER_UDP_BROADCAST].set_callback(&::send_udp_broadcast);
    }
#endif
}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;

    // poll Timer objects
    for(uint32_t i = 0; i < g_num_timers; ++i){ g_timer[i].poll(); }

    if(g_time_accum >= g_update_interval)
    {
        // flash red indicator LED
        digitalWrite(13, g_indicator);
        g_indicator = !g_indicator;

        // read debug inputs
        process_input(Serial);

#ifdef USE_NETWORK
        g_net_helper->update_connections();
        uint32_t num_connections = 0;
        auto wifi_clients = g_net_helper->connected_clients(&num_connections);
        for(uint8_t i = 0; i < num_connections; ++i){ process_input(*wifi_clients[i]); }
#endif

        // do nothing here while debugging
        if(g_run_mode & MODE_DEBUG){ }
        else if(g_run_mode & MODE_RUNNING)
        {
             for(uint8_t i = 0; i < g_num_paths; ++i){ g_mode_helper[i]->process(g_time_accum); }
        }

        for(uint8_t i = 0; i < g_num_paths; ++i){ g_path[i]->update(g_time_accum); }

        // clear time accumulator
        g_time_accum = 0;
    }
}

template <typename T> void process_input(T& the_device)
{
    while(the_device.available())
    {
        // get the new byte:
        uint8_t c = the_device.read();
        if(c == '\0'){ continue; }

        // add it to the buf
        g_serial_buf[g_buf_index % SERIAL_BUFSIZE] = c;
        g_buf_index = (g_buf_index + 1) % SERIAL_BUFSIZE;

        // if the incoming character is a newline, set a flag
        if (c == '\n')
        {
            int index = atoi((const char*)g_serial_buf);
            g_buf_index = 0;
            memset(g_serial_buf, 0, SERIAL_BUFSIZE);

            Serial.println(index);

            if(index >= 0 && index < g_path[0]->num_segments())
            // if(index >= 0 && index < g_num_paths)
            {
                g_run_mode = MODE_DEBUG;

                for(uint32_t i = 0; i < g_path[0]->num_segments(); ++i)
                {
                    g_path[0]->segment(i)->set_active(index == i);
                }
                g_path[0]->segment(index)->set_color(ORANGE);
                g_path[0]->update(0);

                // for(uint8_t i = 0; i < g_num_paths; ++i)
                // {
                //     g_path[i]->set_all_segments(index == i ? ORANGE : BLACK);
                //     g_path[i]->update(g_time_accum);
                // }
            }
            else
            {
                g_run_mode = MODE_RUNNING;
                for(uint8_t i = 0; i < g_num_paths; ++i){ g_mode_helper[i]->reset(); }
            }
        }
    }
}
