#include "Arduino.h"

#pragma once

//! stripped-down fast analogue read.
//  the_pin is the analog input pin number to be read.
uint32_t adc_read(uint8_t the_pin);

//! signature for a ADC-value callback
typedef void (*adc_callback_t)(uint32_t the_value);

class ADC_Sampler
{
 public:
     ADC_Sampler();
     ~ADC_Sampler();

     void begin(int the_pin, uint32_t the_sample_rate);
     void end();
     void set_adc_callback(adc_callback_t the_callback);
};
