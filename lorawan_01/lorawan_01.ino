/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the The Things Network. It's pre-configured for the Adafruit
 * Feather M0 LoRa.
 *
 * This uses OTAA (Over-the-air activation), where where a DevEUI and
 * application key is configured, which are used in an over-the-air
 * activation procedure where a DevAddr and session keys are
 * assigned/generated for use with all further communication.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!

 * To use this sketch, first register your application and device with
 * the things network, to set or generate an AppEUI, DevEUI and AppKey.
 * Multiple devices can use the same AppEUI, but each device has its own
 * DevEUI and AppKey.
 *
 * Do not forget to define the radio type correctly in
 * arduino-lmic/project_config/lmic_project_config.h or from your BOARDS.txt.
 *
 *******************************************************************************/

#include <Scheduler.h>

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "project_config.h"
#include "payload.h"
#include <Adafruit_NeoPixel_ZeroDMA.h>

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t g_app_eui[8]= { 0x6F, 0xB0, 0x01, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getArtEui (u1_t* buf) { memcpy(buf, g_app_eui, 8);}

// This should also be in little endian format, see above.
static const u1_t DEVEUI[8]= { 0x19, 0x51, 0x1F, 0x45, 0xE8, 0x84, 0x59, 0x00 } ;
void os_getDevEui (u1_t* buf) { memcpy(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from the TTN console can be copied as-is.
static const u1_t APPKEY[16] = { 0x2E, 0xA3, 0xD1, 0x87, 0xA0, 0x21, 0xE6, 0xC1, 0x4B, 0x5B, 0x1F, 0xC3, 0xF8, 0x70, 0xD8, 0x35 };
void os_getDevKey (u1_t* buf) {  memcpy(buf, APPKEY, 16);}

static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 15;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 5,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 6,
    .dio = {9, 10, LMIC_UNUSED_PIN}
};
// const lmic_pinmap lmic_pins = {
//     .nss = 7,
//     .rxtx = LMIC_UNUSED_PIN,
//     .rst = 9,
//     .dio = {10, 11, LMIC_UNUSED_PIN}
// };

payload_t g_state = {};
constexpr uint8_t g_num_leds = 8;
constexpr uint8_t g_led_pin = 11;
Adafruit_NeoPixel_ZeroDMA g_strip{g_num_leds, g_led_pin, NEO_RGBW + NEO_KHZ800};

uint32_t g_last_rx_stamp = 0;

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));

            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));

            // ACK ?
            if(LMIC.txrxFlags & TXRX_ACK){ Serial.println(F("Received ack")); }

            // received downlink-data (RX) ?
            if(LMIC.dataLen){ process_rx((uint8_t*)LMIC.frame + LMIC.dataBeg, LMIC.dataLen); }

            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, (uint8_t*)&g_state, sizeof(payload_t), 0);
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup()
{
    while(!Serial && millis() < 5000){}

    Serial.begin(9600);
    Serial.println(F("Starting"));

    pinMode(13, OUTPUT);
    digitalWrite(13, 0);

    // believe it or not, using this once will screw LMIC completely
    // auto bat = map(analogRead(A7), 0, 1023, 0, 255);

    // LMIC init
    os_init();

    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // LMIC_setDrTxpow(DR_SF7, 23);

    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);

    // led control loop
    Scheduler.start(nullptr, blink_loop);

    // // battery measure loop
    // Scheduler.start(nullptr, measure_battery);

    g_strip.begin();
}

void loop()
{
    os_runloop_once();
    yield();
}

void measure_battery()
{
    // can't do, seems like analogRead has a side-effects with LMIC
    g_state.battery = map(analogRead(A7), 0, 1023, 0, 255);
    delay(10000);
}

void blink_loop()
{
    if(millis() - g_last_rx_stamp < 10000)
    {
        uint8_t max_num_leds = min(g_state.num_leds, g_num_leds);

        for(uint8_t i = 0; i < max_num_leds; ++i)
        {
            g_strip.setPixelColor(i, g_state.color[0], g_state.color[1],
                                  g_state.color[2], g_state.color[3]);
            g_strip.setBrightness(g_state.brightness);
        }
        g_strip.show();
        delay(500);
    }

    memset(g_strip.getPixels(), 0, 4 * g_num_leds);
    g_strip.show();
    delay(500);
}

void process_rx(uint8_t *data, size_t num_bytes)
{
    g_last_rx_stamp = millis();

    Serial.print("received ");
    Serial.print(num_bytes);
    Serial.print(" bytes\n");

    Serial.print("data: ");

    for(size_t i = 0; i < num_bytes; ++i)
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if(num_bytes >= sizeof(payload_t) - 1)
    {
        Serial.println("downlink: new payload received");

        // do not copy battery state, so skip last byte
        memcpy(&g_state, data, sizeof(payload_t) - 1);
    }
}
