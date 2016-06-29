#include <Wire.h>
#include "fab_utils/fab_utils.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#define BUTTON_LED_PIN 3
#define BUTTON_PIN 2
#define BUTTON_TIMEOUT 500

#define MIC_PIN A0

// mic sampling
int g_sample_window = 50;
unsigned long g_start_millis = millis();  // Start of sample window
volatile unsigned int g_peak_to_peak = 0;   // peak-to-peak level
unsigned int g_signal_max = 0;
unsigned int g_signal_min = 1024;

// 0.0 ... 1.0
float g_mic_lvl = 0.f;
volatile bool g_show_lvl = true;

// helper variables for time measurement
long g_last_time_stamp = 0;
volatile long g_last_button_press = 0;
uint32_t g_time_accum = 0;
long *g_time_buf = new long[8 * 16];

static const uint8_t PROGMEM
  smile_bmp[] =
  { B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10100101,
    B10011001,
    B01000010,
    B00111100 },
  neutral_bmp[] =
  { B00111100,
    B01000010,
    B10100101,
    B10000001,
    B10111101,
    B10000001,
    B01000010,
    B00111100 },

    cloud_bmp[] =
    { B00000000, B11100000,
      B00001101, B00010000,
      B00010010, B00001000,
      B00100100, B00000100,
      B01000000, B00000100,
      B01000000, B00000100,
      B01000000, B00000100,
      B00111111, B11111000},

    cloud_bmp_mischa[] =
    { B11111111,
      B10000001,
      B10010001,
      B10111001,
      B11111101,
      B10111111,
      B10000001,
      B11111111 },

    cloud_bmp_poop[] =
    { B00001110,
      B00111011,
      B01100001,
      B01000001,
      B01000011,
      B01000010,
      B01000010,
      B00111100 };

Adafruit_8x16matrix matrix = Adafruit_8x16matrix();

class BitMap
{
  const uint8_t* m_data;

public:

  int16_t m_pos_x, m_pos_y;
  uint16_t m_width, m_height;

  BitMap(const uint8_t* data, uint16_t width, uint16_t height):
  m_data(data), m_width(width), m_height(height){};

  inline void draw()
  {
    // matrix.clear();
    matrix.drawBitmap(m_pos_x, m_pos_y, m_data, m_width, m_height, LED_ON);
  }
};

BitMap g_cloud(cloud_bmp, 16, 8);

void display_test()
{
  matrix.setRotation(1);

  matrix.clear();
  matrix.drawBitmap(0, 0, neutral_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(500);

  matrix.drawBitmap(8, 0, neutral_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(500);

  matrix.clear();
  matrix.drawBitmap(0, 0, smile_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(500);

  matrix.drawBitmap(8, 0, smile_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(2500);

  matrix.clear();
  matrix.drawBitmap(0, 0, cloud_bmp, 16, 8, LED_ON);
  matrix.writeDisplay();
  delay(5000);

  matrix.setRotation(0);

  matrix.clear();
  matrix.drawLine(0,0, 7,15, LED_ON);
  matrix.writeDisplay();  // write the changes we just made to the display
  delay(500);

  matrix.clear();
  matrix.drawRect(0,0, 8,16, LED_ON);
  matrix.fillRect(2,2, 4,12, LED_ON);
  matrix.writeDisplay();  // write the changes we just made to the display
  delay(500);

  matrix.clear();
  matrix.drawCircle(3,8, 3, LED_ON);
  matrix.writeDisplay();  // write the changes we just made to the display
  delay(500);

  matrix.setTextSize(1);
  matrix.setRotation(1);
  matrix.setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
  matrix.setTextColor(LED_ON);

  for (int16_t x=0; x>=-185; x--) {
    matrix.clear();
    matrix.setCursor(x,0);
    matrix.print("Travel to ... PoopMandoo");
    matrix.writeDisplay();
    delay(80);
  }
}

void setup()
{
  // setup button LED
  pinMode(BUTTON_LED_PIN, OUTPUT);
  digitalWrite(BUTTON_LED_PIN, HIGH);

  // setup button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // digitalWrite(BUTTON_PIN, HIGH);

  matrix.begin(0x70);  // pass in the address
  matrix.setBrightness(255);

  cli();//disable interrupts

  //set up continuous sampling of analog pin 0
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0;
  ADCSRB = 0;

  ADMUX = (1 << REFS0) | (0 & 0x07);// select analog0, set ref to default

  //ADCSRA |= (1 << ADPS2) | (1 << ADPS0); //set ADC clock with 32 prescaler- 16mHz/32=500kHz
  ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
  ADCSRA |= (1 << ADATE); //enable auto trigger
  ADCSRA |= (1 << ADIE); //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); //enable ADC
  ADCSRA |= (1 << ADSC); //start ADC measurements

  // enable button interrupt (must be pins 2 or 3 on 328P)
  // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_pressed, FALLING);

  sei();//enable interrupts

  memset(g_time_buf, 0, sizeof(long) * 16 * 8);

  // initialize the serial communication:
  // Serial.begin(57600);
}

void draw_lvl(float the_lvl)
{
  //analogWrite(BUTTON_LED_PIN, mic_lvl);
  int mic_map = map_value<float>(the_lvl, 0, 1.f, 0, 16);

  matrix.setRotation(1);
  matrix.drawRect(0,0, mic_map, 8, LED_ON);

  matrix.writeDisplay();  // write the changes we just made to the display
}

void draw_lvl_quad(float the_lvl)
{
  int radius = map_value<float>(the_lvl, 0, .9f, 0, 8);
  radius += radius % 2;

  int pos_x = 8 - radius / 2, pos_y = 4 - radius / 2;

  matrix.setRotation(1);
  matrix.fillRect(pos_x, pos_y, radius, radius, LED_ON);

  // matrix.writeDisplay();  // write the changes we just made to the display
}

void add_random_pixels(uint16_t the_count, uint32_t the_delay_millis)
{
  long time_stamp = millis();

  for(uint16_t i = 0; i < the_count; i++)
  {
    int rnd_x = random<int>(0, 16), rnd_y = random<int>(0, 8);
    int rnd_time = random<int>(- the_delay_millis / 2, the_delay_millis / 2);
    g_time_buf[rnd_x + 8 * rnd_y] = time_stamp + the_delay_millis + rnd_time;
  }
}

void draw_random_pixels()
{
  long time_stamp = millis();

  matrix.setRotation(1);
  for(int x = 0; x < 16; x++)
    for(int y = 0; y < 8; y++)
    {
      matrix.drawPixel(x, y,
                       g_time_buf[x + 8 * y] > time_stamp ? LED_ON : LED_OFF);
    }
  // matrix.writeDisplay();
}

void draw_bitmaps()
{
  g_cloud.m_pos_x = 16;

  for(int i = 0; i < 32; i++)
  {
    matrix.clear();
    g_cloud.draw();
    matrix.writeDisplay();
    g_cloud.m_pos_x--;
    delay(170);
  }
}

char g_serial_buf[128];

void loop()
{
  // read mic val
  g_mic_lvl = map_value<float>(g_peak_to_peak, 0.f, 666.f, 0.f, 1.f);

  // adjust brightness
  //matrix.setBrightness(map_value<uint8_t>(g_peak_to_peak, 0.f, 666.f, 10, 255));

  // time measurement
  uint32_t delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  g_time_accum += delta_time;

  // button pressed?
  if(digitalRead(BUTTON_PIN) == LOW){ button_pressed(); }

  matrix.clear();

  // add random pixel related to loudness
  add_random_pixels(3 * g_mic_lvl, 600);

  if(g_show_lvl)
  {
    draw_random_pixels();
    draw_lvl_quad(g_mic_lvl);
    matrix.writeDisplay();
  }
  else
  {
    digitalWrite(BUTTON_LED_PIN, g_mic_lvl > .55f);
  }

  delay(80);
}

// button interrupt
void button_pressed()
{
  long time_stamp = millis();

  if(g_last_button_press + BUTTON_TIMEOUT < millis())
  {
      g_last_button_press = millis();

      // button LED
      digitalWrite(BUTTON_LED_PIN, LOW);
      // display_test();
      draw_bitmaps();
  }
  digitalWrite(BUTTON_LED_PIN, HIGH);

  // toggle lvl display
  g_show_lvl = !g_show_lvl;
}

// Audio-sampling interrupt
ISR(ADC_vect)
{
  uint16_t sample;

  // collect data from Analog0 (0 - 1023)
  sample = ADC;

  if(sample < 1024)  // toss out spurious readings
  {
      g_signal_min = min(g_signal_min, sample);  // save just the min levels
      g_signal_max = max(g_signal_max, sample);  // save just the max levels
  }

  if(millis() > g_start_millis + g_sample_window)
  {
      g_peak_to_peak = g_signal_max - g_signal_min;  // max - min = peak-peak amplitude
      g_signal_max = 0;
      g_signal_min = 1024;
      g_start_millis = millis();
  }
}
