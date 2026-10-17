/*
 * main.h - MCU include for the TI MSPM0G3507 AM32 port
 *
 * Pulls in the TI device header and the DriverLib modules the port uses.
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_adc12.h>
#include <ti/driverlib/dl_comp.h>
#include <ti/driverlib/dl_dma.h>
#include <ti/driverlib/dl_flashctl.h>
#include <ti/driverlib/dl_gpio.h>
#include <ti/driverlib/dl_timera.h>
#include <ti/driverlib/dl_timerg.h>
#include <ti/driverlib/dl_uart_main.h>
#include <ti/driverlib/dl_wwdt.h>
#include <ti/driverlib/m0p/dl_core.h>
#include <ti/driverlib/m0p/dl_factoryregion.h>
#include <ti/driverlib/m0p/dl_sysctl.h>

/* STM32 HAL names that the shared AM32 core still uses */
#ifndef RESET
#define RESET 0
#endif
#ifndef SET
#define SET 1
#endif

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
