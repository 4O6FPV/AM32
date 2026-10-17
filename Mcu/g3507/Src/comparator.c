/*
 * comparator.c - BEMF zero-crossing detection on COMP0 (MSPM0G3507)
 *
 * The three phase dividers sit on COMP0 positive inputs IN0+/IN2+/IN3+ and
 * the virtual neutral on IN0-. Only the positive mux changes per step.
 * COMP0 raises COMPIFG on a rising output edge and COMPINVIFG on a falling
 * edge (CTL1.IES = rising), so edge selection is just a mask choice.
 */

#include "comparator.h"

#include "common.h"
#include "peripherals.h"
#include "targets.h"

#define COMP_EDGE_RISE COMP_CPU_INT_IMASK_COMPIFG_SET
#define COMP_EDGE_FALL COMP_CPU_INT_IMASK_COMPINVIFG_SET
#define COMP_EDGE_BOTH (COMP_EDGE_RISE | COMP_EDGE_FALL)

/* the edge the next zero crossing will produce, set by changeCompInput() */
static uint32_t armed_edge = COMP_EDGE_FALL;

uint32_t current_EXTI_LINE = 0; /* referenced by the shared core */

uint8_t getCompOutputLevel(void)
{
    return (uint8_t)(MAIN_COMP->STAT & COMP_STAT_OUT_MASK);
}

void maskPhaseInterrupts(void)
{
    MAIN_COMP->CPU_INT.IMASK &= ~COMP_EDGE_BOTH;
    MAIN_COMP->CPU_INT.ICLR = COMP_EDGE_BOTH;
}

void enableCompInterrupts(void)
{
    MAIN_COMP->CPU_INT.ICLR = COMP_EDGE_BOTH;
    MAIN_COMP->CPU_INT.IMASK = (MAIN_COMP->CPU_INT.IMASK & ~COMP_EDGE_BOTH) | armed_edge;
}

void changeCompInput(void)
{
    uint32_t ipsel;

    if (step == 1 || step == 4) { // c floating
        ipsel = PHASE_C_COMP;
    } else if (step == 2 || step == 5) { // a floating
        ipsel = PHASE_A_COMP;
    } else { // b floating
        ipsel = PHASE_B_COMP;
    }
    MAIN_COMP->CTL0 = (MAIN_COMP->CTL0 & ~COMP_CTL0_IPSEL_MASK) | ipsel;

    /* same sense as the G071 port: rising BEMF -> falling comparator output */
    armed_edge = rising ? COMP_EDGE_FALL : COMP_EDGE_RISE;
    if (MAIN_COMP->CPU_INT.IMASK & COMP_EDGE_BOTH) {
        MAIN_COMP->CPU_INT.IMASK = (MAIN_COMP->CPU_INT.IMASK & ~COMP_EDGE_BOTH) | armed_edge;
    }
}
