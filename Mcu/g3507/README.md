# AM32 on the TI MSPM0G3507

Status: **builds, not yet run on hardware.** Treat it as a starting point for
bring-up on a bench supply with current limiting, not as flight firmware.

```
make NDAA_REF_G3507        # -> obj/AM32_NDAA_REF_G3507_<ver>.bin/.hex/.elf
```

The build uses the same pinned toolchain as the rest of AM32
(`make arm_sdk_install`). The TI files the port needs are vendored under
`Drivers/` (BSD-3-Clause / Apache-2.0, see `Drivers/README.md`), so no SDK
install is required.

## Pin map (VQFN-32, RHB)

Every PINCM index and peripheral function below was cross-checked against the
`IOMUX_PINCMx_PF_*` definitions in TI's `mspm0g350x.h`.

| Pin | Port | AM32 role | Function |
|---|---|---|---|
| 7 | PA3 | Phase B high | TIMA0_CCP1 (PF 8) |
| 8 | PA4 | Phase B low | TIMA0_CCP1_CMPL (PF 8) |
| 10 | PA6 | Phase C low | TIMA0_CCP2_CMPL (PF 8) |
| 11 | PA7 | Phase C high | TIMA0_CCP2 (PF 5) |
| 12 | PA8 | Phase A high | TIMA0_CCP0 (PF 5) |
| 13 | PA9 | Phase A low | TIMA0_CCP0_CMPL (PF 7) |
| 14 | PA10 | Signal in | TIMA1_CCP0 (PF 5) |
| 18 | PA14 | BEMF B | COMP0_IN2+ |
| 19 | PA15 | BEMF C | COMP0_IN3+ (DAC0 must stay off: DAC_OUT shares this pin) |
| 22 | PA18 | unused, 10k pull-down | ROM bootloader invoke pin |
| 23 / 24 | PA19 / PA20 | SWDIO / SWCLK | |
| 25 | PA21 | Battery voltage | A1_7 |
| 27 | PA23 | Telemetry TX | UART2_TX (PF 2) |
| 29 | PA25 | Current sense | A0_2 |
| 30 | PA26 | BEMF A | COMP0_IN0+ |
| 31 | PA27 | Virtual neutral | COMP0_IN0- |

Spare: PA0, PA1 (open-drain only), PA2, PA5, PA11, PA12, PA13, PA16, PA17,
PA22, PA24.

The signal input uses TIMA1 rather than TIMG12. TIMG12 has no prescaler, so it
cannot produce the 1 MHz tick AM32 uses to measure servo PWM.

## Resource map

| Use | Peripheral | Clock |
|---|---|---|
| Gate PWM, dead band | TIMA0 CC0-CC2 + complementary outputs | 80 MHz |
| DShot/PWM capture, DShot telemetry out | TIMA1 CC0 + DMA ch0 | 80 MHz |
| INTERVAL_TIMER | TIMG7 | 2 MHz |
| COM_TIMER | TIMG6 | 2 MHz |
| UTILITY_TIMER (delayMicros) | TIMG8 | 1 MHz |
| TEN_KHZ_TIMER (control loop) | TIMG0 | 1 MHz |
| BEMF comparator | COMP0, IRQ via INT_GROUP1 | |
| Current + die temperature | ADC0 MEM0/MEM1 | 4 MHz ADCCLK |
| Battery voltage | ADC1 MEM0 | 4 MHz ADCCLK |
| KISS telemetry | UART2 TX + DMA ch1 | 115200 |
| Settings | flash 0x1F800 (1 KB sectors) | |
| Watchdog | WWDT0, ~0.5 s | LFCLK |
| processDshot() software IRQ | PendSV (lowest priority) | |

Flash layout: AM32 bootloader 0x0000-0x0FFF, application from 0x1000,
`FILE_NAME` at 0x1F7E0, settings at 0x1F800. NONMAIN (BCR/BSL config) is not
touched.

## Changes outside this folder

- `g3507makefile.mk` (new) and `G3507` added to `MCU_TYPES` in `Makefile`
- `Inc/targets.h`: `NDAA_REF_G3507` target, `HARDWARE_GROUP_TI_G3507_A`, and
  the `MCU_G3507` block, which defines the vendor macro `TI_MSPM0`
- `TI_MSPM0` branches in `Src/functions.c` (microsecond timer) and
  `Src/main.c` (dead-time override, COM timer reload, ADC start/temperature,
  bootloader devinfo -> settings address with a 0x0 flash base)

## Not done yet

- **AM32 bootloader for MSPM0.** Until it exists, use
  `scripts/make_bench_image.py` to build an image with a two-word boot stub at
  0x0 and flash it over SWD. No one-wire flashing or configurator support yet.
- **Configurator support.** The AM32 configurator identifies the MCU from the
  bootloader, so a new MCU id has to be added there.
- **Debug probe config.** The OpenOCD bundled with `arm_sdk_install` predates
  MSPM0 support; `openocd.cfg` assumes a newer build with `target/ti/mspm0.cfg`.
  TI UniFlash with an XDS110 (the LP-MSPM0G3507 has one) also works for SWD.
- Gate-driver variants (`PWM_ENABLE_BRIDGE`, inverted outputs), LED strips,
  hall sensors, CAN, and comparator blanking from the PWM timer.

## Bring-up checklist (in order of risk)

1. **DShot capture DMA re-triggering.** Timers reach the DMA only through a
   generic event channel (TIMA1 FPUB_0 -> channel 1 -> DMA FSUB_0). Check on a
   scope/debugger that `dma_buffer` fills with 32 increasing timestamps per
   frame. If the DMA moves only one word, the timer event flag is not being
   cleared between captures; read TRM (SLAU846) chapter 7 "Events" and either
   use a different event channel type or clear GEN_EVENT0 RIS in the flow.
2. **Comparator sense.** With phases on IN+ and the neutral on IN-, COMP0 runs
   with an inverted output so the shared interrupt code sees the same polarity
   as the G071. Confirm `COMP0->STAT.OUT` reports the post-inversion level
   (`getCompOutputLevel()`); if it reports the raw level, invert it there.
3. **Bidirectional DShot output.** Bits are produced by DMA rewriting TIMA1
   CCACT (zero-event action) every bit period, 1.45 us at 80 MHz like the G071
   port. Verify waveform and timing with a logic analyser before enabling
   EDT in a flight controller.
4. **PWM reload.** Variable PWM rewrites TIMA0 LOAD at run time. The datasheet
   lists shadow load for TIMA0; confirm LOAD changes take effect at the period
   boundary (no long pulse when LOAD drops below the current count).
5. **Dead time.** `DEAD_TIME` is in TIMA0 clocks (12.5 ns). 60 = 750 ns.
   Check on the gates with the motor disconnected before applying power.
6. **Temperature.** Uses TI's typical 1.8 mV/C slope and the factory 30 C
   trim against a 3.3 V VDDA reference; check against a thermocouple.
7. **Clock.** SYSPLL setup follows TI's `sysctl_mclk_syspll` example
   (80 MHz MCLK, 40 MHz ULPCLK) including the SYSPLL_ERR_01 lock check.
