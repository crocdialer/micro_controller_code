#include "WifiHelper.h"

/////////////////////////////// WifiHelper IMPL ///////////////////////////////////////////

// wrapper function
void send_udp_broadcast(){ WifiHelper::get()->send_udp_broadcast(DEVICE_ID); }

WifiHelper* WifiHelper::s_instance = nullptr;

WifiHelper* WifiHelper::get()
{
    if(!s_instance){ s_instance = new WifiHelper(); }
    return s_instance;
}

WifiHelper::~WifiHelper()
{
    if(m_wifi_timer && m_owns_timer){ delete m_wifi_timer; }
}

void WifiHelper::send_udp_broadcast(const char* the_string)
{
    m_wifi_udp.beginPacket(m_broadcast_ip, m_broadcast_port);
    m_wifi_udp.write(the_string);
    m_wifi_udp.endPacket();
}

WiFiClient** WifiHelper::connected_clients(uint32_t *num_clients)
{
    if(num_clients){ *num_clients = m_num_wifi_clients; }
    return m_wifi_clients_scratch;
}

bool WifiHelper::setup_wifi(kinski::Timer *timer)
{
    if(m_wifi_timer){ m_wifi_timer->cancel(); }

    if(timer)
    {
        if(m_wifi_timer && m_owns_timer){ delete m_wifi_timer; m_owns_timer = false; }
        m_wifi_timer = timer;
    }
    else
    {
        m_owns_timer = true;
        m_wifi_timer = m_wifi_timer ? m_wifi_timer : new kinski::Timer();
    }

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
        for(uint32_t i = 0; i < g_num_known_networks; ++i)
        {
            // Serial.print("Attempting to connect to SSID: ");
            // Serial.println(g_wifi_known_networks[2 * i]);

            // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
            m_wifi_status = WiFi.begin(g_wifi_known_networks[2 * i],
                                       g_wifi_known_networks[2 * i + 1]);

            // wait 10 seconds for connection:
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

         m_wifi_timer->expires_from_now(m_broadcast_interval);
         m_wifi_timer->set_periodic();
         m_wifi_timer->set_callback(&::send_udp_broadcast);

         return true;
    }
    return false;
}

void WifiHelper::update_connections()
{
    if(m_wifi_status == WL_CONNECTED)
    {
        uint8_t status = 255;
        auto new_connection = m_tcp_server.available(&status);

        if(new_connection)
        {
            // Serial.println("WifiHelper: new connection");

            bool has_empty_slot = false;

            for(uint8_t i = 0; i < m_max_num_wifi_clients; ++i)
            {
                if(!m_wifi_clients[i].connected())
                {
                     m_wifi_clients[i] = new_connection;
                     has_empty_slot = true;
                     break;
                }
            }

            if(!has_empty_slot)
            {
                // m_wifi_timer->cancel();
                // Serial.println("WifiHelper: could not connect tcp-client: max num connections hit");
            }
        }
    }
    uint8_t num_connections = 0;

    for(uint8_t i = 0; i < m_max_num_wifi_clients; ++i)
    {
        if(m_wifi_clients[i].connected())
        {
            ++num_connections;
            m_wifi_clients_scratch[i] = &m_wifi_clients[i];
        }
    }
    m_num_wifi_clients = num_connections;
}
