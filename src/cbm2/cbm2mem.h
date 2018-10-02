/*
 * cbm2mem.h - CBM-II memory handling.
 *
 * Written by
 *  Andre Fachat <fachat@physik.tu-chemnitz.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_CBM2MEM_H
#define VICE_CBM2MEM_H

#include "types.h"

#define CBM2_RAM_SIZE           0x100000        /* maximum 1M */
#define CBM2_ROM_SIZE           0x10000         /* complete bank 15 */
/* 8k ROM (2 banks of 4k), every other 2k is "seen" inverted by the CRTC due
   to some extra logic on the board. we generate a "ROM" that is twice the
   original size at load time so we dont need extra code for this at runtime */
#define CBM2_CHARGEN_ROM_SIZE   0x4000

#define CBM2_CHARGEN500 "chargen" FSDEV_EXT_SEP_STR "500"
#define CBM2_CHARGEN600 "chargen" FSDEV_EXT_SEP_STR "600"
#define CBM2_CHARGEN700 "chargen" FSDEV_EXT_SEP_STR "700"
#define CBM2_BASIC128   "basic" FSDEV_EXT_SEP_STR "128"
#define CBM2_BASIC256   "basic" FSDEV_EXT_SEP_STR "256"
#define CBM2_BASIC500   "basic" FSDEV_EXT_SEP_STR "500"
#define CBM2_KERNAL     "kernal"
#define CBM2_KERNAL500  "kernal" FSDEV_EXT_SEP_STR "500"

extern uint8_t mem_rom[CBM2_ROM_SIZE];
extern uint8_t mem_chargen_rom[CBM2_CHARGEN_ROM_SIZE];

extern void cbm2mem_set_bank_exec(int val);
extern void cbm2mem_set_bank_ind(int val);

extern int cbm2_init_ok;

extern void mem_reset(void);

extern void cbm2_set_tpi1ca(int);
extern void cbm2_set_tpi1cb(int);
extern void cbm2_set_tpi2pc(uint8_t);

extern void c500_set_phi1_bank(int b);
extern void c500_set_phi2_bank(int b);

extern void mem_initialize_memory(void);
extern void mem_powerup(void);
extern void mem_initialize_memory_bank(int i);
extern void mem_set_tape_sense(int value);

extern int cbm2mem_bank_exec;
extern int cbm2mem_bank_ind;

extern void colorram_store(uint16_t addr, uint8_t value);
extern uint8_t colorram_read(uint16_t addr);

extern uint8_t read_unused(uint16_t addr);

extern void mem_handle_pending_alarms_external(int cycles);
extern void mem_handle_pending_alarms_external_write(void);

extern void cbm2io_init(void);
extern void cbm5x0io_init(void);

void cia1_set_extended_keyboard_rows_mask(uint8_t foo);

#endif
