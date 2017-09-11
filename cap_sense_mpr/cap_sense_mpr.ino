#include <Wire.h>
#include "Adafruit_MPR121.h"
#include "RunningMedian.h"
#include "utils.h"
#include "Timer.hpp"

#define USE_WIFI

// #define USE_BLUETOOTH

const int g_update_interval = 33;
char g_serial_buf[512];

//! time management
const int g_update_interval_params = 2000;
int g_time_accum = 0, g_time_accum_params = 0;
long g_last_time_stamp;

constexpr uint32_t g_num_timers = 1;
kinski::Timer g_timer[g_num_timers];

enum TimerEnum{TIMER_UDP_BROADCAST = 0};

#define CMD_QUERY_ID "ID"
#define CMD_START "START"
#define CMD_STOP "STOP"
#define CMD_RESET "RESET"
#define DEVICE_ID "CAPACITIVE_SENSOR"

// bluetooth communication
#ifdef USE_BLUETOOTH
    #include "Adafruit_BluefruitLE_SPI.h"
    #define BLUEFRUIT_SPI_CS 8
    #define BLUEFRUIT_SPI_IRQ 7
    #define BLUEFRUIT_SPI_RST 4    // Optional but recommended, set to -1 if unused
    Adafruit_BluefruitLE_SPI g_bt_serial(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

    const int g_update_interval_bt = 100;
    int g_time_accum_bt = 0;
#endif

#ifdef USE_WIFI
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

// // network SSID
const char* g_wifi_ssid = "egligeil2.4";

// network pw
const char* g_wifi_pw = "#LoftFlower!";

// // network SSID
// const char* g_wifi_ssid = "Sunrise_2.4GHz_BA25E8";
//
// // network pw
// const char* g_wifi_pw = "sJ4C257yyukZ";

// network key index (WEP)
int g_wifi_key_index = 0;

int g_wifi_status = WL_IDLE_STATUS;

// TCP server
WiFiServer g_tcp_server(33333);

// TCP connection
WiFiClient g_wifi_client;

// UDP util
WiFiUDP g_wifi_udp;

// local ip address
uint32_t g_local_ip;

// udp-broadcast
uint32_t g_broadcast_ip;
uint16_t g_broadcast_port = 55555;
constexpr float g_broadcast_interval = 2.f;

void send_udp_broadcast()
{
    g_wifi_udp.beginPacket(g_broadcast_ip, g_broadcast_port);
    g_wifi_udp.write(DEVICE_ID);
    g_wifi_udp.endPacket();
}

bool setup_wifi()
{
    //Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8, 7, 4, 2);

    // check for the presence of the shield:
    if(WiFi.status() == WL_NO_SHIELD)
    {
        Serial.println("WiFi shield not present");
        return false;
    }

    // attempt to connect to WiFi network:
    if(g_wifi_status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(g_wifi_ssid);

        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        g_wifi_status = WiFi.begin(g_wifi_ssid, g_wifi_pw);

        // wait 10 seconds for connection:
        delay(10000);
    }
    if(g_wifi_status == WL_CONNECTED)
    {
         g_tcp_server.begin();
         g_wifi_udp.begin(33334);
         g_local_ip = g_broadcast_ip = WiFi.localIP();
         ((char*) &g_broadcast_ip)[3] = 0xFF;

         g_timer[TIMER_UDP_BROADCAST].expires_from_now(g_broadcast_interval);
         g_timer[TIMER_UDP_BROADCAST].set_periodic();
         g_timer[TIMER_UDP_BROADCAST].set_callback(&send_udp_broadcast);

         return true;
    }
    return false;
}
#endif

bool g_bt_initialized = false;

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

bool has_uart()
{
    #ifdef USE_BLUETOOTH
        // bluetooth config
        if(g_bt_initialized || g_bt_serial.begin(false))
        {
            g_bt_serial.echo(false);
            return true;
        }
    #endif
    #ifdef USE_WIFI
        if(g_wifi_status == WL_CONNECTED){ return true; }
        else if(setup_wifi()){ return true; }
    #endif
    return Serial;
}

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
    while(!has_uart()){ blink_status_led(); }

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

    #ifdef USE_BLUETOOTH
    g_time_accum_bt += delta_time;
    #endif

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

        Serial.write(g_serial_buf);

// #ifdef USE_BLUETOOTH
//         if((g_time_accum_bt >= g_update_interval_bt) && g_bt_serial.isConnected())
//         {
//             if(!g_bt_initialized)
//             {
//                 g_bt_serial.sendCommandCheckOK("AT+HWModeLED=MODE");
//                 g_bt_serial.setMode(BLUEFRUIT_MODE_DATA);
//                 g_bt_initialized = true;
//             }
//             g_time_accum_bt = 0;
//             g_bt_serial.write(g_serial_buf);
//             // g_bt_serial.flush();
//         }else if(g_bt_initialized){ g_bt_initialized = false; }
// #endif

#ifdef USE_WIFI
        auto new_connection = g_tcp_server.available();

        if(new_connection)
        {
            g_timer[TIMER_UDP_BROADCAST].cancel();
            g_wifi_client = new_connection;
        }

        if(g_wifi_client.connected()){ g_wifi_client.write(g_serial_buf); }
        else if(g_wifi_client)
        {
            g_wifi_client = WiFiClient();
            g_timer[TIMER_UDP_BROADCAST].expires_from_now(g_broadcast_interval);
        }
#endif

    }
    if(g_time_accum_params > g_update_interval_params)
    {
        process_input(Serial);
        #ifdef USE_BLUETOOTH
            process_input(g_bt_serial);
        #endif
        #ifdef USE_WIFI
            process_input(g_wifi_client);
        #endif
        g_time_accum_params = 0;
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
            //  sprintf(g_serial_buf, "touch: %d -- release: %d\n", num_buf[0], num_buf[1]);
            //  Serial.print(g_serial_buf);
        }
    }
}
