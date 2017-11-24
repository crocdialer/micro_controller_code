#include "ModeHelpers.h"
#include "Timer.hpp"
#include "device_id.h"

#define USE_NETWORK
#define NO_ETHERNET
// #define NO_WIFI

#ifdef USE_NETWORK
#include "NetworkHelper.h"

// network SSID
static constexpr uint32_t g_num_known_networks = 3;
static const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "egligeil2.4", "#LoftFlower!",
    "Sunrise_2.4GHz_BA25E8", "sJ4C257yyukZ",
    "iWay_Fiber_jz748", "92588963378762374925"
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
constexpr uint32_t g_num_timers = 2;
enum TimerEnum
{
    TIMER_UDP_BROADCAST = 0,
    TIMER_RUNMODE = 1
};
kinski::Timer g_timer[g_num_timers];

// helper for flashing PIN 13 (red onboard LED)
// to indicate update frequency
bool g_indicator = false;

//! define our run-modes here
enum RunMode
{
    MODE_DEBUG = 1 << 0,
    MODE_RUNNING = 1 << 1,
    MODE_STREAMING = 1 << 2
};
uint32_t g_run_mode = MODE_RUNNING;

constexpr uint8_t g_num_paths = 1;
constexpr uint8_t g_path_length = 6;
const uint8_t g_led_pins[] = {11};

LED_Path* g_path[g_num_paths];
ModeHelper* g_mode_helper[g_num_paths];

//! timer callback to reset the runmode after streaming
void set_running(){ g_run_mode = MODE_RUNNING; }

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

    g_timer[TIMER_RUNMODE].set_callback(set_running);
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
        auto net_clients = g_net_helper->connected_clients(&num_connections);
        for(uint8_t i = 0; i < num_connections; ++i){ process_input(*net_clients[i]); }
#endif

        // do nothing here while debugging
        if(g_run_mode & MODE_DEBUG){ }
        else if(g_run_mode & MODE_RUNNING)
        {
             for(uint8_t i = 0; i < g_num_paths; ++i){ g_mode_helper[i]->process(g_time_accum); }
        }

        if(!(g_run_mode & MODE_STREAMING))
            for(uint8_t i = 0; i < g_num_paths; ++i){ g_path[i]->update(g_time_accum); }

        // clear time accumulator
        g_time_accum = 0;
    }
}

template <typename T> void process_input(T& the_device)
{
    g_buf_index = 0;

    while(the_device.available())
    {
        // get the new byte:
        uint8_t c = the_device.read();

        switch(c)
        {
            case '\r':
            case '\0':
                continue;

            case '\n':
                g_serial_buf[g_buf_index] = '\0';
                g_buf_index = 0;
                parse_line(the_device, g_serial_buf);
                break;

            default:
                // add it to the buf
                g_serial_buf[g_buf_index] = c;
                g_buf_index = (g_buf_index + 1) % SERIAL_BUFSIZE;
                break;
        }
    }
}

template <typename T> void parse_line(T& the_device, char *the_line)
{
    constexpr size_t elem_count = 2;
    const char* tokens[2];
    memset(tokens, 0, sizeof(tokens));

    tokens[0] = strtok(the_line, ":");
    int arg = 0;
    uint16_t i = 1;

    // tokenize additional arguments
    for(; tokens[i - 1] && (i < elem_count); i++)
    {
         tokens[i] = strtok(nullptr, " ");
         if(tokens[i]){ arg = atoi(tokens[i]); }
    }

    if(strcmp(tokens[0], CMD_QUERY_ID) == 0)
    {
        char buf[32];
        sprintf(buf, "%s %s\n", CMD_QUERY_ID, DEVICE_ID);
        the_device.write((const uint8_t*)buf, strlen(buf));
    }
    else if(strcmp(tokens[0], CMD_RECV_DATA) == 0)
    {
        // Serial.print("about to receive: ");
        // Serial.print(arg);
        // Serial.println(" bytes");

        if(arg > 0)
        {
            size_t bytes_read = 0;

            while(bytes_read < arg)
            {
                bytes_read += the_device.readBytes((char*)g_path[0]->data() + bytes_read,
                                                   arg - bytes_read);
            }
            g_path[0]->strip()->show();
            g_run_mode = MODE_STREAMING;

            // start a timer to return <g_run_mode> to normal
            g_timer[TIMER_RUNMODE].expires_from_now(2.f);
        }
    }
    else if(strcmp(tokens[0], CMD_SEGMENT) == 0)
    {
        if(arg >= 0 && arg < g_path[0]->num_segments())
        {
            g_run_mode = MODE_DEBUG;

            for(uint32_t i = 0; i < g_path[0]->num_segments(); ++i)
            {
                g_path[0]->segment(i)->set_active(arg == i);
            }
            g_path[0]->segment(arg)->set_color(ORANGE);
            g_path[0]->update(0);
        }
        else
        {
            g_run_mode = MODE_RUNNING;
            for(uint8_t i = 0; i < g_num_paths; ++i){ g_mode_helper[i]->reset(); }
        }
    }
}
