#ifndef __WIFI_HELPER__
#define __WIFI_HELPER__

// #include <EthernetServer.h>
// #include <EthernetClient.h>
// #include <EthernetUdp2.h>

#include <WiFi101.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

class NetworkHelper
{
public:

    static const uint8_t s_default_mac[6];

    //! singleton
    static NetworkHelper* get();

    ~NetworkHelper();

    //!
    // bool setup_ethernet(const uint8_t* the_mac_adress = nullptr);

    //!
    bool setup_wifi(const char** the_known_networks, uint8_t the_num_networks);

    //!
    Client** connected_clients(uint32_t *num_clients = nullptr);

    //!
    void update_connections();

    //!
    void send_udp_broadcast(const char* the_string, uint16_t the_port);

    //!
    void set_tcp_listening_port(uint16_t the_port);

    WiFiServer& tcp_server();

    WiFiUDP& udp_server();

private:

    static NetworkHelper* s_instance;
    static constexpr uint8_t m_max_num_clients = 7;

    NetworkHelper();

    // network key index (WEP)
    int m_wifi_key_index = 0;

    int m_wifi_status;

    // TCP server
    WiFiServer m_tcp_server{33333};

    // client objects
    WiFiClient* m_wifi_clients[m_max_num_clients];

    // UDP util
    WiFiUDP m_wifi_udp;

    // local ip address
    uint32_t m_local_ip;

    // udp-broadcast
    uint32_t m_broadcast_ip;

    ////// Ethernet assets ///////////
    // EthernetServer m_ethernet_tcp{33333};
    // EthernetUDP m_ethernet_udp;
    // EthernetClient m_ethernet_clients[m_max_num_clients];

    // scratch space for refs to active TCP connections
    Client* m_clients_scratch[2 * m_max_num_clients];

    bool m_has_ethernet = false;
};

#endif
