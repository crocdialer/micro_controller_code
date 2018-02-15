#include "ModeHelpers.h"
#include "Timer.hpp"
#include "device_id.h"

#define USE_NETWORK
#define NO_ETHERNET
// #define NO_WIFI

#ifdef USE_NETWORK
#include "NetworkHelper.h"

// Ethernet MAC adress
uint8_t g_mac_adress[6] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x69};

// network SSID
static constexpr uint32_t g_num_known_networks = 3;
static const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "whoopy", "senftoast",
    "egligeil2.4", "#LoftFlower!"
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

constexpr uint8_t g_num_paths = 2;
constexpr uint8_t g_path_lengths[] = {7, 7};
const uint8_t g_led_pins[] = {11, 5};

LED_Path* g_path[g_num_paths];
ModeHelper *g_mode_sinus = nullptr, *g_mode_colour = nullptr, *g_mode_current = nullptr;
CompositeMode *g_mode_composite = nullptr;

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
        uint32_t num_connections = 0;
        auto net_clients = g_net_helper->connected_clients(&num_connections);
        for(uint8_t i = 0; i < num_connections; ++i){ process_input(*net_clients[i]); }
#endif

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
    const char* cmd_token = strtok(the_line, ":");

    if(strcmp(cmd_token, CMD_QUERY_ID) == 0)
    {
        char buf[32];
        sprintf(buf, "%s %s\n", CMD_QUERY_ID, DEVICE_ID);
        the_device.write((const uint8_t*)buf, strlen(buf));
        return;
    }
    else if(strcmp(cmd_token, CMD_RECV_DATA) == 0)
    {
        const char* arg_str = strtok(nullptr, " ");
        uint32_t num_bytes = 0;
        if(arg_str){ num_bytes = atoi(arg_str); }

        for(size_t i = 0; i < g_num_paths; i++)
        {
            size_t bytes_read = 0;
            size_t num_path_bytes = min(num_bytes, g_path[i]->num_bytes());
            num_bytes -= num_path_bytes;

            while(bytes_read < num_path_bytes)
            {
                bytes_read += the_device.readBytes((char*)g_path[i]->data() + bytes_read,
                                                   num_path_bytes - bytes_read);
            }
        }

        for(size_t i = 0; i < g_num_paths; i++){ g_path[i]->strip()->show(); }
        g_run_mode = MODE_STREAMING;

        // start a timer to return <g_run_mode> to normal
        g_timer[TIMER_RUNMODE].expires_from_now(2.f);
        return;
    }
    else if(strcmp(cmd_token, CMD_SEGMENT) == 0)
    {
        const char* arg_str = nullptr;

        // disable all segments
        for (size_t p = 0; p < g_num_paths; p++)
        {
            for(uint32_t i = 0; i < g_path[p]->num_segments(); ++i)
            {
                g_path[p]->segment(i)->set_active(false);
            }
        }

        while((arg_str = strtok(nullptr, " ")))
        {
            int index = atoi(arg_str);

            if(index >= 0 && index < g_path[0]->num_segments())
            {
                g_run_mode = MODE_DEBUG;
                g_mode_current = g_mode_sinus;
                Segment *s = g_path[0]->segment(index);
                s->set_active(true);
                s->set_color(ORANGE);
            }
            else
            {
                g_run_mode = MODE_RUNNING;
                g_mode_current = g_mode_composite;
                for(uint8_t i = 0; i < g_num_paths; ++i){ g_mode_current->reset(g_path[i]); }
                break;
            }
        }
        for(size_t p = 0; p < g_num_paths; p++){ g_path[p]->update(0); }
        return;
    }
    else if(strcmp(cmd_token, CMD_BRIGHTNESS) == 0)
    {
        const char* arg_str = strtok(nullptr, " ");
        float val = g_path[0]->brightness();

        if(arg_str)
        {
            val = clamp<float>(atof(arg_str), 0.f, 1.f);
            g_path[0]->set_brightness(val);
        }
    }
}
