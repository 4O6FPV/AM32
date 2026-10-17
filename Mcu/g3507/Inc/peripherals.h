/*
 * peripherals.h - MSPM0G3507 port
 *
 * Timer conventions (all timers count UP, so they behave like the STM32
 * timers the AM32 core was written against):
 *   TIMA0  PWM, LOAD = TIM1_AUTORELOAD, CC0..2 = phase duty, CC3 = ADC timing
 *   TIMG7  INTERVAL_TIMER, free running at 2 MHz
 *   TIMG6  COM_TIMER, 2 MHz, LOAD event (CTR == LOAD) = commutation
 *   TIMG8  UTILITY_TIMER, free running at 1 MHz (delayMicros)
 *   TIMG0  TEN_KHZ_TIMER, 1 MHz, LOAD event every 1/LOOP_FREQUENCY_HZ
 *   TIMA1  IC_TIMER_REGISTER, DShot/PWM input capture and DShot telemetry out
 */

#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

#include "ADC.h"
#include "main.h"

#define PWM_TIMER TIMA0

#define INTERVAL_TIMER_COUNT (INTERVAL_TIMER->COUNTERREGS.CTR)
#define SET_INTERVAL_TIMER_COUNT(intertime) (INTERVAL_TIMER->COUNTERREGS.CTR = (intertime))

#define RELOAD_WATCHDOG_COUNTER() reloadWatchDogCounter()

#define DISABLE_COM_TIMER_INT() (COM_TIMER->CPU_INT.IMASK &= ~GPTIMER_CPU_INT_IMASK_L_SET)
#define ENABLE_COM_TIMER_INT() (COM_TIMER->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_L_SET)
#define SET_AND_ENABLE_COM_INT(time)                                         \
    (COM_TIMER->COUNTERREGS.CTR = 0, COM_TIMER->COUNTERREGS.LOAD = (time),   \
        COM_TIMER->CPU_INT.ICLR = GPTIMER_CPU_INT_IMASK_L_SET,               \
        COM_TIMER->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_L_SET)
#define SET_COM_TIMER_RELOAD(time) (COM_TIMER->COUNTERREGS.LOAD = (time))

#define SET_PRESCALER_PWM(presc) (PWM_TIMER->COMMONREGS.CPS = (presc))
#define SET_AUTO_RELOAD_PWM(relval) (PWM_TIMER->COUNTERREGS.LOAD = (relval))
#define SET_DUTY_CYCLE_ALL(newdc)                                            \
    (PWM_TIMER->COUNTERREGS.CC_01[0] = (newdc),                              \
        PWM_TIMER->COUNTERREGS.CC_01[1] = (newdc),                           \
        PWM_TIMER->COUNTERREGS.CC_23[0] = (newdc))

void initAfterJump(void);
void initCorePeripherals(void);
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_COMP_Init(void);
void MX_PWM_Timer_Init(void);
void MX_IC_Timer_Init(void);
void MX_IWDG_Init(void);

void resetInputCaptureTimer(void);
void setPWMCompare1(uint16_t compareone);
void setPWMCompare2(uint16_t comparetwo);
void setPWMCompare3(uint16_t comparethree);
void enableCorePeripherals(void);
void reloadWatchDogCounter(void);
void generatePwmTimerEvent(void);
void setDeadTime(uint16_t ticks);

/* IOMUX helpers shared by the HAL files */
#define PINCM_GPIO_FUNC 0x00000001U /* PF value 1 = GPIO on every PINCM */
static inline void pincmSetFunction(uint32_t pincm, uint32_t pf)
{
    IOMUX->SECCFG.PINCM[pincm] = IOMUX_PINCM_PC_CONNECTED | pf;
}
static inline void pincmSetAnalog(uint32_t pincm)
{
    /* PF = 0 and PC = 0 disconnects the digital path (datasheet note 1) */
    IOMUX->SECCFG.PINCM[pincm] = 0;
}

#endif /* PERIPHERALS_H_ */
