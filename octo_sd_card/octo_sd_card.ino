/*  OctoWS2811 VideoSDcard.ino - Video on LEDs, played from SD Card
    http://www.pjrc.com/teensy/td_libs_OctoWS2811.html
    Copyright (c) 2014 Paul Stoffregen, PJRC.COM, LLC

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

Update: The programs to prepare the SD card video file have moved to "extras"
https://github.com/PaulStoffregen/OctoWS2811/tree/master/extras

  The recommended hardware for SD card playing is:

    Teensy 3.1:     http://www.pjrc.com/store/teensy31.html
    Octo28 Apaptor: http://www.pjrc.com/store/octo28_adaptor.html
    SD Adaptor:     http://www.pjrc.com/store/wiz820_sd_adaptor.html
    Long Pins:      http://www.digikey.com/product-search/en?keywords=S1082E-36-ND

  See the included "hardware.jpg" image for suggested pin connections,
  with 2 cuts and 1 solder bridge needed for the SD card pin 3 chip select.

  Required Connections
  --------------------
    pin 2:  LED Strip #1    OctoWS2811 drives 8 LED Strips.
    pin 14: LED strip #2    All 8 are the same length.
    pin 7:  LED strip #3
    pin 8:  LED strip #4    A 100 to 220 ohm resistor should used
    pin 6:  LED strip #5    between each Teensy pin and the
    pin 20: LED strip #6    wire to the LED strip, to minimize
    pin 21: LED strip #7    high frequency ringining & noise.
    pin 5:  LED strip #8
    pin 15 & 16 - Connect together, but do not use
    pin 4:  Do not use

    pin 3:  SD Card, CS
    pin 11: SD Card, MOSI
    pin 12: SD Card, MISO
    pin 13: SD Card, SCLK
*/

#include <OctoWS2811.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <Wire.h>

#define LED_WIDTH    60   // number of LEDs horizontally
#define LED_HEIGHT   32   // number of LEDs vertically (must be multiple of 8)

const uint8_t g_button_pin = 17, g_poti_pin = 18;
uint16_t g_pot_val = 0;

const uint8_t g_num_files = 3;
const char* g_file_names[] = {"TEST1.BIN", "TEST2.BIN", "TEST3.BIN"};
volatile int g_current_index = 0;

// SD card reading
File g_current_file;
uint8_t g_sd_buffer[512];
uint32_t g_sd_buf_pos = 0;
uint32_t g_sd_buf_len = 0;

char g_serial_buf[512];

const int ledsPerStrip = LED_WIDTH * LED_HEIGHT / 8;
DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];
elapsedMicros elapsedSinceLastFrame = 0;
bool playing = false;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, WS2811_800kHz);

AudioPlayQueue     audio;
AudioOutputAnalog  dac;
AudioConnection    patchCord1(audio, dac);

// interrupt service routine for button press
void button_isr()
{
    cli();
    if(playing)
    {
        playing = false;
        Serial.println(g_pot_val);
        g_current_index = (g_current_index + 1) % g_num_files;
    }
    sei();
}

void open_file(const char* the_file_name)
{
    g_current_file.close();
    g_current_file = SD.open(the_file_name, FILE_READ);
    g_sd_buf_pos = g_sd_buf_len = 0;

    if(!g_current_file)
    {
        sprintf(g_serial_buf, "could not open file: %s", the_file_name);
        stopWithErrorMessage(g_serial_buf);
    }
    sprintf(g_serial_buf, "file opened: %s", the_file_name);
    Serial.println(g_serial_buf);
    playing = true;
    elapsedSinceLastFrame = 0;
}

void setup()
{
    pinMode(g_button_pin, INPUT_PULLUP);
    attachInterrupt(g_button_pin, button_isr, FALLING);
    pinMode(g_poti_pin, INPUT);

    AudioMemory(40);
    //while (!Serial) ;
    delay(50);
    Serial.println("VideoSDcard");
    leds.begin();
    leds.show();
    if (!SD.begin(3)) stopWithErrorMessage("Could not access SD card");
    Serial.println("SD card ok");
    open_file(g_file_names[g_current_index]);
}

// read from the SD card, true=ok, false=unable to read
// the SD library is much faster if all reads are 512 bytes
// this function lets us easily read any size, but always
// requests data from the SD library in 512 byte blocks.
//
bool sd_card_read(void *ptr, unsigned int len)
{
    unsigned char *dest = (unsigned char *)ptr;
    unsigned int n;

    while (len > 0)
    {
        if(g_sd_buf_len == 0)
        {
            n = g_current_file.read(g_sd_buffer, 512);
            if (n == 0) return false;
            g_sd_buf_len = n;
            g_sd_buf_pos = 0;
        }
        unsigned int n = g_sd_buf_len;
        if (n > len) n = len;
        memcpy(dest, g_sd_buffer + g_sd_buf_pos, n);
        dest += n;
        g_sd_buf_pos += n;
        g_sd_buf_len -= n;
        len -= n;
    }
    return true;
}

// skip past data from the SD card
void sd_card_skip(unsigned int len)
{
    unsigned char buf[256];

    while (len > 0)
    {
        unsigned int n = len;
        if (n > sizeof(buf)) n = sizeof(buf);
        sd_card_read(buf, n);
        len -= n;
    }
}

void loop()
{
    unsigned char header[5];

    // read current poti value
    g_pot_val = analogRead(g_poti_pin);

    if(playing)
    {
        if(sd_card_read(header, 5))
        {
            if (header[0] == '*')
            {
                // found an image frame
                unsigned int size = (header[1] | (header[2] << 8)) * 3;
                unsigned int usec = header[3] | (header[4] << 8);
                unsigned int readsize = size;
                // Serial.printf("v: %u %u", size, usec);
                if(readsize > sizeof(drawingMemory))
                {
                    readsize = sizeof(drawingMemory);
                }
                if(sd_card_read(drawingMemory, readsize))
                {
                    // Serial.printf(", us = %u", (unsigned int)elapsedSinceLastFrame);
                    // Serial.println();
                    while (elapsedSinceLastFrame < usec) ; // wait
                    elapsedSinceLastFrame -= usec;
                    leds.show();
                }
                else
                {
                    error("unable to read video frame data");
                    return;
                }
                if(readsize < size)
                {
                    sd_card_skip(size - readsize);
                }
            }
            else if(header[0] == '%')
            {
                // found a chunk of audio data
                unsigned int size = (header[1] | (header[2] << 8)) * 2;
                // Serial.printf("a: %u", size);
	            // Serial.println();

	            while (size > 0)
                {
	                unsigned int len = size;
	                if(len > 256){ len = 256; }
	                int16_t *p = audio.getBuffer();
	                if(!sd_card_read(p, len))
                    {
	                       error("unable to read audio frame data");
                           return;
	                }
	                if(len < 256)
                    {
                        for (int i=len; i < 256; i++)
                        {
                            *((char *)p + i) = 0;  // fill rest of buffer with zero
                        }
	                }
                    audio.playBuffer();
	                size -= len;
	            }
            }
            else{ error("unknown header"); return; }
        }
        else{ error("unable to read 5-byte header"); return; }
    }
    else
    {
        delay(1000);
        open_file(g_file_names[g_current_index]);
    }
}

// when any error happens during playback, close the file and restart
void error(const char *str)
{
    Serial.print("error: ");
    Serial.println(str);
    g_current_file.close();
    playing = false;
}

// when an error happens during setup, give up and print a message
// to the serial monitor.
void stopWithErrorMessage(const char *str)
{
    while (1)
    {
        Serial.println(str);
        delay(1000);
    }
}
