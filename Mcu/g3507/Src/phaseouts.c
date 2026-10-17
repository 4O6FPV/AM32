/*
 * phaseouts.c - gate output control for the MSPM0G3507 port
 *
 * Same scheme as the STM32 ports: each gate pin is either handed to TIMA0
 * (IOMUX peripheral function = TIMA0_CCPx / CCPx_CMPL, PWM with dead band)
 * or taken back as a plain GPIO driven high or low. Switching is one IOMUX
 * PINCM write per pin; the GPIO output enables are set once at init.
 *
 * Only the default (complementary, non-inverted) gate driver wiring is
 * implemented. PWM_ENABLE_BRIDGE / USE_INVERTED_* variants are not.
 */

#include "phaseouts.h"

#include "common.h"
#include "peripherals.h"
#include "targets.h"

#if defined(PWM_ENABLE_BRIDGE) || defined(USE_INVERTED_LOW) || defined(USE_INVERTED_HIGH)
#error "G3507 port: enable-bridge and inverted gate variants are not implemented yet"
#endif

extern char prop_brake_active;

#define PIN(bit) (1U << (bit))

static inline void gpioLow(uint32_t bit) { GPIOA->DOUTCLR31_0 = PIN(bit); }
static inline void gpioHigh(uint32_t bit) { GPIOA->DOUTSET31_0 = PIN(bit); }

#define TO_GPIO(iomux) pincmSetFunction((iomux), PINCM_GPIO_FUNC)
#define TO_PWM(iomux, pf) pincmSetFunction((iomux), (pf))

/* one set of PWM/FLOAT/LOW functions per phase */
#define DEFINE_PHASE(X)                                                      \
    static void phase##X##PWM(void)                                          \
    {                                                                        \
        if (!temp_comp_pwm) {                                               \
            gpioLow(PHASE_##X##_GPIO_LOW);                                   \
            TO_GPIO(PHASE_##X##_IOMUX_LOW);                                  \
        } else {                                                             \
            TO_PWM(PHASE_##X##_IOMUX_LOW, PHASE_##X##_PF_LOW);               \
        }                                                                    \
        TO_PWM(PHASE_##X##_IOMUX_HIGH, PHASE_##X##_PF_HIGH);                 \
    }                                                                        \
    static void phase##X##FLOAT(void)                                        \
    {                                                                        \
        gpioLow(PHASE_##X##_GPIO_LOW);                                       \
        gpioLow(PHASE_##X##_GPIO_HIGH);                                      \
        TO_GPIO(PHASE_##X##_IOMUX_LOW);                                      \
        TO_GPIO(PHASE_##X##_IOMUX_HIGH);                                     \
    }                                                                        \
    static void phase##X##LOW(void)                                          \
    {                                                                        \
        gpioLow(PHASE_##X##_GPIO_HIGH);                                      \
        TO_GPIO(PHASE_##X##_IOMUX_HIGH);                                     \
        gpioHigh(PHASE_##X##_GPIO_LOW);                                      \
        TO_GPIO(PHASE_##X##_IOMUX_LOW);                                      \
    }

DEFINE_PHASE(A)
DEFINE_PHASE(B)
DEFINE_PHASE(C)

void proportionalBrake(void)
{
    /* high sides off, low sides on PWM: duty controls braking */
    gpioLow(PHASE_A_GPIO_HIGH);
    gpioLow(PHASE_B_GPIO_HIGH);
    gpioLow(PHASE_C_GPIO_HIGH);
    TO_GPIO(PHASE_A_IOMUX_HIGH);
    TO_GPIO(PHASE_B_IOMUX_HIGH);
    TO_GPIO(PHASE_C_IOMUX_HIGH);
    TO_PWM(PHASE_A_IOMUX_LOW, PHASE_A_PF_LOW);
    TO_PWM(PHASE_B_IOMUX_LOW, PHASE_B_PF_LOW);
    TO_PWM(PHASE_C_IOMUX_LOW, PHASE_C_PF_LOW);
}

void allOff(void)
{
    phaseAFLOAT();
    phaseBFLOAT();
    phaseCFLOAT();
}

void comStep(int newStep)
{
    switch (newStep) {
    case 1: // A-B
        phaseCFLOAT();
        phaseBLOW();
        phaseAPWM();
        break;
    case 2: // C-B
        phaseAFLOAT();
        phaseBLOW();
        phaseCPWM();
        break;
    case 3: // C-A
        phaseBFLOAT();
        phaseALOW();
        phaseCPWM();
        break;
    case 4: // B-A
        phaseCFLOAT();
        phaseALOW();
        phaseBPWM();
        break;
    case 5: // B-C
        phaseAFLOAT();
        phaseCLOW();
        phaseBPWM();
        break;
    case 6: // A-C
        phaseBFLOAT();
        phaseCLOW();
        phaseAPWM();
        break;
    }
}

void fullBrake(void)
{
    phaseALOW();
    phaseBLOW();
    phaseCLOW();
}

void allpwm(void)
{
    phaseAPWM();
    phaseBPWM();
    phaseCPWM();
}

void twoChannelForward(void)
{
    phaseAPWM();
    phaseBLOW();
    phaseCPWM();
}

void twoChannelReverse(void)
{
    phaseALOW();
    phaseBPWM();
    phaseCLOW();
}
