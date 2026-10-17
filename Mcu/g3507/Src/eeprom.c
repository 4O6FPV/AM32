/*
 * eeprom.c - settings storage in main flash for the MSPM0G3507 port
 *
 * MSPM0 flash: 1 KB sectors, programmed in 64-bit words with ECC. The
 * command routines run from SRAM (DriverLib "FromRAM" variants, placed in
 * .ramfunc by the linker script). Like the STM32 ports, the sector is
 * erased when the write starts on a sector boundary.
 */

#include "eeprom.h"

#include <string.h>

#define FLASH_SECTOR_SIZE 0x400U

void save_flash_nolib(uint8_t* data, int length, uint32_t add)
{
    /* 64-bit aligned staging buffer; pad the tail with erased bytes */
    uint32_t words[2];
    int offset = 0;

    __disable_irq();

    if ((add % FLASH_SECTOR_SIZE) == 0) {
        DL_FlashCTL_unprotectSector(FLASHCTL, add, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, add, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    }

    while (offset < length) {
        int chunk = length - offset;
        if (chunk > 8) {
            chunk = 8;
        }
        memset(words, 0xFF, sizeof(words));
        memcpy(words, &data[offset], (size_t)chunk);

        DL_FlashCTL_unprotectSector(FLASHCTL, add + (uint32_t)offset, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, add + (uint32_t)offset, words);
        offset += 8;
    }

    __enable_irq();
}

void read_flash_bin(uint8_t* data, uint32_t add, int out_buff_len)
{
    for (int i = 0; i < out_buff_len; i++) {
        data[i] = *(volatile uint8_t*)(add + (uint32_t)i);
    }
}
