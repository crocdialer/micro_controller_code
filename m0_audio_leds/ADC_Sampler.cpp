#include "ADC_Sampler.h"
#include "wiring_private.h"

namespace
{
    adc_callback_t g_adc_callback = nullptr;
    uint8_t g_sample_pin = A1;
    uint32_t g_sample_rate = 22050;
};

static __inline__ void adc_sync() __attribute__((always_inline, unused));
static void  adc_sync(){ while (ADC->STATUS.bit.SYNCBUSY); }

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

static inline void adc_disable()
{
    ADC->CTRLA.bit.ENABLE = 0x00;
    adc_sync();
}

static inline void adc_enable()
{
    ADC->CTRLA.bit.ENABLE = 0x01;
    adc_sync();
}

void adc_configure();
void tc_configure();

uint32_t adc_read(uint8_t the_pin)
{
    // Selection for the positive ADC input
    adc_sync();
    ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[the_pin].ulADCChannelNumber;

    // Enable ADC
    adc_sync();
    ADC->CTRLA.bit.ENABLE = 0x01;

    // Data ready flag cleared
    ADC->INTFLAG.bit.RESRDY = 1;

    // Start ADC conversion
    adc_sync();
    ADC->SWTRIG.bit.START = 1;

    // Wait till conversion done
    while(!ADC->INTFLAG.bit.RESRDY);
    uint32_t value = ADC->RESULT.reg;

    // Disable the ADC
    adc_sync();
    ADC->CTRLA.bit.ENABLE = 0x00;

    // flush for good measure
    adc_sync();
    ADC->SWTRIG.reg = 0x01;
    return value;
}

void adc_configure()
{
    // gain select as 1X
    // adc_sync();
    // ADC->INPUTCTRL.bit.GAIN = ADC_INPUTCTRL_GAIN_8X_Val;

    // single conversion no averaging
    adc_sync();
    ADC->AVGCTRL.reg = 0x00;

    adc_sync();
    ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_10BIT_Val;

    // Divide Clock by 8 -> ~100kHz
    adc_sync();
    ADC->CTRLB.bit.PRESCALER = ADC_CTRLB_PRESCALER_DIV8_Val;

    // sample length in 1/2 CLK_ADC cycles. default: 3F
    adc_sync();
    ADC->SAMPCTRL.reg = 0x2F;
}

void tc_configure(uint32_t the_sample_rate)
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

    TC5->COUNT16.CC[0].reg = (uint16_t) (SystemCoreClock / the_sample_rate - 1);
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
    if(g_adc_callback){ (*g_adc_callback)(adc_read(g_sample_pin)); }

    // Clear interrupt
    TC5->COUNT16.INTFLAG.bit.MC0 = 1;
}

ADC_Sampler::ADC_Sampler(){}

ADC_Sampler::~ADC_Sampler()
{
     end();
}

void ADC_Sampler::begin(int the_pin, uint32_t the_sample_rate)
{
    g_sample_pin = the_pin;
    analogRead(the_pin);
    adc_disable();
    adc_configure();
    adc_enable();
    tc_configure(the_sample_rate);
    tc_enable();
}

void ADC_Sampler::end()
{
    adc_disable();
    tc_disable();
    tc_reset();
}

void ADC_Sampler::set_adc_callback(adc_callback_t the_callback)
{
    g_adc_callback = the_callback;
}
