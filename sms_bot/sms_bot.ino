/***************************************************
  FONA 800 based SMS Bot
  Read a push-button, send a SMS when button is pressed.
  Receive SMS to customize the message being sent and the target phone number.

  @author crocdialer@googlemail.com
*/

#include "Adafruit_FONA.h"

#define FONA_RX  9
#define FONA_TX  8
#define FONA_RST 4
#define FONA_RI  7

char g_serial_buf[256];

//for notifications from the FONA
char g_notification_buffer[64];

// this is a large buffer for replies
char g_reply_buffer[256];

// the currently used phone number to send messages to
char g_current_target_number[64];
char g_current_text_buffer[256];
const char* g_default_text = "just do it";
const char* g_current_text = g_default_text;

const uint8_t g_button_pin = 12, g_button_led_pin = 13;

// button/logic handling
bool g_button_pressed = false;
bool g_is_ready = true;
uint32_t g_blink_interval = 1000;
long g_timeout_ready = 1000 * 5;

// helper variables for time measurement
long g_last_time_stamp = 0;
uint32_t g_time_accum = 0, g_time_accum_button = 0;
uint32_t g_update_interval = 100;

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines
// and uncomment the HardwareSerial line
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *g_fona_serial = &fonaSS;

// Hardware serial is also possible!
 // HardwareSerial *g_fona_serial = &Serial1;

Adafruit_FONA g_fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

void blink_led()
{
    digitalWrite(g_button_led_pin, LOW);
    delay(200);
    digitalWrite(g_button_led_pin, HIGH);
    delay(200);
}

void setup()
{
    // push button
    pinMode(g_button_pin, INPUT_PULLUP);

    // LED
    pinMode(g_button_led_pin, OUTPUT);

    // while (!Serial);

    Serial.begin(115200);
    // Serial.println(F("FONA SMS caller ID test"));
    // Serial.println(F("Initializing....(May take 3 seconds)"));

    // make it slow so its easy to read!
    g_fona_serial->begin(4800);
    if(!g_fona.begin(*g_fona_serial))
    {
      Serial.println(F("Couldn't find FONA"));
      digitalWrite(g_button_led_pin, 1);
      while(1){ blink_led(); }
    }
    // Serial.println(F("FONA is OK"));

    // Print SIM card IMEI number.
    // char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
    // uint8_t imeiLen = g_fona.getIMEI(imei);
    // if(imeiLen > 0){ Serial.print("SIM card IMEI: "); Serial.println(imei); }
    //
    // Serial.println("FONA Ready");

}

void loop()
{
    // time measurement
    uint32_t delta_time = millis() - g_last_time_stamp;
    g_last_time_stamp = millis();
    g_time_accum += delta_time;
    g_time_accum_button += delta_time;

    // indicator LED
    bool light_led = g_is_ready && (g_last_time_stamp / g_blink_interval % 2);
    digitalWrite(g_button_led_pin, light_led);

    // button
    g_button_pressed = !digitalRead(g_button_pin);

    if(g_is_ready && g_button_pressed)
    {
        send_sms();
        g_time_accum_button = 0;
        g_is_ready = false;
    }

    char* bufPtr = g_notification_buffer;    //handy buffer pointer

    //any data available from the FONA?
    if(g_fona.available())
    {
        //this will be the slot number of the SMS
        int slot = 0;
        int charCount = 0;

        //Read the notification into g_notification_buffer
        do
        {
            *bufPtr = g_fona.read();
            // Serial.write(*bufPtr);
            delay(1);
        } while((*bufPtr++ != '\n') && (g_fona.available()) && (++charCount < (sizeof(g_notification_buffer)-1)));

        //Add a terminal NULL to the notification string
        *bufPtr = 0;

        //Scan the notification string for an SMS received notification.
        //  If it's an SMS message, we'll get the slot number in 'slot'
        if(1 == sscanf(g_notification_buffer, "+CMTI: \"SM\",%d", &slot))
        {
            Serial.print("slot: ");
            Serial.println(slot);

            char callerIDbuffer[32];  //we'll store the SMS sender number in here

            // Retrieve SMS sender address/phone number.
            if(!g_fona.getSMSSender(slot, callerIDbuffer, 31))
            {
                Serial.println("Didn't find SMS message in slot!");
            }
            Serial.print(F("FROM: "));
            Serial.println(callerIDbuffer);

            // retrieve SMS value.
            uint16_t smslen;
            if(g_fona.readSMS(slot, g_reply_buffer, 250, &smslen))
            {
                parse_input(g_reply_buffer);
            }
            else{ Serial.println("Failed!"); }

            if(!g_fona.deleteSMS(slot)){ Serial.println(F("could not delete sms")); }
        }
    }//g_fona.available()

    // process ready timeout
    if(g_time_accum_button >= g_timeout_ready){ g_is_ready = true; }

    if(g_time_accum >= g_update_interval)
    {
        process_serial_input(Serial);
        g_time_accum = 0;
    }
}

void send_sms()
{
    //Send back an automatic response
    Serial.println("sending message...");

    if(!g_fona.sendSMS(g_current_target_number, g_current_text))
    {
        Serial.println(F("Failed"));
    }
    else{ Serial.println(F("Sent!")); }
}

template <typename T> void process_serial_input(T& the_serial)
{
    uint16_t buf_idx = 0;

    while(the_serial.available())
    {
        // get the new byte:
        char c = the_serial.read();

        switch(c)
        {
            case '\r':
            case '\0':
                continue;

            case '\n':
                g_serial_buf[buf_idx] = '\0';
                buf_idx = 0;
                parse_input(g_serial_buf);
                break;

            default:
                g_serial_buf[buf_idx++] = c;
                break;
        }
    }
}

// we expect the format: "CMD_0:VALUE_0 CMD_N:VALUE_N ..."
void parse_input(char *the_line)
{
    Serial.println(the_line);
    const char* delim = ";";
    const size_t max_num_tokens = 3;
    char *token = strtok(the_line, delim);
    uint16_t num_tokens = 0;
    char* tokens[max_num_tokens];

    for(; token && (num_tokens < max_num_tokens); num_tokens++)
    {
        tokens[num_tokens] = token;
        token = strtok(nullptr, delim);
    }

    const size_t max_num = 2;
    num_tokens = 0;

    for(int i = 0; i < num_tokens; ++i)
    {
        // parse tokens as CMD::VALUE
        Serial.println(tokens[i]);

        char *cmd = strtok(tokens[i], ":");
        char *val = strtok(nullptr, ":");

        if(cmd && val){ process_cmd(cmd, val); }
    }
}

bool process_cmd(char* the_cmd, char* the_val)
{
    Serial.print("command: ");Serial.print(the_cmd);
    Serial.print(" - value: ");Serial.println(the_val);

    bool success = false;
    if(strcmp(the_cmd, "phone") == 0)
    {
        strcpy(g_current_target_number, the_val);
        success = true;
    }
    else if(strcmp(the_cmd, "time") == 0)
    {
        g_timeout_ready = atoi(the_val);
        success = true;
    }
    else if(strcmp(the_cmd, "text") == 0)
    {
        strcpy(g_current_text_buffer, the_val);
        g_current_text = g_current_text_buffer;
        success = true;
    }
    return success;
}
