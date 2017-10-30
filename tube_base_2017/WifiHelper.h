#ifndef __WIFI_HELPER__
#define __WIFI_HELPER__

#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

#include "Timer.hpp"
#include "device_id.h"

// // network SSID
static constexpr uint32_t g_num_known_networks = 2;
static const char* g_wifi_known_networks[2 * g_num_known_networks] =
{
    "egligeil2.4", "#LoftFlower!",
    "Sunrise_2.4GHz_BA25E8", "sJ4C257yyukZ",
};

class WifiHelper
{
public:
    bool setup_wifi(kinski::Timer *timer = nullptr);
    WiFiClient** connected_clients(uint32_t *num_clients = nullptr);
    void update_connections();

    static WifiHelper* get();
    ~WifiHelper();
    void send_udp_broadcast(const char* the_string);

private:

    static WifiHelper* s_instance;
    static constexpr uint8_t m_max_num_wifi_clients = 2;

    WifiHelper(){};

    // network key index (WEP)
    int g_wifi_key_index = 0;

    int m_wifi_status = WL_IDLE_STATUS;

    // TCP server
    WiFiServer m_tcp_server{33333};

    // TCP connection
    uint8_t m_num_wifi_clients = 0;
    WiFiClient m_wifi_clients[m_max_num_wifi_clients];
    WiFiClient* m_wifi_clients_scratch[m_max_num_wifi_clients];

    // UDP util
    WiFiUDP m_wifi_udp;

    // local ip address
    uint32_t m_local_ip;

    // udp-broadcast
    uint32_t m_broadcast_ip;
    uint16_t m_broadcast_port = 55555;
    float m_broadcast_interval = 2.f;

    bool m_owns_timer = false;
    kinski::Timer *m_wifi_timer = nullptr;
};

#endif
