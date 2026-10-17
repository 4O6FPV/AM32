/*
 * phaseouts.h - MSPM0G3507 port
 */

#ifndef INC_PHASEOUTS_H_
#define INC_PHASEOUTS_H_

#include "main.h"
#include "common.h"

void allOff(void);
void comStep(int newStep);
void fullBrake(void);
void allpwm(void);
void proportionalBrake(void);
void twoChannelForward(void);
void twoChannelReverse(void);

#endif /* INC_PHASEOUTS_H_ */
