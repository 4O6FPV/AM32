/*
 * peripherals.c - clock, timer, comparator and watchdog setup for the
 * TI MSPM0G3507 AM32 port.
 *
 * Clocking: SYSOSC 32 MHz -> SYSPLL -> MCLK 80 MHz (PD1 bus: TIMA0, TIMA1,
 * TIMG6, TIMG7, TIMG12), ULPCLK = MCLK / 2 = 40 MHz (PD0 bus: TIMG0, TIMG8).
 * The PLL setup mirrors TI's sysctl_mclk_syspll example for the
 * LP-MSPM0G3507, including the SYSPLL_ERR_01 lock check.
 */

#include "peripherals.h"

#include "comparator.h"
#include "serial_telemetry.h"
#include "targets.h"

#define POWER_STARTUP_DELAY 16

/* expected SYSPLLCLK1 / SYSOSC ratio x1000 (40 MHz / 32 MHz) */
#define FCC_EXPECTED_RATIO 1250U
#define FCC_UPPER_BOUND (FCC_EXPECTED_RATIO + 4U)
#define FCC_LOWER_BOUND (FCC_EXPECTED_RATIO - 4U)

static const DL_SYSCTL_SYSPLLConfig sysPLLConfig = {
    .inputFreq = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
    .rDivClk2x = 3,  /* CLK2X = 2 x 160 MHz / 4 = 80 MHz -> MCLK */
    .rDivClk1 = 1,   /* CLK1  = 160 MHz / 4 = 40 MHz (lock check) */
    .rDivClk0 = 0,
    .enableCLK2x = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
    .enableCLK1 = DL_SYSCTL_SYSPLL_CLK1_ENABLE,
    .enableCLK0 = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
    .sysPLLMCLK = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
    .sysPLLRef = DL_SYSCTL_SYSPLL_REF_SYSOSC,
    .qDiv = 9,       /* VCO = 32 MHz / 2 x 10 = 160 MHz */
    .pDiv = DL_SYSCTL_SYSPLL_PDIV_2,
};

static uint32_t measureFCC(DL_SYSCTL_FCC_CLOCK_SOURCE source)
{
    uint32_t timeout = 0;
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK, source);
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0) {
        delay_cycles(977); /* one LFCLK period at 32 MHz */
        if (++timeout > 65) {
            break;
        }
    }
    return DL_SYSCTL_readFCC();
}

/* SYSPLL_ERR_01: confirm the PLL locked at the right ratio */
static bool sysPLLLockedCorrectly(void)
{
    DL_SYSCTL_setFCCPeriods(DL_SYSCTL_FCC_TRIG_CNT_01);
    uint32_t pll = measureFCC(DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK1);
    uint32_t osc = measureFCC(DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    if (osc == 0) {
        return false;
    }
    uint32_t ratio = (pll * 1000U) / osc;
    return (ratio > FCC_LOWER_BOUND) && (ratio < FCC_UPPER_BOUND);
}

void SystemClock_Config(void)
{
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);
    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_configSYSPLL(&sysPLLConfig);
    while (!sysPLLLockedCorrectly()) {
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();
        while ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) != DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD) {
        }
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
}

/* power up a timer and make it a free running / periodic UP counter */
static void initUpCounter(GPTIMER_Regs* timer, uint8_t prescale, uint32_t load)
{
    DL_Timer_reset(timer);
    DL_Timer_enablePower(timer);
    delay_cycles(POWER_STARTUP_DELAY);

    const DL_Timer_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = prescale,
    };
    DL_Timer_setClockConfig(timer, &clk);

    const DL_Timer_TimerConfig cfg = {
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC_UP,
        .period = load,
        .startTimer = DL_TIMER_STOP,
        .genIntermInt = DL_TIMER_INTERM_INT_DISABLED,
        .counterVal = 0,
    };
    DL_Timer_initTimerMode(timer, &cfg);
    DL_Timer_enableClock(timer);
}

void MX_GPIO_Init(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_enablePower(GPIOA);
    delay_cycles(POWER_STARTUP_DELAY);

    /* gate driver pins: drive low as GPIO until the PWM timer takes them */
    const uint32_t gates = (1U << PHASE_A_GPIO_HIGH) | (1U << PHASE_A_GPIO_LOW) | (1U << PHASE_B_GPIO_HIGH) | (1U << PHASE_B_GPIO_LOW) | (1U << PHASE_C_GPIO_HIGH) | (1U << PHASE_C_GPIO_LOW);
    GPIOA->DOUTCLR31_0 = gates;
    GPIOA->DOESET31_0 = gates;
    pincmSetFunction(PHASE_A_IOMUX_HIGH, PINCM_GPIO_FUNC);
    pincmSetFunction(PHASE_A_IOMUX_LOW, PINCM_GPIO_FUNC);
    pincmSetFunction(PHASE_B_IOMUX_HIGH, PINCM_GPIO_FUNC);
    pincmSetFunction(PHASE_B_IOMUX_LOW, PINCM_GPIO_FUNC);
    pincmSetFunction(PHASE_C_IOMUX_HIGH, PINCM_GPIO_FUNC);
    pincmSetFunction(PHASE_C_IOMUX_LOW, PINCM_GPIO_FUNC);
}

void MX_DMA_Init(void)
{
    /* the DMA is always powered on MSPM0; one IRQ line serves all channels */
    NVIC_SetPriority(DMA_INT_IRQn, 1);
    NVIC_EnableIRQ(DMA_INT_IRQn);
}

void MX_PWM_Timer_Init(void)
{
    DL_Timer_reset(PWM_TIMER);
    DL_Timer_enablePower(PWM_TIMER);
    delay_cycles(POWER_STARTUP_DELAY);

    const DL_Timer_ClockConfig clk = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 0,
    };
    DL_TimerA_setClockConfig(PWM_TIMER, &clk);

    /* up counting edge aligned: output high while CTR < CC (STM32 PWM1) */
    const DL_TimerA_PWMConfig pwm = {
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
        .period = (uint32_t)TIM1_AUTORELOAD + 1U,
        .isTimerWithFourCC = true,
        .startTimer = DL_TIMER_STOP,
    };
    DL_TimerA_initPWMMode(PWM_TIMER, &pwm);
    DL_TimerA_setCounterControl(PWM_TIMER, DL_TIMER_CZC_CCCTL0_ZCOND,
        DL_TIMER_CAC_CCCTL0_ACOND, DL_TIMER_CLC_CCCTL0_LCOND);

    for (uint32_t cc = DL_TIMER_CC_0_INDEX; cc <= DL_TIMER_CC_2_INDEX; cc++) {
        DL_TimerA_setCaptureCompareOutCtl(PWM_TIMER, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
            DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_DEAD_BAND,
            (DL_TIMER_CC_INDEX)cc);
        /* new duty takes effect at the start of the next PWM period */
        DL_TimerA_setCaptCompUpdateMethod(PWM_TIMER, DL_TIMER_CC_UPDATE_METHOD_ZERO_EVT,
            (DL_TIMER_CC_INDEX)cc);
        DL_TimerA_setCaptureCompareValue(PWM_TIMER, 0, (DL_TIMER_CC_INDEX)cc);
    }
    /* CC3 is not pinned out; AM32 uses it to time ADC reads */
    DL_TimerA_setCaptureCompareValue(PWM_TIMER, 100, DL_TIMER_CC_3_INDEX);

    setDeadTime(DEAD_TIME);
    DL_TimerA_enableClock(PWM_TIMER);
    DL_TimerA_setCCPDirection(PWM_TIMER, DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT | DL_TIMER_CC2_OUTPUT);
}

void setDeadTime(uint16_t ticks)
{
    /* DEAD_TIME is in TIMA0 clocks (12.5 ns at 80 MHz), same delay on both edges */
    if (ticks > 0xFFF) {
        ticks = 0xFFF;
    }
    DL_TimerA_setDeadBand(PWM_TIMER, ticks, ticks, DL_TIMER_DEAD_BAND_MODE_0);
}

void MX_IC_Timer_Init(void)
{
    /* reconfigured on every receive/send by IO.c; this only powers it up */
    initUpCounter(IC_TIMER_REGISTER, 0, 0xFFFF);
    NVIC_SetPriority(IC_TIMER_IRQ, 0);
}

void MX_COMP_Init(void)
{
    pincmSetAnalog(PHASE_A_COMP_IOMUX);
    pincmSetAnalog(PHASE_B_COMP_IOMUX);
    pincmSetAnalog(PHASE_C_COMP_IOMUX);
    pincmSetAnalog(COMMON_COMP_IOMUX);

    DL_COMP_reset(MAIN_COMP);
    DL_COMP_enablePower(MAIN_COMP);
    delay_cycles(POWER_STARTUP_DELAY);

    /*
     * Phase on IN+, virtual neutral on IN-, output inverted, so the output is
     * high while neutral > phase: the same sense as the G071 port (phase on
     * INM, neutral on INP), which the shared interrupt logic depends on.
     */
    const DL_COMP_Config cfg = {
        .channelEnable = DL_COMP_ENABLE_CHANNEL_POS_NEG,
        .mode = DL_COMP_MODE_FAST,
        .negChannel = DL_COMP_IMSEL_CHANNEL_0,
        .posChannel = DL_COMP_IPSEL_CHANNEL_0,
        .hysteresis = DL_COMP_HYSTERESIS_NONE,
        .polarity = DL_COMP_POLARITY_INV,
    };
    DL_COMP_init(MAIN_COMP, (DL_COMP_Config*)&cfg);
    MAIN_COMP->CTL0 = (MAIN_COMP->CTL0 & ~COMP_CTL0_IMSEL_MASK) | COMMON_COMP;

    DL_COMP_clearInterruptStatus(MAIN_COMP, DL_COMP_INTERRUPT_OUTPUT_EDGE | DL_COMP_INTERRUPT_OUTPUT_EDGE_INV);
    NVIC_SetPriority(COMPARATOR_IRQ, 0);
    NVIC_EnableIRQ(COMPARATOR_IRQ);
}

void MX_IWDG_Init(void)
{
    /* WWDT0 on LFCLK (32.768 kHz) / 4, 2^12 counts -> ~0.5 s, no window.
       Once started it cannot be stopped until reset. */
    DL_WWDT_reset(WWDT0);
    DL_WWDT_enablePower(WWDT0);
    delay_cycles(POWER_STARTUP_DELAY);
    DL_WWDT_initWatchdogMode(WWDT0, DL_WWDT_CLOCK_DIVIDE_4, DL_WWDT_TIMER_PERIOD_12_BITS,
        DL_WWDT_RUN_IN_SLEEP, DL_WWDT_WINDOW_PERIOD_0, DL_WWDT_WINDOW_PERIOD_0);
}

void initCorePeripherals(void)
{
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_COMP_Init();
    MX_PWM_Timer_Init();
    MX_IC_Timer_Init();

    /* PD1 timers at 80 MHz, PD0 timers at 40 MHz */
    initUpCounter(INTERVAL_TIMER, (CPU_FREQUENCY_MHZ / 2) - 1, 0xFFFF);   /* 2 MHz */
    initUpCounter(COM_TIMER, (CPU_FREQUENCY_MHZ / 2) - 1, 0xFFFF);        /* 2 MHz */
    initUpCounter(UTILITY_TIMER, ULPCLK_FREQUENCY_MHZ - 1, 0xFFFF);       /* 1 MHz */
    initUpCounter(TEN_KHZ_TIMER, ULPCLK_FREQUENCY_MHZ - 1,
        (1000000 / LOOP_FREQUENCY_HZ) - 1);                               /* 1 MHz */

    NVIC_SetPriority(COM_TIMER_IRQ, 0);
    NVIC_EnableIRQ(COM_TIMER_IRQ);
    NVIC_SetPriority(TIMG0_INT_IRQn, 2);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);

    /* low priority software interrupt that runs processDshot() */
    NVIC_SetPriority(PendSV_IRQn, 3);

    telem_UART_Init();
}

void initAfterJump(void)
{
    SCB->VTOR = APPLICATION_ADDRESS;
    __enable_irq();
}

void reloadWatchDogCounter(void) { DL_WWDT_restart(WWDT0); }

void setPWMCompare1(uint16_t compareone) { PWM_TIMER->COUNTERREGS.CC_01[0] = compareone; }
void setPWMCompare2(uint16_t comparetwo) { PWM_TIMER->COUNTERREGS.CC_01[1] = comparetwo; }
void setPWMCompare3(uint16_t comparethree) { PWM_TIMER->COUNTERREGS.CC_23[0] = comparethree; }

void generatePwmTimerEvent(void)
{
    /* STM32 UG equivalent: restart the period so new LOAD/CC values apply */
    PWM_TIMER->COUNTERREGS.CTR = 0;
}

void resetInputCaptureTimer(void)
{
    IC_TIMER_REGISTER->COMMONREGS.CPS = 0;
    IC_TIMER_REGISTER->COUNTERREGS.CTR = 0;
}

void enableCorePeripherals(void)
{
    DL_TimerA_startCounter(PWM_TIMER);

#ifndef BRUSHED_MODE
    DL_Timer_startCounter(COM_TIMER);
    COM_TIMER->CPU_INT.ICLR = GPTIMER_CPU_INT_IMASK_L_SET;
    DISABLE_COM_TIMER_INT(); /* armed later by SET_AND_ENABLE_COM_INT */
#endif
    DL_Timer_startCounter(UTILITY_TIMER);
    DL_Timer_startCounter(INTERVAL_TIMER);

    TEN_KHZ_TIMER->CPU_INT.ICLR = GPTIMER_CPU_INT_IMASK_L_SET;
    TEN_KHZ_TIMER->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_L_SET;
    DL_Timer_startCounter(TEN_KHZ_TIMER);

#ifdef USE_ADC
    ADC_Init();
    enableADC_DMA();
    activateADC();
#endif

    DL_COMP_enable(MAIN_COMP);
    delay_cycles(CPU_FREQUENCY_MHZ * 2); /* ~2 us comparator start-up */
}
