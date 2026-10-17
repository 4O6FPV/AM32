/*
 * Startup code and vector table for AM32 on the TI MSPM0G350x.
 *
 * Derived from TI's startup_mspm0g350x_gcc.c (MSPM0 SDK 2.11.00.07).
 * Changes for AM32:
 *  - vector table lives in ".isr_vector" so the AM32 linker script can place
 *    it at the application address (0x1000) after the bootloader
 *  - uses the STM32-style _sidata/_sdata/_edata/_sbss/_ebss/_estack symbols
 *    shared with the other AM32 linker scripts (.ramfunc is copied with .data)
 *
 * Original license follows.
 */
/*****************************************************************************

  Copyright (C) 2023 Texas Instruments Incorporated - http://www.ti.com/

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

   Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the
   distribution.

   Neither the name of Texas Instruments Incorporated nor the names of
   its contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*****************************************************************************/

#include <stdint.h>

extern int main(void);
extern void __libc_init_array(void);

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;

typedef void (*pFunc)(void);

void Default_Handler(void);
void Reset_Handler(void);

/* Processor exceptions */
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Device interrupts (MSPM0G350x) */
void GROUP0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void GROUP1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMG8_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void UART3_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void ADC0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void ADC1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void CANFD0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DAC0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void SPI0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void SPI1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void UART1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void UART2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void UART0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMG0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMG6_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMA0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMA1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMG7_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void TIMG12_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void I2C0_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void I2C1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void AES_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void RTC_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void DMA_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((used, section(".isr_vector")))
void (*const g_pfnVectors[])(void) = {
    (pFunc)&_estack,   /* initial stack pointer */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0, 0, 0, 0, 0, 0, 0,
    SVC_Handler,
    0, 0,
    PendSV_Handler,
    SysTick_Handler,
    GROUP0_IRQHandler,   /* 0  SYSCTL / WWDT / FLASHCTL / DEBUGSS */
    GROUP1_IRQHandler,   /* 1  GPIOA / GPIOB / COMP0-2 / TRNG */
    TIMG8_IRQHandler,    /* 2  */
    UART3_IRQHandler,    /* 3  */
    ADC0_IRQHandler,     /* 4  */
    ADC1_IRQHandler,     /* 5  */
    CANFD0_IRQHandler,   /* 6  */
    DAC0_IRQHandler,     /* 7  */
    0,                   /* 8  */
    SPI0_IRQHandler,     /* 9  */
    SPI1_IRQHandler,     /* 10 */
    0,                   /* 11 */
    0,                   /* 12 */
    UART1_IRQHandler,    /* 13 */
    UART2_IRQHandler,    /* 14 */
    UART0_IRQHandler,    /* 15 */
    TIMG0_IRQHandler,    /* 16 */
    TIMG6_IRQHandler,    /* 17 */
    TIMA0_IRQHandler,    /* 18 */
    TIMA1_IRQHandler,    /* 19 */
    TIMG7_IRQHandler,    /* 20 */
    TIMG12_IRQHandler,   /* 21 */
    0,                   /* 22 */
    0,                   /* 23 */
    I2C0_IRQHandler,     /* 24 */
    I2C1_IRQHandler,     /* 25 */
    0,                   /* 26 */
    0,                   /* 27 */
    AES_IRQHandler,      /* 28 */
    0,                   /* 29 */
    RTC_IRQHandler,      /* 30 */
    DMA_IRQHandler,      /* 31 */
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    /* copy .data (and .ramfunc) initialisers from flash to SRAM */
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    /* zero .bss */
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }

    __libc_init_array();
    main();

    while (1) {
    }
}

void Default_Handler(void)
{
    while (1) {
    }
}
