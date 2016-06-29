#include <FastLED.h>

#define NUM_STRIPES 1
const uint8_t g_data_pins[NUM_STRIPES] = {12};
const uint16_t g_num_leds[NUM_STRIPES] = {110};

// LED definitions
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define LED_PIN_0 12

#define BRIGHTNESS 55   // 0-255

#define SERIAL_BUFSIZE 32
uint8_t g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
int g_update_interval = 16;
bool g_indicator;

class LEDStripe
{
  public:

    LEDStripe(uint32_t the_length):
    m_num_leds(the_length)
    {
      m_leds = new CRGB[the_length];

      for(uint32_t i = 0; i < m_num_leds; i++){ m_leds[i] = CRGB::Black; }
    };

    ~LEDStripe()
    {
      delete[](m_leds);
    };

    void set_color(const CRGB &the_color)
    {
      for(uint32_t i = 0; i < m_num_leds; i++){ m_leds[i] = the_color; }
      // FastLED.show();
    }

    CRGB* leds(){ return m_leds; }
    uint32_t num_leds(){ return m_num_leds; }

  private:

    uint32_t m_num_leds;

    //! this holds the entire pixel color values
    CRGB* m_leds;
};

LEDStripe g_stripes[NUM_STRIPES] =
{
  LEDStripe(300)
};

// void fill_with_palette(uint8_t the_index, uint8_t colorIndex)
// {
//   CRGB* leds = g_stripes[the_index].leds();
//   uint32_t num_leds = g_stripes[the_index].num_leds();
//
//   for( int i = 0; i < num_leds; i++)
//   {
//     leds[i] = ColorFromPalette(g_color_palette, colorIndex, BRIGHTNESS, g_current_blending);
//     colorIndex += STEPS;
//   }
// }

void setup()
{
  for(uint8_t i = 0; i < NUM_STRIPES; i++){ pinMode(g_data_pins[i], OUTPUT); }

  memset(g_serial_buf, 0, SERIAL_BUFSIZE);

  //for(uint32_t i = 0; i < NUM_STRIPES; i++)
  {
    FastLED.addLeds<LED_TYPE, LED_PIN_0, COLOR_ORDER>(g_stripes[0].leds(), g_stripes[0].num_leds());
  }
  FastLED.setBrightness(BRIGHTNESS);
  g_stripes[0].set_color(CRGB::Black);

  // initialize the serial communication:
  Serial.begin(57600);
}

////////////////////////////////////////////////////////////////////////

void update_stripes()
{
  FastLED.show();
}

////////////////////////////////////////////////////////////////////////

void loop()
{
  // time measurement
  int delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  if(g_time_accum >= g_update_interval)
  {
    g_time_accum = 0;
    g_indicator = !g_indicator;
    digitalWrite(13, g_indicator);
    update_stripes();
  }
  else{ delay(g_update_interval - g_time_accum); }
}


void serialEvent()
{
  while (Serial.available())
  {
    // get the new byte:
    uint8_t c = Serial.read();

    // if(c == '\0'){ continue; }

    // add it to the buf
    g_serial_buf[g_buf_index % SERIAL_BUFSIZE] = c;
    g_buf_index = (g_buf_index + 1) % SERIAL_BUFSIZE;

    // if the incoming character is a newline, set a flag
    // if (c == '\n' && g_buf_index >= 4)
    {
      CRGB col(c, c, c);// = c ? CRGB::White : CRGB::Black;
      // memcpy(&col, g_serial_buf, 3);
      g_stripes[0].set_color(col);

      g_buf_index = 0;
      memset(g_serial_buf, 0, SERIAL_BUFSIZE);
    }
  }
}
