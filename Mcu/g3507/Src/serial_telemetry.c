/*
 * serial_telemetry.c - KISS telemetry on UART2 TX for the MSPM0G3507 port
 *
 * 115200 8N1 from BUSCLK (UART2 is on PD0, ULPCLK = 40 MHz). The DMA
 * channel is triggered by the UART TX FIFO request and needs no interrupt.
 */

#include "serial_telemetry.h"

#include "common.h"
#include "kiss_telemetry.h"
#include "peripherals.h"
#include "targets.h"

static const DL_UART_Main_ClockConfig uartClock = {
    .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
};

static const DL_UART_Main_Config uartConfig = {
    .mode = DL_UART_MAIN_MODE_NORMAL,
    .direction = DL_UART_MAIN_DIRECTION_TX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity = DL_UART_MAIN_PARITY_NONE,
    .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
};

void telem_UART_Init(void)
{
    DL_UART_Main_reset(SERIAL_TELEMETRY_UART);
    DL_UART_Main_enablePower(SERIAL_TELEMETRY_UART);
    delay_cycles(16);

    /* Open drain (HIZ1), like LL_GPIO_OUTPUT_OPENDRAIN on the G071 targets: the
     * four MCUs of a 4-in-1 share one TLM wire and a single pull-up. */
    pincmSetFunction(TELEM_TX_IOMUX, TELEM_TX_PF);
    IOMUX->SECCFG.PINCM[TELEM_TX_IOMUX] |= IOMUX_PINCM_HIZ1_ENABLE;

    DL_UART_Main_setClockConfig(SERIAL_TELEMETRY_UART, (DL_UART_Main_ClockConfig*)&uartClock);
    DL_UART_Main_init(SERIAL_TELEMETRY_UART, (DL_UART_Main_Config*)&uartConfig);
    /* 40 MHz / (16 x 115200) = 21.70 -> IBRD 21, FBRD 45 */
    DL_UART_Main_setOversampling(SERIAL_TELEMETRY_UART, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(SERIAL_TELEMETRY_UART, 21, 45);
    DL_UART_Main_enableFIFOs(SERIAL_TELEMETRY_UART);
    DL_UART_Main_setTXFIFOThreshold(SERIAL_TELEMETRY_UART, DL_UART_TX_FIFO_LEVEL_EMPTY);
    DL_UART_Main_enableDMATransmitEvent(SERIAL_TELEMETRY_UART);
    DL_UART_Main_enable(SERIAL_TELEMETRY_UART);

    const DL_DMA_Config cfg = {
        .trigger = TELEM_DMA_TRIGGER,
        .triggerType = DL_DMA_TRIGGER_TYPE_EXTERNAL,
        .transferMode = DL_DMA_SINGLE_TRANSFER_MODE,
        .extendedMode = DL_DMA_NORMAL_MODE,
        .srcWidth = DL_DMA_WIDTH_BYTE,
        .destWidth = DL_DMA_WIDTH_BYTE,
        .srcIncrement = DL_DMA_ADDR_INCREMENT,
        .destIncrement = DL_DMA_ADDR_UNCHANGED,
    };
    DL_DMA_initChannel(DMA, TELEM_DMA_CHANNEL, &cfg);
    DL_DMA_setDestAddr(DMA, TELEM_DMA_CHANNEL, (uint32_t)&SERIAL_TELEMETRY_UART->TXDATA);
}

void send_telem_DMA(uint8_t bytes)
{
    DL_DMA_disableChannel(DMA, TELEM_DMA_CHANNEL);
    DL_DMA_setSrcAddr(DMA, TELEM_DMA_CHANNEL, (uint32_t)aTxBuffer);
    DL_DMA_setTransferSize(DMA, TELEM_DMA_CHANNEL, bytes);
    DL_DMA_enableChannel(DMA, TELEM_DMA_CHANNEL);
}
