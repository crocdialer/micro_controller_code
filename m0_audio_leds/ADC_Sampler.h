#include "Arduino.h"

#pragma once

//! stripped-down fast analogue read.
//  the_pin is the analog input pin number to be read.
uint32_t adc_read(uint8_t the_pin);

//! signature for a ADC-value callback
typedef void (*adc_callback_t)(uint32_t the_value);

/*! this helper class performs a kind of ADC free-running,
/*  taking continous samples from a given ADC pin with a given sample rate
 */
class ADC_Sampler
{
 public:
     ADC_Sampler();
     ~ADC_Sampler();

     //! start continuous sampling with the given ADC pin and samplerate
     void begin(int the_pin, uint32_t the_sample_rate);

     //! stop continuous sampling
     void end();

     //! pass a callback-function pointer to be called when a new sample is taken
     void set_adc_callback(adc_callback_t the_callback);
};
