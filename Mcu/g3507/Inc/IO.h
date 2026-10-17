/*
 * IO.h - MSPM0G3507 port (DShot / servo PWM input on TIMA1 CC0)
 */

#ifndef IO_H_
#define IO_H_

#include "main.h"

void changeToOutput(void);
void changeToInput(void);
void receiveDshotDma(void);
void sendDshotDma(void);

uint8_t getInputPinState(void);
void setInputPolarityRising(void);
void setInputPullDown(void);
void setInputPullUp(void);
void enableHalfTransferInt(void);
void setInputPullNone(void);

extern volatile char inputSet;
extern char dshot;
extern volatile char servoPwm;
extern volatile char send_telemetry;
extern volatile int16_t degrees_celsius;
extern uint16_t ADC_raw_volts;

#endif /* IO_H_ */
