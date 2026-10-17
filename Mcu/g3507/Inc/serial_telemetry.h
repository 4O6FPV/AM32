/*
 * serial_telemetry.h - MSPM0G3507 port (KISS telemetry on UART2 TX + DMA)
 */

#ifndef SERIAL_TELEMETRY_H_
#define SERIAL_TELEMETRY_H_

#include "main.h"

void telem_UART_Init(void);
void send_telem_DMA(uint8_t bytes);

#endif /* SERIAL_TELEMETRY_H_ */
