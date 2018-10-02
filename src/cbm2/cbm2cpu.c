/*
 * cbm2cpu.c - Emulation of the main 6509 processor.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
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

#include "vice.h"

#include "cbm2.h"
#include "mem.h"
#include "types.h"

#ifdef FEATURE_CPUMEMHISTORY
#include "monitor.h"
#endif

/* MACHINE_STUFF should define/undef

 - NEED_REG_PC
 - TRACE
 - JUMP

 The following are optional:

 - PAGE_ZERO
 - PAGE_ONE
 - STORE_IND
 - LOAD_IND

*/

/* ------------------------------------------------------------------------- */

#define PAGE_ZERO mem_page_zero
#define PAGE_ONE  mem_page_one

#define LOAD_ZERO(addr) \
    PAGE_ZERO[(addr) & 0xff]

#define STORE_IND(addr, value) \
    (*_mem_write_ind_tab_ptr[(addr) >> 8])((uint16_t)(addr), (uint8_t)(value))

#define LOAD_IND(addr) \
    (*_mem_read_ind_tab_ptr[(addr) >> 8])((uint16_t)(addr))

#ifdef FEATURE_CPUMEMHISTORY
#warning "CPUMEMHISTORY implementation for xcbm2 is incomplete"
static void memmap_mem_store(unsigned int addr, unsigned int value)
{
    monitor_memmap_store(addr, MEMMAP_RAM_W);
    (*_mem_write_tab_ptr[(addr) >> 8])((uint16_t)(addr), (uint8_t)(value));
}

static void memmap_mark_read(unsigned int addr)
{
    monitor_memmap_store(addr, (memmap_state & MEMMAP_STATE_OPCODE) ? MEMMAP_RAM_X : (memmap_state & MEMMAP_STATE_INSTR) ? 0 : MEMMAP_RAM_R);
    memmap_state &= ~(MEMMAP_STATE_OPCODE);
}

static uint8_t memmap_mem_read(unsigned int addr)
{
    memmap_mark_read(addr);
    return (*_mem_read_tab_ptr[(addr) >> 8])((uint16_t)(addr));
}
#endif

#include "../maincpu.c"
