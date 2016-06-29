#include <fab_utils.h>
#include <Easing.h>

int g_update_interval = 50;
int g_time_accum = 0;
int g_last_time_stamp;

// maximum threshold for change in g/s
const float g_max_g = .5;
const uint16_t g_min_raw_pot_val = 50;

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

// the final accelration data
uint8_t g_accelorations[g_num_sensor_pins * 2];

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
  set_update_rate(50.f);
  
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

void print_values(uint16_t *values, uint8_t num_values)
{
  static char out_buf[1024];
  char *line_ptr = out_buf;

  *out_buf = '\0';

  for (uint8_t i = 0; i < num_values; i++)
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

  // read analogue inputs
  read_values(g_control_pins, g_sensor_pins, g_sensor_values, 0);
  
  // for each accelerometer (using 4 analog values each)
  for(uint8_t i = 0, end_index = g_num_sensor_pins * 8; i < end_index; i += 4)
  {
    // is the panel active at all?
    if(g_sensor_values[i] < g_min_raw_pot_val){ continue; }

    float pot_val = ease_func(map_value<float>(g_sensor_values[i], 0.f, 800.f, 1.f, 0.f));
    
    float thresh = pot_val * g_max_g;
    float thresh2 = thresh * thresh;
   
    vec3 new_val = vec3(analog_value_to_g(g_sensor_values[i + 1]),
                        analog_value_to_g(g_sensor_values[i + 2]),
                        analog_value_to_g(g_sensor_values[i + 3]));
   
    uint8_t idx = i / 4;
    vec3 diff = (g_last_measures[idx] - new_val);
    g_last_measures[idx] = new_val;
    
    // write acceleration value with 8bit precision, 0 if threshold is not hit
    g_accelorations[idx] =  max(diff.length2() > thresh2 ? map_value<float>(diff.length(),
                                                                            0.f, g_max_accelo_val,
                                                                            0, 255) : 0,
                                g_accelorations[idx]);
  }
  //end for each
  
  // trigger LEDs
  for(uint8_t i = 0, end_index = g_num_sensor_pins; i < end_index; i++)
  {
    digitalWrite(g_led_pins[i], g_accelorations[i * 2] || g_accelorations[i * 2 + 1]);
  }

  // timeout reached
  if(g_time_accum >= g_update_interval)
  {
    g_time_accum = 0;
    
    // send our values
    //print_values(g_sensor_values + 16, 8);
    
    // send accel values as byte array
    Serial.write(g_accelorations, sizeof(g_accelorations));
    Serial.println();

    // clear values
    memset(g_accelorations, 0, sizeof(g_accelorations));
  }
}

