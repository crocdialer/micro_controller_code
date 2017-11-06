#include <SPI.h>
#include "NetworkHelper.h"

#ifndef NO_WIFI
#include <WiFi101.h>
#endif

#ifndef NO_ETHERNET
#include <Ethernet2.h>
#endif

/////////////////////////////// NetworkHelper IMPL ///////////////////////////////////////////

const uint8_t NetworkHelper::s_default_mac[6] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
NetworkHelper* NetworkHelper::s_instance = nullptr;

// private constructor
NetworkHelper::NetworkHelper():
m_wifi_status(WL_IDLE_STATUS)
{
#ifndef NO_WIFI
    //Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8, 7, 4, 2);
    memset(m_wifi_clients, 0, sizeof(WiFiClient*) * m_max_num_clients);
#endif
}

NetworkHelper::~NetworkHelper()
{
#ifndef NO_WIFI
    for(int i = 0; i < m_max_num_clients; ++i)
    {
        if(m_wifi_clients[i]){ delete m_wifi_clients[i]; }
    }
#endif
}

NetworkHelper* NetworkHelper::get()
{
    if(!s_instance){ s_instance = new NetworkHelper(); }
    return s_instance;
}

void NetworkHelper::send_udp_broadcast(const char* the_string, uint16_t the_port)
{
#ifndef NO_ETHERNET
    if(m_has_ethernet)
    {
        m_ethernet_udp.beginPacket(m_broadcast_ip, the_port);
        m_ethernet_udp.write(the_string);
        m_ethernet_udp.endPacket();
    }
    else
#endif

#ifndef NO_WIFI
    if(m_wifi_status == WL_CONNECTED)
    {
        m_wifi_udp.beginPacket(m_broadcast_ip, the_port);
        m_wifi_udp.write(the_string);
        m_wifi_udp.endPacket();
    }
#endif
}

size_t NetworkHelper::write(const uint8_t* the_data, size_t the_num_bytes)
{
    uint32_t num_connections = 0;
    auto net_clients = connected_clients(&num_connections);
    size_t bytes_written = 0;

    for(uint32_t i = 0; i < num_connections; ++i)
    {
         auto ret = net_clients[i]->write(the_data, the_num_bytes);
         bytes_written = max(bytes_written, ret);
    }
    return bytes_written;
}

Client** NetworkHelper::connected_clients(uint32_t *the_num_clients)
{
    uint8_t num_clients = 0;

#ifndef NO_WIFI
    if(m_wifi_status == WL_CONNECTED)
    {
        for(uint8_t i = 0; i < m_max_num_clients; ++i)
        {
            Client* client = WiFi._client[i];

            if(client && client->connected())
            {
                m_clients_scratch[num_clients++] = client;
            }
        }
    }
#endif

#ifndef NO_ETHERNET
    if(m_has_ethernet)
    {
        for(int i = 0; i < m_max_num_clients; ++i)
        {
            Client* client = m_ethernet_clients + i;

            if(client && client->connected())
            {
                m_clients_scratch[num_clients++] = client;
            }
        }
    }
#endif

    if(the_num_clients){ *the_num_clients = num_clients; }
    return m_clients_scratch;
}

bool NetworkHelper::setup_ethernet(const uint8_t* the_mac_adress)
{
#ifndef NO_ETHERNET
    if(!the_mac_adress){ the_mac_adress = s_default_mac; }

    Serial.println("Attempting to connect to Ethernet ...");
    m_has_ethernet = Ethernet.begin((uint8_t*)the_mac_adress);

    if(!m_has_ethernet)
    {
        Serial.println("Failed to configure Ethernet using DHCP");
        // // initialize the ethernet device not using DHCP:
        // Ethernet.begin(mac, ip, gateway, subnet);
        return false;
    }
    m_local_ip = m_broadcast_ip = Ethernet.localIP();
    ((char*) &m_broadcast_ip)[3] = 0xFF;

    // start listening for clients
    m_ethernet_tcp.begin();
    m_ethernet_udp.begin(33334);
    return true;
#endif
    return false;
}

bool NetworkHelper::setup_wifi(const char** the_known_networks, uint8_t the_num_networks)
{
#ifndef NO_WIFI
    // check for the presence of the shield:
    if(WiFi.status() == WL_NO_SHIELD)
    {
        Serial.println("WiFi shield not present");
        return false;
    }

    // attempt to connect to WiFi network:
    if(m_wifi_status != WL_CONNECTED)
    {
        for(uint32_t i = 0; i < the_num_networks; ++i)
        {
            Serial.print("Attempting to connect to SSID: ");
            Serial.println(the_known_networks[2 * i]);

            // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
            m_wifi_status = WiFi.begin(the_known_networks[2 * i],
                                       the_known_networks[2 * i + 1]);

            // wait 3 seconds for connection:
            delay(3000);

            if(m_wifi_status == WL_CONNECTED){ break; }
        }
    }
    if(m_wifi_status == WL_CONNECTED)
    {
         m_tcp_server.begin();
         m_wifi_udp.begin(33334);
         m_local_ip = m_broadcast_ip = WiFi.localIP();
         ((char*) &m_broadcast_ip)[3] = 0xFF;
         return true;
    }
#endif
    return false;
}

void NetworkHelper::set_tcp_listening_port(uint16_t the_port)
{
#ifndef NO_WIFI
    if(m_wifi_status == WL_CONNECTED)
    {
        m_tcp_server = WiFiServer(the_port);
        m_tcp_server.begin();
    }
    else
#endif
    {
#ifndef NO_ETHERNET
        if(m_has_ethernet)
        {
            m_ethernet_tcp = EthernetServer(the_port);
            m_ethernet_tcp.begin();
        }
#endif
    }
}

void NetworkHelper::update_connections()
{
#ifndef NO_WIFI
    if(m_wifi_status == WL_CONNECTED)
    {
        uint8_t status = 0xFF;
        WiFiClient* new_connection = m_tcp_server.available(&status);

        // newly created connection
        if(new_connection && !status)
        {
            for(int i = 0; i < m_max_num_clients; ++i)
            {
                if(!m_wifi_clients[i] || !*m_wifi_clients[i] || !m_wifi_clients[i]->connected())
                {
                    if(m_wifi_clients[i]){ delete m_wifi_clients[i]; }
                    m_wifi_clients[i] = new_connection;
                    break;
                }
            }
        }
    }
#endif

#ifndef NO_ETHERNET
    if(m_has_ethernet)
    {
        // Ethernet
        EthernetClient connection = m_ethernet_tcp.available();

        if(connection)
        {
            bool is_new_connection = true;

            for(int i = 0; i < m_max_num_clients; ++i)
            {
                if(m_ethernet_clients[i] == connection)
                {
                    is_new_connection = false;
                    break;
                }
            }

            // find a slot for the new connection
            if(is_new_connection)
            {
                for(int i = 0; i < m_max_num_clients; ++i)
                {
                    if(!m_ethernet_clients[i] && m_ethernet_clients[i] != connection)
                    {
                        connection.flush();
                        m_ethernet_clients[i] = connection;
                        break;
                    }
                }
            }

            // reset dead connections
            for(int i = 0; i < m_max_num_clients; ++i)
            {
                if(!m_ethernet_clients[i].connected()){ m_ethernet_clients[i].stop(); }
            }
        }
    }
#endif
}
