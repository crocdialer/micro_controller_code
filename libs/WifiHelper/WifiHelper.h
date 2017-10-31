#ifndef __WIFI_HELPER__
#define __WIFI_HELPER__

#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>

class WifiHelper
{
public:

    //! singleton
    static WifiHelper* get();

    //!
    bool setup_wifi(const char** the_known_networks, uint8_t the_num_networks);

    //!
    WiFiClient** connected_clients(uint32_t *num_clients = nullptr);

    //!
    void update_connections();

    //!
    void send_udp_broadcast(const char* the_string, uint16_t the_port);

    //!
    void set_tcp_listening_port(uint16_t the_port);

    WiFiServer& tcp_server();

    WiFiUDP& udp_server();

private:

    static WifiHelper* s_instance;
    static constexpr uint8_t m_max_num_wifi_clients = 3;

    WifiHelper();

    // network key index (WEP)
    int m_wifi_key_index = 0;

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
};

#endif
