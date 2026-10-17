/*
 * ADC.h - MSPM0G3507 port
 *
 * Current and die temperature on ADC0, battery voltage on ADC1. Results are
 * read straight from the ADC MEMRES registers; no DMA channel is used.
 */

#ifndef ADC_H_
#define ADC_H_

#include "main.h"
#include "targets.h"

void ADC_DMA_Callback(void);
void enableADC_DMA(void);
void activateADC(void);
void ADC_Init(void);
void startADCConversion(void);
int16_t getConvertedDegrees(uint16_t adcrawtemp);

#endif /* ADC_H_ */
