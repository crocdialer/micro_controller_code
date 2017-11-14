#ifndef __WIFI_HELPER__
#define __WIFI_HELPER__
#define NO_ETHERNET

#ifndef NO_ETHERNET
#include <EthernetServer.h>
#include <EthernetClient.h>
#include <EthernetUdp2.h>
#endif

#ifndef NO_WIFI
#include <WiFi101.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#endif

class NetworkHelper
{
public:

    static const uint8_t s_default_mac[6];

    //! singleton
    static NetworkHelper* get();

    //!
    bool setup_ethernet(const uint8_t* the_mac_adress = nullptr);

    //!
    bool setup_wifi(const char** the_known_networks, uint8_t the_num_networks);

    //!
    Client** connected_clients(uint32_t *num_clients = nullptr);

    //! send data to all connected connected clients
    size_t write(const uint8_t* the_data, size_t the_num_bytes);

    //! send a null-terminated c-string to all connected clients
    inline size_t write(const char* the_string)
    {
        return write((const uint8_t*)the_string, strlen(the_string));
    }

    //! update connections for all interfaces
    //  needs to be called periodically
    void update_connections();

    //!
    void send_udp_broadcast(const char* the_string, uint16_t the_port);

    //!
    void set_tcp_listening_port(uint16_t the_port);

private:

    static NetworkHelper* s_instance;
    static constexpr uint8_t s_max_num_clients = 7;

    NetworkHelper();

    // network key index (WEP)
    int m_wifi_key_index = 0;

    int m_wifi_status;

    // local ip address
    uint32_t m_local_ip;

    // udp-broadcast
    uint32_t m_broadcast_ip;

#ifndef NO_WIFI
    // TCP server
    WiFiServer m_tcp_server{33333};

    // client objects
    WiFiClient m_wifi_clients[s_max_num_clients];

    // UDP util
    WiFiUDP m_wifi_udp;
#endif

#ifndef NO_ETHERNET
    ////// Ethernet assets ///////////
    EthernetServer m_ethernet_tcp{33333};
    EthernetUDP m_ethernet_udp;
    EthernetClient m_ethernet_clients[s_max_num_clients];
#endif
    // scratch space for refs to active TCP connections
    Client* m_clients_scratch[2 * s_max_num_clients];

    bool m_has_ethernet = false;
};

#endif
