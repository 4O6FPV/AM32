/*
 * mspm0_it.c - interrupt handlers for the MSPM0G3507 AM32 port
 *
 * Mapping from the G071 port:
 *   DMA1_Channel1 (DShot DMA)   -> DMA_IRQHandler, channel INPUT_DMA_CHANNEL
 *   ADC1_COMP (comparator EXTI) -> GROUP1_IRQHandler, COMP0 edge flags
 *   TIM14 (COM_TIMER)           -> TIMG6_IRQHandler, LOAD event
 *   TIM6 (10 kHz loop)          -> TIMG0_IRQHandler, LOAD event
 *   EXTI line 15 software IRQ   -> PendSV (lowest priority), processDshot()
 */

#include "ADC.h"
#include "IO.h"
#include "common.h"
#include "comparator.h"
#include "main.h"
#include "peripherals.h"
#include "targets.h"

extern void transfercomplete(void);
extern void PeriodElapsedCallback(void);
extern void interruptRoutine(void);
extern void tenKhzRoutine(void);
extern void processDshot(void);

extern volatile char dshot_telemetry;
extern volatile char armed;
extern volatile char out_put;
extern volatile uint8_t compute_dshot_flag;

uint16_t interrupt_time = 0;

#define COMP_EDGE_FLAGS (COMP_CPU_INT_IMASK_COMPIFG_SET | COMP_CPU_INT_IMASK_COMPINVIFG_SET)

static inline void requestProcessDshot(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void NMI_Handler(void) { }

void HardFault_Handler(void)
{
    while (1) {
    }
}

void SVC_Handler(void) { }
void SysTick_Handler(void) { }

void PendSV_Handler(void)
{
    processDshot();
}

void DMA_IRQHandler(void)
{
    if (!(DMA->CPU_INT.MIS & DL_DMA_INTERRUPT_CHANNEL0)) {
        return;
    }
    DMA->CPU_INT.ICLR = DL_DMA_INTERRUPT_CHANNEL0;

    if (armed && dshot_telemetry) {
        if (out_put) {
            receiveDshotDma();
            compute_dshot_flag = 2;
        } else {
            sendDshotDma();
            compute_dshot_flag = 1;
        }
        requestProcessDshot();
        return;
    }
    transfercomplete();
    requestProcessDshot();
}

void GROUP1_IRQHandler(void)
{
    /* GROUP1 also carries GPIOA/B, COMP1/2 and TRNG; only COMP0 is used */
    uint32_t pending = MAIN_COMP->CPU_INT.MIS & COMP_EDGE_FLAGS;
    if (!pending) {
        return;
    }
    if ((INTERVAL_TIMER->COUNTERREGS.CTR) > (average_interval >> 1)) {
        MAIN_COMP->CPU_INT.ICLR = pending;
        interruptRoutine();
    } else if (getCompOutputLevel() == rising) {
        MAIN_COMP->CPU_INT.ICLR = pending;
    }
    /* otherwise leave the flag set: the IRQ re-enters until the blanking
       window (half the average interval) has passed, as on the G071 */
}

void TIMG6_IRQHandler(void)
{
    /* COM_TIMER */
    interrupt_time = (uint16_t)UTILITY_TIMER->COUNTERREGS.CTR;
    COM_TIMER->CPU_INT.ICLR = GPTIMER_CPU_INT_IMASK_L_SET;
    PeriodElapsedCallback();
    interrupt_time = ((uint16_t)UTILITY_TIMER->COUNTERREGS.CTR) - interrupt_time;
}

void TIMG0_IRQHandler(void)
{
    /* TEN_KHZ_TIMER */
    if (TEN_KHZ_TIMER->CPU_INT.MIS & GPTIMER_CPU_INT_IMASK_L_SET) {
        TEN_KHZ_TIMER->CPU_INT.ICLR = GPTIMER_CPU_INT_IMASK_L_SET;
        tenKhzRoutine();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
