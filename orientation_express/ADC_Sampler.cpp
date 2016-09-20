#include "ADC_Sampler.h"
#include "wiring_private.h"

namespace
{
    adc_callback_t g_adc_callback = nullptr;
    uint8_t g_sample_pin = 0;
    uint32_t g_sample_rate = 0;

};

static __inline__ void ADCsync() __attribute__((always_inline, unused));
static void  ADCsync(){ while (ADC->STATUS.bit.SYNCBUSY); }

static __inline__ void tc_sync() __attribute__((always_inline, unused));
static void  tc_sync(){ while (TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY); }

static inline void tc_enable()
{
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    tc_sync();
}

static inline void tc_disable()
{
    // Disable TC5
    TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
    tc_sync();
}

static inline void tc_reset()
{
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    tc_sync();
    while (TC5->COUNT16.CTRLA.bit.SWRST);
}

static inline void ADCdisable()
{
    ADC->CTRLA.bit.ENABLE = 0x00;
    ADCsync();
}

static inline void ADCenable()
{
    ADC->CTRLA.bit.ENABLE = 0x01;
    ADCsync();
}

uint16_t ADCread()
{
    uint16_t ret;
    ADCsync();
    ADC->SWTRIG.bit.START = 1;

    // Waiting for conversion to complete
    while(!ADC->INTFLAG.bit.RESRDY);

    // Store the value
    ret = ADC->RESULT.reg;

    ADCsync();
    ADC->SWTRIG.bit.START = 0;
    return ret;
}

//##############################################################################
// Stripped-down fast analogue read anaRead()
// ulPin is the analog input pin number to be read.
////##############################################################################
uint32_t fast_analog_read(uint8_t the_pin)
{
    ADCsync();
    // auto old_pin = ADC->INPUTCTRL.bit.MUXPOS;
    ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[the_pin].ulADCChannelNumber; // Selection for the positive ADC input

    ADCsync();
    ADC->CTRLA.bit.ENABLE = 0x01;             // Enable ADC

    ADC->INTFLAG.bit.RESRDY = 1;              // Data ready flag cleared

    ADCsync();
    ADC->SWTRIG.bit.START = 1;                // Start ADC conversion

    while (!ADC->INTFLAG.bit.RESRDY);   // Wait till conversion done
    ADCsync();
    uint32_t valueRead = ADC->RESULT.reg;

    ADCsync();
    ADC->CTRLA.bit.ENABLE = 0x00;             // Disable the ADC
    ADCsync();
    ADC->SWTRIG.reg = 0x01;                    //  and flush for good measure
    ADCsync();
    // ADC->INPUTCTRL.bit.MUXPOS = old_pin;

    return valueRead;
}

ADC_Sampler::ADC_Sampler()
{
     init();
}

ADC_Sampler::~ADC_Sampler(){ end(); }

void ADC_Sampler::begin(int the_pin, uint32_t the_sample_rate)
{
    g_sample_pin = the_pin;
    g_sample_rate = the_sample_rate;
    analogRead(the_pin);
    ADCdisable();
    ADCconfigure();
    ADCenable();
    tcConfigure();
    tc_enable();
}

void ADC_Sampler::end()
{
    ADCdisable();
    tc_disable();
    tc_reset();
}

void ADC_Sampler::set_adc_callback(adc_callback_t the_callback)
{
    g_adc_callback = the_callback;
}

void ADC_Sampler::ADCconfigure()
{
    // gain select as 1X
    // ADCsync();
    // ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_1X_Val;

    // single conversion no averaging
    ADCsync();
    ADC->AVGCTRL.reg = 0x00;

    // ADCsync();
    // ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_8BIT_Val;

    // Divide Clock by 8 -> ~100kHz
    ADCsync();
    ADC->CTRLB.bit.PRESCALER = ADC_CTRLB_PRESCALER_DIV8_Val;

    // set max length sampling-time
    // ADC->SAMPCTRL.reg = 0x1F;
    // ADC->SAMPCTRL.reg = 0x1F;
    ADCsync();

    ADCsetMux();

    // //###################################################################################
    // // ADC setup stuff
    // //###################################################################################
    // ADCsync();
    //
    // // ADC->REFCTRL.bit.REFSEL = ADC_REFCTRL_REFSEL_INTVCC0_Val; //  2.2297 V Supply VDDANA
    // // Set sample length and averaging
    // ADCsync();
    // ADC->AVGCTRL.reg = 0x00 ;       //Single conversion no averaging
    // ADCsync();
    // ADC->SAMPCTRL.reg = 0x0A;  ; //sample length in 1/2 CLK_ADC cycles Default is 3F
    //
    // //Control B register
    // int16_t ctrlb = 0x400;       // Control register B hibyte = prescale, lobyte is resolution and mode
    // ADCsync();
    // ADC->CTRLB.reg =  ctrlb     ;
}

void ADC_Sampler::ADCsetMux()
{
    if(g_sample_pin < A0){ g_sample_pin += A0; }
    pinPeripheral(g_sample_pin, g_APinDescription[g_sample_pin].ulPinType);
    ADCsync();

    // Selection for the positive ADC input
    ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[g_sample_pin].ulADCChannelNumber;
}

void ADC_Sampler::tcConfigure()
{
    // Enable GCLK for TCC2 and TC5 (timer counter input clock)
    GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5)) ;
    while (GCLK->STATUS.bit.SYNCBUSY);

    tc_reset();

    // Set Timer counter Mode to 16 bits
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;

    // Set TC5 mode as match frequency
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;

    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_ENABLE;

    TC5->COUNT16.CC[0].reg = (uint16_t) (SystemCoreClock / g_sample_rate - 1);
    tc_sync();

    g_sample_rate = SystemCoreClock / (TC5->COUNT16.CC[0].reg + 1);

    // Configure interrupt request
    NVIC_DisableIRQ(TC5_IRQn);
    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 0x00);
    NVIC_EnableIRQ(TC5_IRQn);

    // Enable the TC5 interrupt request
    TC5->COUNT16.INTENSET.bit.MC0 = 1;
    tc_sync();
}

void TC5_Handler(void)
{
    // auto v = ADCread();
    auto v = fast_analog_read(g_sample_pin);

    // Clear interrupt
    TC5->COUNT16.INTFLAG.bit.MC0 = 1;

    if(g_adc_callback){ (*g_adc_callback)(v); }
}
