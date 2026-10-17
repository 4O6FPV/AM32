/*
 * comparator.h - MSPM0G3507 port (BEMF zero crossing on COMP0)
 */

#ifndef COMPARATOR_H_
#define COMPARATOR_H_

#include "main.h"

void maskPhaseInterrupts(void);
void changeCompInput(void);
void enableCompInterrupts(void);
uint8_t getCompOutputLevel(void);

extern volatile char rising;
extern char step;

#endif /* COMPARATOR_H_ */
