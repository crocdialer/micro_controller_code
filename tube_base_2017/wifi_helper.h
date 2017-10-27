#ifndef __WIFI_HELPER__
#define __WIFI_HELPER__

#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#include "Timer.hpp"

#define CMD_QUERY_ID "ID"
#define DEVICE_ID "LEDS"

// // network SSID
constexpr uint32_t g_num_known_networks = 2;
const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "Sunrise_2.4GHz_BA25E8", "sJ4C257yyukZ",
    "egligeil2.4", "#LoftFlower!"
};

// network key index (WEP)
int g_wifi_key_index = 0;

int g_wifi_status = WL_IDLE_STATUS;

// TCP server
WiFiServer g_tcp_server(33333);

// TCP connection
constexpr uint8_t g_max_num_wifi_clients = 2;
uint8_t g_num_wifi_clients = 0;

WiFiClient g_wifi_clients[g_max_num_wifi_clients];

// UDP util
WiFiUDP g_wifi_udp;

// local ip address
uint32_t g_local_ip;

// udp-broadcast
uint32_t g_broadcast_ip;
uint16_t g_broadcast_port = 55555;
constexpr float g_broadcast_interval = 2.f;

kinski::Timer *g_wifi_timer = nullptr;

void send_udp_broadcast()
{
    g_wifi_udp.beginPacket(g_broadcast_ip, g_broadcast_port);
    g_wifi_udp.write(DEVICE_ID);
    g_wifi_udp.endPacket();
}

bool setup_wifi(kinski::Timer *timer)
{
    if(g_wifi_timer){ g_wifi_timer->cancel(); }
    g_wifi_timer = timer;

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
        for(uint32_t i = 0; i < g_num_known_networks; ++i)
        {
            Serial.print("Attempting to connect to SSID: ");
            Serial.println(g_wifi_known_networks[2 * i]);

            // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
            g_wifi_status = WiFi.begin(g_wifi_known_networks[2 * i],
                                       g_wifi_known_networks[2 * i + 1]);

            // wait 10 seconds for connection:
            delay(10000);

            if(g_wifi_status == WL_CONNECTED){ break; }
        }
    }
    if(g_wifi_status == WL_CONNECTED)
    {
         g_tcp_server.begin();
         g_wifi_udp.begin(33334);
         g_local_ip = g_broadcast_ip = WiFi.localIP();
         ((char*) &g_broadcast_ip)[3] = 0xFF;

         g_wifi_timer->expires_from_now(g_broadcast_interval);
         g_wifi_timer->set_periodic();
         g_wifi_timer->set_callback(&send_udp_broadcast);

         return true;
    }
    return false;
}

void update_connections()
{
    if(g_wifi_status == WL_CONNECTED)
    {
        auto new_connection = g_tcp_server.available();

        if(new_connection)
        {
            // g_wifi_timer->cancel();
            bool slot_available = false;

            for(uint8_t i = 0; i < g_max_num_wifi_clients; ++i)
            {
                if(!g_wifi_clients[i].connected())
                {
                     g_wifi_clients[i] = new_connection;
                     slot_available = true;
                     break;
                }
            }

            if(!slot_available)
            {
                 Serial.println("could not connect tcp-client: max num connections hit");
            }
        }

        // if(g_wifi_client.connected())
        // {
        //     //  g_wifi_client.write(g_serial_buf);
        // }
        // else if(g_wifi_client)
        // {
        //     g_wifi_client = WiFiClient();
        //     g_wifi_timer->expires_from_now(g_broadcast_interval);
        // }
    }
}

#endif
