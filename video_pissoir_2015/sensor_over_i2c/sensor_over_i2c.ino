#include <Wire.h>

const uint32_t g_sensor_pins[] = {A0, A1};
const int g_led_pin = 3;
const uint16_t g_timer_millis = 1000;

volatile uint32_t g_sensor_val = 0;
volatile uint16_t g_current_millis = g_timer_millis; 

//! read function pointer
uint32_t (*read_sensor_value)();

//! read function for the sharp IR distance-sensor
uint32_t read_sharp_3v()
{
  return analogRead(g_sensor_pins[0]);
}

void setup()
{
  // initialize pins as output:
  pinMode(g_led_pin, OUTPUT);

  // join i2c bus with address x
  Wire.begin(4);
  
  // register callback
  Wire.onRequest(on_request);

  // declare which read function to use
  read_sensor_value = read_sharp_3v;

  // Timer0 is already used for millis() - we'll just interrupt somewhere
  // in the middle and call the "Compare A" function below
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);
}

void loop()
{
  // measure distance continuously
  uint32_t sensor_val = (*read_sensor_value)();
  
  // read potentiometer value
  uint16_t pot_val = analogRead(g_sensor_pins[1]);

  if(sensor_val > pot_val)
  {
    if(sensor_val >= g_sensor_val)
    {
      g_sensor_val = sensor_val;
    }
    // reset timer value
    g_current_millis = g_timer_millis;

    // enable LED
    digitalWrite(g_led_pin, 1); 

  }
  else
  {
    // disbale LED
    digitalWrite(g_led_pin, 0); 
  }
  
  delay(10);
}

// Interrupt is called once a millisecond
SIGNAL(TIMER0_COMPA_vect) 
{
  g_current_millis--;
  if(g_current_millis == 0)
  {
    // timer fires here
    g_sensor_val = 0;
    g_current_millis = g_timer_millis;
  }
}

// function that executes whenever data is requested via i2c 
void on_request()
{
  Wire.write((uint8_t*)&g_sensor_val, 4); // respond with message of x bytes
}
