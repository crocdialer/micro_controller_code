#include <FastLED.h>

#define NUM_STRIPES 5
const uint8_t g_data_pins[NUM_STRIPES] = {12, 11, 10, 9, 8};

const uint16_t g_num_leds[NUM_STRIPES] = {240, 300, 350, 400, 450};

// LED definitions
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define LED_PIN_0 12
#define LED_PIN_1 11
#define LED_PIN_2 10
#define LED_PIN_3 9
#define LED_PIN_4 8

#define BRIGHTNESS 200   // 0-255
#define STEPS        1   // How wide the bands of color are.  1 = more like a gradient, 10 = more like stripes

CRGBPalette16 g_color_palette = HeatColors_p;//PartyColors_p;
TBlendType g_current_blending = BLEND;
uint8_t g_start_color_index = 0;

#define SERIAL_BUFSIZE 32
char g_serial_buf[SERIAL_BUFSIZE];
uint32_t g_buf_index = 0;

#define UPDATE_INTERVAL_IDLE 1500
#define UPDATE_INTERVAL_SCORE 100

long g_last_time_stamp = 0;
uint32_t g_time_accum = 0;
uint32_t g_update_interval = UPDATE_INTERVAL_IDLE;

enum GameState{IDLE = 0, IMPACT = 1, SCORE = 2} g_game_state = IDLE;

int g_current_led_index = 0;
bool g_indicator = false;

class LEDGate
{
  public:

    LEDGate(uint32_t the_length):
    m_num_leds(the_length)
    {
      m_leds = new CRGB[the_length];

      for(uint32_t i = 0; i < m_num_leds; i++){ m_leds[i] = CRGB::Black; }
    };

    ~LEDGate()
    {
      delete[](m_leds);
    };

    void update(uint32_t the_delta_millis)
    {
      for(uint32_t i = 0; i < m_num_leds; i++)
      {
        m_leds[i] = CRGB::Black;
      }
    }

    CRGB* leds(){ return m_leds; }
    uint32_t num_leds(){ return m_num_leds; }

  private:

    uint32_t m_num_leds;

    //! this holds the entire pixel color values
    CRGB* m_leds;
};

LEDGate g_gates[NUM_STRIPES] =
{
  LEDGate(450),
  LEDGate(450),
  LEDGate(300),
  LEDGate(300),
  LEDGate(186)
};

void fill_with_palette(uint8_t the_index, uint8_t colorIndex)
{
  CRGB* leds = g_gates[the_index].leds();
  uint32_t num_leds = g_gates[the_index].num_leds();

  for( int i = 0; i < num_leds; i++)
  {
    leds[i] = ColorFromPalette(g_color_palette, colorIndex, BRIGHTNESS, g_current_blending);
    colorIndex += STEPS;
  }
}

void setup()
{
  for(uint8_t i = 0; i < NUM_STRIPES; i++){ pinMode(g_data_pins[i], OUTPUT); }

  memset(g_serial_buf, 0, SERIAL_BUFSIZE);

  //for(uint32_t i = 0; i < NUM_STRIPES; i++)
  {
    FastLED.addLeds<LED_TYPE, LED_PIN_0, COLOR_ORDER>(g_gates[0].leds(), g_gates[0].num_leds());
    FastLED.addLeds<LED_TYPE, LED_PIN_1, COLOR_ORDER>(g_gates[1].leds(), g_gates[1].num_leds());
    FastLED.addLeds<LED_TYPE, LED_PIN_2, COLOR_ORDER>(g_gates[2].leds(), g_gates[2].num_leds());
    FastLED.addLeds<LED_TYPE, LED_PIN_3, COLOR_ORDER>(g_gates[3].leds(), g_gates[3].num_leds());
    FastLED.addLeds<LED_TYPE, LED_PIN_4, COLOR_ORDER>(g_gates[4].leds(), g_gates[4].num_leds());
  }
  FastLED.setBrightness(BRIGHTNESS);

  // initialize the serial communication:
  Serial.begin(57600);
}

////////////////////////////////////////////////////////////////////////

void update_stripes()
{
  CRGB c = CRGB::Red;

  for(uint8_t g = 0; g < NUM_STRIPES; g++)
  {
    CRGB* leds = g_gates[g].leds();
    int num_leds = g_gates[g].num_leds();

    if(g_current_led_index == g)
    {
      fill_with_palette(g, g_start_color_index);
    }
    else
    {
      for(int i = 0; i < num_leds; i++)
      {
        leds[i] = CRGB::Black;
      }
    }
  }
  FastLED.show();
}

////////////////////////////////////////////////////////////////////////

void loop()
{
  g_start_color_index = g_start_color_index + 1; /* motion speed */

  // time measurement
  int delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  if(g_time_accum >= g_update_interval)
  {
    g_time_accum = 0;
    g_indicator = !g_indicator;
    digitalWrite(13, g_indicator);

    switch(g_game_state)
    {
      case IDLE:
      case IMPACT:
        g_update_interval = UPDATE_INTERVAL_IDLE;
        break;

      case SCORE:
        g_update_interval = UPDATE_INTERVAL_SCORE;
        break;
    }

    // alternate stipes
    g_current_led_index = (g_current_led_index + 1) % NUM_STRIPES;
  }
  update_stripes();
}


void serialEvent()
{
  while (Serial.available())
  {
    // get the new byte:
    char c = Serial.read();

    if(c == '\0'){ continue; }

    // add it to the buf
    g_serial_buf[g_buf_index % SERIAL_BUFSIZE] = c;
    g_buf_index++;

    // if the incoming character is a newline, set a flag
    if (c == '\n')
    {
      // change gamestate here
      int num = atoi(g_serial_buf);

      g_game_state = GameState(num);
      //String poop; poop += num;
      //Serial.println(poop);

      g_buf_index = 0;
      memset(g_serial_buf, 0, SERIAL_BUFSIZE);
    }
  }
}
