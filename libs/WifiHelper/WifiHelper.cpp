#include "WiFi101.h"
#include "WifiHelper.h"

/////////////////////////////// WifiHelper IMPL ///////////////////////////////////////////

WifiHelper* WifiHelper::s_instance = nullptr;

// private constructor
WifiHelper::WifiHelper(){}

WifiHelper* WifiHelper::get()
{
    if(!s_instance){ s_instance = new WifiHelper(); }
    return s_instance;
}

void WifiHelper::send_udp_broadcast(const char* the_string, uint16_t the_port)
{
    m_wifi_udp.beginPacket(m_broadcast_ip, the_port);
    m_wifi_udp.write(the_string);
    m_wifi_udp.endPacket();
}

WiFiClient** WifiHelper::connected_clients(uint32_t *the_num_clients)
{
    uint8_t num_clients = 0;

    for(uint8_t i = 0; i < TCP_SOCK_MAX; ++i)
    {
        WiFiClient* client = WiFi._client[i];

        if(client && client->connected())
        {
            m_wifi_clients_scratch[num_clients++] = client;
        }
    }
    if(the_num_clients){ *the_num_clients = num_clients; }
    return m_wifi_clients_scratch;
}

bool WifiHelper::setup_wifi(const char** the_known_networks, uint8_t the_num_networks)
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
    return false;
}

void WifiHelper::set_tcp_listening_port(uint16_t the_port)
{
    m_tcp_server = WiFiServer(the_port);
    m_tcp_server.begin();
}

void WifiHelper::update_connections()
{
    if(m_wifi_status == WL_CONNECTED)
    {
        uint8_t status = 255;
        auto new_connection = m_tcp_server.available(&status);

        if(new_connection)
        {
            // nothing to be done here ...
            // retrieve the connection later with the intrinsic voodoo of this shit implementation
        }
    }
}

WiFiServer& WifiHelper::tcp_server()
{
    return m_tcp_server;
}

WiFiUDP& WifiHelper::udp_server()
{
    return m_wifi_udp;
}
