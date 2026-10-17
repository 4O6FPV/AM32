/*
 * ADC.c - analog sensing for the MSPM0G3507 port
 *
 * ADC0: MEM0 = current sense (CURRENT_ADC_CHANNEL), MEM1 = die temperature
 *       (channel 11, needs a long sample time)
 * ADC1: MEM0 = battery voltage (VOLTAGE_ADC_CHANNEL)
 *
 * Both run a single software-triggered sequence; main.c starts one every
 * millisecond through startADCConversion() and reads the previous results
 * through ADC_DMA_Callback(). Results stay in MEMRES, so no DMA is used.
 * ADCCLK = SYSOSC 32 MHz / 8 = 4 MHz, 12 bit, VDDA (3.3 V) reference.
 */

#include "ADC.h"

#include "peripherals.h"

extern uint16_t ADC_raw_temp;
extern uint16_t ADC_raw_volts;
extern uint16_t ADC_raw_current;
extern uint16_t ADC_raw_input;

/* die temperature sensor: TI's typical slope with the factory 30 C trim */
#define TEMP_TRIM_DEGC 30
#define TEMP_SLOPE_Q12 1833 /* (3300 mV / 4096) / 1.8 mV per C, x4096 */

static const DL_ADC12_ClockConfig adcClock = {
    .clockSel = DL_ADC12_CLOCK_SYSOSC,
    .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
};

static void adcPowerUp(ADC12_Regs* adc)
{
    DL_ADC12_reset(adc);
    DL_ADC12_enablePower(adc);
    delay_cycles(16);
    DL_ADC12_setClockConfig(adc, (DL_ADC12_ClockConfig*)&adcClock);
    DL_ADC12_setPowerDownMode(adc, DL_ADC12_POWER_DOWN_MODE_MANUAL);
}

void ADC_Init(void)
{
    pincmSetAnalog(CURRENT_ADC_IOMUX);
    pincmSetAnalog(VOLTAGE_ADC_IOMUX);

    adcPowerUp(ADC0);
    DL_ADC12_initSeqSample(ADC0, DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_01,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0, CURRENT_ADC_CHANNEL,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_1, TEMP_ADC_CHANNEL,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP1,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC0, 8);    /* 2 us at 4 MHz */
    DL_ADC12_setSampleTime1(ADC0, 200);  /* 50 us, TI's figure for the temp sensor */

    adcPowerUp(ADC1);
    DL_ADC12_initSeqSample(ADC1, DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_00,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC1, DL_ADC12_MEM_IDX_0, VOLTAGE_ADC_CHANNEL,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setSampleTime0(ADC1, 8);
}

void enableADC_DMA(void)
{
    /* nothing to do: results are read from MEMRES */
}

void activateADC(void)
{
    startADCConversion();
}

void startADCConversion(void)
{
    /* single-sequence mode clears ENC after each sequence */
    DL_ADC12_enableConversions(ADC0);
    DL_ADC12_enableConversions(ADC1);
    DL_ADC12_startConversion(ADC0);
    DL_ADC12_startConversion(ADC1);
}

void ADC_DMA_Callback(void)
{
    ADC_raw_current = (uint16_t)ADC0->ULLMEM.MEMRES[0];
    ADC_raw_temp = (uint16_t)ADC0->ULLMEM.MEMRES[1];
    ADC_raw_volts = (uint16_t)ADC1->ULLMEM.MEMRES[0];
}

int16_t getConvertedDegrees(uint16_t adcrawtemp)
{
    /* T = 30 C - (raw - trim) * (3300/4096 mV) / 1.8 mV/C, trim from the
       factory region (TEMP_SENSE0, measured at 30 C, same 3.3 V scale) */
    int32_t trim = (int32_t)DL_SYSCTL_getTempCalibrationConstant();
    int32_t delta = (int32_t)adcrawtemp - trim;
    return (int16_t)(TEMP_TRIM_DEGC - ((delta * TEMP_SLOPE_Q12) >> 12));
}
