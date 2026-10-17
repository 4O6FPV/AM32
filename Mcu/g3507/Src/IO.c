/*
 * IO.c - DShot / servo PWM input and bidirectional DShot output on the
 * MSPM0G3507 (TIMA1 CC0 on the signal pin, DMA channel INPUT_DMA_CHANNEL).
 *
 * MSPM0 timers cannot trigger the DMA directly. The timer publishes its
 * capture (or zero) event on generic event channel INPUT_EVENT_CHANNEL
 * through FPUB_0, and the DMA subscribes to it through FSUB_0
 * (DMA trigger DMA_GENERIC_SUB0_TRIG).
 *
 * Receive: CC0 captures CTR on both edges, DMA copies each capture into
 *          dma_buffer[] (same layout the STM32 ports produce).
 * Send:    the GCR bit stream is played out one bit per timer period by
 *          letting the DMA rewrite CCACT (the action taken at each zero
 *          event) so each period is driven fully high or fully low.
 *
 * NOT YET VERIFIED ON HARDWARE. Bring-up checks are listed in
 * Mcu/g3507/README.md.
 */

#include "IO.h"

#include "common.h"
#include "dshot.h"
#include "functions.h"
#include "peripherals.h"
#include "serial_telemetry.h"
#include "targets.h"

uint8_t buffer_padding = 7;
char ic_timer_prescaler = CPU_FREQUENCY_MHZ / 6;
uint32_t dma_buffer[64] = { 0 };
volatile char out_put = 0;

/* CCACT words for the telemetry bit stream (one per timer period) */
static uint32_t ccact_buffer[64];

/* telemetry bit period: the G071 port uses ARR 92 at 64 MHz (~1.45 us) */
#define DSHOT_OUT_LOAD ((92U * CPU_FREQUENCY_MHZ) / 64U)

#define CCACT_BIT_HIGH (GPTIMER_CCACT_01_ZACT_CCP_HIGH)
#define CCACT_BIT_LOW (GPTIMER_CCACT_01_ZACT_CCP_LOW)

static uint32_t input_pull = 0;

static void stopInputDma(void)
{
    DL_DMA_disableChannel(DMA, INPUT_DMA_CHANNEL);
    IC_TIMER_REGISTER->COUNTERREGS.CTRCTL &= ~GPTIMER_CTRCTL_EN_MASK;
    IC_TIMER_REGISTER->GEN_EVENT0.IMASK = 0;
}

static void startInputDma(uint32_t src, uint32_t dst, uint32_t count,
    DL_DMA_INCREMENT srcInc, DL_DMA_INCREMENT dstInc)
{
    const DL_DMA_Config cfg = {
        .trigger = DMA_GENERIC_SUB0_TRIG,
        .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL,
        .transferMode = DL_DMA_SINGLE_TRANSFER_MODE,
        .extendedMode = DL_DMA_NORMAL_MODE,
        .srcWidth = DL_DMA_WIDTH_WORD,
        .destWidth = DL_DMA_WIDTH_WORD,
        .srcIncrement = srcInc,
        .destIncrement = dstInc,
    };
    DL_DMA_initChannel(DMA, INPUT_DMA_CHANNEL, &cfg);
    DL_DMA_setSubscriberChanID(DMA, DL_DMA_SUBSCRIBER_INDEX_0, INPUT_EVENT_CHANNEL);
    DL_DMA_setSrcAddr(DMA, INPUT_DMA_CHANNEL, src);
    DL_DMA_setDestAddr(DMA, INPUT_DMA_CHANNEL, dst);
    DL_DMA_setTransferSize(DMA, INPUT_DMA_CHANNEL, count);
    DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    DL_DMA_enableChannel(DMA, INPUT_DMA_CHANNEL);
}

void receiveDshotDma(void)
{
    stopInputDma();
    out_put = 0;

    /* signal pin to TIMA1 CCP0 as an input */
    IOMUX->SECCFG.PINCM[INPUT_PIN_IOMUX] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM_INENA_ENABLE | input_pull | INPUT_PIN_PF;
    IC_TIMER_REGISTER->COMMONREGS.CCPD &= ~GPTIMER_CCPD_C0CCP0_MASK;

    IC_TIMER_REGISTER->COMMONREGS.CPS = (uint8_t)ic_timer_prescaler;
    IC_TIMER_REGISTER->COUNTERREGS.LOAD = 0xFFFF;
    IC_TIMER_REGISTER->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_CM_UP | GPTIMER_CTRCTL_REPEAT_REPEAT_1 | GPTIMER_CTRCTL_CVAE_ZEROVAL;
    IC_TIMER_REGISTER->COUNTERREGS.CCCTL_01[0] = GPTIMER_CCCTL_01_COC_CAPTURE | GPTIMER_CCCTL_01_CCOND_CC_TRIG_EDGE;
    IC_TIMER_REGISTER->COUNTERREGS.IFCTL_01[0] = GPTIMER_IFCTL_01_ISEL_CCPX_INPUT;
    IC_TIMER_REGISTER->COUNTERREGS.CTR = 0;

    /* every capture publishes an event that clocks one DMA transfer */
    DL_Timer_setPublisherChanID(IC_TIMER_REGISTER, DL_TIMER_PUBLISHER_INDEX_0, INPUT_EVENT_CHANNEL);
    IC_TIMER_REGISTER->GEN_EVENT0.ICLR = 0xFFFFFFFFU;
    IC_TIMER_REGISTER->GEN_EVENT0.IMASK = GPTIMER_GEN_EVENT0_IMASK_CCU0_SET | GPTIMER_GEN_EVENT0_IMASK_CCD0_SET;

    startInputDma((uint32_t)&IC_TIMER_REGISTER->COUNTERREGS.CC_01[0], (uint32_t)dma_buffer,
        buffersize, DL_DMA_ADDR_UNCHANGED, DL_DMA_ADDR_INCREMENT);

    IC_TIMER_REGISTER->COUNTERREGS.CTRCTL |= GPTIMER_CTRCTL_EN_ENABLED;
}

void sendDshotDma(void)
{
    stopInputDma();
    out_put = 1;

    /* translate the core's CCR-style GCR values (0 = low, non-zero = high) */
    const uint32_t count = 23 + buffer_padding;
    for (uint32_t i = 0; i < count; i++) {
        ccact_buffer[i] = gcr[i] ? CCACT_BIT_HIGH : CCACT_BIT_LOW;
    }

    IC_TIMER_REGISTER->COMMONREGS.CPS = (uint8_t)output_timer_prescaler;
    IC_TIMER_REGISTER->COUNTERREGS.LOAD = DSHOT_OUT_LOAD;
    IC_TIMER_REGISTER->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_CM_UP | GPTIMER_CTRCTL_REPEAT_REPEAT_1 | GPTIMER_CTRCTL_CVAE_ZEROVAL;
    IC_TIMER_REGISTER->COUNTERREGS.CCCTL_01[0] = GPTIMER_CCCTL_01_COC_COMPARE;
    IC_TIMER_REGISTER->COUNTERREGS.CC_01[0] = DSHOT_OUT_LOAD + 1; /* never matches */
    IC_TIMER_REGISTER->COUNTERREGS.OCTL_01[0] = GPTIMER_OCTL_01_CCPO_FUNCVAL | GPTIMER_OCTL_01_CCPIV_HIGH;
    IC_TIMER_REGISTER->COUNTERREGS.CCACT_01[0] = ccact_buffer[0];
    IC_TIMER_REGISTER->COUNTERREGS.CTR = 0;

    /* each zero event (start of a bit period) loads the next bit's action */
    DL_Timer_setPublisherChanID(IC_TIMER_REGISTER, DL_TIMER_PUBLISHER_INDEX_0, INPUT_EVENT_CHANNEL);
    IC_TIMER_REGISTER->GEN_EVENT0.ICLR = 0xFFFFFFFFU;
    IC_TIMER_REGISTER->GEN_EVENT0.IMASK = GPTIMER_GEN_EVENT0_IMASK_Z_SET;

    startInputDma((uint32_t)&ccact_buffer[1], (uint32_t)&IC_TIMER_REGISTER->COUNTERREGS.CCACT_01[0],
        count - 1, DL_DMA_ADDR_INCREMENT, DL_DMA_ADDR_UNCHANGED);

    /* hand the pin to the timer as an output, then start */
    IC_TIMER_REGISTER->COMMONREGS.CCPD |= GPTIMER_CCPD_C0CCP0_OUTPUT;
    IOMUX->SECCFG.PINCM[INPUT_PIN_IOMUX] = IOMUX_PINCM_PC_CONNECTED | INPUT_PIN_PF;
    IC_TIMER_REGISTER->COUNTERREGS.CTRCTL |= GPTIMER_CTRCTL_EN_ENABLED;
}

void changeToOutput(void) { }
void changeToInput(void) { }

uint8_t getInputPinState(void)
{
    return (GPIOA->DIN31_0 >> INPUT_PIN) & 1U;
}

void setInputPolarityRising(void)
{
    /* both edges are always captured on this port; servo decoding uses the
       buffer length and pin state instead (see transfercomplete()) */
}

static void applyInputPull(uint32_t pull)
{
    input_pull = pull;
    uint32_t pincm = IOMUX->SECCFG.PINCM[INPUT_PIN_IOMUX];
    pincm &= ~(IOMUX_PINCM_PIPU_MASK | IOMUX_PINCM_PIPD_MASK);
    IOMUX->SECCFG.PINCM[INPUT_PIN_IOMUX] = pincm | pull;
}

void setInputPullDown(void) { applyInputPull(IOMUX_PINCM_PIPD_ENABLE); }
void setInputPullUp(void) { applyInputPull(IOMUX_PINCM_PIPU_ENABLE); }
void setInputPullNone(void) { applyInputPull(0); }

void enableHalfTransferInt(void)
{
    /* the G3507 DMA has no early (half transfer) interrupt; not needed
       because both edges are captured */
}
