#include <limits.h>
#include <fab_utils.h>
#include <Easing.h>

#define SERIAL_START_CODE 0x7E
#define SERIAL_END_CODE 0xE7

int g_update_interval = 50;
int g_time_accum = 0;
int g_last_time_stamp;

const uint8_t g_indicator_led = 13;
bool g_indicator = false;
bool g_clear_values_flag = false;

// maximum threshold for change in g/s
const float g_max_g = .5;
const uint16_t g_min_raw_pot_val = 250;

// maximum accelerometer value
const float g_max_accelo_val = 16.f;

// number of used multiplexers
const uint8_t g_num_sensor_pins = 5;
const uint8_t g_sensor_pins[] = {A0, A1, A2, A3, A4};

// common control pins for our analog 4051 multiplexers
const uint8_t g_control_pins[3] = {3, 4, 5};

// control pin for LED #1
const uint8_t g_led_pins[5] = {6, 8, 9, 10, 11};

// our last acceleration vector
vec3 g_last_measures[g_num_sensor_pins * 2];

// stores raw analog input-values
// [pot_0, x_0, y_0, z_0, ...]
uint16_t g_sensor_values[g_num_sensor_pins * 8];

// the final acceleration data
uint16_t g_accelorations[g_num_sensor_pins * 2];

#define FORCE_TIMEOUT 1000
int g_current_force_timeout = FORCE_TIMEOUT;
int g_last_force = 0;

// easing curve for poti values
typedef kinski::animation::EaseOutCirc EaseFunc;
EaseFunc ease_func;

void set_update_rate(float hz)
{
  g_update_interval = (int)(1000.f / hz);
}

void setup()
{
  g_last_time_stamp = millis();
  
  // initialize the serial communication:
  Serial.begin(57600);
  
  // set our refresh rate
  set_update_rate(60.f);
  
  // initialize pins as output:
  pinMode(g_control_pins[0], OUTPUT);
  pinMode(g_control_pins[1], OUTPUT);
  pinMode(g_control_pins[2], OUTPUT);
  
  for (int i = 0; i < g_num_sensor_pins; i++){ pinMode(g_led_pins[i], OUTPUT); }
  for (int i = 0; i < g_num_sensor_pins * 8; i++){ g_sensor_values[i] = 0; }
}

void read_values( const uint8_t the_control_pins[3],
                  const uint8_t the_sensor_pins[],
                  uint16_t *the_output,
                  int the_delay)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // select input from multiplexer
    digitalWrite(the_control_pins[0], i & 0x1);
    digitalWrite(the_control_pins[1], (i >> 1) & 0x1);
    digitalWrite(the_control_pins[2], (i >> 2) & 0x1);
     
    // wait at least a couple of no-ops for switching
    delay(the_delay);
    ;;
     
     // now read from analog inputs
    for (uint8_t j = 0; j < g_num_sensor_pins; j++)
    {
      the_output[j * 8 + i] = analogRead(the_sensor_pins[j]);
    }
  }
}

void print_values(uint16_t *values, uint16_t num_values)
{
  static char out_buf[1024];
  char *line_ptr = out_buf;

  *out_buf = '\0';

  for (uint16_t i = 0; i < num_values; i++)
  {
    line_ptr += sprintf(line_ptr, "analog_%d %d\n", i, values[i]);
  }
  Serial.print(out_buf);
}

float analog_value_to_g(int the_val)
{
  return map_value<float>(the_val, 0, 675, -g_max_accelo_val, g_max_accelo_val);
}

void loop()
{
  // time measurement
  int delta_time = millis() - g_last_time_stamp;
  g_last_time_stamp = millis();
  float delta_secs = delta_time / 1000.f;

  g_time_accum += delta_time;
  g_current_force_timeout -= delta_time;

  // read analogue inputs
  read_values(g_control_pins, g_sensor_pins, g_sensor_values, 0);
  
  uint16_t num_active_panels = 0;
  
  // determines the used value range
  float gain = ease_func(map_value<float>(g_sensor_values[0], 0.f, 800.f, 0.f, 1.f));

  // for each accelerometer (using 4 analog values each)
  for(uint8_t i = 0, end_index = g_num_sensor_pins * 8; i < end_index; i += 4)
  {
    uint8_t idx = i / 4;

    // is the panel active at all?
    if(g_sensor_values[i] < g_min_raw_pot_val)
    {
      g_accelorations[idx] = 0;
      continue; 
    }
    
    num_active_panels++;

    vec3 new_val = vec3(analog_value_to_g(g_sensor_values[i + 1]),
                        analog_value_to_g(g_sensor_values[i + 2]),
                        analog_value_to_g(g_sensor_values[i + 3]));
   
    vec3 diff = (g_last_measures[idx] - new_val);
    g_last_measures[idx] = new_val;
    
    // write acceleration value with 16bit precision, 0 if threshold is not hit
    g_accelorations[idx] =  max(map_value<float>(diff.length(),
                                                 0.f, g_max_accelo_val * gain,
                                                 0, USHRT_MAX),
                                g_accelorations[idx]);
  }
  //end for each
  
  // accumulate forces
  int force = 0;
  
  for(uint8_t i = 0, end_index = g_num_sensor_pins * 2; i < end_index; i++)
  {
    force += g_accelorations[i]; 
  }
  force = num_active_panels ? force / num_active_panels : 0;

  if(force > g_last_force)
  {
    g_last_force = force;
    g_current_force_timeout = FORCE_TIMEOUT;
  }
  
  if(g_current_force_timeout <= 0)
  {
    // set clear flag 
    g_clear_values_flag = true;
  }
  
  const int val_step = USHRT_MAX / 6;
  int max_led_idx = g_last_force / val_step;
  
  // Force indicator LEDs
  for(uint8_t i = 0, end_index = g_num_sensor_pins; i < end_index; i++)
  {
    digitalWrite(g_led_pins[end_index - i - 1], i < max_led_idx);
  }

  // timeout reached
  if(g_time_accum >= g_update_interval)
  {
    g_time_accum = 0;
    
    // send accel values as byte array
    Serial.write(SERIAL_START_CODE);
    Serial.write((uint8_t*)g_accelorations, sizeof(g_accelorations));
    Serial.write(SERIAL_END_CODE);

    // clear values
    if(g_clear_values_flag)
    {
      g_clear_values_flag = false;
  
      // reset indicator light after a timeout
      g_last_force = 0;
      memset(g_accelorations, 0, sizeof(g_accelorations));
    }
    
    // flash indicator
    digitalWrite(g_indicator_led, g_indicator);
    g_indicator = !g_indicator;
  }
}

